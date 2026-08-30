#pragma once

#include "IRenderer.hpp"
#include "PostFXParams.hpp"
#include "RendererMacros.hpp"

#include <vulkan/vulkan.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

struct GLFWwindow;

/**
 * @class VulkanRenderer
 * @brief Vulkan backend with per-quad draws and cached descriptors.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Two frame slots own mapped vertex buffers and synchronization objects. early returns after
 * acquire recreate the image-available semaphore; failed submits also recreate the fence as
 * signaled. recovery retries the same frame slot.
 *
 * Texture uploads are synchronous and may throw. descriptors allocate on first draw and remain
 * cached until shutdown, including entries for replaced image views. missing uploads use white
 * fallbacks or skip drawing. 2D additive flags, PostFX, and clear color arguments are ignored.
 *
 * @warning The 176-byte 2D push block exceeds Vulkan's 128-byte minimum; the device limit is
 * not queried. negative-height viewports also require Vulkan 1.1 or VK_KHR_maintenance1,
 * but instance/device creation declares neither requirement.
 *
 * ```mermaid
 * flowchart LR
 *     classDef acquire fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef record  fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef draw    fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef submit  fill:#7f1d1d,stroke:#ef4444,color:#e2e8f0
 *     classDef present fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 *
 *     A[BeginFrame]:::acquire --> B[Wait fence + acquire image]:::acquire
 *     B --> C[Begin command buffer]:::record
 *     C --> D[Begin render pass]:::record
 *     D --> E[DrawSprite / DrawText appends 6 verts]:::draw
 *     E --> F[SubmitQuad records vkCmdDraw immediately]:::draw
 *     F --> E
 *     E --> G[EndFrame]:::submit
 *     G --> H[End render pass + command buffer]:::submit
 *     H --> I[Submit + present]:::present
 * ```
 *
 * ```mermaid
 * flowchart TD
 *     classDef ok  fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef fix fill:#7f1d1d,stroke:#ef4444,color:#e2e8f0
 *
 *     A[Wait m_InFlightFences frame]:::ok --> B[Acquire swapchain image]:::ok
 *     B --> C[Wait m_ImagesInFlight image]:::ok
 *     C --> D[Record: command buffer, render pass, draws]:::ok
 *     D --> E[EndFrame: end render pass + command buffer]:::ok
 *     E --> F[vkResetFences]:::ok
 *     F --> G[Submit: wait ImageAvailable, signal RenderFinished + fence]:::ok
 *     G --> H[Present: wait RenderFinished]:::ok
 *     H --> I[frame = frame + 1 mod 2]:::ok
 *     B -- OUT_OF_DATE --> W[RecreateImageAvailableSemaphore + RecreateSwapchain]:::fix
 *     B -- acquire error --> R[RecreateImageAvailableSemaphore]:::fix
 *     E -- bounds or vkEndCommandBuffer failure --> R
 *     F -- reset failed --> R
 *     G -- submit failed --> S[Destroy + recreate fence SIGNALED]:::fix
 *     S --> R
 * ```
 *
 * @code
 *   Frame 0: Write to m_VertexBuffers[0], GPU reads m_VertexBuffers[1]
 *   Frame 1: Write to m_VertexBuffers[1], GPU reads m_VertexBuffers[0]
 * @endcode
 */
class VulkanRenderer : public IRenderer
{
public:
    explicit VulkanRenderer(GLFWwindow* window);
    ~VulkanRenderer() override;

    RIFT_DECLARE_COMMON_RENDERER_METHODS;

    void SetFontCandidates(const std::vector<std::string>& fontCandidates) override;

    bool RequiresYFlip() const override { return true; }

    void SetAmbientColor(const glm::vec3& color) override
    {
        if (color == m_AmbientColor)
            return;
        FlushSpriteBatch();
        m_AmbientColor = color;
    }

    int GetDrawCallCount() const override { return m_DrawCallCount; }

private:
    RendererInfo m_Info;  ///< Cached at end of init(); returned by GetBackendInfo().

    /**
     * @struct SpriteVertex
     * @brief Per-vertex data for batched sprite rendering.
     * @author Alex (<https://github.com/lextpf>)
     */
    struct SpriteVertex
    {
        float pos[2];  ///< Screen-space position (x, y).
        float tex[2];  ///< Texture coordinates (u, v).
    };

