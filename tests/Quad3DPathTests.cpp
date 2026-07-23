// under Classic, projected world quads must match the flat pixel formula:
// (worldPos - cameraTopLeft) * screen / visibleWorld.
#include "../src/Billboard.hpp"
#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/RenderModes.hpp"
#include "../src/SceneMath.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float kTol = 1e-3f;

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

cameraRig::RigParams ClassicParams()
{
    cameraRig::RigParams params;
    params.target = {1000.0f, 500.0f};
    params.visibleWorldSize = {320.0f, 180.0f};
    params.sceneRadius = 4096.0f;
    cameraRig::ApplyPreset(params, cameraRig::Preset::Classic);
    return params;
}

glm::vec2 LegacyScreenPos(const cameraRig::RigParams& params, glm::vec2 world, glm::vec2 screen)
{
    const glm::vec2 topLeft = params.target - params.visibleWorldSize * 0.5f;
    return (world - topLeft) * (screen / params.visibleWorldSize);
}
}  // namespace

TEST(Quad3DPathTest, GroundTileLandsOnTheLegacyScreenRect)
{
    const cameraRig::RigParams params = ClassicParams();
    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::vec2 screen{1520.0f, 855.0f};

    const glm::vec2 tileTopLeft{1008.0f, 496.0f};
    const glm::vec2 tileSize{16.0f, 16.0f};

    glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
    sceneMath::MakeGroundQuad(tileTopLeft, tileSize, 0.0f, 0.0f, corners);

    const glm::vec2 expected[sceneMath::QUAD_CORNER_COUNT] = {
        LegacyScreenPos(params, tileTopLeft, screen),
        LegacyScreenPos(params, {tileTopLeft.x + tileSize.x, tileTopLeft.y}, screen),
        LegacyScreenPos(params, tileTopLeft + tileSize, screen),
        LegacyScreenPos(params, {tileTopLeft.x, tileTopLeft.y + tileSize.y}, screen),
    };

    for (int i = 0; i < sceneMath::QUAD_CORNER_COUNT; ++i)
    {
        const std::optional<glm::vec2> pixel =
            cameraRig::WorldToScreen(corners[i], viewProj, screen);
        ASSERT_TRUE(pixel.has_value()) << "corner " << i;
        EXPECT_NEAR(pixel->x, expected[i].x, kTol) << "corner " << i;
        EXPECT_NEAR(pixel->y, expected[i].y, kTol) << "corner " << i;
    }
}

TEST(Quad3DPathTest, ClassicPresetKeepsTilesAxisAlignedAndUnscaled)
{
    // Classic keeps tiles axis-aligned and the same pixel size across the view.
    const cameraRig::RigParams params = ClassicParams();
    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::vec2 screen{1520.0f, 855.0f};
    const glm::vec2 tileSize{16.0f, 16.0f};
    const glm::vec2 scale = screen / params.visibleWorldSize;

    const glm::vec2 origins[] = {
        params.target - params.visibleWorldSize * 0.5f,
        params.target,
        params.target + params.visibleWorldSize * 0.5f - tileSize,
    };

    for (const glm::vec2& origin : origins)
    {
        glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
        sceneMath::MakeGroundQuad(origin, tileSize, 0.0f, 0.0f, corners);

        glm::vec2 pixels[sceneMath::QUAD_CORNER_COUNT];
        for (int i = 0; i < sceneMath::QUAD_CORNER_COUNT; ++i)
        {
            const std::optional<glm::vec2> p =
                cameraRig::WorldToScreen(corners[i], viewProj, screen);
            ASSERT_TRUE(p.has_value());
            pixels[i] = *p;
        }

        EXPECT_NEAR(pixels[sceneMath::QUAD_TOP_LEFT].y, pixels[sceneMath::QUAD_TOP_RIGHT].y, kTol);
        EXPECT_NEAR(
            pixels[sceneMath::QUAD_TOP_LEFT].x, pixels[sceneMath::QUAD_BOTTOM_LEFT].x, kTol);

        EXPECT_NEAR(pixels[sceneMath::QUAD_TOP_RIGHT].x - pixels[sceneMath::QUAD_TOP_LEFT].x,
                    tileSize.x * scale.x,
                    kTol);
        EXPECT_NEAR(pixels[sceneMath::QUAD_BOTTOM_LEFT].y - pixels[sceneMath::QUAD_TOP_LEFT].y,
                    tileSize.y * scale.y,
                    kTol);
    }
}

TEST(Quad3DPathTest, PerspectivePresetActuallyConverges)
{
    // the DS preset uses perspective; distant tiles must project smaller.
    cameraRig::RigParams params = ClassicParams();
    cameraRig::ApplyPreset(params, cameraRig::Preset::DS);

    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::vec2 screen{1520.0f, 855.0f};
    const glm::vec2 tileSize{16.0f, 16.0f};

    auto screenWidthOfTileAt = [&](glm::vec2 origin)
    {
        glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
        sceneMath::MakeGroundQuad(origin, tileSize, 0.0f, 0.0f, corners);
        const glm::vec2 tl =
            *cameraRig::WorldToScreen(corners[sceneMath::QUAD_TOP_LEFT], viewProj, screen);
        const glm::vec2 tr =
            *cameraRig::WorldToScreen(corners[sceneMath::QUAD_TOP_RIGHT], viewProj, screen);
        return tr.x - tl.x;
    };

    // map-north (smaller world Y) is away from the camera at yaw 0.
    const float nearWidth = screenWidthOfTileAt(params.target + glm::vec2(0.0f, 40.0f));
    const float farWidth = screenWidthOfTileAt(params.target - glm::vec2(0.0f, 40.0f));

    EXPECT_GT(nearWidth, farWidth);
}

