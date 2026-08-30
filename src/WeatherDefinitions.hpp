#pragma once

#include "EnumTraits.hpp"

#include <glm/glm.hpp>
#include <utility>

/**
 * @enum WeatherState
 * @brief Selects ambient, particle, and sky settings.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
enum class WeatherState
{

    Clear = 0,

    LightRain,
    HeavyRain,
    Thunderstorm,
    Blizzard,

    // Fog uses 48-96 pixel puffs; fogAlphaMultiplier limits their opacity.
    Fog,
    HeatHaze,
    Sandstorm,

    FallingLeaves,
    CherryBlossoms,
    PollenStorm,

    Aurora,
    MeteorShower,
    FireflySwarm,
    AshFall,
    EmberStorm,

    GodRays
};

template <>
struct EnumTraits<WeatherState> : EnumTraitsBase<WeatherState, EnumTraits<WeatherState>>
{
    static constexpr size_t Count = 17;
    static constexpr std::string_view Names[] = {"Clear",
                                                 "LightRain",
                                                 "HeavyRain",
                                                 "Thunderstorm",
                                                 "Blizzard",
                                                 "Fog",
                                                 "HeatHaze",
                                                 "Sandstorm",
                                                 "FallingLeaves",
                                                 "CherryBlossoms",
                                                 "PollenStorm",
                                                 "Aurora",
                                                 "MeteorShower",
                                                 "FireflySwarm",
                                                 "AshFall",
                                                 "EmberStorm",
                                                 "GodRays"};

    static_assert(std::to_underlying(WeatherState::GodRays) == Count - 1,
                  "Update EnumTraits<WeatherState> when adding new WeatherState values");
};

/**
 * @enum WeatherParticleType
 * @brief Renderer-independent effect IDs translated by ParticleSystem.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
enum class WeatherParticleType
{
    None = 0,      ///< No additional particles.
    Rain,          ///< Maps to ParticleType::Rain.
    Snow,          ///< Maps to ParticleType::Snow.
    Fog,           ///< Maps to ParticleType::Fog.
    Leaf,          ///< Maps to ParticleType::DriftingLeaf.
    Blossom,       ///< Maps to ParticleType::CherryBlossom.
    Pollen,        ///< Maps to ParticleType::Pollen.
    Ash,           ///< Maps to ParticleType::Ash.
    Ember,         ///< Maps to ParticleType::Ember.
    Sand,          ///< Maps to ParticleType::Sand.
    Firefly,       ///< Maps to ParticleType::Firefly.
    Wisp,          ///< Maps to ParticleType::Wisp. Aurora's sparse aurora-dust layer.
    Sunshine,      ///< Maps to ParticleType::Sunshine. used by GodRays for rainbow-tinted beams.
    Smoke,         ///< Maps to ParticleType::Smoke. drifting haze layer for AshFall/EmberStorm.
    Zap,           ///< Maps to ParticleType::Zap. Thunderstorm's electric crackle layer.
    Wind,          ///< Maps to ParticleType::Wind. Sandstorm's gust-streak layer.
    Aurora,        ///< Maps to ParticleType::Aurora. Aurora's hand-painted mote layer.
    Constellation  ///< Maps to ParticleType::Constellation. MeteorShower's settling stardust.
};

/**
 * @enum LightSchedule
 * @brief Light schedule with smooth dawn and dusk transitions.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
enum class LightSchedule
{
    AlwaysOn = 0,  ///< Full intensity 24h.
    NightOnly,     ///< Full 22:00-04:00, fades 04:00-06:00 and 20:00-22:00.
    DuskToDawn     ///< Full 20:00-04:00, fades 04:00-07:00 and 18:00-20:00.
};

template <>
struct EnumTraits<LightSchedule> : EnumTraitsBase<LightSchedule, EnumTraits<LightSchedule>>
{
    static constexpr size_t Count = 3;
    static constexpr std::string_view Names[] = {"AlwaysOn", "NightOnly", "DuskToDawn"};

    static_assert(std::to_underlying(LightSchedule::DuskToDawn) == Count - 1,
                  "Update EnumTraits<LightSchedule> when adding new schedule values");
};

/**
 * @struct WorldLight
 * @brief Map-owned lamp resolved by worldLights::Build.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Flat rendering uses camera-relative pools; 3D pools sit at the lamp's surface height.
 */
struct WorldLight
{
    glm::vec2 position{0.0f};             ///< World pixel coords (center of pool).
    glm::vec3 color{1.0f, 0.85f, 0.55f};  ///< RGB tint (default: warm lantern).
    float radius{64.0f};                  ///< Soft-circle radius in world pixels.
    LightSchedule schedule{LightSchedule::NightOnly};
};

/**
 * @struct WeatherDefinition
 * @brief Settings in the static WeatherState table.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
struct WeatherDefinition
{
    /// Ambient tint multiplier composed on top of the TimeManager tint.
    glm::vec3 ambientTintMultiplier{1.0f, 1.0f, 1.0f};

    /// If any component is < 0, no override (use TimeManager::GetSkyColor).
    glm::vec3 skyColorOverride{-1.0f, -1.0f, -1.0f};

    /// Primary weather particle. none disables weather-driven spawning.
    WeatherParticleType particleType{WeatherParticleType::None};

    /// Particles spawned per second across the visible viewport (before intensity).
    float baseSpawnRate{0.0f};

    /// Hard cap on simultaneously live weather particles.
    int maxWeatherParticles{0};

    /// Multiplier on the type's default sprite size.
    float particleSizeScale{1.0f};

    /// If >= 0, overrides TimeManager::GetStarVisibility (clamped 0-1).
    float starVisibilityOverride{-1.0f};

    /// Hide sun/moon body sprites entirely (rays still draw if sun is up).
    bool showCelestialBodies{true};

    /// If > 0, lightning flashes this often (seconds, with +/-30% jitter).
    float lightningIntervalSeconds{0.0f};

    /// Render aurora bands in the upper sky.
    bool showAurora{false};

    /// Spawn rate multiplier on shooting stars (1.0 = default cadence).
    float meteorRateMultiplier{1.0f};

    /// Wind intensity 0-1 - affects horizontal drift of leaf/pollen/ash/sand.
    float windIntensity{0.5f};

    /// Optional second spawn slot; none disables it.
    WeatherParticleType secondaryParticleType{WeatherParticleType::None};

    /**
     * @brief Spawn rate (particles/sec) for the secondary particle.
     *
     * ignored when `secondaryParticleType` is `None`.
     */
    float secondaryBaseSpawnRate{0.0f};

    /**
     * @brief Hard cap on simultaneously live secondary particles.
     *
     * 0 = unlimited (still subject to global ParticleSystem cap).
     */
    int secondaryMaxWeatherParticles{0};

    /// Scales Fog render alpha independently of spawn density.
    float fogAlphaMultiplier{1.0f};

    /// Unused; the weather tint still applies.
    float hazeAmplitude{0.0f};
};

/**
 * @fn const WeatherDefinition& GetWeatherDefinition(WeatherState state)
 * @brief Returns a program-lifetime definition; invalid states select clear.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
const WeatherDefinition& GetWeatherDefinition(WeatherState state);

/**
 * @fn float ComputeLightIntensity(LightSchedule schedule, float hourOfDay)
 * @brief Smooth light intensity from 0 to 1 for the selected schedule.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * @param schedule Activation rule; disabled schedules contribute no light.
 * @param hourOfDay Hours from zero inclusive to 24 exclusive.
 */
float ComputeLightIntensity(LightSchedule schedule, float hourOfDay);
