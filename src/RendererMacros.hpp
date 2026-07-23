#pragma once

/**
 * @brief Keeps shared backend override declarations identical.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Update this macro with irenderer when changing a shared signature.
 * SetFontCandidates, RequiresYFlip, SetAmbientColor, and GetDrawCallCount remain
 * backend declarations. OpenGL also overrides the optional large-text hooks.
 *
 * Append a semicolon at each use.
 * @see irenderer
 */
#define RIFT_DECLARE_COMMON_RENDERER_METHODS                                                 \
    [[nodiscard]] bool Init() override;                                                      \
    void Shutdown() override;                                                                \
    void BeginFrame() override;                                                              \
    void EndFrame() override;                                                                \
    void BeginScene() override;                                                              \
    void EndSceneApplyPostFX(const PostFXParams& params) override;                           \
    void DrawSprite(const Texture& texture,                                                  \
                    glm::vec2 position,                                                      \
                    glm::vec2 size,                                                          \
                    float rotation,                                                          \
                    glm::vec3 color) override;                                               \
    void DrawSpriteRegion(const Texture& texture,                                            \
                          glm::vec2 position,                                                \
                          glm::vec2 size,                                                    \
                          glm::vec2 texCoord,                                                \
                          glm::vec2 texSize,                                                 \
                          float rotation,                                                    \
                          glm::vec3 color,                                                   \
                          bool flipY,                                                        \
                          bool tileFlipX,                                                    \
                          bool tileFlipY) override;                                          \
    void DrawSpriteAlpha(const Texture& texture,                                             \
                         glm::vec2 position,                                                 \
                         glm::vec2 size,                                                     \
                         float rotation,                                                     \
                         glm::vec4 color,                                                    \
                         bool additive) override;                                            \
    void DrawSpriteAtlas(const Texture& texture,                                             \
                         glm::vec2 position,                                                 \
                         glm::vec2 size,                                                     \
                         glm::vec2 uvMin,                                                    \
                         glm::vec2 uvMax,                                                    \
                         float rotation,                                                     \
                         glm::vec4 color,                                                    \
                         bool additive) override;                                            \
    void DrawColoredRect(glm::vec2 position, glm::vec2 size, glm::vec4 color, bool additive) \
        override;                                                                            \
    void DrawQuad3D(const Texture& texture,                                                  \
                    const glm::vec3 corners[4],                                              \
                    glm::vec2 texCoord,                                                      \
                    glm::vec2 texSize,                                                       \
                    glm::vec4 color,                                                         \
                    renderModes::BlendMode blend,                                            \
                    renderModes::DepthMode depth,                                            \
                    bool flipY,                                                              \
                    bool tileFlipX,                                                          \
                    bool tileFlipY,                                                          \
                    renderModes::LightMode light) override;                                  \
    void SetViewProjection(const glm::mat4& viewProjection) override;                        \
    void SetProjection(const glm::mat4& projection) override;                                \
    void SetViewport(int x, int y, int width, int height) override;                          \
    void Clear(float r, float g, float b, float a) override;                                 \
    void UploadTexture(const Texture& texture) override;                                     \
    void DrawText(const std::string& text,                                                   \
                  glm::vec2 position,                                                        \
                  float scale,                                                               \
                  glm::vec3 color,                                                           \
                  float outlineSize,                                                         \
                  float alpha) override;                                                     \
    float GetTextAscent(float scale) const override;                                         \
    float GetTextWidth(const std::string& text, float scale) const override;                 \
    [[nodiscard]] RendererInfo GetBackendInfo() const override
