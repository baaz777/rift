// both light-pool paths share schedule resolution; expected alpha values remain literal to avoid
// repeating the implementation.

#include "../src/SkyDrawList.hpp"
#include "../src/WeatherDefinitions.hpp"
#include "../src/WorldLightPools.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

constexpr float kNight = 23.0f;
constexpr float kNoon = 12.0f;

WorldLight MakeLight(glm::vec2 position, LightSchedule schedule, float radius = 64.0f)
{
    WorldLight light;
    light.position = position;
    light.schedule = schedule;
    light.radius = radius;
    light.color = glm::vec3(1.0f, 0.85f, 0.55f);
    return light;
}

}  // namespace

TEST(WorldLightPoolTest, DaylightProducesNoPools)
{
    const std::vector<WorldLight> lights{MakeLight({100.0f, 100.0f}, LightSchedule::AlwaysOn),
                                         MakeLight({200.0f, 100.0f}, LightSchedule::AlwaysOn),
                                         MakeLight({300.0f, 100.0f}, LightSchedule::AlwaysOn)};
    skyDraw::LightPoolList pools;

    worldLights::Build(lights, kNight, 0.0f, pools);
    EXPECT_TRUE(pools.empty());

    worldLights::Build(lights, kNight, worldLights::NIGHT_GATE, pools);
    EXPECT_TRUE(pools.empty());

    worldLights::Build(lights, kNight, 1.0f, pools);
    EXPECT_EQ(pools.size(), 3u);
}

TEST(WorldLightPoolTest, PoolKeepsTheLampPositionAndRadius)
{
    const std::vector<WorldLight> lights{
        MakeLight({1000.0f, 500.0f}, LightSchedule::AlwaysOn, 48.0f)};
    skyDraw::LightPoolList pools;
    worldLights::Build(lights, kNight, 1.0f, pools);

    ASSERT_EQ(pools.size(), 1u);
    EXPECT_FLOAT_EQ(pools[0].centreWorld.x, 1000.0f);
    EXPECT_FLOAT_EQ(pools[0].centreWorld.y, 500.0f);
    EXPECT_FLOAT_EQ(pools[0].radius, 48.0f);
    EXPECT_FLOAT_EQ(pools[0].color.r, 1.0f);
    EXPECT_FLOAT_EQ(pools[0].color.g, 0.85f);
    EXPECT_FLOAT_EQ(pools[0].color.b, 0.55f);
}

// alpha = ComputeLightIntensity(schedule, hour) * nightFactor * 0.6.
TEST(WorldLightPoolTest, PoolAlphaIsScheduleTimesNightTimesSixTenths)
{
    ASSERT_FLOAT_EQ(ComputeLightIntensity(LightSchedule::AlwaysOn, kNight), 1.0f);
    ASSERT_FLOAT_EQ(ComputeLightIntensity(LightSchedule::NightOnly, kNight), 1.0f);

    const std::vector<WorldLight> lights{MakeLight({0.0f, 0.0f}, LightSchedule::NightOnly)};
    skyDraw::LightPoolList pools;
    worldLights::Build(lights, kNight, 0.8f, pools);

    ASSERT_EQ(pools.size(), 1u);
    EXPECT_FLOAT_EQ(pools[0].color.a, 0.48f);
}

// each lamp also checks its own schedule, even when weather darkens the scene at noon.
TEST(WorldLightPoolTest, ScheduleGatesEachLightIndependently)
{
    ASSERT_FLOAT_EQ(ComputeLightIntensity(LightSchedule::NightOnly, kNoon), 0.0f);

    const std::vector<WorldLight> lights{MakeLight({100.0f, 0.0f}, LightSchedule::NightOnly),
                                         MakeLight({200.0f, 0.0f}, LightSchedule::AlwaysOn)};
    skyDraw::LightPoolList pools;
    worldLights::Build(lights, kNoon, 0.5f, pools);

    ASSERT_EQ(pools.size(), 1u);
    EXPECT_FLOAT_EQ(pools[0].centreWorld.x, 200.0f) << "the AlwaysOn lamp is the survivor";
    EXPECT_FLOAT_EQ(pools[0].color.a, 0.3f);
}

// preserve map order and clear the reused output buffer to avoid stale lights.
TEST(WorldLightPoolTest, BuildPreservesMapOrderAndClearsTheBuffer)
{
    const std::vector<WorldLight> many{MakeLight({10.0f, 0.0f}, LightSchedule::AlwaysOn),
                                       MakeLight({20.0f, 0.0f}, LightSchedule::AlwaysOn),
                                       MakeLight({30.0f, 0.0f}, LightSchedule::AlwaysOn)};
    skyDraw::LightPoolList pools;
    worldLights::Build(many, kNight, 1.0f, pools);
    ASSERT_EQ(pools.size(), 3u);
    EXPECT_FLOAT_EQ(pools[0].centreWorld.x, 10.0f);
    EXPECT_FLOAT_EQ(pools[1].centreWorld.x, 20.0f);
    EXPECT_FLOAT_EQ(pools[2].centreWorld.x, 30.0f);

    const std::vector<WorldLight> few{MakeLight({99.0f, 0.0f}, LightSchedule::AlwaysOn)};
    worldLights::Build(few, kNight, 1.0f, pools);
    ASSERT_EQ(pools.size(), 1u);
    EXPECT_FLOAT_EQ(pools[0].centreWorld.x, 99.0f);

    worldLights::Build({}, kNight, 1.0f, pools);
    EXPECT_TRUE(pools.empty());
}

TEST(WorldLightPoolTest, BuildLeavesSurfaceHeightForTheCaller)
{
    const std::vector<WorldLight> lights{MakeLight({0.0f, 0.0f}, LightSchedule::AlwaysOn)};
    skyDraw::LightPoolList pools;
    worldLights::Build(lights, kNight, 1.0f, pools);

    ASSERT_EQ(pools.size(), 1u);
    EXPECT_FLOAT_EQ(pools[0].surfaceHeight, 0.0f);
}
