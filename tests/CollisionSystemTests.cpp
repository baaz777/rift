// the 16x16 hitbox sits at the feet. on a 16x16 tile, feet at
// (tx * 16 + 8, ty * 16 + 16) place the box over that tile.

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "../src/CollisionSystem.hpp"
#include "../src/Hitbox.hpp"
#include "../src/Tilemap.hpp"

namespace
{
constexpr float TILE = 16.0f;

glm::vec2 FeetAtTile(int tx, int ty)
{
    return glm::vec2(tx * TILE + TILE * 0.5f, ty * TILE + TILE);
}

class CollisionSystemTest : public ::testing::Test
{
protected:
    Tilemap tilemap;
    Hitbox hitbox;  // default player hitbox (one tile); CollisionSystem is stateless free fns

    void SetUp() override { tilemap.SetTilemapSize(20, 20, false); }

    void PaintSolid(int x, int y) { tilemap.SetTileCollision(x, y, true); }
};
}  // namespace

TEST(CollisionSystemStatic, FollowAlpha_ZeroDelta_Yields_Zero)
{
    EXPECT_FLOAT_EQ(CollisionSystem::CalculateFollowAlpha(0.0f, 0.2f), 0.0f);
}

TEST(CollisionSystemStatic, FollowAlpha_ReachesEpsilon_At_SettleTime)
{
    // the formula should produce alpha = 1 - epsilon when dt == settleTime.
    const float settle = 0.2f;
    const float eps = 0.01f;
    float a = CollisionSystem::CalculateFollowAlpha(settle, settle, eps);
    EXPECT_NEAR(a, 1.0f - eps, 1e-4f);
}

TEST(CollisionSystemStatic, FollowAlpha_Is_FrameRateIndependent)
{
    // two dt steps must equal one 2*dt step: alpha = 1 - (1 - alphaStep)^n.
    const float settle = 0.2f;
    const float dt = 0.033f;
    float a1 = CollisionSystem::CalculateFollowAlpha(dt, settle);
    float a2 = CollisionSystem::CalculateFollowAlpha(2.0f * dt, settle);

    // remaining distance after two dt steps = (1-a1)^2; after one 2dt step = (1-a2).
    float twoStep = (1.0f - a1) * (1.0f - a1);
    float oneStep = (1.0f - a2);
    EXPECT_NEAR(twoStep, oneStep, 1e-5f);
}

TEST(CollisionSystemStatic, FollowAlpha_ClampedToUnit)
{
    float a = CollisionSystem::CalculateFollowAlpha(1000.0f, 0.2f);
    EXPECT_LE(a, 1.0f);
    EXPECT_GE(a, 0.0f);
}

TEST_F(CollisionSystemTest, Strict_Empty_NoCollision)
{
    glm::vec2 pos = FeetAtTile(5, 5);
    EXPECT_FALSE(CollisionSystem::CollidesWithTilesStrict(hitbox, pos, &tilemap, 0, 0, false));
}

TEST_F(CollisionSystemTest, Strict_CenteredOnSolidTile_IsBlocked)
{
    PaintSolid(5, 5);
    glm::vec2 pos = FeetAtTile(5, 5);
    EXPECT_TRUE(CollisionSystem::CollidesWithTilesStrict(hitbox, pos, &tilemap, 0, 1, false));
}

TEST_F(CollisionSystemTest, Strict_OffscreenTilesAreSkipped)
{
    // strict collision skips out-of-bounds tiles; callers enforce map boundaries.
    glm::vec2 pos = FeetAtTile(-1, 5);
    EXPECT_FALSE(CollisionSystem::CollidesWithTilesStrict(hitbox, pos, &tilemap, -1, 0, false));
}

TEST_F(CollisionSystemTest, Strict_WalkingIntoWall_IsBlocked)
{
    PaintSolid(6, 5);
    glm::vec2 pos = FeetAtTile(6, 5);  // feet dead-center of solid tile
    EXPECT_TRUE(CollisionSystem::CollidesWithTilesStrict(hitbox, pos, &tilemap, 1, 0, false));
}

TEST_F(CollisionSystemTest, Strict_StandingInClearedAdjacentTile_NotBlocked)
{
    PaintSolid(6, 5);
    glm::vec2 pos = FeetAtTile(4, 5);  // two tiles away, safely clear
    EXPECT_FALSE(CollisionSystem::CollidesWithTilesStrict(hitbox, pos, &tilemap, 1, 0, false));
}

TEST_F(CollisionSystemTest, CollidesAt_ForwardsToStrict)
{
    PaintSolid(5, 5);
    glm::vec2 pos = FeetAtTile(5, 5);
    EXPECT_TRUE(CollisionSystem::CollidesAt(hitbox, pos, &tilemap, nullptr, 0, 0, false));
}

TEST_F(CollisionSystemTest, CollidesAt_NpcOverlap_IsBlocked)
{
    std::vector<CharacterCollisionBody> npcs = {
        CharacterCollisionBody{FeetAtTile(5, 5), {SupportSurface::Ground, 0}}};
    glm::vec2 pos = FeetAtTile(5, 5);
    EXPECT_TRUE(CollisionSystem::CollidesAt(hitbox, pos, &tilemap, &npcs, 0, 0, false));
}

TEST_F(CollisionSystemTest, NpcOnOtherSurfaceDoesNotBlock)
{
    std::vector<CharacterCollisionBody> npcs = {
        CharacterCollisionBody{FeetAtTile(5, 5), {SupportSurface::Elevation, 10}}};
    EXPECT_FALSE(
        CollisionSystem::CollidesAt(hitbox, FeetAtTile(5, 5), &tilemap, &npcs, 0, 0, false));
}

TEST_F(CollisionSystemTest, NpcOnDifferentElevationHeightDoesNotBlock)
{
    std::vector<CharacterCollisionBody> npcs = {
        CharacterCollisionBody{FeetAtTile(5, 5), {SupportSurface::Elevation, 10}}};
    EXPECT_FALSE(CollisionSystem::CollidesAt(
        hitbox, FeetAtTile(5, 5), &tilemap, &npcs, 0, 0, false, {SupportSurface::Elevation, 6}));
    EXPECT_TRUE(CollisionSystem::CollidesAt(
        hitbox, FeetAtTile(5, 5), &tilemap, &npcs, 0, 0, false, {SupportSurface::Elevation, 10}));
}
