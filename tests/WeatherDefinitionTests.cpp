// the definition table and EnumTraits must cover the same weather states.

#include <gtest/gtest.h>

#include "../src/TimeManager.hpp"
#include "../src/WeatherDefinitions.hpp"

#include <utility>

TEST(WeatherDefinitionTests, EveryEnumValueResolves)
{
    for (size_t i = 0; i < EnumTraits<WeatherState>::Count; ++i)
    {
        auto state = static_cast<WeatherState>(i);
        const WeatherDefinition& def = GetWeatherDefinition(state);

        EXPECT_GE(def.ambientTintMultiplier.r, 0.0f);
        EXPECT_GE(def.ambientTintMultiplier.g, 0.0f);
        EXPECT_GE(def.ambientTintMultiplier.b, 0.0f);
    }
}

TEST(WeatherDefinitionTests, FromStringRoundTrips)
{
    for (size_t i = 0; i < EnumTraits<WeatherState>::Count; ++i)
    {
        auto state = static_cast<WeatherState>(i);
        std::string_view name = EnumTraits<WeatherState>::ToString(state);
        auto parsed = EnumTraits<WeatherState>::FromString(name);
        ASSERT_TRUE(parsed.has_value()) << "FromString failed for: " << name;
        EXPECT_EQ(*parsed, state);
    }
}

TEST(WeatherDefinitionTests, FromStringIsCaseSensitive)
{
    EXPECT_EQ(EnumTraits<WeatherState>::FromString("Thunderstorm"),
              std::optional<WeatherState>{WeatherState::Thunderstorm});
    EXPECT_FALSE(EnumTraits<WeatherState>::FromString("thunderstorm").has_value());
    EXPECT_FALSE(EnumTraits<WeatherState>::FromString("THUNDERSTORM").has_value());
}

TEST(WeatherDefinitionTests, FromStringRejectsGarbage)
{
    EXPECT_FALSE(EnumTraits<WeatherState>::FromString("").has_value());
    EXPECT_FALSE(EnumTraits<WeatherState>::FromString("nope").has_value());
}

TEST(WeatherDefinitionTests, ClearHasDefaults)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Clear);
    EXPECT_EQ(def.particleType, WeatherParticleType::None);
    EXPECT_EQ(def.lightningIntervalSeconds, 0.0f);
    EXPECT_FALSE(def.showAurora);
    EXPECT_TRUE(def.showCelestialBodies);
    EXPECT_FLOAT_EQ(def.meteorRateMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, ThunderstormHasLightning)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Thunderstorm);
    EXPECT_GT(def.lightningIntervalSeconds, 0.0f);
    EXPECT_EQ(def.particleType, WeatherParticleType::Rain);
    EXPECT_GT(def.maxWeatherParticles, 0);
}

TEST(WeatherDefinitionTests, AuroraDoesNotForceNight)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Aurora);
    EXPECT_TRUE(def.showAurora);
    EXPECT_LT(def.starVisibilityOverride, 0.0f);
    EXPECT_LT(def.skyColorOverride.x, 0.0f);
    EXPECT_TRUE(def.showCelestialBodies);
    EXPECT_FLOAT_EQ(def.ambientTintMultiplier.r, 1.0f);
    EXPECT_FLOAT_EQ(def.ambientTintMultiplier.g, 1.0f);
    EXPECT_FLOAT_EQ(def.ambientTintMultiplier.b, 1.0f);
}

TEST(WeatherDefinitionTests, MeteorShowerBoostsMeteorRate)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::MeteorShower);
    EXPECT_GT(def.meteorRateMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, IntensityZeroProducesNeutralAmbient)
{
    TimeManager tm;
    tm.Initialize();
    tm.SetTime(12.0f);
    tm.SetWeatherIntensity(0.0f);

    glm::vec3 clearColor = tm.GetAmbientColor();
    tm.SetWeather(WeatherState::Thunderstorm);
    glm::vec3 stormColor = tm.GetAmbientColor();

    EXPECT_FLOAT_EQ(clearColor.r, stormColor.r);
    EXPECT_FLOAT_EQ(clearColor.g, stormColor.g);
    EXPECT_FLOAT_EQ(clearColor.b, stormColor.b);
}

