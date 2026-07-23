#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/ParticleCards.hpp"
#include "../src/SceneMath.hpp"
#include "../src/SkyCards.hpp"
#include "../src/SkyDrawList.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
constexpr float kTol = 1e-3f;
constexpr float kPixelTol = 0.02f;
constexpr glm::vec2 kScreen{1520.0f, 855.0f};
constexpr glm::vec2 kVisible{320.0f, 180.0f};

constexpr glm::vec2 kFlatCam{840.0f, 410.0f};

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

cameraRig::RigParams MakeRig(cameraRig::Preset preset,
                             float pitchDeg = 0.0f,
                             float yawDeg = 0.0f,
                             glm::vec2 visible = kVisible)
{
    cameraRig::RigParams params;
    params.target = {1000.0f, 500.0f};
    params.visibleWorldSize = visible;
    params.sceneRadius = 4096.0f;
    params.pitchRadians = Degrees(pitchDeg);
    params.yawRadians = Degrees(yawDeg);
    cameraRig::ApplyPreset(params, preset);
    return params;
}

std::array<cameraRig::RigParams, 4> AllRigs()
{
    return {MakeRig(cameraRig::Preset::Classic),
            MakeRig(cameraRig::Preset::DS),
            MakeRig(cameraRig::Preset::Free, 35.0f, 45.0f),
            MakeRig(cameraRig::Preset::Free, 12.0f, 217.0f)};
}

skyDraw::Element MakeElement(glm::vec2 pos, glm::vec2 size, float rotation = 0.0f)
{
    skyDraw::Element e;
    e.sprite = skyDraw::Sprite::Glow;
    e.layer = skyDraw::Layer::DawnWash;
    e.pos = pos;
    e.size = size;
    e.rotation = rotation;
    e.additive = true;
    return e;
}

glm::vec2 Project(const cameraRig::RigParams& rig, glm::vec3 scenePoint)
{
    const std::optional<glm::vec2> px =
        cameraRig::WorldToScreen(scenePoint, cameraRig::BuildViewProjection(rig), kScreen);
    EXPECT_TRUE(px.has_value());
    return px.value_or(glm::vec2(0.0f));
}

void ExpectVec3Near(glm::vec3 actual, glm::vec3 expected, float tolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

}  // namespace

// derive the viewport origin from a target that differs from the expected literal.
TEST(SkyCardsTest, ViewportTopLeftIsTheFlatCameraPosition)
{
    for (const cameraRig::RigParams& rig : AllRigs())
    {
        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        const glm::vec2 topLeft = skyCards::ViewportTopLeft(frame);
        EXPECT_NEAR(topLeft.x, kFlatCam.x, kTol);
        EXPECT_NEAR(topLeft.y, kFlatCam.y, kTol);
        EXPECT_NE(topLeft.x, frame.focusWorld.x);
        EXPECT_NE(topLeft.y, frame.focusWorld.y);
    }
}

TEST(SkyCardsTest, ClassicSheetCentreEqualsTheWorldPosition)
{
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::Classic);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    const glm::vec2 pos{40.0f, -20.0f};
    const glm::vec2 size{60.0f, 30.0f};
    const glm::vec3 centre = skyCards::SheetCentre(frame, pos, size);
    ExpectVec3Near(
        centre, sceneMath::ToScene(kFlatCam + pos + size * 0.5f, frame.focusHeight), kTol);
}

// the flat orthographic map, stated as pixels: pos * screen / visible.
TEST(SkyCardsTest, ClassicQuadLandsOnTheFlatScreenRect)
{
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::Classic);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    const skyDraw::Element e = MakeElement({40.0f, -20.0f}, {60.0f, 30.0f});
    glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
    skyCards::MakeSkyQuad(frame, e, corners);

    const glm::vec2 expectTL{190.0f, -95.0f};
    const glm::vec2 expectBR{475.0f, 47.5f};
    EXPECT_NEAR(Project(rig, corners[0]).x, expectTL.x, kPixelTol);
    EXPECT_NEAR(Project(rig, corners[0]).y, expectTL.y, kPixelTol);
    EXPECT_NEAR(Project(rig, corners[2]).x, expectBR.x, kPixelTol);
    EXPECT_NEAR(Project(rig, corners[2]).y, expectBR.y, kPixelTol);
}

