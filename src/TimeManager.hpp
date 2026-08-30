#pragma once

#include "WeatherDefinitions.hpp"

#include <glm/glm.hpp>

/**
 * @enum TimePeriod
 * @brief Partitions the 24-hour clock into named lighting periods.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
enum class TimePeriod
{
    Dawn,       ///< 05:00-07:00 - sunrise transition.
    Morning,    ///< 07:00-10:00 - early day, golden hour.
    Midday,     ///< 10:00-16:00 - full daylight.
    Afternoon,  ///< 16:00-18:00 - late day warmth.
    Dusk,       ///< 18:00-20:00 - sunset transition.
    Evening,    ///< 20:00-22:00 - early night.
    Night,      ///< 22:00-04:00 - deep night.
    LateNight   ///< 04:00-05:00 - pre-dawn darkness.
};

/**
 * @struct ResolvedWeatherChannels
 * @brief Exact resolved endpoints for transition retargeting.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Captures avoid inverting weather intensity and day/night sentinel formulas.
 */
struct ResolvedWeatherChannels
{
    glm::vec3 ambient{1.0f};     ///< GetAmbientColor output at capture time.
    glm::vec3 sky{0.0f};         ///< GetSkyColor output at capture time.
    float starVisibility{0.0f};  ///< GetStarVisibility output at capture time.
};

/**
 * @class TimeManager
 * @brief Owns the game clock and resolves time-dependent lighting and weather channels.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Hours wrap at 24. solar noon is 13:00, midway between sunrise 6:00 and sunset 20:00.
 * Moonrise is 19:00 and moonset 7:00; phases repeat every eight days.
 * Initialize restores a 24-real-second day. apply custom duration after initialization.
 *
 * Natural stars fade in 18:00-20:00 and out 5:00-7:00. dawn effects ramp 4:30-5:30,
 * hold to 6:30, and fade to 8:00. weather can override star visibility.
 *
 * @code
 *     0.0 ------- 6.0 --------- 13.0 -------- 20.0 ------ 24.0
 *  Midnight     Sunrise      Solar noon      Sunset     Midnight
 * @endcode
 *
 * ```mermaid
 * graph LR
 *     classDef night fill:#1a1a2e,stroke:#4a4a6a,color:#e2e8f0
 *     classDef dawn fill:#614385,stroke:#9b59b6,color:#e2e8f0
 *     classDef day fill:#f39c12,stroke:#e67e22,color:#1a1a2e
 *     classDef dusk fill:#c0392b,stroke:#e74c3c,color:#e2e8f0
 *
 *     subgraph "24-Hour Cycle"
 *         N["Night<br/>22:00-04:00<br/>Stars + Moon"]:::night
 *         LN["Late Night<br/>04:00-05:00<br/>Pre-dawn"]:::night
 *         D["Dawn<br/>05:00-07:00<br/>Sun rising"]:::dawn
 *         M["Morning<br/>07:00-10:00<br/>Golden hour"]:::day
 *         MD["Midday<br/>10:00-16:00<br/>Full sun"]:::day
 *         A["Afternoon<br/>16:00-18:00<br/>Warm light"]:::day
 *         DU["Dusk<br/>18:00-20:00<br/>Sun setting"]:::dusk
 *         E["Evening<br/>20:00-22:00<br/>Moon rising"]:::night
 *     end
 *
 *     N --> LN --> D --> M --> MD --> A --> DU --> E --> N
 * ```
 *
 * $$
 * sunArc = \frac{time - sunrise}{sunset - sunrise}
 * $$
 *
 * @code
 *   Phase 0: New Moon         (invisible)
 *   Phase 1: Waxing Crescent
 *   Phase 2: First Quarter
 *   Phase 3: Waxing Gibbous
 *   Phase 4: Full Moon        (brightest)
 *   Phase 5: Waning Gibbous
 *   Phase 6: Last Quarter
 *   Phase 7: Waning Crescent
 * @endcode
 *
 * @verbatim
 * hour    0     2     4     6     8     10    12    14    16    18    20    22    24
 *         |-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
 * period  |   Night   |LN| Dawn|Morning |      Midday     | Aft |Dusk | Eve |Night
 * sun                       ^rise 6:00                                ^set 20:00
 * moon                         ^set 7:00                           ^rise 19:00
 * stars   ===== 1.0 =====\____ ............ 0.0 ................ ____/==== 1.0 ====
 * dawnFx               ////^^^\\\\\
 * @endverbatim
 *
 * $$
 * gameHours = \frac{realSeconds \times 24 \times timeScale}{dayDuration}
 * $$
 *
 * @code
 * TimeManager time;
 * time.Initialize();  // 24 s per day - the shipped setting
 * time.SetTime(6.0f); // Start at sunrise
 *
 * // In game loop:
 * time.Update(deltaTime);
 *
 * // For rendering:
 * glm::vec3 ambient = time.GetAmbientColor();
 * float sunArc = time.GetSunArc();
 * float starVis = time.GetStarVisibility();
 *
 * // For gameplay:
 * if (time.IsNight()) {
 *     SpawnNightCreatures();
 * }
 * @endcode
 */
