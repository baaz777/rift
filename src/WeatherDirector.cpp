#include "WeatherDirector.hpp"

#include "AmbienceConfig.hpp"
#include "WeatherBlend.hpp"

#include <algorithm>
#include <cmath>

namespace
{
// Use the event duration for both entering and leaving a forecast night event.
bool IsNightEventState(WeatherState state)
{
    return state == WeatherState::Aurora || state == WeatherState::MeteorShower ||
           state == WeatherState::FireflySwarm;
}
}  // namespace

void WeatherDirector::Update(float deltaTime, TimeManager& time)
{
    // Gust phases and clock persist across transitions and midnight; only base strength blends.
    m_Clock += deltaTime;
    m_WindStrength =
        GustWindStrength(time.GetEffectiveWeatherDefinition().windIntensity, m_Clock, m_GustPhases);
    m_WindDir = GustWindDirection(m_Clock, m_GustPhases);

    // Reconcile current forecast state so time jumps and ordinary boundary crossings agree.
    if (m_Enabled && m_AutoWeather)
    {
        const int64_t day = time.GetDayCount();
        const int64_t frontIndex = ForecastFrontIndex(m_ForecastSeed, day);
        if (m_ManualHoldSet && frontIndex != m_ManualHoldFront)
        {
            m_ManualHoldSet = false;  // New front: the world takes over again
        }
        if (!m_ManualHoldSet)
        {
            const float hour = time.GetTimeOfDay();
            const ForecastEntry today = ForecastForDay(m_ForecastSeed, day);
            const ForecastEntry yesterday = ForecastForDay(m_ForecastSeed, day - 1);
            WeatherState desired = today.front;
            bool eventWindow = false;
            if (today.hasNightEvent && hour >= 20.0f)
            {
                desired = today.nightEvent;
                eventWindow = true;
            }
            else if (yesterday.hasNightEvent && hour < 5.0f)
            {
                desired = yesterday.nightEvent;  // Overlay spans midnight
                eventWindow = true;
            }
            const WeatherState target = m_Active ? m_ToState : time.GetWeather();
            if (desired != target)
            {
                // Event release uses the same transition duration as event onset.
                const bool eventEdge = eventWindow || IsNightEventState(target);
                StartWeatherChange(time,
                                   desired,
                                   eventEdge ? ambience::WEATHER_EVENT_TRANSITION_SECONDS
                                             : ambience::WEATHER_TRANSITION_SECONDS);
            }
        }
    }

    if (m_Active)
    {
        m_Elapsed += deltaTime;
        if (m_Elapsed >= m_Duration)
        {
            m_Active = false;
            if (m_FogHoldActive)
            {
                m_FogDecayActive = true;
                m_FogDecayElapsed = 0.0f;
            }
            else
            {
                time.ClearWeatherBlend();
                return;
            }
        }
        Publish(time);
        return;
    }

    if (m_FogDecayActive)
    {
        m_FogDecayElapsed += deltaTime;
        if (m_FogDecayElapsed >= ambience::WEATHER_FOG_HOLD_DECAY_SECONDS)
        {
            m_FogDecayActive = false;
            m_FogHoldActive = false;
            time.ClearWeatherBlend();
            return;
        }
        Publish(time);
    }
}

void WeatherDirector::RequestWeather(TimeManager& time, WeatherState target, float durationSeconds)
{
    if (m_Enabled)
    {
        // Enabled manual requests hold the current front; title-world requests must not retain a
        // hold.
        m_ManualHoldFront = ForecastFrontIndex(m_ForecastSeed, time.GetDayCount());
        m_ManualHoldSet = true;
    }
    StartWeatherChange(time, target, durationSeconds);
}

void WeatherDirector::StartWeatherChange(TimeManager& time,
                                         WeatherState target,
                                         float durationSeconds)
{
    const bool sameTarget =
        (m_Active && target == m_ToState) || (!m_Active && target == time.GetWeather());
    if (sameTarget)
    {
        return;
    }

    if (!m_Enabled || durationSeconds <= 0.0f)
    {
        // Hard cut: debug/console-on-title path. clears any in-flight state.
        m_Active = false;
        m_FogHoldActive = false;
        m_FogDecayActive = false;
        m_UseResolvedFrom = false;
        m_FromState = target;  // No stale from/to pair after a hard cut
        m_ToState = target;
        time.SetWeather(target);
        time.ClearWeatherBlend();
        return;
    }

    // Capture resolved channels before retargeting; sentinel formulas cannot be inverted reliably.
    if (m_Active)
    {
        m_ResolvedFrom.ambient = time.GetAmbientColor();
        m_ResolvedFrom.sky = time.GetSkyColor();
        m_ResolvedFrom.starVisibility = time.GetStarVisibility();
        m_UseResolvedFrom = true;
        m_FromCelestialFade = time.GetCelestialFade();
        m_FromAuroraFade = time.GetAuroraFade();
        m_FromDef = m_Effective;
        m_FromState = m_ToState;
    }
    else
    {
        m_UseResolvedFrom = false;
        m_FromState = time.GetWeather();
        m_FromDef = GetWeatherDefinition(m_FromState);
        if (m_FogDecayActive)
        {
            // Retarget from the live fog-decay multiplier to avoid a discontinuity.
            m_FromDef.fogAlphaMultiplier = m_Effective.fogAlphaMultiplier;
        }
        m_FromCelestialFade = m_FromDef.showCelestialBodies ? 1.0f : 0.0f;
        m_FromAuroraFade = m_FromDef.showAurora ? 1.0f : 0.0f;
    }

    // Hold outgoing fog alpha so surviving puffs do not brighten as the weather clears.
    const WeatherDefinition& toDef = GetWeatherDefinition(target);
    m_FogDecayActive = false;
    m_FogHoldActive = WeatherSpawnsFogType(m_FromDef) && !WeatherSpawnsFogType(toDef);
    if (m_FogHoldActive)
    {
        m_FogHoldValue = m_FromDef.fogAlphaMultiplier;
    }

    m_ToState = target;
    m_Duration = durationSeconds;
    m_Elapsed = 0.0f;
    m_Active = true;
    time.SetWeather(target);  // GetWeather reports the destination from frame 1
    Publish(time);
}