TEST(WeatherDefinitionTests, IntensityOneAppliesFullTint)
{
    TimeManager tm;
    tm.Initialize();
    tm.SetTime(12.0f);
    tm.SetWeather(WeatherState::Thunderstorm);
    tm.SetWeatherIntensity(1.0f);

    glm::vec3 stormColor = tm.GetAmbientColor();

    EXPECT_LT(stormColor.r, 0.7f);
    EXPECT_LT(stormColor.g, 0.7f);
}

TEST(WeatherDefinitionTests, SetWeatherIntensityClampsToRange)
{
    TimeManager tm;
    tm.Initialize();

    tm.SetWeatherIntensity(-0.5f);
    EXPECT_FLOAT_EQ(tm.GetWeatherIntensity(), 0.0f);

    tm.SetWeatherIntensity(1.5f);
    EXPECT_FLOAT_EQ(tm.GetWeatherIntensity(), 1.0f);

    tm.SetWeatherIntensity(0.5f);
    EXPECT_FLOAT_EQ(tm.GetWeatherIntensity(), 0.5f);
}

TEST(WeatherDefinitionTests, HeavyRainHidesStars)
{
    TimeManager tm;
    tm.Initialize();
    tm.SetTime(23.0f);  // deep night
    tm.SetWeather(WeatherState::Clear);
    EXPECT_FLOAT_EQ(tm.GetStarVisibility(), 1.0f);

    tm.SetWeather(WeatherState::HeavyRain);
    tm.SetWeatherIntensity(1.0f);
    EXPECT_FLOAT_EQ(tm.GetStarVisibility(), 0.0f);
}

TEST(WeatherDefinitionTests, EnumTraitsCountMatchesGodRays)
{
    static_assert(EnumTraits<WeatherState>::Count == 17, "WeatherState enum has 17 values");
    EXPECT_EQ(std::to_underlying(WeatherState::GodRays), EnumTraits<WeatherState>::Count - 1);
}