    /**
     * @fn static void BuildQuadVertices(SpriteVertex outVertices[6], const glm::vec2 corners[4], \
     * const glm::vec2 texCoords[4])
     * @brief Expands corners in top-left, top-right, bottom-right, bottom-left order.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void BuildQuadVertices(SpriteVertex outVertices[6],
                                  const glm::vec2 corners[4],
                                  const glm::vec2 texCoords[4]);

    /**
     * @fn bool SubmitQuad(VkDescriptorSet descriptorSet, const SpriteVertex vertices[6], \
     * glm::vec3 spriteColor, float spriteAlpha, bool useColorOnly = false, glm::vec4 colorOnly = \
     * glm::vec4(0.0f), bool applyAmbient = true)
     * @brief Records one immediate six-vertex draw.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param descriptorSet Sampled image binding compatible with the sprite pipeline layout.
     * @param vertices Six view-space vertices copied into this frame's vertex buffer.
     * @param spriteColor RGB multiplier for sampled texture colors.
     * @param spriteAlpha Additional opacity multiplier for textured draws.
     * @param useColorOnly Use the uniform rectangle color instead of sampling the texture.
     * @param colorOnly RGBA rectangle color when texture sampling is bypassed.
     * @param applyAmbient False preserves self-lit color.
     * @return False without an active frame or enough vertex capacity.
     */
    bool SubmitQuad(VkDescriptorSet descriptorSet,
                    const SpriteVertex vertices[6],
                    glm::vec3 spriteColor,
                    float spriteAlpha,
                    bool useColorOnly = false,
                    glm::vec4 colorOnly = glm::vec4(0.0f),
                    bool applyAmbient = true);

    int m_DrawCallCount = 0;         ///< Draw calls this frame.
    glm::vec3 m_AmbientColor{1.0f};  ///< Current ambient light color.

    /**
     * @struct Glyph
     * @brief Per-character Vulkan texture and metrics for text rendering.
     * @author Alex (<https://github.com/lextpf>)
     */
    struct Glyph
    {
        VkImage image{VK_NULL_HANDLE};          ///< Vulkan image for this glyph.
        VkDeviceMemory memory{VK_NULL_HANDLE};  ///< Device memory backing the image.

        /**
         * @brief Borrows m_WhiteTextureImageView for zero-size glyphs.
         *
         * Compare before destruction to avoid releasing the shared fallback twice.
         */
        VkImageView imageView{VK_NULL_HANDLE};

        glm::ivec2 size{0, 0};     ///< Glyph dimensions in pixels.
        glm::ivec2 bearing{0, 0};  ///< Offset from baseline to top-left.
        unsigned int advance{0};   ///< Horizontal advance to next character.
    };

    /**
     * @fn void LoadFont()
     * @brief Load ttf font and create per-glyph Vulkan textures.
     * @author Alex (<https://github.com/lextpf>)
     */
    void LoadFont();

    void CreateGlyphTexture(int width,
                            int height,
                            const std::vector<unsigned char>& rgbaData,
                            Glyph& outGlyph);

    std::map<char, Glyph> m_Glyphs;             ///< Cached glyph textures.
    std::vector<std::string> m_FontCandidates;  ///< Project-specific font candidates.

#ifdef USE_FREETYPE
    FT_Library m_FreeType{nullptr};
    FT_Face m_Face{nullptr};
#endif

    VkInstance m_Instance{VK_NULL_HANDLE};              ///< Vulkan API entry point.
    VkPhysicalDevice m_PhysicalDevice{VK_NULL_HANDLE};  ///< Selected GPU.
    VkDevice m_Device{VK_NULL_HANDLE};                  ///< Logical device for commands.
    VkQueue m_GraphicsQueue{VK_NULL_HANDLE};            ///< Queue for draw commands.
    VkQueue m_PresentQueue{VK_NULL_HANDLE};             ///< Queue for presentation.