void WeatherDirector::Reset(TimeManager& time)
{
    m_Active = false;
    m_FogHoldActive = false;
    m_FogDecayActive = false;
    m_UseResolvedFrom = false;
    m_Elapsed = 0.0f;
    m_ManualHoldSet = false;  // World reload: the forecast is free to drive again
    time.ClearWeatherBlend();

    // Reset published wind for the title backdrop while retaining the gust clock.
    m_WindDir = glm::normalize(ambience::WEATHER_WIND_BASE_DIR);
    m_WindStrength = 0.5f;
}

void WeatherDirector::SetForecastSeed(uint64_t seed)
{
    m_ForecastSeed = seed;

    m_GustPhases = GustPhases(SplitMix64(m_ForecastSeed));
}

void WeatherDirector::SetAutoWeather(bool enabled)
{
    m_AutoWeather = enabled;
    if (enabled)
    {
        m_ManualHoldSet = false;  // Re-enabling autonomy takes over immediately
    }
}

ForecastEntry WeatherDirector::GetForecast(const TimeManager& time, int64_t dayOffset) const
{
    return ForecastForDay(m_ForecastSeed, static_cast<int64_t>(time.GetDayCount()) + dayOffset);
}

float WeatherDirector::Progress() const
{
    return m_Active ? BlendSmoothstep(m_Elapsed / std::max(m_Duration, 0.001f)) : 1.0f;
}

WeatherDirector::Transition WeatherDirector::GetTransition() const
{
    Transition t;
    t.from = m_FromState;
    t.to = m_ToState;
    t.active = m_Active;
    t.progress = Progress();
    return t;
}

WeatherDirector::SpawnStreams WeatherDirector::GetSpawnStreams() const
{
    SpawnStreams streams;
    if (m_Active)
    {
        streams.outgoing = &m_FromDef;
        streams.incoming = &GetWeatherDefinition(m_ToState);
        streams.weight = Progress();
    }
    return streams;
}

void WeatherDirector::Publish(TimeManager& time)
{
    const WeatherDefinition& fromDef = m_FromDef;
    const WeatherDefinition& toDef = GetWeatherDefinition(m_ToState);

    if (m_Active)
    {
        const float s = Progress();
        m_Effective = BlendWeatherDefinitions(fromDef, toDef, s);
        if (m_FogHoldActive)
        {
            m_Effective.fogAlphaMultiplier = m_FogHoldValue;
        }
        time.SetWeatherBlend(&m_FromDef, &toDef, s, &m_Effective);
        if (m_UseResolvedFrom)
        {
            time.SetWeatherBlendResolvedFrom(m_ResolvedFrom);
        }
        const float celestialFade =
            std::lerp(m_FromCelestialFade, toDef.showCelestialBodies ? 1.0f : 0.0f, s);
        const float auroraFade = std::lerp(m_FromAuroraFade, toDef.showAurora ? 1.0f : 0.0f, s);
        time.SetWeatherFades(celestialFade, auroraFade);
        return;
    }

    // Post-transition fog decay: destination def with an easing multiplier.
    const float decayT =
        std::clamp(m_FogDecayElapsed / ambience::WEATHER_FOG_HOLD_DECAY_SECONDS, 0.0f, 1.0f);
    m_Effective = toDef;
    m_Effective.fogAlphaMultiplier = std::lerp(m_FogHoldValue, toDef.fogAlphaMultiplier, decayT);
    // Clear blend endpoints while the effective definition continues fog decay.
    time.SetWeatherBlend(nullptr, nullptr, 1.0f, &m_Effective);
    time.SetWeatherFades(toDef.showCelestialBodies ? 1.0f : 0.0f, toDef.showAurora ? 1.0f : 0.0f);
}
