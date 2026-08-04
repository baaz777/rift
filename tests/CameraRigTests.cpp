// the Classic preset must reproduce the flat mapping:
// (worldPos - cameraTopLeft) * screen / visibleWorld.
#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/SceneMath.hpp"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace
{
constexpr float kTol = 1e-3f;
constexpr float kAngleTol = 1e-5f;

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

cameraRig::RigParams MakeParams(cameraRig::Preset preset)
{
    cameraRig::RigParams params;
    params.target = {1000.0f, 500.0f};
    params.visibleWorldSize = {320.0f, 180.0f};
    params.sceneRadius = 4096.0f;
    cameraRig::ApplyPreset(params, preset);
    return params;
}
}  // namespace

TEST(CameraRigTest, ClampPitchHoldsTheOrbitAboveTheHorizon)
{
    EXPECT_NEAR(cameraRig::ClampPitch(Degrees(-30.0f)), cameraRig::MIN_PITCH_RADIANS, kAngleTol);
    EXPECT_NEAR(cameraRig::ClampPitch(Degrees(200.0f)), cameraRig::MAX_PITCH_RADIANS, kAngleTol);
    EXPECT_NEAR(cameraRig::ClampPitch(Degrees(45.0f)), Degrees(45.0f), kAngleTol);
}

TEST(CameraRigTest, WrapYawKeepsDraggingFinite)
{
    EXPECT_NEAR(cameraRig::WrapYaw(Degrees(370.0f)), Degrees(10.0f), 1e-4f);
    EXPECT_NEAR(cameraRig::WrapYaw(Degrees(-370.0f)), Degrees(-10.0f), 1e-4f);

    EXPECT_NEAR(cameraRig::WrapYaw(Degrees(720.0f)), 0.0f, 1e-4f);
    EXPECT_NEAR(cameraRig::WrapYaw(Degrees(180.0f)), rift::PiF, 1e-4f);
}

TEST(CameraRigTest, DistanceFramesTheRequestedHeight)
{
    // tan(45 deg) == 1, so a 90 degree FOV frames a height equal to twice the
    // distance.
    EXPECT_NEAR(cameraRig::DistanceForVisibleHeight(180.0f, Degrees(90.0f)), 90.0f, kTol);
    // a narrow FOV moves the camera back to preserve framing.
    EXPECT_GT(cameraRig::DistanceForVisibleHeight(180.0f, cameraRig::DS_FOV_RADIANS), 600.0f);
}

TEST(CameraRigTest, EyeDirectionPlacesTheCameraSouthAtYawZero)
{
    const glm::vec3 horizon = cameraRig::EyeDirection(0.0f, 0.0f);
    EXPECT_NEAR(horizon.x, 0.0f, kTol);
    EXPECT_NEAR(horizon.y, 0.0f, kTol);
    EXPECT_NEAR(horizon.z, 1.0f, kTol);  // map south

    const glm::vec3 overhead = cameraRig::EyeDirection(0.0f, cameraRig::MAX_PITCH_RADIANS);
    EXPECT_NEAR(overhead.y, 1.0f, kTol);  // straight up

    const glm::vec3 east = cameraRig::EyeDirection(Degrees(90.0f), 0.0f);
    EXPECT_NEAR(east.x, 1.0f, kTol);  // yaw swings toward map east
}

TEST(CameraRigTest, UpVectorIsWellDefinedLookingStraightDown)
{
    // the Classic preset points along world-up, which makes a cross-product basis degenerate.
    const glm::vec3 up = cameraRig::UpVector(0.0f, cameraRig::MAX_PITCH_RADIANS);
    EXPECT_NEAR(glm::length(up), 1.0f, kTol);
    EXPECT_NEAR(up.z, -1.0f, kTol);  // map north is up on screen
}

TEST(CameraRigTest, UpVectorIsWorldUpAtTheHorizon)
{
    const glm::vec3 up = cameraRig::UpVector(Degrees(33.0f), 0.0f);
    EXPECT_NEAR(up.x, 0.0f, kTol);
    EXPECT_NEAR(up.y, 1.0f, kTol);
    EXPECT_NEAR(up.z, 0.0f, kTol);
}

