#pragma once

#include "IRenderer.hpp"
#include "PostFXParams.hpp"
#include "RendererMacros.hpp"

#include <glad/glad.h>
#include <map>
#include <string>
#include <vector>

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

/**
 * @class OpenGLRenderer
 * @brief OpenGL 4.6 renderer with sprite, rect, particle, text and 3D batches.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Texture, blend state and capacity changes flush the affected batch. cross-type drains
 * are asymmetric; the drain matrix defines painter order. non-empty sprite, rect
 * and particle flushes also drain text. BeginScene and EndSceneApplyPostFX leave text queued,
 * so scene text reaches the swapchain after compositing.
 *
 * 2D batches share geometry shaders. sampled alpha below 0.1 is discarded in modes 0
 * and 3; vertex alpha does not affect this cutoff. 2D winding differs for text, with culling off.
 *
 * ASCII glyphs use body and headline atlases with 8 texels of padding. The particle
 * batch shares the rect VAO/VBO; their flushes must run sequentially.
 *
 * ```mermaid
 * flowchart LR
 * classDef add fill:#134e3a,stroke:#10b981,color:#e2e8f0
 * classDef flush fill:#7f1d1d,stroke:#ef4444,color:#e2e8f0
 * classDef check fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *
 * A["DrawSprite(texA)"]:::add --> B{Same texture?}:::check
 * B -->|Yes| C["Add vertices to batch"]:::add
 * B -->|No| D["FlushBatch()"]:::flush
 * D --> E["Record texture (bound at next flush)"]:::add
 * E --> C
 * C --> J{Batch full?}:::check
 * J -->|Yes| D
 * J -->|No| F["DrawSprite(texA)"]:::add
 * F --> B
 * G["EndFrame()"]:::flush --> H["FlushBatch()"]:::flush
 * H --> I["Caller swaps buffers"]
 * ```
 *
 * @code
 * // Sprite quad as two triangles (6 vertices)
 * // TL--TR      TL, BR, BL (triangle 1)
 * // | \  |  =>  TL, TR, BR (triangle 2)
 * // BL--BR
 *
 * DrawSprite(texA, pos1)  // vertices 0-5   -> batch size: 6
 * DrawSprite(texA, pos2)  // vertices 6-11  -> batch size: 12
 * DrawSprite(texA, pos3)  // vertices 12-17 -> batch size: 18
 * DrawSprite(texB, pos4)  // FLUSH! draw 18 vertices, then start new batch
 * @endcode
 *
 * | trigger                       | Sprite | Rect | Particle | text | 3D  |
 * |-------------------------------|--------|------|----------|------|-----|
 * | DrawSprite / DrawSpriteRegion |        | x    |          |      |     |
 * | DrawSpriteAtlas / ...Alpha    | x      | x    |          |      |     |
 * | DrawColoredRect               | x      |      |          |      |     |
 * | DrawQuad3D                    | x      | x    | x        |      |     |
 * | DrawText / DrawTextLarge      | x      | x    | x        |      |     |
 * | SetAmbientColor               | x      |      |          |      |     |
 * | SetViewProjection             |        |      |          |      | x   |
 * | SetProjection                 | x      | x    | x        | x    | x   |
 * | BeginScene / endsceneapplypfx | x      | x    | x        |      | x   |
 * | EndFrame                      | x      | x    | x        | x    | x   |
 * | FlushBatch/Rect/Particle      |        |      |          | x*   |     |
 */
struct GLFWwindow;

/**
 * @fn void SetDebugDrawSleep(GLFWwindow* window, bool enabled)
 * @brief Present each batch and pause two seconds for render-order inspection.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The process-wide setting survives renderer switches. window is borrowed and must
 * outlive the enabled period. input and game time continue during the pauses.
 */
void SetDebugDrawSleep(GLFWwindow* window, bool enabled);

/**
 * @fn void ResetDebugDrawCallIndex()
 * @brief Reset trace numbering once per frame.
 * @author Alex (<https://github.com/lextpf>)
 */
