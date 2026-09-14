#pragma once

#include "WeatherDefinitions.hpp"

#include <cstdint>

/**
 * @brief Deterministic weather interpolation and forecast math.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */

/**
 * @fn float BlendSmoothstep(float t)
 * @brief Smoothstep with input clamped to 0-1.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
float BlendSmoothstep(float t);

/**
 * @fn bool WeatherSpawnsFogType(const WeatherDefinition& def)
 * @brief Detects fog in either spawn slot for the director's alpha hold.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
bool WeatherSpawnsFogType(const WeatherDefinition& def);

/**
 * @fn WeatherDefinition BlendWeatherDefinitions(const WeatherDefinition& a, const \
 * WeatherDefinition& b, float t)
 * @brief Interpolates weather definitions without easing.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * `t` <= 0 returns a unchanged; t >= 1 returns b unchanged, including sentinels.
 *
 * | field                 | interior rule                               |
 * |-----------------------|---------------------------------------------|
 * | plain floats          | linear interpolation                        |
 * | lightning interval    | interpolate frequency                       |
 * | changed particle type | incoming rate starts at zero                |
 * | integer caps          | mix and round; shared types use minimum cap |
 * | sentinels/bools/types | copy b; other consumers resolve fades       |
 *
 * Cap 0 means unlimited.
 */
WeatherDefinition BlendWeatherDefinitions(const WeatherDefinition& a,
                                          const WeatherDefinition& b,
                                          float t);

/**
 * @fn uint64_t SplitMix64(uint64_t x)
 * @brief SplitMix64 mixer used for gusts, fronts, and forecast rolls.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
uint64_t SplitMix64(uint64_t x);

/**
 * @fn glm::vec3 GustPhases(uint64_t seed)
 * @brief Deterministic gust phases in radians, each from zero to 2*pi.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * X and Y drive strength; Z drives direction.
 */
glm::vec3 GustPhases(uint64_t seed);

/**
 * @fn float GustWindStrength(float base, double clockSeconds, const glm::vec3& phases)
 * @brief Gusted wind strength, always non-negative.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Strength = base * (1 + GUST_AMP * (0.6*sin(2*pi*t/t1 + p1) +
 * 0.4*sin(2*pi*t/t2 + p2))), clamped at zero.
 *
 * @param base Base wind strength (weather's steady-state value).
 * @param clockSeconds Real-time clock, seconds.
 * @param phases Gust phase offsets from GustPhases (.x/.y used).
 * @return Gusted strength, never negative.
 */
float GustWindStrength(float base, double clockSeconds, const glm::vec3& phases);

/**
 * @fn glm::vec2 GustWindDirection(double clockSeconds, const glm::vec3& phases)
 * @brief Gust wind direction.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * ambience::WEATHER_WIND_BASE_DIR rotated by a slow sine wander of
 * +/- WEATHER_WIND_WANDER_DEG.
 *
 * @param clockSeconds Real-time clock, seconds.
 * @param phases Gust phase offsets from GustPhases (.z used).
 * @return Normalized wind direction.
 */
glm::vec2 GustWindDirection(double clockSeconds, const glm::vec3& phases);

/**
 * @fn int WeatherCapForType(const WeatherDefinition& def, WeatherParticleType type)
 * @brief Shared-type cap across both spawn slots.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Zero means unspawned or uncapped; cap consumers treat either as unlimited.
 */
int WeatherCapForType(const WeatherDefinition& def, WeatherParticleType type);

/**
 * @struct ForecastEntry
 * @brief Front weather and an optional night event from 20:00 to 5:00.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
struct ForecastEntry
{
    WeatherState front{WeatherState::Clear};       ///< Front weather holding this day.
    bool hasNightEvent{false};                     ///< Whether a night event overlays tonight.
    WeatherState nightEvent{WeatherState::Clear};  ///< Night event, valid only if hasNightEvent.
};

/**
 * @fn int64_t ForecastFrontIndex(uint64_t seed, int64_t dayIndex)
 * @brief Resolves hash-jittered fronts without gaps or overlaps.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * @param seed Stable world seed used to derive front boundaries and forecast choices.
 * @param dayIndex May be negative.
 * @return Nondecreasing index; advances by one at each front boundary.
 */
int64_t ForecastFrontIndex(uint64_t seed, int64_t dayIndex);

/**
 * @fn ForecastEntry ForecastForDay(uint64_t seed, int64_t dayIndex)
 * @brief Deterministic, allocation-free forecast in constant time.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * The front containing day 0 is clear. Boundary jitter can make its index -1.
 *
 * @param seed Stable world seed used to derive front boundaries and forecast choices.
 * @param dayIndex May be negative.
 */
ForecastEntry ForecastForDay(uint64_t seed, int64_t dayIndex);

/**
 * @brief Sky-only overlay merges preserve the base at zero blend.
 * @ingroup Effects
 *
 * Scalar and aurora merges can only increase their channels; tint multiplies and may darken.
 */

/**
 * @fn float BlendOverlayScalar(float base, float overlay, float blend)
 * @brief Fade in an overlay that can raise a scalar channel.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Uses lerp(base, max(base, overlay), clamp(blend, 0, 1)); the channel never decreases.
 */
float BlendOverlayScalar(float base, float overlay, float blend);

/**
 * @fn glm::vec3 BlendOverlayTint(glm::vec3 baseColor, glm::vec3 overlayTint, float amount)
 * @brief Fade the overlay tint into an ambient color multiplier.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Uses baseColor * mix(vec3(1), overlayTint, clamp(amount, 0, 1)).
 */
glm::vec3 BlendOverlayTint(glm::vec3 baseColor, glm::vec3 overlayTint, float amount);

/**
 * @fn float BlendOverlayAuroraFade(float baseFade, float blend, bool overlayHasAurora)
 * @brief Combine aurora fades without hiding the base weather's aurora.
 * @author Alex (<https://github.com/lextpf>)
 *
 * With overlay aurora, return max(baseFade, clamp(blend, 0, 1)); otherwise retain `baseFade`.
 */
float BlendOverlayAuroraFade(float baseFade, float blend, bool overlayHasAurora);
