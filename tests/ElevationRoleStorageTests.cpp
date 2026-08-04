// height is shared per cell, while elevation participation is stored independently on each layer.

#include "../src/ElevationRole.hpp"
#include "../src/Tilemap.hpp"

#include <gtest/gtest.h>

TEST(ElevationRoleStorageTest, RoleIsPerLayerNotPerCell)
{
    // roles are per layer so water can remain below a raised bridge deck.
    Tilemap tm;
    tm.SetTilemapSize(8, 8, false);

    tm.SetLayerElevationRole(3, 3, 0, ElevationRole::Ground);
    tm.SetLayerElevationRole(3, 3, 2, ElevationRole::Raised);

    EXPECT_EQ(tm.GetLayerElevationRole(3, 3, 0), ElevationRole::Ground);
    EXPECT_EQ(tm.GetLayerElevationRole(3, 3, 2), ElevationRole::Raised);
}

TEST(ElevationRoleStorageTest, ElevationItselfStaysPerCell)
{
    // height is shared per cell; each layer chooses whether to use it.
    Tilemap tm;
    tm.SetTilemapSize(8, 8, false);
    tm.SetElevation(3, 3, 6);

    EXPECT_EQ(tm.GetElevation(3, 3), 6);
    EXPECT_EQ(tm.GetLayerElevationRole(3, 3, 0), ElevationRole::Ground);
    EXPECT_EQ(tm.GetLayerElevationRole(3, 3, 2), ElevationRole::Ground);
}

TEST(ElevationRoleStorageTest, EveryCellStartsGround)
{
    Tilemap tm;
    tm.SetTilemapSize(4, 4, false);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            EXPECT_EQ(tm.GetLayerElevationRole(x, y, 0), ElevationRole::Ground)
                << "at (" << x << ", " << y << ")";
        }
    }
}

TEST(ElevationRoleStorageTest, ResizeKeepsTheArrayAddressable)
{
    // write the last cell of the last layer to catch arrays omitted from resize_all.
    Tilemap tm;
    tm.SetTilemapSize(16, 16, false);
    tm.SetLayerElevationRole(15, 15, 9, ElevationRole::Ramp);
    EXPECT_EQ(tm.GetLayerElevationRole(15, 15, 9), ElevationRole::Ramp);
}

TEST(ElevationRoleStorageTest, ResizingAgainResetsToGround)
{
    Tilemap tm;
    tm.SetTilemapSize(8, 8, false);
    tm.SetLayerElevationRole(2, 2, 0, ElevationRole::Ramp);
    ASSERT_EQ(tm.GetLayerElevationRole(2, 2, 0), ElevationRole::Ramp);

    tm.SetTilemapSize(8, 8, false);
    EXPECT_EQ(tm.GetLayerElevationRole(2, 2, 0), ElevationRole::Ground);
}

TEST(ElevationRoleStorageTest, OutOfRangeReadsAreGroundNotACrash)
{
    Tilemap tm;
    tm.SetTilemapSize(4, 4, false);
    EXPECT_EQ(tm.GetLayerElevationRole(-1, 0, 0), ElevationRole::Ground);
    EXPECT_EQ(tm.GetLayerElevationRole(0, 99, 0), ElevationRole::Ground);
    EXPECT_EQ(tm.GetLayerElevationRole(0, 0, 99), ElevationRole::Ground);
}

TEST(ElevationRoleStorageTest, OutOfRangeWritesAreIgnoredNotACrash)
{
    Tilemap tm;
    tm.SetTilemapSize(4, 4, false);
    tm.SetLayerElevationRole(-1, 0, 0, ElevationRole::Raised);
    tm.SetLayerElevationRole(0, 0, 99, ElevationRole::Raised);
    EXPECT_EQ(tm.GetLayerElevationRole(0, 0, 0), ElevationRole::Ground);
}
