#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

/**
 * @class Texture
 * @brief Retains RGBA pixels and rebuildable OpenGL or Vulkan resources.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * All calls require the render thread. images are stored vertically flipped for OpenGL;
 * Vulkan compensates in UVs. non-RGBA input expands to four channels.
 *
 * GL names are deleted only while their owning context generation is current; stale names
 * are abandoned to avoid deleting a new context's resource. Vulkan resources must be
 * released before their VkDevice. The destructor logs and frees surviving Vulkan handles
 * through the borrowed device.
 *
 * @verbatim
 *   [Image File] --LoadFromFile()--> [CPU Buffer] --Upload--> [GPU Memory]
 *                                         |                        |
 *                                    m_ImageData              m_OpenGLID or
 *                                   (always kept)             m_VulkanImage
 * @endverbatim
 *
 * @verbatim
 *   OpenGL:              Vulkan:
 *   (0,1)-----(1,1)      (0,0)-----(1,0)
 *     |         |          |         |
 *     |  Image  |          |  Image  |
 *     |         |          |         |
 *   (0,0)-----(1,0)      (0,1)-----(1,1)
 * @endverbatim
 *
 * @code{.cpp}
 * Texture tex;
 * tex.LoadFromFile("sprites/player.png");  // CPU copy now; GL upload too if a context is current.
 *
 * std::vector<unsigned char> pixels(64 * 64 * 4);
 * tex.LoadFromData(pixels.data(), 64, 64, 4, true);
 *
 * tex.Bind(0);
 * tex.Unbind();
 *
 * tex.RecreateOpenGLTexture();  // After a context switch, rebuild from the CPU copy.
 *
 * tex.CreateVulkanTexture(device, physicalDevice, commandPool, queue);
 * tex.DestroyVulkanTexture(device);
 * @endcode
 */
class Texture
{
public:
    Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /**
     * @fn Texture(Texture&& other) noexcept
     * @brief Leaves the source empty.
     * @author Alex (<https://github.com/lextpf>)
     */
    Texture(Texture&& other) noexcept;

    /// Releases owned resources and leaves the source empty.
    Texture& operator=(Texture&& other) noexcept;

    ~Texture();

    /**
     * @fn bool LoadFromFile(const std::string& path)
     * @brief Decodes pixels and retains an RGBA CPU copy.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Uploads to OpenGL if a context is current; Vulkan upload is deferred.
     * Failures log and return false. decode failure preserves state; later validation failure
     * clears pixels but retains rejected dimensions. test GetImageData().empty() for load state.
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @fn bool LoadFromData(unsigned char* data, int width, int height, int channels, bool flipY \
     * = true)
     * @brief Copies consecutive pixels and expands non-RGBA input.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param data Non-null buffer of width * height * channels bytes.
     * @param width Positive pixel count.
     * @param height Positive pixel count.
     * @param channels 1 grayscale, 2 grayscale-alpha, 3 RGB, or 4 RGBA.
     * @param flipY Flips rows for the OpenGL origin.
     * @return False for invalid input.
     */
    bool LoadFromData(unsigned char* data, int width, int height, int channels, bool flipY = true);

    /**
     * @fn void Bind(unsigned int slot = 0) const
     * @brief Activate texture unit slot and bind this texture to it.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Bind(unsigned int slot = 0) const;

    /**
     * @fn void Unbind() const
     * @brief Clears GL_TEXTURE_2D on the currently active unit.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Does not select the unit used by the last Bind call.
     */
    void Unbind() const;

    /**
     * @fn unsigned int GetID() const
     * @brief Get the OpenGL texture name, or 0 when nothing is uploaded.
     * @author Alex (<https://github.com/lextpf>)
     */
    unsigned int GetID() const { return m_OpenGLID; }

    /**
     * @fn void RecreateOpenGLTexture() const
     * @brief Recreates a GL texture from retained pixels after a context change.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Requires CPU pixels and a current GL context. Otherwise logs an error and leaves the name 0.
     */
    void RecreateOpenGLTexture() const;

    /**
     * @fn static void AdvanceOpenGLContextGeneration()
     * @brief Advances generation once per new GL context, before texture use.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void AdvanceOpenGLContextGeneration();

    /**
     * @fn static std::uint64_t GetCurrentOpenGLContextGeneration()
     * @brief Current GL context generation; stale texture names must not be used.
     * @author Alex (<https://github.com/lextpf>)
     */
    static std::uint64_t GetCurrentOpenGLContextGeneration();

    /**
     * @fn VkImageView GetVulkanImageView() const
     * @brief Get the image view shaders sample, or VK_NULL_HANDLE before creation.
     * @author Alex (<https://github.com/lextpf>)
     */
    VkImageView GetVulkanImageView() const { return m_VulkanImageView; }

