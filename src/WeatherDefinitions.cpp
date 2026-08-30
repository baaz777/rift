#include "WeatherDefinitions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
// Table order must match WeatherState.
const std::array<WeatherDefinition, 17> kWeatherTable = {{
    // Clear
    WeatherDefinition{},

    // LightRain
    WeatherDefinition{
        .ambientTintMultiplier = {0.80f, 0.82f, 0.90f},
        .particleType = WeatherParticleType::Rain,
        .baseSpawnRate = 200.0f,
        .maxWeatherParticles = 10000,
        .windIntensity = 0.4f,
    },
    // HeavyRain
    WeatherDefinition{
        .ambientTintMultiplier = {0.65f, 0.68f, 0.80f},
        .skyColorOverride = {0.42f, 0.45f, 0.55f},
        .particleType = WeatherParticleType::Rain,
        .baseSpawnRate = 600.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.0f,
        .showCelestialBodies = false,
        .windIntensity = 0.8f,
    },
    // Thunderstorm
    WeatherDefinition{
        .ambientTintMultiplier = {0.50f, 0.52f, 0.65f},
        .skyColorOverride = {0.30f, 0.32f, 0.42f},
        .particleType = WeatherParticleType::Rain,
        .baseSpawnRate = 1000.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.0f,
        .showCelestialBodies = false,
        .lightningIntervalSeconds = 8.0f,
        .windIntensity = 1.0f,
        .secondaryParticleType = WeatherParticleType::Zap,
        .secondaryBaseSpawnRate = 3.0f,
        .secondaryMaxWeatherParticles = 80,
    },
    // Blizzard
    WeatherDefinition{
        .ambientTintMultiplier = {0.80f, 0.83f, 0.95f},
        .skyColorOverride = {0.78f, 0.80f, 0.85f},
        .particleType = WeatherParticleType::Snow,
        .baseSpawnRate = 550.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.0f,
        .showCelestialBodies = false,
        .windIntensity = 1.0f,
        .secondaryParticleType = WeatherParticleType::Fog,
        .secondaryBaseSpawnRate = 15.0f,  // Thinned blizzard mist (was 45, then 27)
        .secondaryMaxWeatherParticles = 10000,
        .fogAlphaMultiplier = 0.4f,  // Softened further so it isn't a whiteout wall (was 0.85, 0.6)
    },

    // Fog
    WeatherDefinition{
        .ambientTintMultiplier = {0.80f, 0.83f, 0.87f},
        .particleType = WeatherParticleType::Fog,
        .baseSpawnRate = 110.0f,      // Thinned so fog reads as haze, not a wall (was 180)
        .maxWeatherParticles = 2500,  // Lower ceiling holds the thin-out when zoomed out (was 5000)
        .particleSizeScale = 1.0f,
        .windIntensity = 0.2f,
        .fogAlphaMultiplier = 0.65f,
    },
    // HeatHaze: keep tint at or below white; hazeAmplitude is unused.
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 0.99f, 0.95f},
        .windIntensity = 0.1f,
        .hazeAmplitude = 2.0f,
    },
    // Sandstorm
    WeatherDefinition{
        .ambientTintMultiplier = {0.75f, 0.65f, 0.50f},
        .skyColorOverride = {0.70f, 0.55f, 0.40f},
        .particleType = WeatherParticleType::Sand,
        .baseSpawnRate = 400.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.0f,
        .showCelestialBodies = false,
        // 0.5 = the calm anchor reproducing the pre-wind drift speed; raise for gustier, lower for
        // stiller.
        .windIntensity = 0.5f,
        .secondaryParticleType = WeatherParticleType::Wind,
        .secondaryBaseSpawnRate = 30.0f,
        .secondaryMaxWeatherParticles = 500,
    },

    // FallingLeaves
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 0.95f, 0.85f},
        .particleType = WeatherParticleType::Leaf,
        .baseSpawnRate = 60.0f,
        .maxWeatherParticles = 2000,
        // 0.5 = the calm anchor reproducing the pre-wind drift speed; raise for gustier, lower for
        // stiller.
        .windIntensity = 0.5f,
    },
    // CherryBlossoms
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 0.80f, 0.92f},
        .particleType = WeatherParticleType::Blossom,
        .baseSpawnRate = 230.0f,
        .maxWeatherParticles = 10000,
        .windIntensity = 0.35f,
        .secondaryParticleType = WeatherParticleType::Fog,
        .secondaryBaseSpawnRate = 9.0f,  // Lighter sakura wash (was 15)
        .secondaryMaxWeatherParticles = 10000,
        .fogAlphaMultiplier = 0.5f,
    },
    // PollenStorm
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 0.98f, 0.85f},
        .particleType = WeatherParticleType::Pollen,
        .baseSpawnRate = 60.0f,
        .maxWeatherParticles = 2000,
        .windIntensity = 0.5f,
    },

    // Aurora: retain natural clock lighting and stars.
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 1.00f, 1.00f},
        .particleType = WeatherParticleType::Aurora,
        .baseSpawnRate = 9.0f,
        .maxWeatherParticles = 140,
        .particleSizeScale = 0.72f,
        .showAurora = true,
        .secondaryParticleType = WeatherParticleType::Wisp,
        .secondaryBaseSpawnRate = 3.0f,
        .secondaryMaxWeatherParticles = 60,
    },
    // MeteorShower
    WeatherDefinition{
        .ambientTintMultiplier = {0.95f, 0.95f, 1.00f},
        .particleType = WeatherParticleType::Constellation,
        .baseSpawnRate = 8.0f,
        .maxWeatherParticles = 120,
        .starVisibilityOverride = 1.0f,
        .meteorRateMultiplier = 12.0f,
    },
    // FireflySwarm
    WeatherDefinition{
        .ambientTintMultiplier = {0.90f, 1.00f, 0.85f},
        .particleType = WeatherParticleType::Firefly,
        .baseSpawnRate = 600.0f,
        .maxWeatherParticles = 10000,
        .particleSizeScale = 1.0f,
    },
    // AshFall
    WeatherDefinition{
        .ambientTintMultiplier = {0.70f, 0.65f, 0.60f},
        .skyColorOverride = {0.55f, 0.50f, 0.48f},
        .particleType = WeatherParticleType::Ash,
        .baseSpawnRate = 150.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.2f,
        .windIntensity = 0.3f,
        .secondaryParticleType = WeatherParticleType::Smoke,
        .secondaryBaseSpawnRate = 10.0f,
        .secondaryMaxWeatherParticles = 250,
    },
    // EmberStorm
    WeatherDefinition{
        .ambientTintMultiplier = {0.85f, 0.60f, 0.45f},
        .skyColorOverride = {0.55f, 0.30f, 0.20f},
        .particleType = WeatherParticleType::Ember,
        .baseSpawnRate = 250.0f,
        .maxWeatherParticles = 10000,
        .starVisibilityOverride = 0.4f,
        .windIntensity = 0.7f,
        .secondaryParticleType = WeatherParticleType::Smoke,
        .secondaryBaseSpawnRate = 8.0f,
        .secondaryMaxWeatherParticles = 200,
    },

    // GodRays
    WeatherDefinition{
        .ambientTintMultiplier = {1.00f, 0.98f, 1.00f},
        .particleType = WeatherParticleType::Sunshine,
        .baseSpawnRate = 6.0f,
        .maxWeatherParticles = 200,
        .windIntensity = 0.0f,
        .secondaryParticleType = WeatherParticleType::Fog,
        .secondaryBaseSpawnRate = 15.0f,  // Thinner mist behind the beams (was 25)
        .secondaryMaxWeatherParticles = 1500,
        .fogAlphaMultiplier = 0.40f,
    },
}};

