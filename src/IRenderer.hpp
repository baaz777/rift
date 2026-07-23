#pragma once

#include "PostFXParams.hpp"
#include "RenderModes.hpp"
#include "Texture.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

/**
 * @struct RendererInfo
 * @brief Backend identity populated after Init().
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
struct RendererInfo
{
    std::string backendName;  ///< "OpenGL" or "Vulkan".
    std::string apiVersion;
    std::string vendor;
    std::string device;
    std::string driverVersion;  ///< Driver version string (Vulkan); empty for GL.
    int maxTextureSize = 0;     ///< Largest 2D texture dimension supported.
};

/**
 * @class IRenderer
 * @brief Rendering contract shared by OpenGL and Vulkan.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Game owns the renderer. renderer.set replaces both the renderer and GLFW window;
 * Cached references and GPU handles become invalid.
 *
 * 2D draws use the last SetProjection matrix; callers subtract the camera position.
 * DrawQuad3D uses SetViewProjection with scene coordinates. Scene projection spans
 * screen pixels / PIXEL_SCALE / zoom; UI projection spans screen pixels. Tile width
 * and height are independent map values.
 *
 * BeginScene binds the OpenGL scene target before Clear. EndSceneApplyPostFX composites
 * it to the swapchain; subsequent UI draws bypass grading. Vulkan draws directly to the swapchain.
 *
 * Edit RIFT_DECLARE_COMMON_RENDERER_METHODS whenever a pure virtual signature changes.
 *
 * ```mermaid
 * classDiagram
 * class IRenderer:::abstract {
 *     <<interface>>
 *     +Init()
 *     +Shutdown()
 *     +BeginFrame()
 *     +EndFrame()
 *     +DrawSprite()
 *     +DrawText()
 * }
 * class OpenGLRenderer:::opengl {
 *     +Init()
 *     +DrawSprite()
 * }
 * class VulkanRenderer:::vulkan {
 *     +Init()
 *     +DrawSprite()
 * }
 * IRenderer <|-- OpenGLRenderer
 * IRenderer <|-- VulkanRenderer
 * style IRenderer fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * style OpenGLRenderer fill:#134e3a,stroke:#10b981,color:#e2e8f0
 * style VulkanRenderer fill:#2e1f5e,stroke:#8b5cf6,color:#e2e8f0
 * ```
 *
 * ```mermaid
 * flowchart LR
 * W["World Space"]:::space
 * V["View Space
 * (world pixels)"]:::space
 * U["UI Space
 * (screen pixels)"]:::space
 * N["Normalized Device
 * Coordinates"]:::space
 * F["Framebuffer
 * pixels"]:::space
 * classDef space fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * W -->|subtract camera| V
 * V -->|scene ortho| N
 * U -->|UI ortho| N
 * N -->|viewport| F
 * ```
 *
 * ```mermaid
 * flowchart LR
 * BF["BeginFrame()"]:::stage
 * subgraph SCENE["offscreen scene FBO on OpenGL, swapchain on Vulkan"]
 * BS["BeginScene()"]:::stage
 * CL["Clear()"]:::stage
 * SP["SetProjection
 * (scene ortho)"]:::stage
 * D2["2D scene draws"]:::stage
 * D3["SetViewProjection
 * + DrawQuad3D"]:::stage
 * BS --> CL --> SP --> D2 --> D3
 * end
 * PX["EndSceneApplyPostFX()"]:::stage
 * UI["SetProjection (UI ortho)
 * + UI draws
 * swapchain, ungraded"]:::stage
 * EF["EndFrame()"]:::stage
 * classDef stage fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * BF --> BS
 * D3 --> PX --> UI --> EF
 * ```
 *
 * $$
 * M_{ortho} = \begin{bmatrix}
 *   \frac{2}{r-l} & 0 & 0 & -\frac{r+l}{r-l} \\
 *   0 & \frac{2}{t-b} & 0 & -\frac{t+b}{t-b} \\
 *   0 & 0 & -\frac{2}{f-n} & -\frac{f+n}{f-n} \\
 *   0 & 0 & 0 & 1
 * \end{bmatrix}
 * $$
 *
 * $$
 * u = \frac{pixelX}{textureWidth}, \quad v = \frac{pixelY}{textureHeight}
 * $$
 *
 * L/t = 0; r/b = view width/height; n = -1; f = 1.
 * Scene extents use world pixels; UI extents use screen pixels.
 *
 * A 1280x720 window at `PIXEL_SCALE` 5 and zoom 1.0 gives a 256x144 view extent:
 * - $ l=0, \; r=256, \; t=0, \; b=144 $
 * - $ (128, 72) \rightarrow (0, 0) $ (center)
 * - $ (0, 0) \rightarrow (-1, +1) $ (top-left)
 * - $ (256, 144) \rightarrow (+1, -1) $ (bottom-right)
 *
 * The UI ortho for the same window uses $ r=1280, \; b=720 $ instead.
 *
 * For a 256x256 texture, pixel (128, 64) becomes UV (0.5, 0.25).
 */
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    /**
     * @fn void SetFontCandidates(const std::vector<std::string>& fontCandidates)
     * @brief Call before Init(); font paths are tried in order before backend fallbacks.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void SetFontCandidates(const std::vector<std::string>& fontCandidates) = 0;

    /**
     * @fn bool Init()
     * @brief Call after window creation and before drawing.
     * @author Alex (<https://github.com/lextpf>)
     *
     * on false, call Shutdown and destroy the renderer.
     */
    [[nodiscard]] virtual bool Init() = 0;

    /**
     * @fn void Shutdown()
     * @brief Release GPU resources before destroying the window.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void Shutdown() = 0;

    /**
     * @fn void BeginFrame()
     * @brief Call before drawing.
     * @author Alex (<https://github.com/lextpf>)
     *
     * OpenGL clearing requires a separate Clear call.
     */
    virtual void BeginFrame() = 0;

    /**
     * @fn void EndFrame()
     * @brief Flush and submit the frame.
     * @author Alex (<https://github.com/lextpf>)
     *
     * the caller swaps GLFW buffers for OpenGL.
     */
    virtual void EndFrame() = 0;

    /**
     * @fn void BeginScene()
     * @brief Bind the OpenGL offscreen scene target before clearing.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Vulkan draws directly to the swapchain.
     */
    virtual void BeginScene() = 0;

    /**
     * @fn void EndSceneApplyPostFX(const PostFXParams& params)
     * @brief Composite the OpenGL scene, then route subsequent UI draws to the swapchain.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Vulkan ignores params.
     */
    virtual void EndSceneApplyPostFX(const PostFXParams& params) = 0;

    /**
     * @fn void DrawSprite(const Texture& texture, glm::vec2 position, glm::vec2 size = \
     * glm::vec2(32.0f, 32.0f), float rotation = 0.0f, glm::vec3 color = glm::vec3(1.0f))
     * @brief Sample the full texture, with position at its top-left in the current projection.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `size` is in pixels. rotation is clockwise in degrees about the sprite center.
     * DrawSpriteRegion takes pixel UV bounds; DrawSpriteAtlas takes normalized UV bounds.
     * OpenGL ignores color for DrawSprite and DrawSpriteRegion; use alpha or atlas draws for tint.
     *
     * $$
     * \vec{uv}_{min} = \frac{\vec{texCoord}}{\vec{textureSize}}
     * $$
     *
     * $$
     * \vec{uv}_{max} = \frac{\vec{texCoord} + \vec{texSize}}{\vec{textureSize}}
     * $$
     */
    virtual void DrawSprite(const Texture& texture,
                            glm::vec2 position,
                            glm::vec2 size = glm::vec2(32.0f, 32.0f),
                            float rotation = 0.0f,
                            glm::vec3 color = glm::vec3(1.0f)) = 0;

    /**
     * @fn void DrawSpriteAlpha(const Texture& texture, glm::vec2 position, glm::vec2 size, float \
     * rotation, glm::vec4 color, bool additive = false)
     * @brief Apply RGBA tint to a full texture.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `position` is the top-left in the current projection; size is in pixels.
     * `rotation` is clockwise in degrees. Vulkan ignores additive.
     */
    virtual void DrawSpriteAlpha(const Texture& texture,
                                 glm::vec2 position,
                                 glm::vec2 size,
                                 float rotation,
                                 glm::vec4 color,
                                 bool additive = false) = 0;

    /**
     * @fn void DrawSpriteRegion(const Texture& texture, glm::vec2 position, glm::vec2 size, \
     * glm::vec2 texCoord, glm::vec2 texSize, float rotation = 0.0f, glm::vec3 color = \
     * glm::vec3(1.0f), bool flipY = true, bool tileFlipX = false, bool tileFlipY = false)
     * @brief Sample a texture region in pixels.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `position` is the top-left in the current projection; size is in pixels.
     * `rotation` is clockwise in degrees. flipY selects the texture-origin convention;
     * tileFlipX and tileFlipY mirror the source region before rotation.
     * OpenGL ignores color; use alpha or atlas draws for tint.
     */
    virtual void DrawSpriteRegion(const Texture& texture,
                                  glm::vec2 position,
                                  glm::vec2 size,
                                  glm::vec2 texCoord,
                                  glm::vec2 texSize,
                                  float rotation = 0.0f,
                                  glm::vec3 color = glm::vec3(1.0f),
                                  bool flipY = true,
                                  bool tileFlipX = false,
                                  bool tileFlipY = false) = 0;

    /**
     * @fn void DrawSpriteAtlas(const Texture& texture, glm::vec2 position, glm::vec2 size, \
     * glm::vec2 uvMin, glm::vec2 uvMax, float rotation, glm::vec4 color, bool additive = false)
     * @brief Sample normalized UV bounds with RGBA tint.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `position` is the top-left in the current projection; size is in pixels.
     * `rotation` is clockwise in degrees. Vulkan ignores additive.
     */
    virtual void DrawSpriteAtlas(const Texture& texture,
                                 glm::vec2 position,
                                 glm::vec2 size,
                                 glm::vec2 uvMin,
                                 glm::vec2 uvMax,
                                 float rotation,
                                 glm::vec4 color,
                                 bool additive = false) = 0;

    /**
     * @fn void DrawColoredRect(glm::vec2 position, glm::vec2 size, glm::vec4 color, bool \
     * additive = false)
     * @brief Fill a rectangle in the current projection with RGBA components from 0 to 1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `position` is the top-left. Vulkan ignores additive.
     *
     * $$
     * C_{out} = C_{src} \times \alpha + C_{dst} \times (1 - \alpha)
     * $$
     *
     * $$
     * C_{out} = C_{src} \times \alpha + C_{dst}
     * $$
     *
     * c_src is the rectangle color; c_dst is the existing pixel; alpha is opacity.
     * - $ \alpha = 0.5 $: 50% mix of both colors
     * - $ \alpha = 1.0 $: fully opaque, destination hidden
     * - $ \alpha = 0.0 $: fully transparent, destination unchanged
     */
    virtual void DrawColoredRect(glm::vec2 position,
                                 glm::vec2 size,
                                 glm::vec4 color,
                                 bool additive = false) = 0;

    /**
     * @fn void DrawQuad3D(const Texture& texture, const glm::vec3 corners[4], glm::vec2 \
     * texCoord, glm::vec2 texSize, glm::vec4 color = glm::vec4(1.0f), renderModes::BlendMode \
     * blend = renderModes::BlendMode::Alpha, renderModes::DepthMode depth = \
     * renderModes::DepthMode::TestAndWrite, bool flipY = true, bool tileFlipX = false, bool \
     * tileFlipY = false, renderModes::LightMode light = renderModes::LightMode::Ambient)
     * @brief Submit a scene-space quad using the last SetViewProjection matrix.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `corners` are ordered TL, TR, BR, BL; do not subtract the camera.
     * texCoord and texSize are in pixels. flipY selects the texture-origin convention;
     * tileFlipX and tileFlipY mirror the source region. light selects ambient tint or
     * self-lighting.
     *
     * @code
     * corners[0] (TL)----corners[1] (TR)
     *      |                   |
     * corners[3] (BL)----corners[2] (BR)
     * @endcode
     */
    virtual void DrawQuad3D(const Texture& texture,
                            const glm::vec3 corners[4],
                            glm::vec2 texCoord,
                            glm::vec2 texSize,
                            glm::vec4 color = glm::vec4(1.0f),
                            renderModes::BlendMode blend = renderModes::BlendMode::Alpha,
                            renderModes::DepthMode depth = renderModes::DepthMode::TestAndWrite,
                            bool flipY = true,
                            bool tileFlipX = false,
                            bool tileFlipY = false,
                            renderModes::LightMode light = renderModes::LightMode::Ambient) = 0;

    /**
     * @fn void SetViewProjection(const glm::mat4& viewProjection)
     * @brief Use projection * view.
     * @author Alex (<https://github.com/lextpf>)
     *
     * changing the matrix flushes the pending 3D batch.
     */
    virtual void SetViewProjection(const glm::mat4& viewProjection) = 0;

    /**
     * @fn void SetProjection(const glm::mat4& projection)
     * @brief Flush all pending batches before changing the 2D projection; this preserves painter
     * order.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void SetProjection(const glm::mat4& projection) = 0;

    /**
     * @fn void SetViewport(int x, int y, int width, int height)
     * @brief Viewport in screen pixels, with x/y at the bottom-left.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    /**
     * @fn void SetViewSize(glm::vec2 size)
     * @brief Store the zoomed world-view extent in world pixels; match the scene projection
     * dimensions.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetViewSize(glm::vec2 size) { m_ViewSize = size; }

    [[nodiscard]] glm::vec2 GetViewSize() const { return m_ViewSize; }

    /**
     * @fn void Clear(float r, float g, float b, float a)
     * @brief RGBA components range from 0 to 1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Vulkan ignores them and clears during BeginFrame.
     */
    virtual void Clear(float r, float g, float b, float a) = 0;

    /**
     * @fn void UploadTexture(const Texture& texture)
     * @brief Ensure the texture has storage for the active graphics backend.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The texture remains owned by the caller. Vulkan retains its address for shutdown cleanup;
     * keep it alive and at the same address until the renderer shuts down.
     * Its retained CPU pixels supply the upload.
     * OpenGL skips a texture already uploaded in the current context generation; Vulkan recreates
     * its resources and waits for the upload to finish, so repeated calls can be expensive.
     * Upload Vulkan textures before the frame that samples them. A missing image view may render
     * as the white fallback instead of uploading during a render pass.
     */
    virtual void UploadTexture(const Texture& texture) = 0;

    /**
     * @fn void DrawText(const std::string& text, glm::vec2 position, float scale = 1.0f, \
     * glm::vec3 color = glm::vec3(1.0f), float outlineSize = 1.0f, float alpha = 0.85f)
     * @brief Draw text with X at the left edge and Y at the glyph baseline in the current
     * projection.
     * @author Alex (<https://github.com/lextpf>)
     *
     * ASCII is supported; other glyphs depend on the font and backend.
     * Alpha ranges from 0 to 1. outlineSize scales outline thickness.
     */
    virtual void DrawText(const std::string& text,
                          glm::vec2 position,
                          float scale = 1.0f,
                          glm::vec3 color = glm::vec3(1.0f),
                          float outlineSize = 1.0f,
                          float alpha = 0.85f) = 0;

    /**
     * @fn float GetTextAscent(float scale = 1.0f) const
     * @brief Scaled font ascent in pixels, measured above the baseline.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual float GetTextAscent(float scale = 1.0f) const = 0;

    /**
     * @fn float GetTextWidth(const std::string& text, float scale = 1.0f) const
     * @brief Width in pixels from scaled glyph advances.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual float GetTextWidth(const std::string& text, float scale = 1.0f) const = 0;

    /**
     * @fn void DrawTextLarge(const std::string& text, glm::vec2 position, float scale = 1.0f, \
     * glm::vec3 color = glm::vec3(1.0f), float outlineSize = 1.0f, float alpha = 0.85f)
     * @brief Draw text relative to the headline atlas logical size.
     * @author Alex (<https://github.com/lextpf>)
     *
     * OpenGL uses 96 px headline metrics and 24 px body metrics; both atlases bake at 96 px.
     * The default implementation scales the body atlas by the headline ratio.
     */
    virtual void DrawTextLarge(const std::string& text,
                               glm::vec2 position,
                               float scale = 1.0f,
                               glm::vec3 color = glm::vec3(1.0f),
                               float outlineSize = 1.0f,
                               float alpha = 0.85f);

    /**
     * @fn float GetTextWidthLarge(const std::string& text, float scale = 1.0f) const
     * @brief Headline width in pixels; the default scales body metrics by the headline ratio.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] virtual float GetTextWidthLarge(const std::string& text,
                                                  float scale = 1.0f) const;

    /**
     * @fn bool RequiresYFlip() const
     * @brief Both backends return true to sample pre-flipped textures consistently.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual bool RequiresYFlip() const = 0;

    /**
     * @fn void SetAmbientColor(const glm::vec3& color)
     * @brief RGB multiplier for lit sprites; (1, 1, 1) leaves colors unchanged.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void SetAmbientColor(const glm::vec3& color) = 0;

    /**
     * @fn int GetDrawCallCount() const
     * @brief GPU batch flush count since BeginFrame.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual int GetDrawCallCount() const = 0;

    /**
     * @fn RendererInfo GetBackendInfo() const
     * @brief Empty strings and zero limits before Init().
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] virtual RendererInfo GetBackendInfo() const = 0;

protected:
    glm::vec2 m_ViewSize{0.0f};

    /**
     * @fn static void RotateCorners(glm::vec2 corners[4], glm::vec2 size, float rotation)
     * @brief Rotate corners ordered TL, TR, BR, BL about the sprite center; rotation is in
     * degrees.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void RotateCorners(glm::vec2 corners[4], glm::vec2 size, float rotation);
};