void ResetDebugDrawCallIndex();

bool IsDebugDrawSleepEnabled();

class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer();
    ~OpenGLRenderer() override;

    RIFT_DECLARE_COMMON_RENDERER_METHODS;

    void DrawTextLarge(const std::string& text,
                       glm::vec2 position,
                       float scale,
                       glm::vec3 color,
                       float outlineSize,
                       float alpha) override;

    [[nodiscard]] float GetTextWidthLarge(const std::string& text, float scale) const override;

    void SetFontCandidates(const std::vector<std::string>& fontCandidates) override;

    bool RequiresYFlip() const override { return true; }

    void SetAmbientColor(const glm::vec3& color) override;

    int GetDrawCallCount() const override { return m_DrawCallCount; }

private:
    RendererInfo m_Info;

    /**
     * @fn unsigned int EnsureTextureReady(const Texture& texture)
     * @brief Ensure the texture belongs to the current context; return 0 on failure.
     * @author Alex (<https://github.com/lextpf>)
     */
    unsigned int EnsureTextureReady(const Texture& texture);

    /**
     * @fn void SetupQuad()
     * @brief All 2D buffers use attributes 0 = position, 1 = UV, 2 = RGBA; disable 2 without color
     * data.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetupQuad();

    void CreateWhiteTexture();

    /**
     * @fn void LoadFont(const std::string& fontPath)
     * @brief Build body and headline glyph atlases from the same font face.
     * @author Alex (<https://github.com/lextpf>)
     */
    void LoadFont(const std::string& fontPath);

    /// Missing glyphs are skipped; control characters may have zero-size cells.
    struct Character
    {
        glm::ivec2 Size;
        glm::ivec2 Bearing;  ///< Offset from baseline to top-left, in atlas texels.
        /// Horizontal advance in 1/64 px; shift right by 6 before scaling.
        unsigned int Advance;

        float u0, v0, u1, v1;
    };

    /// Multiply atlas-texel metrics by BODY_METRIC_NORM for logical body size.
    std::map<char, Character> m_Characters;
    /// RGBA glyph coverage, with white RGB; 0 when no font is loaded. mipmaps support minification.
    unsigned int m_FontAtlasTexture;
    /// Atlas dimensions in texels; both rounded up to a power of two.
    int m_FontAtlasWidth, m_FontAtlasHeight;

    std::map<char, Character> m_HeadlineCharacters;
    unsigned int m_HeadlineFontAtlasTexture = 0;
    int m_HeadlineFontAtlasWidth = 0;
    int m_HeadlineFontAtlasHeight = 0;

    /// Physical bake size in pixels; body text is supersampled 4x.
    static constexpr int BODY_FONT_PIXEL_SIZE = 96;
    /// Logical pixel size at DrawText scale 1.
    static constexpr int BODY_FONT_LOGICAL_PIXEL_SIZE = 24;
    /// Physical bake size in pixels.
    static constexpr int HEADLINE_FONT_PIXEL_SIZE = 96;
    /// Logical pixel size at DrawTextLarge scale 1.
    static constexpr int HEADLINE_FONT_LOGICAL_PIXEL_SIZE = 96;
    /// Logical/physical metric ratio; screen-space outline offsets do not use this ratio.
    static constexpr float BODY_METRIC_NORM =
        static_cast<float>(BODY_FONT_LOGICAL_PIXEL_SIZE) / static_cast<float>(BODY_FONT_PIXEL_SIZE);
    /// Logical/physical ratio for headline metrics.
    static constexpr float HEADLINE_METRIC_NORM =
        static_cast<float>(HEADLINE_FONT_LOGICAL_PIXEL_SIZE) /
        static_cast<float>(HEADLINE_FONT_PIXEL_SIZE);

    /**
     * @fn void BuildAtlasInto(int pixelSize, std::map<char, Character>& outChars, unsigned int& \
     * outTexture, int& outWidth, int& outHeight)
     * @brief Replace the destination atlas and clear its glyph map.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Requires m_Face; log and return if absent. pixelSize is the bake height in pixels;
     * Rows wrap at 2048 texels for sizes of at least 64 px, otherwise 512.
     * Output dimensions are powers of two.
     */
    void BuildAtlasInto(int pixelSize,
                        std::map<char, Character>& outChars,
                        unsigned int& outTexture,
                        int& outWidth,
                        int& outHeight);

    void DrawTextImpl(const std::string& text,
                      glm::vec2 position,
                      float scale,
                      glm::vec3 color,
                      float outlineSize,
                      float alpha,
                      float metricNorm,
                      const std::map<char, Character>& chars,
                      unsigned int atlasTexture);

    [[nodiscard]] float GetTextWidthImpl(const std::string& text,
                                         float scale,
                                         float metricNorm,
                                         const std::map<char, Character>& chars) const;

    std::vector<std::string> m_FontCandidates;

