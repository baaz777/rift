// sprite rows follow camera yaw; movement stays in world axes. yaw zero is the identity.
#include "../src/CameraFacing.hpp"
#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"

#include <gtest/gtest.h>

namespace
{
constexpr float kTol = 1e-5f;

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

constexpr glm::vec2 kViewport{1000.0f, 500.0f};
}  // namespace

TEST(CameraOrbitInputTest, DraggingRightSwingsTheCameraWest)
{
    // orbit opposite to the drag so the ground follows the cursor.
    cameraRig::OrbitAngles a;
    a.yawRadians = 0.0f;
    a.pitchRadians = Degrees(45.0f);

    const cameraRig::OrbitAngles moved = cameraRig::ApplyOrbitDrag(a, {100.0f, 0.0f}, kViewport);
    EXPECT_LT(moved.yawRadians, 0.0f);
}

TEST(CameraOrbitInputTest, AFullSweepRotatesTheDocumentedAmount)
{
    cameraRig::OrbitAngles a;
    a.yawRadians = 0.0f;
    a.pitchRadians = Degrees(45.0f);

    const cameraRig::OrbitAngles moved =
        cameraRig::ApplyOrbitDrag(a, {kViewport.x, 0.0f}, kViewport);
    EXPECT_NEAR(moved.yawRadians, cameraRig::WrapYaw(-cameraRig::DRAG_SWEEP_RADIANS), 1e-4f);
}

TEST(CameraOrbitInputTest, DraggingDownRaisesTheCameraTowardTopDown)
{
    cameraRig::OrbitAngles a;
    a.yawRadians = 0.0f;
    a.pitchRadians = Degrees(45.0f);

    const cameraRig::OrbitAngles down = cameraRig::ApplyOrbitDrag(a, {0.0f, 50.0f}, kViewport);
    EXPECT_GT(down.pitchRadians, a.pitchRadians);

    const cameraRig::OrbitAngles up = cameraRig::ApplyOrbitDrag(a, {0.0f, -50.0f}, kViewport);
    EXPECT_LT(up.pitchRadians, a.pitchRadians);
}

TEST(CameraOrbitInputTest, DragCannotPushThePitchOutOfRange)
{
    cameraRig::OrbitAngles a;
    a.pitchRadians = Degrees(45.0f);

    const cameraRig::OrbitAngles farDown =
        cameraRig::ApplyOrbitDrag(a, {0.0f, 100000.0f}, kViewport);
    EXPECT_NEAR(farDown.pitchRadians, cameraRig::MAX_PITCH_RADIANS, kTol);

    const cameraRig::OrbitAngles farUp =
        cameraRig::ApplyOrbitDrag(a, {0.0f, -100000.0f}, kViewport);
    EXPECT_NEAR(farUp.pitchRadians, cameraRig::MIN_PITCH_RADIANS, kTol);
}

TEST(CameraOrbitInputTest, RepeatedDraggingKeepsYawBounded)
{
    // wrapping the angle prevents loss of precision after many rotations.
    cameraRig::OrbitAngles a;
    for (int i = 0; i < 500; ++i)
    {
        a = cameraRig::ApplyOrbitDrag(a, {kViewport.x, 0.0f}, kViewport);
    }
    EXPECT_LE(std::abs(a.yawRadians), rift::PiF + kTol);
}

TEST(CameraOrbitInputTest, DragIsResolutionIndependent)
{
    const cameraRig::OrbitAngles a;
    const cameraRig::OrbitAngles small =
        cameraRig::ApplyOrbitDrag(a, {50.0f, 0.0f}, {500.0f, 250.0f});
    const cameraRig::OrbitAngles large =
        cameraRig::ApplyOrbitDrag(a, {200.0f, 0.0f}, {2000.0f, 1000.0f});
    EXPECT_NEAR(small.yawRadians, large.yawRadians, kTol);
}

TEST(CameraFacingTest, IdentityAtYawZero)
{
    for (const CharacterDirection dir : {CharacterDirection::UP,
                                         CharacterDirection::DOWN,
                                         CharacterDirection::LEFT,
                                         CharacterDirection::RIGHT})
    {
        EXPECT_EQ(cameraFacing::ScreenFacing(dir, 0.0f), dir);
    }
}

TEST(CameraFacingTest, OrbitingBehindACharacterShowsTheirBack)
{
    // at half a turn, world-south faces away from the viewer and uses the up row.
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::DOWN, rift::PiF),
              CharacterDirection::UP);
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::UP, rift::PiF),
              CharacterDirection::DOWN);
}

TEST(CameraFacingTest, QuarterTurnMapsToProfileRows)
{
    // from the east, world-south motion crosses the screen.
    const float yaw = Degrees(90.0f);
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::DOWN, yaw), CharacterDirection::LEFT);
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::UP, yaw), CharacterDirection::RIGHT);
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::RIGHT, yaw), CharacterDirection::DOWN);
    EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::LEFT, yaw), CharacterDirection::UP);
}

TEST(CameraFacingTest, SmallYawDoesNotChangeTheRow)
{
    // row changes occur at the 45 degree bisector to avoid flicker near cardinal headings.
    for (const float yawDeg : {-40.0f, -10.0f, 0.0f, 10.0f, 40.0f})
    {
        EXPECT_EQ(cameraFacing::ScreenFacing(CharacterDirection::DOWN, Degrees(yawDeg)),
                  CharacterDirection::DOWN)
            << yawDeg;
    }
}

TEST(CameraFacingTest, EveryYawYieldsSomeRowAndStaysAFullTurnConsistent)
{
    int seen[4] = {0, 0, 0, 0};
    for (int deg = -180; deg < 180; ++deg)
    {
        const CharacterDirection row =
            cameraFacing::ScreenFacing(CharacterDirection::DOWN, Degrees(static_cast<float>(deg)));
        ++seen[static_cast<int>(row)];
    }
    for (const int count : seen)
    {
        EXPECT_GT(count, 0);
    }
}