TEST(SkyCardsTest, SheetElementKeepsTheFlatPixelUnderEveryCamera)
{
    const skyDraw::Element e = MakeElement({40.0f, -20.0f}, {60.0f, 30.0f});
    const std::array<glm::vec2, 4> expected{glm::vec2{190.0f, -95.0f},
                                            glm::vec2{475.0f, -95.0f},
                                            glm::vec2{475.0f, 47.5f},
                                            glm::vec2{190.0f, 47.5f}};

    for (const cameraRig::RigParams& rig : AllRigs())
    {
        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
        skyCards::MakeSkyQuad(frame, e, corners);
        for (int i = 0; i < sceneMath::QUAD_CORNER_COUNT; ++i)
        {
            const glm::vec2 px = Project(rig, corners[i]);
            EXPECT_NEAR(px.x, expected[static_cast<std::size_t>(i)].x, kPixelTol) << "corner " << i;
            EXPECT_NEAR(px.y, expected[static_cast<std::size_t>(i)].y, kPixelTol) << "corner " << i;
        }
    }
}

// projection uses the supplied extent; half the extent means zoom 2.
TEST(SkyCardsTest, ZoomDoesNotMoveTheProjectedPixel)
{
    const cameraRig::RigParams rig =
        MakeRig(cameraRig::Preset::Classic, 0.0f, 0.0f, glm::vec2(160.0f, 90.0f));
    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    const glm::vec2 pos{40.0f, -20.0f};
    const glm::vec2 size{60.0f, 30.0f};
    const glm::vec2 d = pos + size * 0.5f - frame.halfVisible;
    const glm::vec2 scale = kScreen / rig.visibleWorldSize;
    const glm::vec2 expected = kScreen * 0.5f + d * scale;

    const glm::vec2 px = Project(rig, skyCards::SheetCentre(frame, pos, size));
    EXPECT_NEAR(px.x, expected.x, kPixelTol);
    EXPECT_NEAR(px.y, expected.y, kPixelTol);
}

// at zoom 1, a flash of twice the visible extent offset by half centers on the focus.
TEST(SkyCardsTest, LightningFlashCentreIsTheFocusAtZoomOne)
{
    for (const cameraRig::RigParams& rig : AllRigs())
    {
        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        const glm::vec3 centre = skyCards::SheetCentre(frame, -kVisible * 0.5f, kVisible * 2.0f);
        ExpectVec3Near(centre, sceneMath::ToScene(rig.target, frame.focusHeight), kTol);
    }
}

TEST(SkyCardsTest, LightningFlashCoversTwiceTheViewport)
{
    const skyDraw::Element e = MakeElement(-kVisible * 0.5f, kVisible * 2.0f);
    for (const cameraRig::RigParams& rig : AllRigs())
    {
        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
        skyCards::MakeSkyQuad(frame, e, corners);

        const glm::vec2 tl = Project(rig, corners[0]);
        const glm::vec2 br = Project(rig, corners[2]);
        EXPECT_NEAR(tl.x, -760.0f, kPixelTol);
        EXPECT_NEAR(tl.y, -427.5f, kPixelTol);
        EXPECT_NEAR(br.x, 2280.0f, kPixelTol);
        EXPECT_NEAR(br.y, 1282.5f, kPixelTol);
    }
}

TEST(SkyCardsTest, RotationMatchesTheFlatRotateCorners)
{
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::Classic);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    const glm::vec2 pos{40.0f, -20.0f};
    const glm::vec2 size{60.0f, 30.0f};
    constexpr float kRotation = 37.0f;

    glm::vec2 flat[4] = {{0.0f, 0.0f}, {size.x, 0.0f}, {size.x, size.y}, {0.0f, size.y}};
    MockRenderer::FlatRotate(flat, size, kRotation);

    const skyDraw::Element e = MakeElement(pos, size, kRotation);
    glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
    skyCards::MakeSkyQuad(frame, e, corners);

    for (int i = 0; i < sceneMath::QUAD_CORNER_COUNT; ++i)
    {
        const glm::vec2 world = kFlatCam + pos + flat[i];
        ExpectVec3Near(corners[i], sceneMath::ToScene(world, frame.focusHeight), 1e-4f);
    }
}

// all sky corners share focus depth, so an oversized flash cannot cross the depth planes.
TEST(SkyCardsTest, AllSkyQuadsShareTheFocusPlane)
{
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);
    const glm::vec3 focus = sceneMath::ToScene(frame.focusWorld, frame.focusHeight);

    const std::array<glm::vec2, 4> spread{glm::vec2{0.0f, 0.0f},
                                          glm::vec2{300.0f, 10.0f},
                                          glm::vec2{-200.0f, 170.0f},
                                          glm::vec2{-160.0f, -90.0f}};
    for (const glm::vec2& pos : spread)
    {
        const glm::vec3 centre = skyCards::SheetCentre(frame, pos, glm::vec2(20.0f, 20.0f));
        EXPECT_NEAR(glm::dot(centre - focus, frame.towardEye), 0.0f, 1e-3f);
    }
}