TEST(WeatherDefinitionTests, BlizzardHasSecondaryFog)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Blizzard);
    EXPECT_EQ(def.particleType, WeatherParticleType::Snow);
    EXPECT_EQ(def.secondaryParticleType, WeatherParticleType::Fog);
    EXPECT_GT(def.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(def.secondaryMaxWeatherParticles, 0);
    // Blizzard fog stays soft so snow remains the dominant layer.
    EXPECT_LT(def.fogAlphaMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, CherryBlossomsHasMistSecondary)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::CherryBlossoms);
    EXPECT_EQ(def.particleType, WeatherParticleType::Blossom);
    EXPECT_EQ(def.secondaryParticleType, WeatherParticleType::Fog);
    EXPECT_GT(def.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(def.secondaryMaxWeatherParticles, 0);

    EXPECT_LT(def.fogAlphaMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, FogStateSoftensAlpha)
{
    EXPECT_NEAR(GetWeatherDefinition(WeatherState::Fog).fogAlphaMultiplier, 0.65f, 0.001f);
}

TEST(WeatherDefinitionTests, NonFogWeathersHaveDefaultMultiplier)
{
    EXPECT_FLOAT_EQ(GetWeatherDefinition(WeatherState::Clear).fogAlphaMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(GetWeatherDefinition(WeatherState::HeavyRain).fogAlphaMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, OnlyLayeredWeathersHaveSecondaryParticle)
{
    for (size_t i = 0; i < EnumTraits<WeatherState>::Count; ++i)
    {
        auto state = static_cast<WeatherState>(i);
        if (state == WeatherState::Blizzard || state == WeatherState::CherryBlossoms ||
            state == WeatherState::GodRays || state == WeatherState::Thunderstorm ||
            state == WeatherState::Sandstorm || state == WeatherState::Aurora ||
            state == WeatherState::AshFall || state == WeatherState::EmberStorm)
        {
            continue;
        }
        EXPECT_EQ(GetWeatherDefinition(state).secondaryParticleType, WeatherParticleType::None)
            << "state=" << EnumTraits<WeatherState>::ToString(state);
    }
}

TEST(WeatherDefinitionTests, NewSecondaryLayersUseDedicatedParticles)
{
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);
    EXPECT_EQ(storm.secondaryParticleType, WeatherParticleType::Zap);
    EXPECT_GT(storm.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(storm.secondaryMaxWeatherParticles, 0);

    const WeatherDefinition& sand = GetWeatherDefinition(WeatherState::Sandstorm);
    EXPECT_EQ(sand.secondaryParticleType, WeatherParticleType::Wind);
    EXPECT_GT(sand.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(sand.secondaryMaxWeatherParticles, 0);

    const WeatherDefinition& ashFall = GetWeatherDefinition(WeatherState::AshFall);
    EXPECT_EQ(ashFall.secondaryParticleType, WeatherParticleType::Smoke);
    EXPECT_GT(ashFall.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(ashFall.secondaryMaxWeatherParticles, 0);

    const WeatherDefinition& embers = GetWeatherDefinition(WeatherState::EmberStorm);
    EXPECT_EQ(embers.secondaryParticleType, WeatherParticleType::Smoke);
    EXPECT_GT(embers.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(embers.secondaryMaxWeatherParticles, 0);

    const WeatherDefinition& meteors = GetWeatherDefinition(WeatherState::MeteorShower);
    EXPECT_EQ(meteors.particleType, WeatherParticleType::Constellation);
    EXPECT_GT(meteors.baseSpawnRate, 0.0f);
    EXPECT_GT(meteors.maxWeatherParticles, 0);
}

TEST(WeatherDefinitionTests, GodRaysHasSunshineAndFogSecondary)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::GodRays);
    EXPECT_EQ(def.particleType, WeatherParticleType::Sunshine);
    EXPECT_EQ(def.secondaryParticleType, WeatherParticleType::Fog);
    EXPECT_GT(def.secondaryBaseSpawnRate, 0.0f);
    EXPECT_GT(def.secondaryMaxWeatherParticles, 0);
    EXPECT_GT(def.fogAlphaMultiplier, 0.0f);
    EXPECT_LT(def.fogAlphaMultiplier, 1.0f);
}

TEST(WeatherDefinitionTests, AuroraHasAuroraPrimaryAndWispSecondary)
{
    // sparse Aurora and Wisp streams leave room for precipitation in the overlay.
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Aurora);
    EXPECT_TRUE(def.showAurora);
    EXPECT_EQ(def.particleType, WeatherParticleType::Aurora);
    EXPECT_GT(def.baseSpawnRate, 0.0f);
    EXPECT_GT(def.maxWeatherParticles, 0);
    EXPECT_LE(def.baseSpawnRate, 10.0f);
    EXPECT_LE(def.maxWeatherParticles, 150);
    EXPECT_EQ(def.secondaryParticleType, WeatherParticleType::Wisp);
    EXPECT_GT(def.secondaryBaseSpawnRate, 0.0f);
    EXPECT_LT(def.secondaryBaseSpawnRate, def.baseSpawnRate);
    EXPECT_GT(def.secondaryMaxWeatherParticles, 0);
    EXPECT_LE(def.secondaryBaseSpawnRate, 3.0f);
    EXPECT_LE(def.secondaryMaxWeatherParticles, 60);
}

TEST(WeatherDefinitionTests, MergedFogHasNoSecondary)
{
    const WeatherDefinition& def = GetWeatherDefinition(WeatherState::Fog);
    EXPECT_EQ(def.particleType, WeatherParticleType::Fog);
    EXPECT_EQ(def.secondaryParticleType, WeatherParticleType::None);
}