TEST(Quad3DPathTest, MockRecordsSubmissionsWithPassState)
{
    MockRenderer renderer;
    // call through IRenderer because it declares the default arguments; overrides omit them.
    IRenderer& api = renderer;
    const Texture texture;

    glm::vec3 ground[sceneMath::QUAD_CORNER_COUNT];
    sceneMath::MakeGroundQuad({0.0f, 0.0f}, {16.0f, 16.0f}, 0.0f, 0.0f, ground);

    api.SetViewProjection(cameraRig::BuildViewProjection(ClassicParams()));
    api.DrawQuad3D(texture,
                   ground,
                   {0.0f, 0.0f},
                   {16.0f, 16.0f},
                   glm::vec4(1.0f),
                   renderModes::BlendMode::Alpha,
                   renderModes::DepthMode::TestAndWrite);

    glm::vec3 upright[sceneMath::QUAD_CORNER_COUNT];
    billboard::MakeQuad({8.0f, 0.0f, 16.0f},
                        {16.0f, 32.0f},
                        0.0f,
                        Degrees(51.34f),
                        billboard::DefaultDamping(billboard::Role::Character),
                        upright);
    api.DrawQuad3D(texture,
                   upright,
                   {0.0f, 0.0f},
                   {16.0f, 32.0f},
                   glm::vec4(1.0f),
                   renderModes::BlendMode::Additive,
                   renderModes::DepthMode::TestOnly);

    ASSERT_EQ(renderer.quads3D.size(), 2u);
    EXPECT_EQ(renderer.quads3D[0].depth, renderModes::DepthMode::TestAndWrite);
    EXPECT_EQ(renderer.quads3D[0].blend, renderModes::BlendMode::Alpha);
    EXPECT_EQ(renderer.quads3D[1].depth, renderModes::DepthMode::TestOnly);
    EXPECT_EQ(renderer.quads3D[1].blend, renderModes::BlendMode::Additive);

    EXPECT_NEAR(renderer.quads3D[0].corners[sceneMath::QUAD_TOP_LEFT].y, 0.0f, kTol);
    EXPECT_GT(renderer.quads3D[1].corners[sceneMath::QUAD_TOP_LEFT].y, 0.0f);

    renderer.ClearRecorded();
    EXPECT_TRUE(renderer.quads3D.empty());
}

TEST(Quad3DPathTest, RenderModeNamesRoundTrip)
{
    EXPECT_EQ(EnumTraits<renderModes::DepthMode>::ToString(renderModes::DepthMode::TestAndWrite),
              "TestAndWrite");
    EXPECT_EQ(EnumTraits<renderModes::BlendMode>::FromString("Additive"),
              renderModes::BlendMode::Additive);
    EXPECT_FALSE(EnumTraits<renderModes::DepthMode>::FromString("Always").has_value());
}

TEST(Quad3DPathTest, LightModeDefaultsToAmbientAndReachesTheMock)
{
    // day/night ambient and self-lighting modes must survive interface dispatch.
    MockRenderer renderer;
    IRenderer& api = renderer;
    const Texture texture;

    glm::vec3 ground[sceneMath::QUAD_CORNER_COUNT];
    sceneMath::MakeGroundQuad({0.0f, 0.0f}, {16.0f, 16.0f}, 0.0f, 0.0f, ground);

    api.DrawQuad3D(texture,
                   ground,
                   {0.0f, 0.0f},
                   {16.0f, 16.0f},
                   glm::vec4(1.0f),
                   renderModes::BlendMode::Alpha,
                   renderModes::DepthMode::TestAndWrite);

    api.DrawQuad3D(texture,
                   ground,
                   {0.0f, 0.0f},
                   {16.0f, 16.0f},
                   glm::vec4(1.0f),
                   renderModes::BlendMode::Additive,
                   renderModes::DepthMode::None,
                   true,
                   false,
                   false,
                   renderModes::LightMode::SelfLit);

    ASSERT_EQ(renderer.quads3D.size(), 2u);
    EXPECT_EQ(renderer.quads3D[0].light, renderModes::LightMode::Ambient);
    EXPECT_EQ(renderer.quads3D[1].light, renderModes::LightMode::SelfLit);
    EXPECT_TRUE(renderer.quads3D[0].flipY);

    api.DrawQuad3D(texture,
                   ground,
                   {0.0f, 0.0f},
                   {16.0f, 16.0f},
                   glm::vec4(1.0f),
                   renderModes::BlendMode::Alpha,
                   renderModes::DepthMode::TestAndWrite,
                   false);

    ASSERT_EQ(renderer.quads3D.size(), 3u);
    EXPECT_FALSE(renderer.quads3D[2].flipY);
    EXPECT_EQ(renderer.quads3D[2].light, renderModes::LightMode::Ambient);
}

TEST(Quad3DPathTest, LightModeNamesRoundTrip)
{
    EXPECT_EQ(EnumTraits<renderModes::LightMode>::ToString(renderModes::LightMode::SelfLit),
              "SelfLit");
    EXPECT_EQ(EnumTraits<renderModes::LightMode>::FromString("Ambient"),
              renderModes::LightMode::Ambient);
    EXPECT_FALSE(EnumTraits<renderModes::LightMode>::FromString("Emissive").has_value());
}