TEST(CameraRigTest, BasisIsOrthonormalAndOrbitsTheFocus)
{
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    params.yawRadians = Degrees(37.0f);

    const cameraRig::Basis b = cameraRig::MakeBasis(params);
    EXPECT_NEAR(glm::length(b.forward), 1.0f, kTol);
    EXPECT_NEAR(glm::length(b.right), 1.0f, kTol);
    EXPECT_NEAR(glm::length(b.up), 1.0f, kTol);
    EXPECT_NEAR(glm::dot(b.forward, b.up), 0.0f, kTol);
    EXPECT_NEAR(glm::dot(b.forward, b.right), 0.0f, kTol);
    EXPECT_NEAR(glm::dot(b.right, b.up), 0.0f, kTol);

    EXPECT_NEAR(glm::distance(b.eye, b.focus), b.distance, kTol);
    EXPECT_NEAR(b.focus.x, params.target.x, kTol);
    EXPECT_NEAR(b.focus.z, params.target.y, kTol);
    EXPECT_GT(b.eye.y, b.focus.y);  // the camera is always above the ground
}

TEST(CameraRigTest, PresetsSelectTheDocumentedAngles)
{
    const cameraRig::RigParams classic = MakeParams(cameraRig::Preset::Classic);
    EXPECT_EQ(classic.kind, cameraRig::ProjectionKind::Orthographic);
    EXPECT_NEAR(classic.pitchRadians, cameraRig::MAX_PITCH_RADIANS, kAngleTol);
    EXPECT_NEAR(classic.yawRadians, 0.0f, kAngleTol);

    const cameraRig::RigParams ds = MakeParams(cameraRig::Preset::DS);
    EXPECT_EQ(ds.kind, cameraRig::ProjectionKind::Perspective);
    EXPECT_NEAR(ds.pitchRadians, Degrees(51.34f), 1e-4f);
    EXPECT_NEAR(ds.fovYRadians, Degrees(15.0f), 1e-4f);
}

TEST(CameraRigTest, FreePresetKeepsTheUserAngles)
{
    cameraRig::RigParams params;
    params.yawRadians = Degrees(120.0f);
    params.pitchRadians = Degrees(35.0f);
    cameraRig::ApplyPreset(params, cameraRig::Preset::Free);

    EXPECT_NEAR(params.yawRadians, Degrees(120.0f), 1e-4f);
    EXPECT_NEAR(params.pitchRadians, Degrees(35.0f), 1e-4f);
    EXPECT_EQ(params.kind, cameraRig::ProjectionKind::Perspective);
}

TEST(CameraRigTest, FreePresetClampsAnOutOfRangeOrbit)
{
    cameraRig::RigParams params;
    params.pitchRadians = Degrees(-5.0f);
    params.yawRadians = Degrees(400.0f);
    cameraRig::ApplyPreset(params, cameraRig::Preset::Free);

    EXPECT_NEAR(params.pitchRadians, cameraRig::MIN_PITCH_RADIANS, kAngleTol);
    EXPECT_NEAR(params.yawRadians, Degrees(40.0f), 1e-4f);
}

TEST(CameraRigTest, PresetNamesRoundTrip)
{
    EXPECT_EQ(EnumTraits<cameraRig::Preset>::ToString(cameraRig::Preset::DS), "DS");
    EXPECT_EQ(EnumTraits<cameraRig::Preset>::FromString("Classic"), cameraRig::Preset::Classic);
    EXPECT_EQ(
        EnumTraits<cameraRig::ProjectionKind>::ToString(cameraRig::ProjectionKind::Perspective),
        "Perspective");
}

TEST(CameraRigTest, ClassicPresetReproducesTheFlatMapping)
{
    // the flat mapping subtracts cameraTopLeft, then applies ortho(0, visibleW, visibleH, 0).
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::vec2 screen{1520.0f, 855.0f};

    const glm::vec2 topLeft = params.target - params.visibleWorldSize * 0.5f;

    const glm::vec2 samples[] = {
        topLeft,                             // view corner
        params.target,                       // centre
        topLeft + params.visibleWorldSize,   // opposite corner
        topLeft + glm::vec2(73.0f, 21.0f),   // arbitrary interior point
        topLeft + glm::vec2(311.5f, 4.25f),  // sub-pixel offsets
    };

    for (const glm::vec2& world : samples)
    {
        const std::optional<glm::vec2> pixel =
            cameraRig::WorldToScreen(sceneMath::ToScene(world), viewProj, screen);
        ASSERT_TRUE(pixel.has_value());

        const glm::vec2 expected = (world - topLeft) * (screen / params.visibleWorldSize);
        EXPECT_NEAR(pixel->x, expected.x, kTol) << "world " << world.x << "," << world.y;
        EXPECT_NEAR(pixel->y, expected.y, kTol) << "world " << world.x << "," << world.y;
    }
}