class TimeManager
{
public:
    /**
     * @fn TimeManager()
     * @brief Starts at 12:00 with a 24-second day, scale 1, and clear weather.
     * @author Alex (<https://github.com/lextpf>)
     */
    TimeManager();

    /**
     * @fn void Initialize()
     * @brief Restores defaults and clears weather blends and overlays.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Initialize();

    /**
     * @fn void Update(float deltaTime)
     * @brief Advances real-time weather effects, then the unpaused game clock.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Call once per frame. do not also call UpdateWeatherEffects; it would advance fades twice.
     * Weather overlay fades continue while the clock is paused.
     *
     * @param deltaTime Real seconds.
     */
    void Update(float deltaTime);

    /**
     * @fn void UpdateWeatherEffects(float deltaTime)
     * @brief Advances overlay fades while leaving the game clock fixed.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param deltaTime Real seconds.
     */
    void UpdateWeatherEffects(float deltaTime);

    /**
     * @fn float GetTimeOfDay() const
     * @brief Hours from zero inclusive to 24 exclusive.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetTimeOfDay() const { return m_CurrentTime; }

    TimePeriod GetTimePeriod() const;

    /**
     * @fn int GetDayCount() const
     * @brief Elapsed days from zero; changes at midnight and controls moon phase.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetDayCount() const { return m_DayCount; }

    /**
     * @fn bool IsDay() const
     * @brief True from 6:00 inclusive to 20:00 exclusive.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsDay() const;

    /**
     * @fn bool IsNight() const
     * @brief True outside the 6:00-20:00 daylight interval.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsNight() const;

    /**
     * @fn float GetSunArc() const
     * @brief Normalized sunrise-to-sunset arc, or -1 below the horizon.
     * @author Alex (<https://github.com/lextpf>)
     *
     * 0.5 is solar noon at 13:00.
     */
    float GetSunArc() const;

    /**
     * @fn float GetMoonArc() const
     * @brief Normalized moonrise-to-moonset arc, or -1 below the horizon.
     * @author Alex (<https://github.com/lextpf>)
     *
     * 0.5 is 1:00.
     *
     * @pre Moonrise and moonset must remain 12 hours apart; the denominator is fixed.
     */
    float GetMoonArc() const;

    /**
     * @fn int GetMoonPhase() const
     * @brief Phase index 0-7; 0 new, 2 first quarter, 4 full, 6 last quarter.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetMoonPhase() const;

    /**
     * @brief Resolves weather and overlay contributions per lighting channel.
     *
     * B is GetOverlayBlend, int is weather intensity, and n is imposed night.
     * Imposed night is clamp(resolved stars - natural stars, 0, 1). It pulls ambient toward
     * (0.30, 0.30, 0.45), sky toward (0.04, 0.04, 0.12), and fades celestial bodies.
     * Aurora has no star override. GetSunColor uses time alone.
     *
     * @verbatim
     * stage                | ambient      | sky          | stars       | celestFade | auroraFade
     * ---------------------|--------------|--------------|-------------|------------|-----------
     * time-of-day anchor   | anchor       | natural      | natural     | --         | --
     * weather x intensity  | tint mix     | override mix | override    | bool       | bool
     * director blend (t)   | lerp         | lerp         | lerp        | published  | published
     * manual overlay       | w = b * int  | --           | w = b       | --         | w = b
     * imposed night        | mix -> night | mix -> night | (its input) | x (1 - n)  | --
     * @endverbatim
     */