    VkSurfaceKHR m_Surface{VK_NULL_HANDLE};          ///< Window surface.
    VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};      ///< Presentation swapchain.
    std::vector<VkImage> m_SwapchainImages;          ///< Swapchain images.
    std::vector<VkImageView> m_SwapchainImageViews;  ///< Views into swapchain images.
    std::vector<VkFramebuffer> m_SwapchainFramebuffers;
    VkExtent2D m_SwapchainExtent{};                        ///< Swapchain dimensions.
    VkFormat m_SwapchainImageFormat{VK_FORMAT_UNDEFINED};  ///< Pixel format.

    VkRenderPass m_RenderPass{VK_NULL_HANDLE};          ///< Defines attachment usage.
    VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};  ///< Descriptor/push constant layout.
    VkPipeline m_GraphicsPipeline{VK_NULL_HANDLE};      ///< Compiled shader + state.

    VkCommandPool m_CommandPool{VK_NULL_HANDLE};  ///< Command buffer allocator.

    /**
     * @brief Allocated per initial swapchain image but indexed by frame slot.
     *
     * Swapchain recreation does not resize this vector.
     */
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;  ///< Swapchain image ready.
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;  ///< Rendering complete.
    std::vector<VkFence> m_InFlightFences;                ///< CPU-GPU sync.
    std::vector<VkFence> m_ImagesInFlight;                ///< Per-image fence tracking.
    VkFence m_TransferFence{VK_NULL_HANDLE};  ///< Fence for synchronous transfer operations.

    size_t m_CurrentFrame{0};       ///< Current frame index (0 or 1).
    uint32_t m_ImageIndex{0};       ///< Acquired swapchain image index.
    bool m_FrameActive{false};      ///< True after BeginFrame started a render pass.
    GLFWwindow* m_Window{nullptr};  ///< GLFW window reference.
    glm::mat4 m_Projection{1.0f};   ///< Current orthographic projection.

    /**
     * @brief Shares the render pass and command buffer between 2D and 3D draws.
     *
     * Each draw path must claim its pipeline. clear m_Bound3DPipeline at BeginFrame because
     * bindings do not survive a command buffer.
     *
     * ```mermaid
     * stateDiagram-v2
     *     [*] --> Bound2D: BeginFrame binds m_GraphicsPipeline, clears m_Bound3DPipeline
     *     Bound2D --> Bound3D: DrawQuad3D - bind depth/blend pipeline, re-apply Y-flip
     *     Bound3D --> Bound3D: same depth/blend pair - no rebind; other pair - rebind
     *     Bound3D --> Bound2D: SubmitQuad rebinds 2D, clears m_Bound3DPipeline
     * ```
     */

    /**
     * @struct Vertex3D
     * @brief Must match OpenGLRenderer::BatchVertex3D and geometry3d.vert.
     * @author Alex (<https://github.com/lextpf>)
     */
    struct Vertex3D
    {
        float x, y, z;     ///< Scene-space position (see sceneMath).
        float u, v;        ///< Texture coordinates.
        float r, g, b, a;  ///< Per-vertex RGBA tint.
    };

    /**
     * @struct Push3D
     * @brief 80-byte geometry3d push layout.
     * @author Alex (<https://github.com/lextpf>)
     */
    struct Push3D
    {
        glm::mat4 viewProjection;  ///< Offset 0, 64 bytes.
        glm::vec3 ambientColor;    ///< Offset 64, 12 bytes.
        float alphaCutoff;         ///< Offset 76, 4 bytes.
    };

    /**
     * @brief Combined view-projection for the 3D path, already corrected for Vulkan's
     * clip space by `SetViewProjection`.
     */
    glm::mat4 m_ViewProjection{1.0f};

    VkImage m_DepthImage{VK_NULL_HANDLE};
    VkDeviceMemory m_DepthImageMemory{VK_NULL_HANDLE};
    VkImageView m_DepthImageView{VK_NULL_HANDLE};
    VkFormat m_DepthFormat{VK_FORMAT_UNDEFINED};

    /// Six pipelines indexed by DepthMode then BlendMode.
    VkPipeline m_Pipeline3D[renderModes::DEPTH_MODE_COUNT][renderModes::BLEND_MODE_COUNT]{};
    VkPipelineLayout m_Pipeline3DLayout{VK_NULL_HANDLE};
    /// Last pipeline bound this frame, to skip redundant vkCmdBindPipeline calls.
    VkPipeline m_Bound3DPipeline{VK_NULL_HANDLE};

    /**
     * @fn VkFormat FindDepthFormat() const
     * @brief Pick a supported depth format, preferring 32-bit float.
     * @author Alex (<https://github.com/lextpf>)
     */
    VkFormat FindDepthFormat() const;
    /**
     * @fn void CreateDepthResources()
     * @brief Create the depth image/memory/view for the current swapchain extent.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CreateDepthResources();
    /**
     * @fn void DestroyDepthResources()
     * @brief Destroy the depth image/memory/view (swapchain recreate + shutdown).
     * @author Alex (<https://github.com/lextpf>)
     */
    void DestroyDepthResources();
    /**
     * @fn void CreatePipeline3D()
     * @brief Create the six geometry3d pipelines and their shared layout.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CreatePipeline3D();

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t DESCRIPTOR_POOL_MAX_SETS = 1000;
    VkBuffer m_VertexBuffers[MAX_FRAMES_IN_FLIGHT]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory m_VertexBufferMemories[MAX_FRAMES_IN_FLIGHT]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    void* m_VertexBuffersMapped[MAX_FRAMES_IN_FLIGHT]{nullptr, nullptr};  ///< Persistent mapping.
    /// Unused; every draw uses non-indexed vertices.
    VkBuffer m_IndexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_IndexBufferMemory{VK_NULL_HANDLE};
    VkDeviceSize m_VertexBufferSize{0};
    uint32_t m_CurrentVertexCount{0};

    /// Separate storage preserves Vertex3D stride for firstVertex addressing.
    VkBuffer m_Vertex3DBuffers[MAX_FRAMES_IN_FLIGHT]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory m_Vertex3DMemories[MAX_FRAMES_IN_FLIGHT]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    void* m_Vertex3DMapped[MAX_FRAMES_IN_FLIGHT]{nullptr, nullptr};
    VkDeviceSize m_Vertex3DBufferSize{0};
    uint32_t m_Current3DVertexCount{0};

    /// Unused batch state; all draw paths submit quads immediately.
    VkImageView m_BatchImageView{VK_NULL_HANDLE};          ///< Always VK_NULL_HANDLE today.
    VkDescriptorSet m_BatchDescriptorSet{VK_NULL_HANDLE};  ///< Always VK_NULL_HANDLE today.
    /// Remains zero while no batch descriptor is assigned.
    uint32_t m_BatchStartVertex{0};
    /**
     * @fn void FlushSpriteBatch()
     * @brief Returns without drawing while the batch descriptor is null.
     * @author Alex (<https://github.com/lextpf>)
     */
    void FlushSpriteBatch();

    /// Unused; uploads allocate temporary staging buffers.
    VkBuffer m_StagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_StagingBufferMemory{VK_NULL_HANDLE};
    void* m_StagingBufferMapped{nullptr};

    VkDescriptorPool m_DescriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_DescriptorSetLayout{VK_NULL_HANDLE};
    VkSampler m_TextureSampler{VK_NULL_HANDLE};  ///< Shared texture sampler.
    std::unordered_map<VkImageView, VkDescriptorSet> m_DescriptorSetCache;
    std::vector<VkDescriptorPool> m_OverflowPools;  ///< Additional pools created on overflow.
    bool m_DescriptorPoolWarned{false};

    VkImage m_WhiteTextureImage{VK_NULL_HANDLE};
    VkDeviceMemory m_WhiteTextureImageMemory{VK_NULL_HANDLE};
    VkImageView m_WhiteTextureImageView{VK_NULL_HANDLE};
    /// Unused sampler; descriptors use m_TextureSampler.
    VkSampler m_WhiteTextureSampler{VK_NULL_HANDLE};

    /**
     * @struct TextureResources
     * @brief Borrowed handles; never destroy cache entries.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Uploaded entries borrow the texture image view; fallback entries borrow the white texture.
     */
    struct TextureResources
    {
        VkImage image;          ///< VK_NULL_HANDLE unless this is a white-texture entry.
        VkDeviceMemory memory;  ///< VK_NULL_HANDLE unless this is a white-texture entry.
        VkImageView imageView;  ///< Image view for shader sampling.
        bool initialized;       ///< True once the entry resolved to a usable view.
    };
    std::unordered_map<const Texture*, TextureResources> m_TextureCache;
    std::vector<const Texture*> m_UploadedTextures;
    std::unordered_set<const Texture*> m_UploadedTextureSet;  ///< o(1) dedup for uploads.

    void CreateInstance();

    void CreateSurface();

    void PickPhysicalDevice();

    void CreateLogicalDevice();

    void CreateSwapchain();

    void CreateImageViews();
    /**
     * @fn void CreateRenderPass()
     * @brief Create the single-subpass render pass: color attachment 0, depth 1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @pre `CreateDepthResources()` has run - the format is read from `m_DepthFormat`.
     */
    void CreateRenderPass();

    void CreateGraphicsPipeline();

    void CreateFramebuffers();

    void CreateCommandPool();
    /**
     * @fn void CreateCommandBuffers()
     * @brief Allocate the primary command buffers, one per swapchain image.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @note Only run from init(); `RecreateSwapchain` does not reallocate them.
     */
    void CreateCommandBuffers();

    void CreateSyncObjects();
    /**
     * @fn void RecreateImageAvailableSemaphore(size_t frame)
     * @brief Replaces an acquire semaphore after an ambiguous or unconsumed signal.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RecreateImageAvailableSemaphore(size_t frame);

    void CreateBuffers();

    void CreateDescriptorPool();

    void CreateWhiteTexture();

    void CreateTextureSampler();

    void CleanupSwapchain();

    void RecreateSwapchain();
    bool m_FramebufferResized{false};  ///< Set by resize callback to trigger swapchain recreation.

    /**
     * @fn TextureResources& GetOrCreateTexture(const Texture& texture)
     * @brief Unused cache path; draw calls resolve image views directly.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return Borrowed entry valid until the cache is cleared.
     */
    TextureResources& GetOrCreateTexture(const Texture& texture);
    /**
     * @fn VkDescriptorSet GetOrCreateDescriptorSet(VkImageView imageView)
     * @brief Caches descriptors with the shared sampler until shutdown.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Pool exhaustion retains an additional DESCRIPTOR_POOL_MAX_SETS pool.
     *
     * @return VK_NULL_HANDLE for null views, missing pool, or allocation failure.
     */
    VkDescriptorSet GetOrCreateDescriptorSet(VkImageView imageView);
    /**
     * @fn glm::mat4 CalculateModelMatrix(glm::vec2 position, glm::vec2 size, float rotation)
     * @brief Rotates about the sprite centre; rotation is in degrees.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::mat4 CalculateModelMatrix(glm::vec2 position, glm::vec2 size, float rotation);

    /**
     * @fn uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
     * @brief Returns a matching memory type or throws std::runtime_error.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Buffer helpers can also throw through VK_CHECK. init catches failures; frame-loop calls
     * must allow propagation.
     */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    /**
     * @fn void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags \
     * properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
     * @brief Allocates buffer storage; size is in bytes.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CreateBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer,
                      VkDeviceMemory& bufferMemory);
    /**
     * @fn void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
     * @brief Synchronously copies size bytes through a one-shot command.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    /**
     * @fn void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, \
     * VkImageLayout newLayout)
     * @brief Records a color-image barrier in the current frame.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @pre Between BeginFrame and EndFrame.
     * @note Supports only undefined to TRANSFER_DST_OPTIMAL and then SHADER_READ_ONLY_OPTIMAL;
     * Other pairs throw std::runtime_error. format is unused.
     */
    void TransitionImageLayout(VkImage image,
                               VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    /**
     * @fn void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t \
     * height)
     * @brief Records a buffer-to-image copy in the current frame.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param buffer Source buffer with tightly packed pixels for the full image extent.
     * @param image Destination image with storage for the requested extent.
     * @param width Pixels.
     * @param height Pixels.
     * @pre Between BeginFrame and EndFrame; image layout is TRANSFER_DST_OPTIMAL.
     */
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    /**
     * @fn void UploadStagingBufferToImage(VkBuffer stagingBuffer, VkImage image, uint32_t width, \
     * uint32_t height)
     * @brief Copies and transitions an image using a synchronous one-shot submit.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param stagingBuffer Host upload buffer with tightly packed pixels for the full image
     * extent.
     * @param image Destination image with storage for the requested extent.
     * @param width Pixels.
     * @param height Pixels.
     */
    void UploadStagingBufferToImage(VkBuffer stagingBuffer,
                                    VkImage image,
                                    uint32_t width,
                                    uint32_t height);

    uint32_t m_GraphicsFamily{0};
    uint32_t m_PresentFamily{0};

    const std::vector<const char*> m_ValidationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char*> m_DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
};
