#include <gtest/gtest.h>

#include "../src/AmbienceConfig.hpp"
#include "../src/WeatherBlend.hpp"
#include "../src/WeatherDefinitions.hpp"

#include <glm/gtc/constants.hpp>

// endpoint blends retain sentinel values; interior lightning blends use frequency rather than
// interval.
namespace
{

void ExpectDefinitionEq(const WeatherDefinition& x, const WeatherDefinition& y)
{
    EXPECT_EQ(x.ambientTintMultiplier, y.ambientTintMultiplier);
    EXPECT_EQ(x.skyColorOverride, y.skyColorOverride);
    EXPECT_EQ(x.particleType, y.particleType);
    EXPECT_EQ(x.baseSpawnRate, y.baseSpawnRate);
    EXPECT_EQ(x.maxWeatherParticles, y.maxWeatherParticles);
    EXPECT_EQ(x.particleSizeScale, y.particleSizeScale);
    EXPECT_EQ(x.starVisibilityOverride, y.starVisibilityOverride);
    EXPECT_EQ(x.showCelestialBodies, y.showCelestialBodies);
    EXPECT_EQ(x.lightningIntervalSeconds, y.lightningIntervalSeconds);
    EXPECT_EQ(x.showAurora, y.showAurora);
    EXPECT_EQ(x.meteorRateMultiplier, y.meteorRateMultiplier);
    EXPECT_EQ(x.windIntensity, y.windIntensity);
    EXPECT_EQ(x.secondaryParticleType, y.secondaryParticleType);
    EXPECT_EQ(x.secondaryBaseSpawnRate, y.secondaryBaseSpawnRate);
    EXPECT_EQ(x.secondaryMaxWeatherParticles, y.secondaryMaxWeatherParticles);
    EXPECT_EQ(x.fogAlphaMultiplier, y.fogAlphaMultiplier);
    EXPECT_EQ(x.hazeAmplitude, y.hazeAmplitude);
}
}  // namespace

// endpoints return every field verbatim, including sentinels. WeatherDirector handles fog hold.
TEST(WeatherBlend, EndpointIdentity)
{
    const WeatherDefinition& fog = GetWeatherDefinition(WeatherState::Fog);
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);

    ExpectDefinitionEq(BlendWeatherDefinitions(fog, storm, 0.0f), fog);
    ExpectDefinitionEq(BlendWeatherDefinitions(fog, storm, -0.5f), fog);
    ExpectDefinitionEq(BlendWeatherDefinitions(fog, storm, 1.0f), storm);
    ExpectDefinitionEq(BlendWeatherDefinitions(fog, storm, 1.5f), storm);
}

