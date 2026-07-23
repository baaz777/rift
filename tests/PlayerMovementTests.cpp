// a real Tilemap and player entity exercise movement, collision, and release settling together.

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "../src/AnimationState.hpp"
#include "../src/CharacterConstants.hpp"
#include "../src/CharacterKinematics.hpp"
#include "../src/CollisionSystem.hpp"
#include "../src/Elevation.hpp"
#include "../src/EntityStore.hpp"
#include "../src/Hitbox.hpp"
#include "../src/Motor.hpp"
#include "../src/PlayerInputState.hpp"
#include "../src/PlayerModes.hpp"
#include "../src/PlayerSystem.hpp"
#include "../src/Tilemap.hpp"
#include "../src/Transform.hpp"

#include <entt/entt.hpp>

namespace
{
constexpr float TILE = 16.0f;
constexpr float DT = 1.0f / 60.0f;

float AlignedCenterX(float x)
{
    return std::round((x - TILE * 0.5f) / TILE) * TILE + TILE * 0.5f;
}
float AlignedBottomY(float y)
{
    return std::round(y / TILE) * TILE;
}

class PlayerMovementTest : public ::testing::Test
{
protected:
    entt::registry world;
    entt::entity player = entt::null;
    Tilemap tilemap;

    void SetUp() override
    {
        tilemap.SetTilemapSize(40, 40, false);
        player = EntityStore::SpawnPlayer(world);
    }

    glm::vec2 Pos() const { return world.get<Transform>(player).position; }
    bool IsMoving() const { return world.get<PlayerInputState>(player).isMoving; }
};
}  // namespace

TEST_F(PlayerMovementTest, GlideToRestLandsTileAligned)
{
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 3.0f, 10.0f * TILE + 1.0f));

    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }
    ASSERT_TRUE(IsMoving());

    int frames = 0;
    while (IsMoving() && frames < 2000)
    {
        PlayerSystem::Move(world, player, glm::vec2(0.0f), DT, &tilemap, nullptr);
        ++frames;
    }
    for (int i = 0; i < 60; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(0.0f), DT, &tilemap, nullptr);  // settle
    }

    glm::vec2 pos = Pos();
    EXPECT_NEAR(pos.x, AlignedCenterX(pos.x), 0.6f);
    EXPECT_NEAR(pos.y, AlignedBottomY(pos.y), 0.6f);
}

TEST_F(PlayerMovementTest, ReleaseDoesNotStopInstantly)
{
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));
    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }
    glm::vec2 before = Pos();
    PlayerSystem::Move(world, player, glm::vec2(0.0f), DT, &tilemap, nullptr);  // one idle frame
    glm::vec2 after = Pos();
    EXPECT_GT(glm::length(after - before), 0.0f);  // still gliding
    EXPECT_TRUE(IsMoving());
}

TEST_F(PlayerMovementTest, WallStopsMovementCleanly)
{
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));
    // three wall tiles prevent sliding around the middle tile during this probe.
    tilemap.SetTileCollision(11, 9, true);
    tilemap.SetTileCollision(11, 10, true);
    tilemap.SetTileCollision(11, 11, true);

    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }

    EXPECT_LT(Pos().x, 11.0f * TILE);

    EXPECT_LT(std::abs(world.get<Motor>(player).velocity.x), 5.0f);
}

// walking animation follows velocity through the release glide.
TEST_F(PlayerMovementTest, AnimationFollowsVelocityNotInput)
{
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));
    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }
    ASSERT_TRUE(IsMoving());

    PlayerSystem::Move(world, player, glm::vec2(0.0f), DT, &tilemap, nullptr);  // released, gliding
    EXPECT_TRUE(IsMoving());                                                    // still in motion

    int frames = 0;
    while (IsMoving() && frames < 2000)
    {
        PlayerSystem::Move(world, player, glm::vec2(0.0f), DT, &tilemap, nullptr);
        ++frames;
    }
    EXPECT_FALSE(IsMoving());  // eventually idles when velocity reaches zero
}

TEST_F(PlayerMovementTest, InputCaptureStopClearsLatchedWalkAnimation)
{
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));
    for (int i = 0; i < 30; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
        PlayerSystem::Update(world, player, DT);
    }

    ASSERT_TRUE(IsMoving());
    ASSERT_GT(glm::length(world.get<Motor>(player).velocity), 0.0f);
    ASSERT_NE(world.get<PlayerModes>(player).animationType, AnimationType::IDLE);

    // console capture skips movement but still runs cosmetic updates; stop must clear both walk
    // inputs.
    PlayerSystem::Stop(world, player);
    for (int i = 0; i < 60; ++i)
    {
        PlayerSystem::Update(world, player, DT);
    }

    const Motor& motor = world.get<Motor>(player);
    const AnimationState& animation = world.get<AnimationState>(player);
    EXPECT_FALSE(IsMoving());
    EXPECT_FLOAT_EQ(motor.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(motor.velocity.y, 0.0f);
    EXPECT_EQ(world.get<PlayerModes>(player).animationType, AnimationType::IDLE);
    EXPECT_EQ(animation.currentFrame, 0);
    EXPECT_EQ(animation.walkSequenceIndex, 0);
}

