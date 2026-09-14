#include <gtest/gtest.h>

#include "../src/ParticleSystem.hpp"
#include "../src/TimeManager.hpp"
#include "../src/WeatherDefinitions.hpp"
#include "../src/WeatherDirector.hpp"

#include <glm/glm.hpp>

// fog opacity is checked at full daylight, where the day boost peaks. zone fog
// and weather fog have separate softening rules; both must stay below the ceiling.
namespace
{

float MaxFogAlpha(const ParticleSystem& ps)
{
    float maxA = 0.0f;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Fog)
        {
            maxA = std::max(maxA, p.color.a);
        }
    }
    return maxA;
}

int FogCount(const ParticleSystem& ps)
{
    int n = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Fog)
        {
            ++n;
        }
    }
    return n;
}
}  // namespace

TEST(FogOpacity, ZoneFogStaysLightUnderNonFogWeather)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);   // midday: maximum day boost.
    ps.SetNightFactor(0.0f);  // full day: worst case for fog opacity.
    ps.SetPlayerPosition({0.0f, 0.0f});
    ps.SetMaxParticlesPerZone(200);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};

    // zone fog uses its fixed 0.5 softening even when the active weather has no fog.
    std::vector<ParticleZone> zones;
    zones.emplace_back(cameraPos, viewSize, ParticleType::Fog);
    ps.SetZones(&zones);

    // Run long enough for puffs to fully fade in and reach their peak alpha.
    for (int i = 0; i < 120; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
        EXPECT_LE(MaxFogAlpha(ps), 0.28f) << "zone fog too opaque at frame " << i;
    }

    EXPECT_GT(FogCount(ps), 0) << "test vacuous: no fog spawned";
}

// Fog weather applies its 0.65 multiplier before the per-puff ceiling check.
TEST(FogOpacity, WeatherFogStaysLight)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetPlayerPosition({0.0f, 0.0f});

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};

    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Fog), 1.0f);

    for (int i = 0; i < 120; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
        EXPECT_LE(MaxFogAlpha(ps), 0.34f) << "weather fog too opaque at frame " << i;
    }

    EXPECT_GT(FogCount(ps), 0) << "test vacuous: no weather fog spawned";
}

// this viewport fills the weather fog cap; the steady-state count checks that bound.
TEST(FogOpacity, WeatherFogPopulationStaysBounded)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Fog), 1.0f);

    // Run well past the fill time so the population reaches steady state.
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
    }

    EXPECT_GT(FogCount(ps), 0) << "test vacuous: no weather fog spawned";
    EXPECT_LE(FogCount(ps), 3000) << "weather fog population back to a wall";
}

// residual fog can outlive the transition by 18 s; its alpha must remain bounded.
TEST(FogOpacity, FogToClearTransitionHoldsCeiling)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetPlayerPosition({0.0f, 0.0f});

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};

    TimeManager time;
    time.Initialize();
    time.SetTime(12.0f);
    time.SetWeather(WeatherState::Fog);
    WeatherDirector director;
    director.SetEnabled(true);

    ps.SetWeatherState(&time.GetEffectiveWeatherDefinition(), 1.0f);
    for (int i = 0; i < 120; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
    }
    ASSERT_GT(FogCount(ps), 0) << "test vacuous: no fog at steady state";

    // 10 s transition to Clear, then run 20 s past completion (decay window
    // is 18 s). 0.2 s steps: 50 transition frames + 100 decay frames.
    director.RequestWeather(time, WeatherState::Clear, 10.0f);
    for (int i = 0; i < 150; ++i)
    {
        director.Update(0.2f, time);
        ps.SetWind(director.GetWindDirection(), director.GetWindStrength());
        const WeatherDirector::SpawnStreams streams = director.GetSpawnStreams();
        ps.SetWeatherTransition(streams.outgoing, streams.incoming, streams.weight);
        ps.SetWeatherState(&time.GetEffectiveWeatherDefinition(), time.GetWeatherIntensity());
        ps.Update(0.2f, cameraPos, viewSize);
        // weather fog softening is capped at 0.65 even as the published multiplier
        // decays toward Clear's 1.0.
        EXPECT_LE(MaxFogAlpha(ps), 0.34f) << "fog popped during transition at frame " << i;
        EXPECT_LE(FogCount(ps), 3000) << "fog population wall at frame " << i;
    }
}

// Fog -> Blizzard shares the Fog particle type, so both streams must share the smaller cap.
TEST(FogOpacity, FogToBlizzardTransitionKeepsPopulationBounded)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetPlayerPosition({0.0f, 0.0f});

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};

    TimeManager time;
    time.Initialize();
    time.SetTime(12.0f);
    time.SetWeather(WeatherState::Fog);
    WeatherDirector director;
    director.SetEnabled(true);

    ps.SetWeatherState(&time.GetEffectiveWeatherDefinition(), 1.0f);
    for (int i = 0; i < 120; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
    }
    ASSERT_GT(FogCount(ps), 0) << "test vacuous: no fog at steady state";

    director.RequestWeather(time, WeatherState::Blizzard, 10.0f);
    for (int i = 0; i < 100; ++i)
    {
        director.Update(0.2f, time);
        ps.SetWind(director.GetWindDirection(), director.GetWindStrength());
        const WeatherDirector::SpawnStreams streams = director.GetSpawnStreams();
        ps.SetWeatherTransition(streams.outgoing, streams.incoming, streams.weight);
        ps.SetWeatherState(&time.GetEffectiveWeatherDefinition(), time.GetWeatherIntensity());
        ps.Update(0.2f, cameraPos, viewSize);
        EXPECT_LE(FogCount(ps), 3000) << "fog population wall at frame " << i;
    }
}