TEST(SkyCardsTest, YawDoesNotFlipTheSheetVertically)
{
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::Free, 40.0f, 180.0f);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    const glm::vec2 size{4.0f, 4.0f};
    const glm::vec2 upper = Project(rig, skyCards::SheetCentre(frame, {160.0f, 60.0f}, size));
    const glm::vec2 lower = Project(rig, skyCards::SheetCentre(frame, {160.0f, 120.0f}, size));
    EXPECT_GT(lower.y, upper.y);
}

// check a grid of positions to ensure culling keeps every quad visible in the flat frame.
TEST(SkyCardsTest, SheetCullKeepsEveryQuadThatTouchesTheViewport)
{
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));

    for (float x = -900.0f; x <= 900.0f; x += 37.0f)
    {
        for (float y = -600.0f; y <= 600.0f; y += 29.0f)
        {
            for (float s : {1.0f, 6.0f, 64.0f})
            {
                const skyDraw::Element e = MakeElement({x, y}, {s, s});
                const bool touchesViewport =
                    x + s > 0.0f && x < kVisible.x && y + s > 0.0f && y < kVisible.y;
                if (touchesViewport)
                {
                    EXPECT_TRUE(skyCards::KeepOnSheet(frame, e))
                        << "dropped a visible quad at " << x << "," << y << " size " << s;
                }
            }
        }
    }

    EXPECT_FALSE(skyCards::KeepOnSheet(frame, MakeElement({760.0f, 90.0f}, {1.0f, 1.0f})));
    EXPECT_TRUE(skyCards::KeepOnSheet(frame, MakeElement({-240.0f, -310.0f}, {800.0f, 800.0f})));
    EXPECT_TRUE(skyCards::KeepOnSheet(frame, MakeElement(-kVisible * 0.5f, kVisible * 2.0f)));
}

TEST(SkyCardsTest, NegativeSizeDoesNotBreakTheCull)
{
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::Element mirrored = MakeElement({160.0f, 90.0f}, {-40.0f, 40.0f});
    EXPECT_TRUE(skyCards::KeepOnSheet(frame, mirrored));
}

TEST(SkyCardsTest, LightPoolQuadIsAGroundSquareAtItsSurfaceHeight)
{
    glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];
    skyCards::MakeLightPoolQuad({1000.0f, 500.0f}, 64.0f, 8.0f, corners);

    ExpectVec3Near(corners[0], {936.0f, 8.0f, 436.0f}, kTol);
    ExpectVec3Near(corners[1], {1064.0f, 8.0f, 436.0f}, kTol);
    ExpectVec3Near(corners[2], {1064.0f, 8.0f, 564.0f}, kTol);
    ExpectVec3Near(corners[3], {936.0f, 8.0f, 564.0f}, kTol);

    glm::vec3 ground[sceneMath::QUAD_CORNER_COUNT];
    skyCards::MakeLightPoolQuad({1000.0f, 500.0f}, 64.0f, 0.0f, ground);
    EXPECT_NE(corners[0].y, ground[0].y);
}

// uniform thinning must preserve coverage across an over-budget sky layer.
TEST(SkyCardsTest, StrideKeepSpreadsTheDrop)
{
    constexpr std::size_t kCount = 500;
    for (std::size_t allowance : {1u, 7u, 123u, 499u})
    {
        std::size_t kept = 0;
        std::size_t run = 0;
        std::size_t longestRun = 0;
        for (std::size_t i = 0; i < kCount; ++i)
        {
            if (skyCards::StrideKeep(i, kCount, allowance))
            {
                ++kept;
                run = 0;
            }
            else
            {
                ++run;
                longestRun = std::max(longestRun, run);
            }
        }
        EXPECT_EQ(kept, allowance) << "allowance " << allowance;
        const std::size_t bound = (kCount + allowance - 1) / allowance;
        EXPECT_LE(longestRun, bound) << "allowance " << allowance;
    }

    for (std::size_t i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(skyCards::StrideKeep(i, 10, 10));
        EXPECT_TRUE(skyCards::StrideKeep(i, 10, 99));
    }
}

// reserved layers must survive a budget overrun in full.
TEST(SkyCardsTest, ReservedLayersCoverTheLegibleTail)
{
    EXPECT_EQ(skyCards::ReserveFor(skyDraw::Layer::Flash), 1u);
    EXPECT_EQ(skyCards::ReserveFor(skyDraw::Layer::Bolt), 49u);
    EXPECT_EQ(skyCards::ReserveFor(skyDraw::Layer::Meteor), 18u);
    EXPECT_EQ(skyCards::ReserveFor(skyDraw::Layer::Aurora), 0u);
    EXPECT_EQ(skyCards::ReserveFor(skyDraw::Layer::Star), 0u);
}