    /**
     * @fn glm::vec3 GetAmbientColor() const
     * @brief RGB multiplier with weather, transition, overlay, and imposed-night contributions.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec3 GetAmbientColor() const;

    /**
     * @fn glm::vec3 GetSkyColor() const
     * @brief RGB sky with weather overrides, transition, and imposed-night contributions.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The manual overlay affects sky color only through imposed night.
     */
    glm::vec3 GetSkyColor() const;

    /**
     * @fn glm::vec3 GetSunColor() const
     * @brief Time-only sun tint; zero below the horizon.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Gate on GetSunArc before using this color as a multiplier.
     */
    glm::vec3 GetSunColor() const;

    /**
     * @fn float GetStarVisibility() const
     * @brief Star visibility from 0 to 1 after weather, transition, and overlay effects.
     * @author Alex (<https://github.com/lextpf>)
     *
     * May reach 1 at noon or 0 at midnight. Use GetNaturalStarVisibility for clock-only consumers.
     */
    float GetStarVisibility() const;

    /**
     * @fn float GetDawnIntensity() const
     * @brief Dawn intensity from 0 to 1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Ramps 4:30-5:30, holds until 6:30, then fades to zero at 8:00.
     */
    float GetDawnIntensity() const;

    WeatherState GetWeather() const { return m_Weather; }

    void SetWeather(WeatherState weather) { m_Weather = weather; }

    /**
     * @fn float GetWeatherIntensity() const
     * @brief Particle-rate and effect-strength multiplier from 0 to 1.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetWeatherIntensity() const { return m_WeatherIntensity; }

    /**
     * @fn void SetWeatherIntensity(float value)
     * @brief Clamps particle-rate and effect-strength scaling to 0-1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Boolean lightning, aurora, and celestial visibility flags are unaffected.
     */
    void SetWeatherIntensity(float value);

    /**
     * @fn float GetNaturalStarVisibility() const
     * @brief Clock-only visibility from 0 to 1, unaffected by weather overrides.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetNaturalStarVisibility() const;

    /**
     * @fn void SetWeatherBlend(const WeatherDefinition* from, const WeatherDefinition* to, float \
     * t, const WeatherDefinition* effective)
     * @brief Publishes borrowed endpoints and an optional effective definition.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Each publication clears the resolved-from capture. republish that capture afterwards
     * when retargeting. null endpoints select single-weather getters while effective can still
     * provide fog decay. supply both endpoints or neither.
     *
     * The pointed-to definitions must remain alive and at stable addresses until replaced or
     * cleared. This call does not copy them; later owner updates are visible to weather getters.
     *
     * @param from Borrowed outgoing endpoint, or null with `to`.
     * @param to Borrowed incoming endpoint, or null with `from`.
     * @param t Blend weight clamped to [0, 1]; ignored with null endpoints.
     * @param effective Borrowed effective definition; null selects the current weather table entry.
     */
    void SetWeatherBlend(const WeatherDefinition* from,
                         const WeatherDefinition* to,
                         float t,
                         const WeatherDefinition* effective);

    /**
     * @fn void SetWeatherBlendResolvedFrom(const ResolvedWeatherChannels& resolved)
     * @brief Captures exact getter values for a continuous retarget.
     * @author Alex (<https://github.com/lextpf>)
     *
     * SetWeatherBlend and ClearWeatherBlend invalidate this capture.
     */
    void SetWeatherBlendResolvedFrom(const ResolvedWeatherChannels& resolved);

    /**
     * @fn void ClearWeatherBlend()
     * @brief Clears endpoints, capture, and fades; restores single-weather resolution.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ClearWeatherBlend();

    /**
     * @fn bool HasWeatherBlend() const
     * @brief True when both endpoints are non-null.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool HasWeatherBlend() const { return m_BlendFrom != nullptr && m_BlendTo != nullptr; }

    /**
     * @fn const WeatherDefinition& GetEffectiveWeatherDefinition() const
     * @brief Published effective definition, or the current weather table entry.
     * @author Alex (<https://github.com/lextpf>)
     */
    const WeatherDefinition& GetEffectiveWeatherDefinition() const;