#ifdef USE_FREETYPE
    FT_Library m_FreeType;
    FT_Face m_Face;
#endif

    unsigned int m_VAO, m_VBO, m_EBO;
    /// Shared by all 2D batches; useColorOnly selects texture and color behavior.
    unsigned int m_ShaderProgram;
    glm::mat4 m_Projection;
    unsigned int m_WhiteTexture;  ///< Owned 1x1 opaque-white texture backing rect draws.

    GLint m_ModelLoc;
    GLint m_ProjectionLoc;
    GLint m_ColorLoc;
    GLint m_AlphaLoc;
    GLint m_AmbientColorLoc;  ///< GLSL `ambientColor`; day/night ambient light color.
    GLint m_UseColorOnlyLoc;  ///< Color mode selector (0=texture, 1=uniform, 2=vertex, 3=tex*vert).
    /// Ambient multiplier for lit geometry; self-lit particles and sky bypass it.
    glm::vec3 m_AmbientColor;

    /// Scene-space positions transformed by viewProjection; per-vertex color avoids tint flushes.
    struct BatchVertex3D
    {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    unsigned int m_Geometry3DProgram = 0;
    GLint m_VP3DLoc = -1;
    GLint m_Ambient3DLoc = -1;
    GLint m_AlphaCutoff3DLoc = -1;  ///< GLSL `alphaCutoff`; varies per pass.

    glm::mat4 m_ViewProjection{1.0f};

    std::vector<BatchVertex3D> m_Batch3DVertices;
    unsigned int m_Batch3DVAO = 0;
    unsigned int m_Batch3DVBO = 0;
    unsigned int m_CurrentBatch3DTexture = 0;
    /// Flush before changing blend, depth or light state; these values apply to the whole batch.
    renderModes::BlendMode m_Batch3DBlend = renderModes::BlendMode::Alpha;
    renderModes::DepthMode m_Batch3DDepth = renderModes::DepthMode::TestAndWrite;
    renderModes::LightMode m_Batch3DLight = renderModes::LightMode::Ambient;

    void SetupBatch3DBuffers();

    void ApplyPass3DState(renderModes::BlendMode blend, renderModes::DepthMode depth);

    void FlushBatch3D();

    /**
     * @brief Text capacity in quads across calls sharing an atlas; each outlined glyph consumes
     * five quads.
     */
    static constexpr size_t MAX_TEXT_QUADS = 2048;

    /// Glyph alpha multiplies per-vertex color, so outline and foreground share a batch.
    struct TextVertex
    {
        float x, y;
        float u, v;
        float r, g, b, a;  ///< Per-vertex RGBA tint (multiplied by glyph alpha).
    };

    std::vector<TextVertex> m_TextBatchVertices;
    unsigned int m_TextVAO, m_TextVBO;
    /// 0 means no pending text. changing the atlas flushes the current text batch.
    unsigned int m_CurrentTextAtlas = 0;

    void FlushTextBatch();

    /**
     * @brief Quad capacity for sprite, rect, particle and 3D buffers; each allocates its own vertex
     * stride.
     */
    static constexpr size_t MAX_BATCH_SPRITES = 10000;
    static constexpr size_t VERTICES_PER_SPRITE = 6;

    struct BatchVertex
    {
        float x, y;
        float u, v;
    };

    std::vector<BatchVertex> m_BatchVertices;
    unsigned int m_BatchVAO, m_BatchVBO;
    unsigned int m_CurrentBatchTexture;

    void FlushBatch();

    struct ColoredVertex
    {
        float x, y;
        float u, v;  ///< Texture coords (unused for rects).
        float r, g, b, a;
    };

    std::vector<ColoredVertex> m_RectBatchVertices;
    unsigned int m_RectBatchVAO, m_RectBatchVBO;
    bool m_RectBatchAdditive;

    void FlushRectBatch();

    void SetupRectBatchBuffers();

    /// Particles share the rect VAO/VBO and capacity; flushes must run sequentially.

    std::vector<ColoredVertex> m_ParticleBatchVertices;
    unsigned int m_CurrentParticleTexture;
    bool m_ParticleBatchAdditive;

    void FlushParticleBatch();

    int m_DrawCallCount = 0;
    bool m_Initialized = false;

    /// Borrowed reason for the next traced flush; the flush consumes and clears it.
    const char* m_PendingFlushReason = nullptr;

    /**
     * @fn void PrepFlushReason(const char* reason)
     * @brief Store a reason for the next traced flush.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline void PrepFlushReason(const char* reason) { m_PendingFlushReason = reason; }

    /**
     * @fn std::string LoadShaderFromFile(const std::string& filepath)
     * @brief Return an empty string if the shader file cannot be read.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::string LoadShaderFromFile(const std::string& filepath);

    /**
     * @fn unsigned int CompileShaderProgram(const std::string& vertSrc, const std::string& \
     * fragSrc, const char* debugLabel)
     * @brief Return the linked program, or 0 on compile or link failure.
     * @author Alex (<https://github.com/lextpf>)
     */
    unsigned int CompileShaderProgram(const std::string& vertSrc,
                                      const std::string& fragSrc,
                                      const char* debugLabel);

    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;

    /**
     * @brief OpenGL scene and bloom pipeline.
     *
     * The bright pass gates on HSV saturation. The composite projects bloom onto chroma
     * to avoid added luminance. postFXEnabled bypasses the composite effects, but the mip chain
     * still runs.
     *
     * @verbatim
     *  world draws
     *       |
     *       v
     *  [scene FBO  RGB16F]-----------------------------------+   (HDR, values > 1)
     *       |                                                |
     *       v  BloomPrefilter.frag                           |
     *  HSV-saturation bright pass  -->  mip0 (1/2 res)       |
     *       |                                                |
     *       v  BloomDownsample.frag                          |
     *  mip0 -> mip1 -> mip2 -> mip3 -> mip4   (down to 1/32) |
     *       |                                                |
     *       v  BloomUpsample.frag, additive (GL_ONE,GL_ONE)  |
     *  mip4 += -> mip3 += -> mip2 += -> mip1 += -> mip0      |
     *       |                                                |
     *       v                                                v
     *  [bloom = mip0] ------> PostFXComposite.frag <---------+
     *                                 |
     *        1. scene sample w/ chromatic aberration
     *        2. + chroma-only bloom (zero net luminance)
     *        3. lift / gamma / gain grading
     *        4. saturation pump
     *        5. vignette + edge desaturation
     *        6. film grain
     *        7. soft-shoulder tonemap
     *                                 |
     *                                 v
     *                          [swapchain] --> sharp UI drawn after
     * @endverbatim
     */

    /**
     * @brief RGB16F scene color preserves highlights above 1; depth is write-only.
     *
     * reallocate on viewport resize.
     */
    unsigned int m_SceneFBO = 0;
    unsigned int m_SceneColorTex = 0;  ///< RGB16F color attachment; sampled by the composite.
    unsigned int m_SceneDepthRBO = 0;  ///< DEPTH_COMPONENT24 renderbuffer; write-only.
    int m_SceneFBOWidth = 0;
    int m_SceneFBOHeight = 0;

    /**
     * @brief Mip 0 is half scene size; each later mip halves again.
     *
     * keep ambience::BLOOM_MIP_LEVELS consistent.
     */
    static constexpr int kBloomMipLevels = 5;
    unsigned int m_BloomMipFBO[kBloomMipLevels] = {};
    /// RGB16F, LINEAR-filtered, CLAMP_TO_EDGE; index 0 also holds the final bloom composite input.
    unsigned int m_BloomMipTex[kBloomMipLevels] = {};
    int m_BloomMipWidth[kBloomMipLevels] = {};
    int m_BloomMipHeight[kBloomMipLevels] = {};

    unsigned int m_PostVAO = 0;

    unsigned int m_PostProgram = 0;
    unsigned int m_BloomThresholdProgram = 0;
    unsigned int m_BloomDownProgram = 0;
    unsigned int m_BloomUpProgram = 0;

    /// Uniform location -1 means absent; writes to it have no effect.
    GLint m_PostULoc_Scene = -1;
    GLint m_PostULoc_Bloom = -1;
    GLint m_PostULoc_BloomIntensity = -1;
    GLint m_PostULoc_Lift = -1;
    GLint m_PostULoc_Gamma = -1;
    GLint m_PostULoc_Gain = -1;
    GLint m_PostULoc_Saturation = -1;
    GLint m_PostULoc_CAStrength = -1;
    GLint m_PostULoc_VignetteIntensity = -1;
    GLint m_PostULoc_VignetteInnerR = -1;
    GLint m_PostULoc_VignetteOuterR = -1;
    GLint m_PostULoc_VignetteAspectY = -1;
    GLint m_PostULoc_EdgeDesat = -1;
    GLint m_PostULoc_GrainIntensity = -1;
    GLint m_PostULoc_GrainChromaMix = -1;
    GLint m_PostULoc_Time = -1;
    GLint m_PostULoc_TonemapKnee = -1;
    GLint m_PostULoc_Enabled = -1;

    GLint m_BloomThresholdULoc_Scene = -1;
    GLint m_BloomThresholdULoc_SatThreshold = -1;

    GLint m_BloomDownULoc_Input = -1;
    GLint m_BloomDownULoc_SrcTexelSize = -1;

    GLint m_BloomUpULoc_Input = -1;
    /// Source mip texel size (1/width, 1/height) for the upsample tent filter.
    GLint m_BloomUpULoc_SrcTexelSize = -1;

    /// True only after BeginScene binds its FBO; false makes EndSceneApplyPostFX skip compositing.
    bool m_SceneBound = false;

    /**
     * @fn void EnsureSceneFramebuffer(int width, int height)
     * @brief Reallocate on size change; non-positive sizes and matching allocations do nothing.
     * @author Alex (<https://github.com/lextpf>)
     */
    void EnsureSceneFramebuffer(int width, int height);
    /**
     * @fn void EnsureBloomFramebuffers(int width, int height)
     * @brief Scene size in pixels; mip 0 is half size and each later mip halves again.
     * @author Alex (<https://github.com/lextpf>)
     */
    void EnsureBloomFramebuffers(int width, int height);

    void DestroySceneFramebuffer();

    void DestroyBloomFramebuffers();
    /**
     * @fn bool InitPostFXShaders()
     * @brief Return false on any post-FX shader failure; successfully linked programs remain
     * allocated.
     * @author Alex (<https://github.com/lextpf>)
     *
     * m_PostVAO stays 0 on failure. Init logs and continues, but BeginScene requires valid
     * post-FX programs and VAO: the composite path does not guard missing handles.
     */
    bool InitPostFXShaders();
    /**
     * @fn void RunBloomPrep()
     * @brief Leave final bloom in m_BloomMipTex(0) and GL_BLEND disabled.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RunBloomPrep();
};