TEST(WeatherBlend, SmoothstepEndpointsAndMonotonic)
{
    EXPECT_FLOAT_EQ(BlendSmoothstep(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(BlendSmoothstep(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(BlendSmoothstep(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(BlendSmoothstep(2.0f), 1.0f);
    float prev = 0.0f;
    for (int i = 1; i <= 10; ++i)
    {
        float v = BlendSmoothstep(static_cast<float>(i) / 10.0f);
        EXPECT_GE(v, prev);
        prev = v;
    }
}

TEST(WeatherBlend, SpawnsFogTypeDetectsPrimaryAndSecondary)
{
    EXPECT_TRUE(WeatherSpawnsFogType(GetWeatherDefinition(WeatherState::Fog)));       // primary
    EXPECT_TRUE(WeatherSpawnsFogType(GetWeatherDefinition(WeatherState::Blizzard)));  // secondary
    EXPECT_FALSE(WeatherSpawnsFogType(GetWeatherDefinition(WeatherState::Clear)));
    EXPECT_FALSE(WeatherSpawnsFogType(GetWeatherDefinition(WeatherState::LightRain)));
}

TEST(WeatherBlend, PlainFloatsMixLinearly)
{
    const WeatherDefinition& clear = GetWeatherDefinition(WeatherState::Clear);
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);
    WeatherDefinition mid = BlendWeatherDefinitions(clear, storm, 0.5f);

    EXPECT_FLOAT_EQ(mid.ambientTintMultiplier.r,
                    0.5f * (clear.ambientTintMultiplier.r + storm.ambientTintMultiplier.r));
    EXPECT_FLOAT_EQ(mid.windIntensity, 0.5f * (clear.windIntensity + storm.windIntensity));
    EXPECT_FLOAT_EQ(mid.fogAlphaMultiplier,
                    0.5f * (clear.fogAlphaMultiplier + storm.fogAlphaMultiplier));
    EXPECT_FLOAT_EQ(mid.hazeAmplitude, 0.5f * (clear.hazeAmplitude + storm.hazeAmplitude));
    EXPECT_FLOAT_EQ(mid.meteorRateMultiplier,
                    0.5f * (clear.meteorRateMultiplier + storm.meteorRateMultiplier));
    EXPECT_FLOAT_EQ(mid.particleSizeScale,
                    0.5f * (clear.particleSizeScale + storm.particleSizeScale));
}

// interior blends copy sentinels, booleans, and particle types from the destination.
TEST(WeatherBlend, DestinationCopiedFields)
{
    const WeatherDefinition& clear = GetWeatherDefinition(WeatherState::Clear);
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);
    WeatherDefinition mid = BlendWeatherDefinitions(clear, storm, 0.25f);

    EXPECT_EQ(mid.skyColorOverride, storm.skyColorOverride);
    EXPECT_EQ(mid.starVisibilityOverride, storm.starVisibilityOverride);
    EXPECT_EQ(mid.showCelestialBodies, storm.showCelestialBodies);
    EXPECT_EQ(mid.showAurora, storm.showAurora);
    EXPECT_EQ(mid.particleType, storm.particleType);
    EXPECT_EQ(mid.secondaryParticleType, storm.secondaryParticleType);
}

// blend lightning frequency so a transition cannot produce a near-zero flash interval.
TEST(WeatherBlend, LightningBlendsInFrequencySpace)
{
    const WeatherDefinition& clear = GetWeatherDefinition(WeatherState::Clear);         // 0 = off
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);  // 8 s
    ASSERT_FLOAT_EQ(storm.lightningIntervalSeconds, 8.0f);

    for (int i = 1; i < 20; ++i)
    {
        float t = static_cast<float>(i) / 20.0f;
        float interval = BlendWeatherDefinitions(clear, storm, t).lightningIntervalSeconds;
        if (interval > 0.0f)
        {
            EXPECT_GE(interval, 8.0f) << "strobing interval at t=" << t;
        }
    }

    EXPECT_GT(BlendWeatherDefinitions(clear, storm, 0.95f).lightningIntervalSeconds, 0.0f);

    EXPECT_FLOAT_EQ(BlendWeatherDefinitions(clear, storm, 0.01f).lightningIntervalSeconds, 0.0f);
}

// different particle types ramp independently; outgoing rain rate must not enter the snow stream.
TEST(WeatherBlend, TypeMismatchRampsIncomingRateFromZero)
{
    const WeatherDefinition& storm =
        GetWeatherDefinition(WeatherState::Thunderstorm);  // Rain 1000/s
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);  // Snow 550/s
    WeatherDefinition mid = BlendWeatherDefinitions(storm, blizzard, 0.5f);

    EXPECT_EQ(mid.particleType, WeatherParticleType::Snow);
    EXPECT_FLOAT_EQ(mid.baseSpawnRate, blizzard.baseSpawnRate * 0.5f);

    // same-type slots mix normally: Clear(0, type None)->LightRain(Rain 200):
    // types differ (None vs Rain) so it also ramps -> 100 at t=0.5. same-type
    // case: LightRain -> HeavyRain (both Rain) mixes 200..600.
    const WeatherDefinition& light = GetWeatherDefinition(WeatherState::LightRain);
    const WeatherDefinition& heavy = GetWeatherDefinition(WeatherState::HeavyRain);
    WeatherDefinition rainMid = BlendWeatherDefinitions(light, heavy, 0.5f);
    EXPECT_FLOAT_EQ(rainMid.baseSpawnRate, 0.5f * (light.baseSpawnRate + heavy.baseSpawnRate));
}

// shared types use the smaller cap across both endpoints; zero means uncapped.
// Fog (primary, 2500) -> Blizzard (secondary, 10000) must retain the 2500 cap.
TEST(WeatherBlend, SharedTypeCapFloor)
{
    const WeatherDefinition& fog = GetWeatherDefinition(WeatherState::Fog);
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);
    ASSERT_EQ(fog.particleType, WeatherParticleType::Fog);
    ASSERT_EQ(blizzard.secondaryParticleType, WeatherParticleType::Fog);
    ASSERT_EQ(fog.maxWeatherParticles, 2500);
    ASSERT_EQ(blizzard.secondaryMaxWeatherParticles, 10000);

    WeatherDefinition mid = BlendWeatherDefinitions(fog, blizzard, 0.5f);
    EXPECT_EQ(mid.secondaryParticleType, WeatherParticleType::Fog);
    EXPECT_EQ(mid.secondaryMaxWeatherParticles, 2500);
}

// reverse the endpoints so the shared cap must be found in a different slot.
TEST(WeatherBlend, SharedTypeCapFloorReverseDirection)
{
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);
    const WeatherDefinition& fog = GetWeatherDefinition(WeatherState::Fog);

    for (float t : {0.1f, 0.5f, 0.9f})
    {
        WeatherDefinition mid = BlendWeatherDefinitions(blizzard, fog, t);
        EXPECT_EQ(mid.maxWeatherParticles, 2500) << "cap not floored at t=" << t;
    }
}