    /**
     * @fn VkSampler GetVulkanSampler() const
     * @brief Get the sampler shaders filter with, or VK_NULL_HANDLE before creation.
     * @author Alex (<https://github.com/lextpf>)
     */
    VkSampler GetVulkanSampler() const { return m_VulkanSampler; }

    /**
     * @fn void CreateVulkanTexture(VkDevice device, VkPhysicalDevice physicalDevice, \
     * VkCommandPool commandPool, VkQueue queue) const
     * @brief Allocates and uploads a nearest-filtered Vulkan texture.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Empty pixels log and leave handles unchanged. other failures throw std::runtime_error,
     * including non-RGBA input, size overflow, or an undersized CPU buffer.
     *
     * Existing Vulkan resources are destroyed before replacement. The staging copy completes
     * synchronously before return, so this call can block the render thread.
     *
     * @param device Borrowed for destruction; must outlive this texture's Vulkan handles.
     * @param physicalDevice Physical device used to select compatible memory types.
     * @param commandPool Pool for temporary transfer commands on the supplied queue family.
     * @param queue Queue used for the upload; completion is awaited before return.
     */
    void CreateVulkanTexture(VkDevice device,
                             VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool,
                             VkQueue queue) const;

    /**
     * @fn void DestroyVulkanTexture(VkDevice device) const
     * @brief Releases Vulkan handles; repeated calls are safe.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param device VK_NULL_HANDLE reuses the stored device. If both are null, handles are
     *               abandoned.
     */
    void DestroyVulkanTexture(VkDevice device) const;

    /**
     * @fn int GetWidth() const
     * @brief Pixel width; zero before loading, not a load-success test.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetWidth() const { return m_Width; }

    /**
     * @fn int GetHeight() const
     * @brief Pixel height; zero before loading, not a load-success test.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetHeight() const { return m_Height; }

    /**
     * @fn int GetChannels() const
     * @brief Four channels after a successful load, regardless of source format.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetChannels() const { return m_Channels; }

    /**
     * @fn std::uint64_t GetOpenGLContextGeneration() const
     * @brief Get the context generation this texture's GL name was created in.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::uint64_t GetOpenGLContextGeneration() const { return m_OpenGLContextGeneration; }

    const std::vector<unsigned char>& GetImageData() const { return m_ImageData; }

    /**
     * @fn glm::vec3 SampleDominantNonSkinColor(glm::vec3 fallback) const
     * @brief Selects a saturated accent color from CPU pixels.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Maximizes saturation * value after rejecting these pixels:
     *
     * | channel    | rejected range                         |
     * |------------|----------------------------------------|
     * | alpha      | below 128                              |
     * | saturation | below 0.30                             |
     * | value      | below 0.25                             |
     * | skin band  | hue 0-30 degrees, saturation 0.20-0.60 |
     *
     * $ saturation \times value $
     *
     * @return Fallback if no pixel survives.
     */
    glm::vec3 SampleDominantNonSkinColor(glm::vec3 fallback) const;

private:
    /**
     * @fn void CreateOpenGLTexture(const unsigned char* data, bool flipY) const
     * @brief Uses nearest filtering and CLAMP_TO_EDGE wrapping.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param data RGBA pixels for the retained width and height; must be non-null.
     * @param flipY Unused; callers flip pixels before upload.
     */
    void CreateOpenGLTexture(const unsigned char* data, bool flipY) const;

    static std::uint64_t s_CurrentOpenGLContextGeneration;  ///< Global context generation counter.

    /// Mutable GPU handles cache the unchanged CPU pixels.
    mutable unsigned int m_OpenGLID{0};  ///< GL texture name; 0 = not created.
    /// Diagnostic context address; generation determines ownership.
    mutable void* m_OpenGLContextTag{nullptr};
    /// Upload generation; abandon the GL name on mismatch.
    mutable std::uint64_t m_OpenGLContextGeneration{0};

    mutable VkImage m_VulkanImage{VK_NULL_HANDLE};               ///< Device-local image.
    mutable VkDeviceMemory m_VulkanImageMemory{VK_NULL_HANDLE};  ///< Memory backing the image.
    mutable VkImageView m_VulkanImageView{VK_NULL_HANDLE};       ///< View shaders sample through.
    mutable VkSampler m_VulkanSampler{VK_NULL_HANDLE};           ///< Nearest-neighbor sampler.
    /// Borrowed device; release Vulkan resources before destroying it.
    mutable VkDevice m_VulkanDevice{VK_NULL_HANDLE};

    int m_Width{0};                          ///< Width in pixels.
    int m_Height{0};                         ///< Height in pixels.
    int m_Channels{0};                       ///< Stored channels; 4 after any successful load.
    std::vector<unsigned char> m_ImageData;  ///< Retained RGBA pixels; source for every upload.
};
