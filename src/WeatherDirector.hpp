#pragma once

#include "AmbienceConfig.hpp"
#include "TimeManager.hpp"
#include "WeatherBlend.hpp"
#include "WeatherDefinitions.hpp"

/**
 * @class WeatherDirector
 * @brief Publishes forecast-driven transitions through TimeManager.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Game owns the director. TimeManager borrows its outgoing and effective definitions;
 * Keep the director alive and at the same address until those publications are cleared.
 * Incoming definitions come from the weather table. Enabled manual requests hold the
 * current forecast front.
 *
 * Transitions publish endpoint blends and fades each frame. Retargets capture exact
 * resolved channels and republish that capture after each SetWeatherBlend.
 * Fog-to-nonfog transitions hold outgoing alpha, then publish effective-only fog decay.
 * Disabled or nonpositive-duration requests cut directly and collapse transition endpoints.
 *
 * ```mermaid
 * stateDiagram-v2
 *     [*] --> Idle
 *     Idle --> Transition: StartWeatherChange<br/>SetWeatherBlend + SetWeatherFades
 *     Transition --> Transition: retarget<br/>+ SetWeatherBlendResolvedFrom
 *     Transition --> Idle: elapsed >= duration, no fog hold<br/>ClearWeatherBlend
 *     Transition --> FogDecay: elapsed >= duration, fog hold engaged<br/>SetWeatherBlend null,null
 *     FogDecay --> Idle: decay done<br/>ClearWeatherBlend
 *     FogDecay --> Transition: StartWeatherChange<br/>seeds from-def with live fog alpha
 *     Transition --> Idle: hard cut or Reset<br/>SetWeather + ClearWeatherBlend
 *     FogDecay --> Idle: hard cut or Reset<br/>ClearWeatherBlend
 * ```
 */
class WeatherDirector
{
public:
    /// Snapshot of the active transition for debug UI / console status.
    struct Transition
    {
        WeatherState from{WeatherState::Clear};
        WeatherState to{WeatherState::Clear};
        float progress{1.0f};  ///< Eased blend weight; 1.0 when idle.
        bool active{false};
    };

    /**
     * @fn void Update(float deltaTime, TimeManager& time)
     * @brief Advances and publishes weather after TimeManager::Update during gameplay.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Skipping this call pauses transitions, fog decay, and gusts.
     *
     * @param deltaTime Real seconds, unaffected by time scale or clock pause.
     * @param time Clock and weather state that receives borrowed transition definitions.
     */
    void Update(float deltaTime, TimeManager& time);

    /**
     * @fn void RequestWeather(TimeManager& time, WeatherState target, float durationSeconds)
     * @brief Requests manual weather and holds the current forecast front when enabled.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Nonpositive duration or a disabled director cuts directly. Requesting the current target
     * keeps the transition but refreshes the hold. Retargets start from resolved blended values;
     * TimeManager reports the target immediately.
     */
    void RequestWeather(TimeManager& time, WeatherState target, float durationSeconds);

    /**
     * @fn void Reset(TimeManager& time)
     * @brief Clears transitions, fog, manual holds, and published blends.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Call with TimeManager::Initialize. preserves automatic-weather settings and gust
     * clock/phases;
     * Resets published wind to base direction and strength 0.5.
     */
    void Reset(TimeManager& time);

    /**
     * @fn void SetEnabled(bool enabled)
     * @brief Control whether weather requests use transitions.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Disabled requests set weather immediately. Gameplay worlds enable transitions; title worlds
     * disable them.
     */
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    bool IsTransitioning() const { return m_Active; }
    Transition GetTransition() const;

    /**
     * @fn void SetForecastSeed(uint64_t seed)
     * @brief Sets the forecast seed and recomputes session-constant gust phases.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetForecastSeed(uint64_t seed);

    /**
     * @fn uint64_t GetForecastSeed() const
     * @brief The active forecast seed (default: a fixed constant).
     * @author Alex (<https://github.com/lextpf>)
     */
    uint64_t GetForecastSeed() const { return m_ForecastSeed; }

    /**
     * @fn void SetAutoWeather(bool enabled)
     * @brief Enables forecast reconciliation or leaves weather under manual control.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Re-enabling clears a manual hold so reconciliation resumes immediately.
     */
    void SetAutoWeather(bool enabled);
    bool IsAutoWeather() const { return m_AutoWeather; }