    /**
     * @fn void SetWeatherFades(float celestialFade, float auroraFade)
     * @brief Publishes smooth celestial and aurora fades in place of boolean gates.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Both inputs clamp to 0-1. ClearWeatherBlend removes the publication.
     */
    void SetWeatherFades(float celestialFade, float auroraFade);

    /**
     * @fn float GetCelestialFade() const
     * @brief Celestial fade from 0 to 1, scaled by one minus imposed night.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Uses the published fade or the effective definition's boolean. The overlay contributes
     * only through imposed night.
     */
    float GetCelestialFade() const;

    /**
     * @fn float GetAuroraFade() const
     * @brief Aurora fade from 0 to 1, including the manual overlay.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Uses the published fade or effective boolean. Imposed night does not scale this channel.
     */
    float GetAuroraFade() const;

    /**
     * @fn void SetWeatherOverlay(WeatherState state)
     * @brief Fades in overlay stars, aurora, meteor rate, and ambient tint.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Update or UpdateWeatherEffects advances the fade.
     */
    void SetWeatherOverlay(WeatherState state);

    /**
     * @fn void ClearWeatherOverlay()
     * @brief Fades out the overlay while retaining its state for resolution.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ClearWeatherOverlay();

    /**
     * @fn bool HasWeatherOverlay() const
     * @brief Reports the overlay target; may be false during fade-out.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool HasWeatherOverlay() const { return m_OverlayActive; }

    /**
     * @fn WeatherState GetWeatherOverlay() const
     * @brief Last overlay state, retained during fade-out.
     * @author Alex (<https://github.com/lextpf>)
     */
    WeatherState GetWeatherOverlay() const { return m_OverlayWeather; }

    /**
     * @fn float GetOverlayBlend() const
     * @brief Eased contribution from 0 to 1.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetOverlayBlend() const { return m_OverlayBlend; }

    /**
     * @fn float GetEffectiveMeteorRate() const
     * @brief Overlay-resolved meteor rate; 1 is the default cadence.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetEffectiveMeteorRate() const;

    /**
     * @fn void SetTimeScale(float scale)
     * @brief Game-time multiplier; 1 is normal speed.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTimeScale(float scale) { m_TimeScale = scale; }

    float GetTimeScale() const { return m_TimeScale; }

    /**
     * @fn void SetDayDuration(float seconds)
     * @brief Real seconds per game day; nonpositive values become 0.001.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Initialize resets this to 24 seconds.
     */
    void SetDayDuration(float seconds) { m_DayDuration = (seconds > 0.0f) ? seconds : 0.001f; }

    /**
     * @fn float GetDayDuration() const
     * @brief Real seconds per game day.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetDayDuration() const { return m_DayDuration; }

    /**
     * @fn void SetTime(float hours)
     * @brief Wraps hours into the day without changing day count.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTime(float hours);

    /**
     * @fn void AdvanceTime(float hours)
     * @brief Advances signed hours and updates day count across midnight.
     * @author Alex (<https://github.com/lextpf>)
     */
    void AdvanceTime(float hours);

    void SetPaused(bool paused) { m_Paused = paused; }

    bool IsPaused() const { return m_Paused; }

    void TogglePause() { m_Paused = !m_Paused; }

private:
    glm::vec3 LerpColor(const glm::vec3& a, const glm::vec3& b, float t) const;

    /**
     * @fn float GetTransitionFactor(float time, float start, float end) const
     * @brief Returns a clamped linear ramp, or zero when end is not after start.
     * @author Alex (<https://github.com/lextpf>)
     *
     * $$
     * f(t; a, b) =
     * \begin{cases}
     * 0, & b \le a \\
     * \operatorname{clamp}\left(\frac{t-a}{b-a}, 0, 1\right), & b > a
     * \end{cases}
     * $$
     */
    float GetTransitionFactor(float time, float start, float end) const;