float Smoothstep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

const WeatherDefinition& GetWeatherDefinition(WeatherState state)
{
    auto idx = static_cast<size_t>(std::to_underlying(state));
    if (idx >= kWeatherTable.size())
        return kWeatherTable[0];
    return kWeatherTable[idx];
}

float ComputeLightIntensity(LightSchedule schedule, float hourOfDay)
{
    if (schedule == LightSchedule::AlwaysOn)
        return 1.0f;

    // Normalize hour to [0, 24).
    float h = std::fmod(hourOfDay, 24.0f);
    if (h < 0.0f)
        h += 24.0f;

    // Schedule windows. Both wrap midnight, and the daytime gap counts as zero,
    // and ramp at the boundaries.
    float rampOnStart = (schedule == LightSchedule::DuskToDawn) ? 18.0f : 20.0f;
    float rampOnEnd = (schedule == LightSchedule::DuskToDawn) ? 20.0f : 22.0f;
    float rampOffStart = 4.0f;
    float rampOffEnd = (schedule == LightSchedule::DuskToDawn) ? 7.0f : 6.0f;

    if (h >= rampOnStart && h < rampOnEnd)
        return Smoothstep((h - rampOnStart) / (rampOnEnd - rampOnStart));
    if (h >= rampOffStart && h < rampOffEnd)
        return 1.0f - Smoothstep((h - rampOffStart) / (rampOffEnd - rampOffStart));
    if (h >= rampOnEnd || h < rampOffStart)
        return 1.0f;
    return 0.0f;
}