TEST(CameraRigTest, ClassicPresetKeepsScreenAxesAlignedWithWorldAxes)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::vec2 screen{1520.0f, 855.0f};

    const glm::vec2 centre =
        *cameraRig::WorldToScreen(sceneMath::ToScene(params.target), viewProj, screen);
    const glm::vec2 east = *cameraRig::WorldToScreen(
        sceneMath::ToScene(params.target + glm::vec2(16.0f, 0.0f)), viewProj, screen);
    const glm::vec2 south = *cameraRig::WorldToScreen(
        sceneMath::ToScene(params.target + glm::vec2(0.0f, 16.0f)), viewProj, screen);

    // world +X maps right and world +Y maps down, despite the scene using Y for height.
    EXPECT_GT(east.x, centre.x);
    EXPECT_NEAR(east.y, centre.y, kTol);
    EXPECT_GT(south.y, centre.y);
    EXPECT_NEAR(south.x, centre.x, kTol);
}

TEST(CameraRigTest, ClassicGroundFootprintIsExactlyTheVisibleRect)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const cameraRig::GroundBounds bounds = cameraRig::GroundFootprintAabb(params);

    EXPECT_TRUE(bounds.complete);
    const glm::vec2 topLeft = params.target - params.visibleWorldSize * 0.5f;
    EXPECT_NEAR(bounds.min.x, topLeft.x, kTol);
    EXPECT_NEAR(bounds.min.y, topLeft.y, kTol);
    EXPECT_NEAR(bounds.max.x, topLeft.x + params.visibleWorldSize.x, kTol);
    EXPECT_NEAR(bounds.max.y, topLeft.y + params.visibleWorldSize.y, kTol);
}

TEST(CameraRigTest, ScreenToGroundInvertsWorldToScreen)
{
    // editor picking must invert projection under perspective and camera rotation.
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    params.yawRadians = Degrees(41.0f);
    params.pitchRadians = Degrees(55.0f);

    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const glm::vec2 screen{1520.0f, 855.0f};

    const glm::vec2 samples[] = {
        params.target,
        params.target + glm::vec2(50.0f, -30.0f),
        params.target + glm::vec2(-120.0f, 64.0f),
    };

    for (const glm::vec2& world : samples)
    {
        const std::optional<glm::vec2> pixel =
            cameraRig::WorldToScreen(sceneMath::ToScene(world), viewProj, screen);
        ASSERT_TRUE(pixel.has_value());

        const std::optional<glm::vec2> back =
            cameraRig::ScreenToGround(*pixel, screen, invViewProj);
        ASSERT_TRUE(back.has_value());
        EXPECT_NEAR(back->x, world.x, 0.05f);
        EXPECT_NEAR(back->y, world.y, 0.05f);
    }
}

TEST(CameraRigTest, ScreenToGroundRoundTripsUnderTheClassicPreset)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const glm::mat4 invViewProj = glm::inverse(cameraRig::BuildViewProjection(params));
    const glm::vec2 screen{1520.0f, 855.0f};

    // the top-left pixel maps to the visible world rectangle's top-left corner.
    const std::optional<glm::vec2> hit =
        cameraRig::ScreenToGround({0.0f, 0.0f}, screen, invViewProj);
    ASSERT_TRUE(hit.has_value());
    const glm::vec2 topLeft = params.target - params.visibleWorldSize * 0.5f;
    EXPECT_NEAR(hit->x, topLeft.x, kTol);
    EXPECT_NEAR(hit->y, topLeft.y, kTol);
}

TEST(CameraRigTest, ScreenToGroundRespectsAnElevatedPlane)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const glm::mat4 invViewProj = glm::inverse(cameraRig::BuildViewProjection(params));
    const glm::vec2 screen{1520.0f, 855.0f};

    const std::optional<glm::vec2> hit =
        cameraRig::ScreenToGround({760.0f, 427.5f}, screen, invViewProj, 32.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->x, params.target.x, 0.5f);
    EXPECT_NEAR(hit->y, params.target.y, 0.5f);
}

TEST(CameraRigTest, ScreenToGroundReturnsNothingAboveTheHorizon)
{
    // a ray toward the sky has no ground intersection.
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    params.pitchRadians = cameraRig::MIN_PITCH_RADIANS;
    params.fovYRadians = Degrees(60.0f);

    const glm::mat4 invViewProj = glm::inverse(cameraRig::BuildViewProjection(params));
    const glm::vec2 screen{1520.0f, 855.0f};

    EXPECT_FALSE(cameraRig::ScreenToGround({760.0f, 0.0f}, screen, invViewProj).has_value());
}

TEST(CameraRigTest, WorldToScreenRejectsPointsBehindTheCamera)
{
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    params.pitchRadians = Degrees(20.0f);

    const glm::mat4 viewProj = cameraRig::BuildViewProjection(params);
    const cameraRig::Basis basis = cameraRig::MakeBasis(params);

    const glm::vec3 behind = basis.eye - basis.forward * 1000.0f;
    EXPECT_FALSE(cameraRig::WorldToScreen(behind, viewProj, {1520.0f, 855.0f}).has_value());
}