    /**
     * @fn glm::vec3 NaturalSkyColor() const
     * @brief Natural (weatherless) sky color for the current hour.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec3 NaturalSkyColor() const;
    /**
     * @fn float NaturalStarVisibility() const
     * @brief Natural (weatherless) star visibility for the current hour.
     * @author Alex (<https://github.com/lextpf>)
     */
    float NaturalStarVisibility() const;
    /**
     * @fn float SkyDayNightFactor() const
     * @brief Ramped day/night factor for the sky override (replaces binary IsDay()).
     * @author Alex (<https://github.com/lextpf>)
     */
    float SkyDayNightFactor() const;
    /**
     * @fn glm::vec3 ComputeAmbientColor(const WeatherDefinition& def) const
     * @brief Ambient color as it would render under def.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec3 ComputeAmbientColor(const WeatherDefinition& def) const;
    /**
     * @fn glm::vec3 ComputeSkyColor(const WeatherDefinition& def) const
     * @brief Sky color as it would render under def.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec3 ComputeSkyColor(const WeatherDefinition& def) const;
    /**
     * @fn float ComputeStarVisibility(const WeatherDefinition& def) const
     * @brief Star visibility as it would render under def.
     * @author Alex (<https://github.com/lextpf>)
     */
    float ComputeStarVisibility(const WeatherDefinition& def) const;

    /**
     * @fn float ImposedNightAmount() const
     * @brief Excess resolved star visibility above the natural hour, clamped to 0-1.
     * @author Alex (<https://github.com/lextpf>)
     */
    float ImposedNightAmount() const;

    float m_CurrentTime;             ///< Current time in hours (0.0-24.0).
    int m_DayCount;                  ///< Days elapsed (for moon phases).
    float m_TimeScale;               ///< Time progression multiplier (1.0 = normal).
    float m_DayDuration;             ///< Real seconds per game day (24 s default: 1 game hour/s).
    WeatherState m_Weather;          ///< Current weather condition.
    float m_WeatherIntensity{1.0f};  ///< Particle/effect density 0-1.
    bool m_Paused{false};            ///< Whether time progression is paused.
    WeatherState m_OverlayWeather{
        WeatherState::Clear};     ///< Manual sky overlay (valid while blend > 0).
    bool m_OverlayActive{false};  ///< Overlay set (blend target = 1).
    float m_OverlayBlend{0.0f};   ///< Eased 0-1 fade of the whole overlay.

    /// Borrowed director storage; must outlive blend use.
    const WeatherDefinition* m_BlendFrom{nullptr};       ///< Outgoing endpoint (null = no blend).
    const WeatherDefinition* m_BlendTo{nullptr};         ///< Incoming endpoint.
    const WeatherDefinition* m_BlendEffective{nullptr};  ///< Director-owned blended def.
    float m_BlendT{0.0f};                                ///< Eased blend weight 0 to 1.
    bool m_HasResolvedFrom{false};  ///< Use m_ResolvedFrom instead of m_BlendFrom formulas.
    ResolvedWeatherChannels m_ResolvedFrom{};  ///< Captured retarget from-values.
    float m_CelestialFade{-1.0f};              ///< Published fade; < 0 = derive from def bool.
    float m_AuroraFade{-1.0f};                 ///< Published fade; < 0 = derive from def bool.

    static constexpr float DAWN_START = 5.0f;      ///< Dawn begins.
    static constexpr float DAWN_END = 7.0f;        ///< Dawn ends, morning begins.
    static constexpr float MORNING_END = 10.0f;    ///< Morning ends, midday begins.
    static constexpr float MIDDAY_END = 16.0f;     ///< Midday ends, afternoon begins.
    static constexpr float AFTERNOON_END = 18.0f;  ///< Afternoon ends, dusk begins.
    static constexpr float DUSK_END = 20.0f;       ///< Dusk ends, evening begins.
    static constexpr float EVENING_END = 22.0f;    ///< Evening ends, night begins.
    static constexpr float NIGHT_END = 4.0f;       ///< Night ends (wraps), late night begins.

    static constexpr float SUNRISE_TIME = 6.0f;    ///< Sun rises above horizon.
    static constexpr float SUNSET_TIME = 20.0f;    ///< Sun sets below horizon.
    static constexpr float MOONRISE_TIME = 19.0f;  ///< Moon rises above horizon.
    static constexpr float MOONSET_TIME = 7.0f;    ///< Moon sets below horizon.

    static constexpr int MOON_CYCLE_DAYS = 8;  ///< Days for complete lunar cycle.
};