TEST(WeatherBlend, SplitMix64Deterministic)
{
    EXPECT_EQ(SplitMix64(42), SplitMix64(42));
    EXPECT_NE(SplitMix64(42), SplitMix64(43));
    EXPECT_NE(SplitMix64(0), 0u);  // zero input must not fix-point to zero
}

TEST(WeatherBlend, GustStrengthEnvelope)
{
    const glm::vec3 phases = GustPhases(SplitMix64(7));

    EXPECT_FLOAT_EQ(GustWindStrength(0.0f, 3.25, phases), 0.0f);

    for (double t = 0.0; t < 30.0; t += 0.05)
    {
        float s = GustWindStrength(0.5f, t, phases);
        EXPECT_GE(s, 0.0f);
        EXPECT_GE(s, 0.5f * (1.0f - ambience::WEATHER_GUST_AMP) - 1e-4f);
        EXPECT_LE(s, 0.5f * (1.0f + ambience::WEATHER_GUST_AMP) + 1e-4f);
    }

    float prev = GustWindStrength(0.5f, 10.0, phases);
    for (double t = 10.0 + 1.0 / 60.0; t < 12.0; t += 1.0 / 60.0)
    {
        float cur = GustWindStrength(0.5f, t, phases);
        EXPECT_LT(std::abs(cur - prev), 0.02f) << "strength stepped at t=" << t;
        prev = cur;
    }

    EXPECT_FLOAT_EQ(GustWindStrength(0.5f, 4.5, phases), GustWindStrength(0.5f, 4.5, phases));
}

TEST(WeatherBlend, GustDirectionWanderCone)
{
    const glm::vec3 phases = GustPhases(SplitMix64(7));
    const glm::vec2 base = glm::normalize(ambience::WEATHER_WIND_BASE_DIR);
    const float maxAngle = glm::radians(ambience::WEATHER_WIND_WANDER_DEG) + 1e-4f;

    glm::vec2 prev = GustWindDirection(0.0, phases);
    for (double t = 0.0; t < 50.0; t += 0.1)
    {
        glm::vec2 dir = GustWindDirection(t, phases);
        EXPECT_NEAR(glm::length(dir), 1.0f, 1e-4f);
        float cosang = glm::dot(dir, base);
        EXPECT_GE(cosang, std::cos(maxAngle)) << "left the wander cone at t=" << t;
        EXPECT_GT(glm::dot(dir, prev), 0.99f) << "direction jumped at t=" << t;
        prev = dir;
    }
}

TEST(WeatherBlend, GustPhasesVaryBySeed)
{
    EXPECT_NE(GustPhases(SplitMix64(1)), GustPhases(SplitMix64(2)));
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_GE(GustPhases(SplitMix64(9))[i], 0.0f);
        EXPECT_LT(GustPhases(SplitMix64(9))[i], glm::two_pi<float>() + 1e-4f);
    }
}

TEST(WeatherBlend, WeatherCapForTypeChecksBothSlots)
{
    const WeatherDefinition& fog = GetWeatherDefinition(WeatherState::Fog);
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);
    EXPECT_EQ(WeatherCapForType(fog, WeatherParticleType::Fog), 2500);        // primary slot
    EXPECT_EQ(WeatherCapForType(blizzard, WeatherParticleType::Fog), 10000);  // secondary slot
    EXPECT_EQ(WeatherCapForType(blizzard, WeatherParticleType::Snow), 10000);
    EXPECT_EQ(WeatherCapForType(fog, WeatherParticleType::Rain), 0);
    EXPECT_EQ(WeatherCapForType(fog, WeatherParticleType::None), 0);
}