TEST(CameraRigTest, YawedFootprintCoversTheRotatedDiagonal)
{
    // a 45 degree yaw expands the ground AABB beyond the unrotated view; weather
    // spawning must cover the expanded region.
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    const cameraRig::GroundBounds straight = cameraRig::GroundFootprintAabb(params);

    params.yawRadians = Degrees(45.0f);
    const cameraRig::GroundBounds yawed = cameraRig::GroundFootprintAabb(params);

    EXPECT_TRUE(yawed.complete);
    EXPECT_GT(yawed.max.x - yawed.min.x, straight.max.x - straight.min.x);
    EXPECT_GT(yawed.max.y - yawed.min.y, straight.max.y - straight.min.y);
}

TEST(CameraRigTest, FootprintIsFlaggedIncompleteWhenTheHorizonIsVisible)
{
    cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    params.pitchRadians = cameraRig::MIN_PITCH_RADIANS;
    params.fovYRadians = Degrees(60.0f);

    const cameraRig::GroundBounds bounds = cameraRig::GroundFootprintAabb(params);
    EXPECT_FALSE(bounds.complete);

    EXPECT_TRUE(std::isfinite(bounds.min.x));
    EXPECT_TRUE(std::isfinite(bounds.max.y));
    EXPECT_LE(bounds.min.x, params.target.x);
    EXPECT_GE(bounds.max.x, params.target.x);
}

TEST(CameraRigTest, FootprintAlwaysContainsTheFocus)
{
    for (const float pitchDeg : {15.0f, 45.0f, 90.0f})
    {
        for (const float yawDeg : {0.0f, 90.0f, 217.0f})
        {
            cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
            params.pitchRadians = Degrees(pitchDeg);
            params.yawRadians = Degrees(yawDeg);

            const cameraRig::GroundBounds b = cameraRig::GroundFootprintAabb(params);
            EXPECT_LE(b.min.x, params.target.x + kTol) << pitchDeg << " " << yawDeg;
            EXPECT_GE(b.max.x, params.target.x - kTol) << pitchDeg << " " << yawDeg;
            EXPECT_LE(b.min.y, params.target.y + kTol) << pitchDeg << " " << yawDeg;
            EXPECT_GE(b.max.y, params.target.y - kTol) << pitchDeg << " " << yawDeg;
        }
    }
}

TEST(CameraRigTest, PerspectiveDepthRangeBracketsTheFocus)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::DS);
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    cameraRig::DepthRange(params, nearPlane, farPlane);

    const float distance =
        cameraRig::DistanceForVisibleHeight(params.visibleWorldSize.y, params.fovYRadians);
    EXPECT_GT(nearPlane, 0.0f);
    EXPECT_LT(nearPlane, distance);
    EXPECT_GT(farPlane, distance);
}

TEST(CameraRigTest, PerspectiveNearPlaneGrowsWithDistance)
{
    // the near plane excludes empty space in front of the camera to preserve depth precision.
    const cameraRig::RigParams close = MakeParams(cameraRig::Preset::DS);
    cameraRig::RigParams distant = close;
    distant.visibleWorldSize *= 8.0f;

    float n0 = 0.0f;
    float f0 = 0.0f;
    float n1 = 0.0f;
    float f1 = 0.0f;
    cameraRig::DepthRange(close, n0, f0);
    cameraRig::DepthRange(distant, n1, f1);

    EXPECT_GT(n1, n0);
    EXPECT_GT(f1, f0);
}

TEST(CameraRigTest, OrthographicDepthRangeIsSymmetricAboutTheFocus)
{
    const cameraRig::RigParams params = MakeParams(cameraRig::Preset::Classic);
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    cameraRig::DepthRange(params, nearPlane, farPlane);

    const float distance =
        cameraRig::DistanceForVisibleHeight(params.visibleWorldSize.y, params.fovYRadians);
    EXPECT_NEAR((nearPlane + farPlane) * 0.5f, distance, kTol);
}

TEST(CameraRigTest, FocusClampsToTheMapRatherThanAViewportRect)
{
    const glm::vec2 mapSize{2000.0f, 2000.0f};
    EXPECT_EQ(cameraRig::ClampFocusToMap({-50.0f, 100.0f}, mapSize), glm::vec2(0.0f, 100.0f));
    EXPECT_EQ(cameraRig::ClampFocusToMap({5000.0f, 100.0f}, mapSize), glm::vec2(2000.0f, 100.0f));
    EXPECT_EQ(cameraRig::ClampFocusToMap({500.0f, 500.0f}, mapSize), glm::vec2(500.0f, 500.0f));
}