// a perpendicular corner slide redirects forward motion. keep forward velocity
// to avoid accelerating from rest again on each contact.
TEST_F(PlayerMovementTest, CornerSlideRoundsAtFullSpeedNotJelly)
{
    // approach one solid tile from the left with clear space above, so held right
    // input can slide around its corner.
    tilemap.SetTileCollision(15, 11, true);
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(13.0f * TILE + 8.0f, 11.0f * TILE + 16.0f));

    constexpr int FRAMES = 55;
    const float startX = Pos().x;
    for (int i = 0; i < FRAMES; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }
    const float forwardProgress = Pos().x - startX;

    // full-speed travel is about 45 px. a corner slide reaches about 38 px;
    // resetting forward velocity gives about 29 px. the 33 px bound separates them.
    EXPECT_GT(forwardProgress, 33.0f);
}

TEST_F(PlayerMovementTest, LaneSnapCorrectionDoesNotBecomeDiagonalInput)
{
    // lane correction may graze a diagonal tile during cardinal movement. probing
    // that correction as diagonal input would reject a permitted corner overlap.
    tilemap.SetTileCollision(6, 4, true);
    tilemap.SetCornerCutBlocked(6, 4, Tilemap::CORNER_BL, true);
    PlayerSystem::SetPositionRaw(world, player, glm::vec2(89.0f, 92.0f));

    const glm::vec2 before = Pos();
    PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);

    EXPECT_GT(Pos().x, before.x);
    EXPECT_GT(Pos().y, before.y);
}

// keep both players walking to isolate cadence changes from movement-state changes.
TEST_F(PlayerMovementTest, FasterSpeedAnimatesQuicker)
{
    auto framesForAdvances = [](float speedMult, int advances)
    {
        entt::registry w;
        entt::entity p = EntityStore::SpawnPlayer(w);
        PlayerSystem::SetPositionRaw(w, p, glm::vec2(5.0f * TILE + 8.0f, 5.0f * TILE + 16.0f));
        w.get<PlayerModes>(p).speedMultiplier = speedMult;

        for (int i = 0; i < 180; ++i)
        {
            PlayerSystem::Move(w, p, glm::vec2(1.0f, 0.0f), DT, nullptr, nullptr);
            PlayerSystem::Update(w, p, DT);
        }
        int seen = 0;
        int frames = 0;
        int last = w.get<AnimationState>(p).walkSequenceIndex;
        while (seen < advances && frames < 5000)
        {
            PlayerSystem::Move(w, p, glm::vec2(1.0f, 0.0f), DT, nullptr, nullptr);
            PlayerSystem::Update(w, p, DT);
            if (w.get<AnimationState>(p).walkSequenceIndex != last)
            {
                last = w.get<AnimationState>(p).walkSequenceIndex;
                ++seen;
            }
            ++frames;
        }
        return frames;
    };

    int normalFrames = framesForAdvances(1.0f, 8);  // ~50 px/s, walk state
    int fastFrames = framesForAdvances(2.0f, 8);    // ~100 px/s, walk state
    EXPECT_LT(fastFrames, normalFrames);
}

TEST_F(PlayerMovementTest, PerpendicularBridgeCrossingWalksUnder)
{
    for (int x = 8; x <= 14; ++x)
    {
        tilemap.SetElevation(x, 10, 10);
    }
    tilemap.SetTileCollision(10, 10, true);
    PlayerSystem::SetPositionRaw(world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE));

    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(0.0f, 1.0f), DT, &tilemap, nullptr);
    }

    EXPECT_GT(Pos().y, 11.0f * TILE);
    EXPECT_EQ(world.get<Elevation>(player).surface, SupportSurface::Ground);
    EXPECT_EQ(world.get<Elevation>(player).plane, 0);
}

TEST_F(PlayerMovementTest, RampEntryPromotesPlayerOntoDeck)
{
    for (int y = 9; y <= 11; ++y)
    {
        tilemap.SetElevation(11, y, 6);
        for (int x = 12; x <= 18; ++x)
        {
            tilemap.SetElevation(x, y, 10);
        }
    }
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));

    for (int i = 0; i < 120; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }

    EXPECT_GT(Pos().x, 12.0f * TILE);
    EXPECT_EQ(world.get<Elevation>(player).surface, SupportSurface::Elevation);
    EXPECT_EQ(world.get<Elevation>(player).plane, 10);
}

TEST_F(PlayerMovementTest, DeckCollisionBlocksBeforeSupportCommit)
{
    for (int y = 8; y <= 12; ++y)
    {
        tilemap.SetElevation(11, y, 6);
        for (int x = 12; x <= 18; ++x)
        {
            tilemap.SetElevation(x, y, 10);
        }
        tilemap.SetTileCollision(12, y, true);
    }
    PlayerSystem::SetPositionRaw(
        world, player, glm::vec2(10.0f * TILE + 8.0f, 10.0f * TILE + 16.0f));

    for (int i = 0; i < 180; ++i)
    {
        PlayerSystem::Move(world, player, glm::vec2(1.0f, 0.0f), DT, &tilemap, nullptr);
    }

    const Elevation& elevation = world.get<Elevation>(player);
    EXPECT_LT(Pos().x, 12.0f * TILE);
    EXPECT_EQ(elevation.surface, SupportSurface::Elevation);
    EXPECT_EQ(elevation.plane, 6);
    EXPECT_FALSE(
        CollisionSystem::CollidesWithTilesStrict(world.get<Hitbox>(player),
                                                 Pos(),
                                                 &tilemap,
                                                 0,
                                                 0,
                                                 false,
                                                 CharacterKinematics::GetSupport(elevation)));
}
