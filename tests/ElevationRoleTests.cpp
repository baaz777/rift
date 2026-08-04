// ramp joins use the neighboring layer role as well as cell height.

#include "../src/ElevationRole.hpp"

#include <gtest/gtest.h>

TEST(ElevationRoleTest, GroundIgnoresItsCellElevation)
{
    // Ground ignores cell elevation, so water can remain below a bridge.
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(0, ElevationRole::Ground), 0.0f);
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(6, ElevationRole::Ground), 0.0f);
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(-8, ElevationRole::Ground), 0.0f);
}

TEST(ElevationRoleTest, RaisedIsExactlyItsCellElevation)
{
    // height uses pixels at 1:1 scale, matching Elevation::offset for actors.
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(6, ElevationRole::Raised), 6.0f);
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(10, ElevationRole::Raised), 10.0f);
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(-4, ElevationRole::Raised), -4.0f);
}

TEST(ElevationRoleTest, ARampCellIsRaisedToo)
{
    EXPECT_FLOAT_EQ(elevationRole::SurfaceHeight(4, ElevationRole::Ramp), 4.0f);
}

TEST(ElevationRoleTest, RampMeetsARaisedNeighbourAtTheNeighboursHeight)
{
    // a ramp edge must meet a deck exactly; averaging their heights would create a step.
    const elevationRole::NeighbourSurface deck{6, ElevationRole::Raised};
    EXPECT_FLOAT_EQ(elevationRole::EdgeHeight(4, deck), 6.0f);
}

TEST(ElevationRoleTest, RampMeetsBareGroundAtZero)
{
    const elevationRole::NeighbourSurface grass{0, ElevationRole::Ground};
    EXPECT_FLOAT_EQ(elevationRole::EdgeHeight(2, grass), 0.0f);

    // a Ground neighbor contributes zero, regardless of its cell elevation.
    const elevationRole::NeighbourSurface unmarked{6, ElevationRole::Ground};
    EXPECT_FLOAT_EQ(elevationRole::EdgeHeight(2, unmarked), 0.0f);
}

TEST(ElevationRoleTest, RampMeetsARampNeighbourAtTheAverage)
{
    // adjacent ramps share the midpoint height to keep the slope continuous.
    const elevationRole::NeighbourSurface other{2, ElevationRole::Ramp};
    EXPECT_FLOAT_EQ(elevationRole::EdgeHeight(4, other), 3.0f);
}

TEST(ElevationRoleTest, TheShippedBridgeRampIsContinuous)
{
    // along X: ground 0 -> ramp 2 -> ramp 4 -> deck 6.
    // each high edge must meet the next low edge.
    const elevationRole::NeighbourSurface ground{0, ElevationRole::Ground};
    const elevationRole::NeighbourSurface ramp2{2, ElevationRole::Ramp};
    const elevationRole::NeighbourSurface ramp4{4, ElevationRole::Ramp};
    const elevationRole::NeighbourSurface deck{6, ElevationRole::Raised};

    const float cellA_low = elevationRole::EdgeHeight(2, ground);
    const float cellA_high = elevationRole::EdgeHeight(2, ramp4);
    const float cellB_low = elevationRole::EdgeHeight(4, ramp2);
    const float cellB_high = elevationRole::EdgeHeight(4, deck);

    EXPECT_FLOAT_EQ(cellA_low, 0.0f);
    EXPECT_FLOAT_EQ(cellA_high, 3.0f);
    EXPECT_FLOAT_EQ(cellB_low, 3.0f);   // same seam as cellA_high
    EXPECT_FLOAT_EQ(cellB_high, 6.0f);  // lands on the deck exactly
    EXPECT_FLOAT_EQ(cellA_high, cellB_low);
}

TEST(ElevationRoleTest, RoleNamesRoundTrip)
{
    for (const ElevationRole role : EnumValues<ElevationRole>())
    {
        const std::string_view name = EnumTraits<ElevationRole>::ToString(role);
        const std::optional<ElevationRole> parsed = EnumTraits<ElevationRole>::FromString(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_EQ(*parsed, role);
    }

    EXPECT_EQ(EnumTraits<ElevationRole>::Count, 3u);
    EXPECT_FALSE(EnumTraits<ElevationRole>::FromString("Elevated").has_value());
}

TEST(ElevationRoleTest, GroundIsTheDefaultRole)
{
    // Ground must be zero because tile arrays use value initialization.
    EXPECT_EQ(std::to_underlying(ElevationRole::Ground), 0);
    EXPECT_EQ(ElevationRole{}, ElevationRole::Ground);
}
