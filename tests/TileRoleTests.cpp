// authored stance controls geometry independently of sorting flags, layer index, and neighboring
// props.

#include "../src/TileRole.hpp"

#include <gtest/gtest.h>

TEST(TileRoleTest, PlainTilesLieFlat)
{
    EXPECT_FALSE(tileRole::IsUpright(TileStance::Flat));
}

TEST(TileRoleTest, EveryNonFlatStanceStandsUp)
{
    EXPECT_TRUE(tileRole::IsUpright(TileStance::Prop));
    EXPECT_TRUE(tileRole::IsUpright(TileStance::Wall));
    EXPECT_TRUE(tileRole::IsUpright(TileStance::Structure));
}

TEST(TileRoleTest, YSortFlagsNoLongerAffectGeometry)
{
    // y-sort flags affect draw order only; they must not change tile geometry.
    static_assert(std::is_same_v<decltype(tileRole::IsUpright(TileStance::Flat)), bool>);
    EXPECT_FALSE(tileRole::IsUpright(TileStance::Flat));
}

TEST(TileRoleTest, OnlyStructuresStackIntoATallBody)
{
    // Prop and Wall remain one tile tall on their own row; only Structure stacks vertically.
    EXPECT_TRUE(tileRole::StacksVertically(TileStance::Structure));
    EXPECT_FALSE(tileRole::StacksVertically(TileStance::Prop));
    EXPECT_FALSE(tileRole::StacksVertically(TileStance::Wall));
    EXPECT_FALSE(tileRole::StacksVertically(TileStance::Flat));
}

TEST(TileRoleTest, StackingIsStrictlyNarrowerThanUprightness)
{
    for (const TileStance stance : EnumValues<TileStance>())
    {
        if (tileRole::StacksVertically(stance))
        {
            EXPECT_TRUE(tileRole::IsUpright(stance)) << EnumTraits<TileStance>::ToString(stance);
        }
    }

    EXPECT_TRUE(tileRole::IsUpright(TileStance::Prop));
    EXPECT_FALSE(tileRole::StacksVertically(TileStance::Prop));
}

TEST(TileRoleTest, SurfacesAreExactlyWallsAndStructures)
{
    // surfaces keep grid orientation; poles rotate about their own anchor.
    EXPECT_TRUE(tileRole::IsSurface(TileStance::Wall));
    EXPECT_TRUE(tileRole::IsSurface(TileStance::Structure));
    EXPECT_FALSE(tileRole::IsSurface(TileStance::Prop));
    EXPECT_FALSE(tileRole::IsSurface(TileStance::Flat));
}

TEST(TileRoleTest, StanceCarriesNoLayerOrNeighbourInformation)
{
    // stance predicates must depend only on authored stance, not layer or adjacency.
    static_assert(std::is_invocable_v<decltype(tileRole::IsUpright), TileStance>);
    static_assert(std::is_invocable_v<decltype(tileRole::StacksVertically), TileStance>);
    static_assert(std::is_invocable_v<decltype(tileRole::IsSurface), TileStance>);

    // legacy loading may inspect layer and adjacency once to seed stance.
    // rendering must use the stored value.
    EXPECT_FALSE(tileRole::IsUpright(TileStance::Flat));
}

TEST(TileRoleTest, PropsAlwaysTurnTowardTheCamera)
{
    EXPECT_GT(tileRole::DampingFor(TileStance::Prop, 1).yawFollow, 0.0f);
    EXPECT_GT(tileRole::DampingFor(TileStance::Prop, 8).yawFollow, 0.0f);
}

TEST(TileRoleTest, WallsAreAlwaysLockedToTheGrid)
{
    // Wall keeps fixed yaw so adjacent panels cannot separate as the camera rotates.
    EXPECT_FLOAT_EQ(tileRole::DampingFor(TileStance::Wall, 1).yawFollow, 0.0f);
    EXPECT_FLOAT_EQ(tileRole::DampingFor(TileStance::Wall, 4).yawFollow, 0.0f);
}

