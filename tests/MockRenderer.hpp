#pragma once

#include "../src/IRenderer.hpp"
#include "../src/RendererMacros.hpp"

#include <array>
#include <string>
#include <vector>

/**
 * @class MockRenderer
 * @brief records selected draw calls without a graphics context.
 * @author Alex (https://github.com/lextpf)
 * @ingroup Rendering
 *
 * records quads, alpha/atlas sprites, and colored rectangles. DrawSprite and
 * DrawSpriteRegion discard calls. merge sprites2D and rects by sequence to recover
 * their shared submission order. viewProjection and ambient retain their last values.
 *
 * RequiresYFlip, GetDrawCallCount, and GetBackendInfo return fixed placeholders.
 */
class MockRenderer : public IRenderer
{
public:
    RIFT_DECLARE_COMMON_RENDERER_METHODS;

    void SetFontCandidates(const std::vector<std::string>&) override {}
    bool RequiresYFlip() const override { return true; }
    void SetAmbientColor(const glm::vec3& color) override { ambient = color; }
    int GetDrawCallCount() const override { return 0; }

    /// exposes the protected flat rotation for geometry comparisons.
    static void FlatRotate(glm::vec2 corners[4], glm::vec2 size, float rotation)
    {
        RotateCorners(corners, size, rotation);
    }

    /// scene-space corners and state from one DrawQuad3D call.
    struct Quad3D
    {
        std::array<glm::vec3, 4> corners{};
        glm::vec2 texCoord{0.0f};
        glm::vec2 texSize{0.0f};
        glm::vec4 color{1.0f};
        renderModes::BlendMode blend = renderModes::BlendMode::Alpha;
        renderModes::DepthMode depth = renderModes::DepthMode::TestAndWrite;
        bool flipY = true;
        renderModes::LightMode light = renderModes::LightMode::Ambient;
    };

    /// one DrawColoredRect call.
    struct Rect
    {
        glm::vec2 position{0.0f};
        glm::vec2 size{0.0f};
        glm::vec4 color{1.0f};
        bool additive = false;
        std::size_t sequence = 0;
    };

    /// one DrawSpriteAlpha or DrawSpriteAtlas call; alpha calls use the full UV region.
    struct Sprite2D
    {
        glm::vec2 position{0.0f};  ///< top-left, in the caller's projection units.
        glm::vec2 size{0.0f};
        glm::vec2 uvMin{0.0f};
        glm::vec2 uvMax{1.0f};
        float rotation = 0.0f;  ///< degrees about the quad center.
        glm::vec4 color{1.0f};
        bool additive = false;
        bool fromAtlas = false;
        std::size_t sequence = 0;
    };

    std::vector<Quad3D> quads3D;
    std::vector<Rect> rects;
    std::vector<Sprite2D> sprites2D;
    glm::mat4 viewProjection{1.0f};
    glm::vec3 ambient{1.0f};

    /// clears submissions and sequence numbers; retains viewProjection and ambient.
    void ClearRecorded()
    {
        quads3D.clear();
        rects.clear();
        sprites2D.clear();
        m_Sequence = 0;
    }

private:
    std::size_t m_Sequence = 0;
};

inline bool MockRenderer::Init()
{
    return true;
}
inline void MockRenderer::Shutdown() {}
inline void MockRenderer::BeginFrame() {}
inline void MockRenderer::EndFrame() {}
inline void MockRenderer::BeginScene() {}
inline void MockRenderer::EndSceneApplyPostFX(const PostFXParams&) {}

inline void MockRenderer::DrawSprite(const Texture&, glm::vec2, glm::vec2, float, glm::vec3) {}

inline void MockRenderer::DrawSpriteRegion(
    const Texture&, glm::vec2, glm::vec2, glm::vec2, glm::vec2, float, glm::vec3, bool, bool, bool)
{
}

inline void MockRenderer::DrawSpriteAlpha(const Texture&,
                                          glm::vec2 position,
                                          glm::vec2 size,
                                          float rotation,
                                          glm::vec4 color,
                                          bool additive)
{
    Sprite2D record;
    record.position = position;
    record.size = size;
    record.uvMin = glm::vec2(0.0f);
    record.uvMax = glm::vec2(1.0f);
    record.rotation = rotation;
    record.color = color;
    record.additive = additive;
    record.fromAtlas = false;
    record.sequence = m_Sequence++;
    sprites2D.push_back(record);
}

inline void MockRenderer::DrawSpriteAtlas(const Texture&,
                                          glm::vec2 position,
                                          glm::vec2 size,
                                          glm::vec2 uvMin,
                                          glm::vec2 uvMax,
                                          float rotation,
                                          glm::vec4 color,
                                          bool additive)
{
    Sprite2D record;
    record.position = position;
    record.size = size;
    record.uvMin = uvMin;
    record.uvMax = uvMax;
    record.rotation = rotation;
    record.color = color;
    record.additive = additive;
    record.fromAtlas = true;
    record.sequence = m_Sequence++;
    sprites2D.push_back(record);
}

inline void MockRenderer::DrawColoredRect(glm::vec2 position,
                                          glm::vec2 size,
                                          glm::vec4 color,
                                          bool additive)
{
    Rect record;
    record.position = position;
    record.size = size;
    record.color = color;
    record.additive = additive;
    record.sequence = m_Sequence++;
    rects.push_back(record);
}

inline void MockRenderer::DrawQuad3D(const Texture&,
                                     const glm::vec3 corners[4],
                                     glm::vec2 texCoord,
                                     glm::vec2 texSize,
                                     glm::vec4 color,
                                     renderModes::BlendMode blend,
                                     renderModes::DepthMode depth,
                                     bool flipY,
                                     bool,
                                     bool,
                                     renderModes::LightMode light)
{
    Quad3D record;
    record.corners = {corners[0], corners[1], corners[2], corners[3]};
    record.texCoord = texCoord;
    record.texSize = texSize;
    record.color = color;
    record.blend = blend;
    record.depth = depth;
    record.flipY = flipY;
    record.light = light;
    quads3D.push_back(record);
}

inline void MockRenderer::SetViewProjection(const glm::mat4& matrix)
{
    viewProjection = matrix;
}

inline void MockRenderer::SetProjection(const glm::mat4&) {}
inline void MockRenderer::SetViewport(int, int, int, int) {}
inline void MockRenderer::Clear(float, float, float, float) {}
inline void MockRenderer::UploadTexture(const Texture&) {}

inline void MockRenderer::DrawText(const std::string&, glm::vec2, float, glm::vec3, float, float) {}

inline float MockRenderer::GetTextAscent(float) const
{
    return 0.0f;
}

inline float MockRenderer::GetTextWidth(const std::string&, float) const
{
    return 0.0f;
}

inline RendererInfo MockRenderer::GetBackendInfo() const
{
    return {};
}