    /**
     * @fn bool IsManualHold() const
     * @brief True while a manual RequestWeather() is holding reconciliation back for the rest of
     * the current forecast front (console `weather.status`).
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsManualHold() const { return m_ManualHoldSet; }

    /**
     * @fn ForecastEntry GetForecast(const TimeManager& time, int64_t dayOffset) const
     * @brief The forecast for a day relative to time's current day.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param time Source of the current day count.
     * @param dayOffset Days from today; zero selects today and negative values select the past.
     * @return The forecast entry for that day under the active seed.
     */
    ForecastEntry GetForecast(const TimeManager& time, int64_t dayOffset) const;

    /**
     * @brief Outgoing and incoming particle definitions; both null while idle.
     *
     * Check both pointers before using transition spawning. The outgoing definition borrows
     * director storage and can change on a retarget. Consume the streams in the current frame;
     * Copy the definition if a later frame needs the same values.
     */
    struct SpawnStreams
    {
        const WeatherDefinition* outgoing{nullptr};  ///< Outgoing endpoint; null when idle.
        const WeatherDefinition* incoming{nullptr};  ///< Target's table def; null when idle.
        float weight{0.0f};                          ///< Eased blend weight 0 to 1.
    };

    SpawnStreams GetSpawnStreams() const;

    /**
     * @fn glm::vec2 GetWindDirection() const
     * @brief Gusted wind direction (normalized).
     * @author Alex (<https://github.com/lextpf>)
     *
     * updated every Update() call; frozen while paused (no Update calls).
     */
    glm::vec2 GetWindDirection() const { return m_WindDir; }

    /**
     * @fn float GetWindStrength() const
     * @brief Gusted wind strength for the current frame, derived from the effective weather def's
     * windIntensity through GustWindStrength (WeatherBlend.hpp).
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetWindStrength() const { return m_WindStrength; }

private:
    /**
     * @fn void Publish(TimeManager& time)
     * @brief Recompute m_Effective and republish blend + fades for the current state.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Publish(TimeManager& time);

    /**
     * @fn void StartWeatherChange(TimeManager& time, WeatherState target, float durationSeconds)
     * @brief Shares transition logic between manual requests and forecast reconciliation.
     * @author Alex (<https://github.com/lextpf>)
     */
    void StartWeatherChange(TimeManager& time, WeatherState target, float durationSeconds);

    /**
     * @fn float Progress() const
     * @brief Eased 0 to 1 progress of the active transition; 1.0 when idle.
     * @author Alex (<https://github.com/lextpf>)
     *
     * shared by GetTransition(), GetSpawnStreams(), and Publish().
     */
    float Progress() const;

    bool m_Enabled{false};  ///< Off until a gameplay world loads.
    bool m_Active{false};   ///< A transition is in flight.
    WeatherState m_FromState{WeatherState::Clear};
    WeatherState m_ToState{WeatherState::Clear};
    WeatherDefinition m_FromDef{};    ///< Outgoing endpoint (copy; stable storage).
    WeatherDefinition m_Effective{};  ///< Published blended def (stable storage).
    float m_Elapsed{0.0f};
    float m_Duration{0.0f};

    bool m_FogHoldActive{false};   ///< Holding outgoing fogAlphaMultiplier.
    float m_FogHoldValue{1.0f};    ///< Held multiplier value.
    bool m_FogDecayActive{false};  ///< Post-transition decay in progress.
    float m_FogDecayElapsed{0.0f};

    /// Republished after each SetWeatherBlend, which invalidates captures.
    bool m_UseResolvedFrom{false};             ///< True while a retarget capture is live.
    ResolvedWeatherChannels m_ResolvedFrom{};  ///< Captured ambient, sky and star channels.

    /// Captures boolean endpoints for a new transition, or live fades for retargeting.
    float m_FromCelestialFade{1.0f};  ///< Sun/moon body fade at transition start.
    float m_FromAuroraFade{0.0f};     ///< Aurora band fade at transition start.

    double m_Clock{0.0};  ///< Real-seconds accumulator for the gust envelope.
    glm::vec2 m_WindDir{  /// Gusted wind direction (normalized).
                        glm::normalize(ambience::WEATHER_WIND_BASE_DIR)};
    float m_WindStrength{0.5f};  ///< Gusted strength; 0.5 = engine-wide base.

    uint64_t m_ForecastSeed{0x9E3779B97F4A7C15ULL};
    bool m_AutoWeather{true};  ///< False = manual sticky (see SetAutoWeather).

    /// Cleared at a front boundary, reset, or SetAutoWeather(true).
    bool m_ManualHoldSet{false};   ///< True while the hold suspends reconciliation.
    int64_t m_ManualHoldFront{0};  ///< Forecast front the console override was made on.

    /// Seed-derived phases stay constant across midnight.
    glm::vec3 m_GustPhases{GustPhases(SplitMix64(0x9E3779B97F4A7C15ULL))};
};