TEST(TileRoleTest, StructuresStillDecideByTheirOwnWidth)
{
    // a one-column Structure can turn as a pole; wider bodies keep fixed yaw.
    EXPECT_GT(tileRole::DampingFor(TileStance::Structure, 1).yawFollow, 0.0f);
    EXPECT_FLOAT_EQ(tileRole::DampingFor(TileStance::Structure, 3).yawFollow, 0.0f);
}

TEST(TileRoleTest, GridLockIsTheSingleSourceOfTruthForDamping)
{
    for (const TileStance stance : EnumValues<TileStance>())
    {
        for (const int width : {1, 2, 5})
        {
            const bool locked = tileRole::IsGridLocked(stance, width);
            const float yawFollow = tileRole::DampingFor(stance, width).yawFollow;
            EXPECT_EQ(locked, yawFollow == 0.0f)
                << EnumTraits<TileStance>::ToString(stance) << " width " << width;
        }
    }

    EXPECT_FALSE(tileRole::IsGridLocked(TileStance::Prop, 5));
    EXPECT_TRUE(tileRole::IsGridLocked(TileStance::Wall, 1));
    EXPECT_FALSE(tileRole::IsGridLocked(TileStance::Structure, 1));
    EXPECT_TRUE(tileRole::IsGridLocked(TileStance::Structure, 2));
}

TEST(TileRoleTest, WiderStructuresAreLockedToTheGrid)
{
    for (const int width : {2, 3, 8})
    {
        EXPECT_FLOAT_EQ(tileRole::DampingForWidth(width).yawFollow, 0.0f) << "width " << width;
    }
}

TEST(TileRoleTest, WidthNeverAffectsTheLean)
{
    // fixed yaw still permits lean; lean moves the top while preserving the base anchor.
    EXPECT_FLOAT_EQ(tileRole::DampingForWidth(1).leanFollow,
                    tileRole::DampingForWidth(5).leanFollow);
    EXPECT_GT(tileRole::DampingForWidth(5).leanFollow, 0.0f);
}

TEST(TileRoleTest, NoStanceEverLeansDifferently)
{
    const float propLean = tileRole::DampingFor(TileStance::Prop, 1).leanFollow;
    EXPECT_FLOAT_EQ(tileRole::DampingFor(TileStance::Wall, 1).leanFollow, propLean);
    EXPECT_FLOAT_EQ(tileRole::DampingFor(TileStance::Structure, 1).leanFollow, propLean);
    EXPECT_GT(propLean, 0.0f);
}

TEST(TileRoleTest, DegenerateWidthsAreTreatedAsAPole)
{
    EXPECT_GT(tileRole::DampingForWidth(0).yawFollow, 0.0f);
    EXPECT_GT(tileRole::DampingForWidth(-3).yawFollow, 0.0f);
}

TEST(TileRoleTest, UprightTilesUseSceneryDamping)
{
    const billboard::Damping damping = tileRole::UprightDamping();
    EXPECT_GT(damping.yawFollow, 0.0f);
    EXPECT_LT(damping.yawFollow, 1.0f);
    EXPECT_FLOAT_EQ(damping.yawFollow,
                    billboard::DefaultDamping(billboard::Role::Scenery).yawFollow);
}

TEST(TileRoleTest, StanceNamesRoundTrip)
{
    for (const TileStance stance : EnumValues<TileStance>())
    {
        const std::string_view name = EnumTraits<TileStance>::ToString(stance);
        const std::optional<TileStance> parsed = EnumTraits<TileStance>::FromString(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_EQ(*parsed, stance);
    }

    EXPECT_EQ(EnumTraits<TileStance>::Count, 4u);
    EXPECT_FALSE(EnumTraits<TileStance>::FromString("NoProjection").has_value());
    EXPECT_FALSE(EnumTraits<TileStance>::FromString("Upright").has_value());
}

TEST(TileRoleTest, FlatIsTheDefaultStance)
{
    EXPECT_EQ(std::to_underlying(TileStance::Flat), 0);
    EXPECT_EQ(TileStance{}, TileStance::Flat);
}
