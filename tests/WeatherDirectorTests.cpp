#include <gtest/gtest.h>

#include "../src/AmbienceConfig.hpp"
#include "../src/TimeManager.hpp"
#include "../src/WeatherBlend.hpp"
#include "../src/WeatherDefinitions.hpp"
#include "../src/WeatherDirector.hpp"

#include <glm/glm.hpp>

// retargeting must preserve published weather values, including during residual fog decay.
namespace
{

void StepFrames(WeatherDirector& d, TimeManager& t, int frames, float dt = 1.0f / 60.0f)
{
    for (int i = 0; i < frames; ++i)
    {
        d.Update(dt, t);
    }
}
}  // namespace

// GetWeather exposes the destination from the first transition frame.
TEST(WeatherDirector, LifecycleReportsDestinationFromStart)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Thunderstorm, 1.0f);
    EXPECT_EQ(time.GetWeather(), WeatherState::Thunderstorm);
    EXPECT_TRUE(director.IsTransitioning());
    EXPECT_TRUE(time.HasWeatherBlend());

    StepFrames(director, time, 70);  // > 1.0 s at 60 fps
    EXPECT_FALSE(director.IsTransitioning());
    EXPECT_FALSE(time.HasWeatherBlend());
    EXPECT_EQ(time.GetWeather(), WeatherState::Thunderstorm);
}

TEST(WeatherDirector, ZeroDurationHardCuts)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Fog, 0.0f);
    EXPECT_EQ(time.GetWeather(), WeatherState::Fog);
    EXPECT_FALSE(director.IsTransitioning());
    EXPECT_FALSE(time.HasWeatherBlend());
}

TEST(WeatherDirector, DisabledDegradesToHardSet)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;  // enabled defaults to false

    director.RequestWeather(time, WeatherState::Blizzard, 10.0f);
    EXPECT_EQ(time.GetWeather(), WeatherState::Blizzard);
    EXPECT_FALSE(director.IsTransitioning());
    EXPECT_FALSE(time.HasWeatherBlend());
}

TEST(WeatherDirector, SameTargetRequestIsNoOp)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Fog, 1.0f);
    StepFrames(director, time, 30);
    float progressBefore = director.GetTransition().progress;
    director.RequestWeather(time, WeatherState::Fog, 1.0f);
    EXPECT_FLOAT_EQ(director.GetTransition().progress, progressBefore);
}

TEST(WeatherDirector, RetargetIsContinuousAtFractionalIntensity)
{
    TimeManager time;
    time.Initialize();
    time.SetTime(0.0f);              // midnight - overrides at full contrast
    time.SetWeatherIntensity(0.5f);  // the case the naive snapshot got wrong
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Thunderstorm, 2.0f);
    StepFrames(director, time, 60);  // mid-transition (~0.5 eased)

    glm::vec3 ambientBefore = time.GetAmbientColor();
    glm::vec3 skyBefore = time.GetSkyColor();
    float starsBefore = time.GetStarVisibility();

    director.RequestWeather(time, WeatherState::Fog, 2.0f);  // retarget, t=0

    EXPECT_NEAR(time.GetAmbientColor().r, ambientBefore.r, 1e-4f);
    EXPECT_NEAR(time.GetAmbientColor().g, ambientBefore.g, 1e-4f);
    EXPECT_NEAR(time.GetAmbientColor().b, ambientBefore.b, 1e-4f);
    EXPECT_NEAR(time.GetSkyColor().r, skyBefore.r, 1e-4f);
    EXPECT_NEAR(time.GetSkyColor().b, skyBefore.b, 1e-4f);
    EXPECT_NEAR(time.GetStarVisibility(), starsBefore, 1e-4f);
}

// Fog -> Clear holds fogAlphaMultiplier through the blend, then decays it after completion.
TEST(WeatherDirector, FogHoldThenDecay)
{
    TimeManager time;
    time.Initialize();
    time.SetWeather(WeatherState::Fog);
    WeatherDirector director;
    director.SetEnabled(true);

    const float fogMul = GetWeatherDefinition(WeatherState::Fog).fogAlphaMultiplier;  // 0.65

    director.RequestWeather(time, WeatherState::Clear, 1.0f);
    StepFrames(director, time, 30);  // mid-transition
    EXPECT_FLOAT_EQ(time.GetEffectiveWeatherDefinition().fogAlphaMultiplier, fogMul);

    StepFrames(director, time, 40);  // transition complete, decay running
    float justAfter = time.GetEffectiveWeatherDefinition().fogAlphaMultiplier;
    EXPECT_LT(justAfter, 1.0f);    // not snapped to Clear's 1.0
    EXPECT_GE(justAfter, fogMul);  // decaying upward from the held value

    StepFrames(director, time, static_cast<int>(19.0f * 60.0f));
    EXPECT_FLOAT_EQ(time.GetEffectiveWeatherDefinition().fogAlphaMultiplier, 1.0f);
    EXPECT_FALSE(time.HasWeatherBlend());
}

// retarget during decay must capture the published fog multiplier to avoid a jump.
TEST(WeatherDirector, RetargetDuringFogDecayKeepsFogContinuity)
{
    TimeManager time;
    time.Initialize();
    time.SetWeather(WeatherState::Fog);
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Clear, 1.0f);
    StepFrames(director, time, 70);   // complete the 1 s transition
    StepFrames(director, time, 300);  // ~5 s into the 18 s decay
    const float decaying = time.GetEffectiveWeatherDefinition().fogAlphaMultiplier;
    ASSERT_GT(decaying, GetWeatherDefinition(WeatherState::Fog).fogAlphaMultiplier);
    ASSERT_LT(decaying, 1.0f);

    director.RequestWeather(time, WeatherState::Blizzard, 1.0f);
    const float afterRetarget = time.GetEffectiveWeatherDefinition().fogAlphaMultiplier;
    EXPECT_NEAR(afterRetarget, decaying, 0.02f) << "fog multiplier snapped on retarget";
}

TEST(WeatherDirector, ResetClearsTransitionAndBlend)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);

    director.RequestWeather(time, WeatherState::Thunderstorm, 10.0f);
    StepFrames(director, time, 10);
    director.Reset(time);

    EXPECT_FALSE(director.IsTransitioning());
    EXPECT_FALSE(time.HasWeatherBlend());

    EXPECT_EQ(time.GetWeather(), WeatherState::Thunderstorm);
}

// the title path publishes wind without updating the director; reset must clear stale gusts.
TEST(WeatherDirector, ResetRestoresCalmWindDefaults)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    director.RequestWeather(time, WeatherState::Blizzard, 0.0f);  // base 1.0
    StepFrames(director, time, 90);  // gust envelope well away from 0.5
    ASSERT_NE(director.GetWindStrength(), 0.5f);

    director.Reset(time);
    EXPECT_FLOAT_EQ(director.GetWindStrength(), 0.5f);
    EXPECT_NEAR(
        glm::dot(director.GetWindDirection(), glm::normalize(ambience::WEATHER_WIND_BASE_DIR)),
        1.0f,
        1e-4f);
}

// wind scales the effective definition by a time-varying, deterministic gust envelope.
TEST(WeatherDirector, WindTracksEffectiveBaseThroughGusts)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);

    EXPECT_NEAR(glm::length(director.GetWindDirection()), 1.0f, 1e-4f);
    EXPECT_FLOAT_EQ(director.GetWindStrength(), 0.5f);

    director.RequestWeather(time, WeatherState::Blizzard, 0.0f);  // windIntensity 1.0
    StepFrames(director, time, 60);
    const float base = GetWeatherDefinition(WeatherState::Blizzard).windIntensity;
    EXPECT_GE(director.GetWindStrength(), base * (1.0f - ambience::WEATHER_GUST_AMP) - 1e-3f);
    EXPECT_LE(director.GetWindStrength(), base * (1.0f + ambience::WEATHER_GUST_AMP) + 1e-3f);

    float s0 = director.GetWindStrength();
    StepFrames(director, time, 120);  // 2 s
    float s1 = director.GetWindStrength();
    StepFrames(director, time, 120);
    float s2 = director.GetWindStrength();
    EXPECT_TRUE(s0 != s1 || s1 != s2) << "gust envelope is flat";
}

TEST(WeatherDirector, WindContinuousAcrossTransition)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    StepFrames(director, time, 30);

    director.RequestWeather(time, WeatherState::Blizzard, 2.0f);  // 0.5 -> 1.0 base
    float prev = director.GetWindStrength();
    for (int i = 0; i < 150; ++i)  // through completion at 120 frames
    {
        director.Update(1.0f / 60.0f, time);
        float cur = director.GetWindStrength();
        EXPECT_LT(std::abs(cur - prev), 0.03f) << "wind stepped at frame " << i;
        prev = cur;
    }
}

// stream pointers must refer to stable member or table storage throughout a transition.
TEST(WeatherDirector, SpawnStreamsExposeTransitionEndpoints)
{
    TimeManager time;
    time.Initialize();
    time.SetWeather(WeatherState::Thunderstorm);
    WeatherDirector director;
    director.SetEnabled(true);

    EXPECT_EQ(director.GetSpawnStreams().outgoing, nullptr);

    director.RequestWeather(time, WeatherState::Blizzard, 1.0f);
    StepFrames(director, time, 30);  // mid-transition
    WeatherDirector::SpawnStreams streams = director.GetSpawnStreams();
    ASSERT_NE(streams.outgoing, nullptr);
    ASSERT_NE(streams.incoming, nullptr);
    EXPECT_EQ(streams.outgoing->particleType, WeatherParticleType::Rain);
    EXPECT_EQ(streams.incoming->particleType, WeatherParticleType::Snow);
    EXPECT_GT(streams.weight, 0.0f);
    EXPECT_LT(streams.weight, 1.0f);

    StepFrames(director, time, 40);  // complete
    EXPECT_EQ(director.GetSpawnStreams().outgoing, nullptr);
}

namespace
{

void StepWithClock(WeatherDirector& d, TimeManager& t, int frames, float dt = 1.0f / 60.0f)
{
    for (int i = 0; i < frames; ++i)
    {
        t.Update(dt);
        d.Update(dt, t);
    }
}
}  // namespace

TEST(WeatherDirector, ForecastChangesWeatherAcrossFrontBoundary)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    // find a seed with a non-Clear day-6 front so the test survives pool-weight changes.
    uint64_t seed = 1;
    for (; seed < 200; ++seed)
    {
        ForecastEntry e = ForecastForDay(seed, 6);
        if (e.front != WeatherState::Clear && !ForecastForDay(seed, 6).hasNightEvent &&
            !ForecastForDay(seed, 5).hasNightEvent)
        {
            break;
        }
    }
    ASSERT_LT(seed, 200u) << "no suitable seed found (pool weights changed?)";
    director.SetForecastSeed(seed);

    time.SetTime(12.0f);
    time.AdvanceTime(24.0f * 6.0f);
    director.Update(1.0f / 60.0f, time);
    const WeatherState want = ForecastForDay(seed, 6).front;
    EXPECT_EQ(time.GetWeather(), want) << "reconciliation didn't fire";
    EXPECT_TRUE(director.IsTransitioning());
}

// manual weather holds through the current front; the next front resumes the forecast.
TEST(WeatherDirector, ManualOverrideHoldsUntilNextFront)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    director.SetForecastSeed(7);

    time.SetTime(12.0f);
    director.Update(1.0f / 60.0f, time);  // settle day 0 (boot front: Clear)

    director.RequestWeather(time, WeatherState::Sandstorm, 0.0f);  // manual, instant
    StepWithClock(director, time, 60);                             // ~1 game hour
    EXPECT_EQ(time.GetWeather(), WeatherState::Sandstorm) << "forecast stomped the manual hold";

    time.AdvanceTime(24.0f * 7.0f);
    director.Update(1.0f / 60.0f, time);
    director.Update(1.0f / 60.0f, time);
    EXPECT_EQ(time.GetWeather(), ForecastForDay(7, time.GetDayCount()).front)
        << "hold never expired (forecast did not resume)";
}

TEST(WeatherDirector, AutoWeatherToggle)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    director.SetForecastSeed(7);
    director.SetAutoWeather(false);

    time.SetTime(12.0f);
    time.AdvanceTime(24.0f * 10.0f);
    StepWithClock(director, time, 30);
    EXPECT_EQ(time.GetWeather(), WeatherState::Clear) << "auto=off must not reconcile";

    director.SetAutoWeather(true);
    director.Update(1.0f / 60.0f, time);
    // compare with the forecast even if day 10 is Clear, so unchanged weather does not hide a
    // missed update.
    const int64_t day = 10;
    EXPECT_EQ(time.GetWeather(), ForecastForDay(7, day).front);
}

// night events engage at 20:00 and yield to the front by 05:00.
TEST(WeatherDirector, EventNightEngagesAndReleases)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    uint64_t seed = 1;
    int64_t day = -1;
    for (; seed < 400 && day < 0; ++seed)
    {
        for (int64_t d = 1; d < 12; ++d)
        {
            if (ForecastForDay(seed, d).hasNightEvent)
            {
                day = d;
                break;
            }
        }
        if (day >= 0)
        {
            break;
        }
    }
    ASSERT_GE(day, 0) << "no event night in seed sweep";
    director.SetForecastSeed(seed);

    const ForecastEntry e = ForecastForDay(seed, day);
    time.SetTime(19.0f);
    time.AdvanceTime(24.0f * static_cast<float>(day));
    director.Update(1.0f / 60.0f, time);  // settle pre-event front

    while (time.GetTimeOfDay() < 20.5f && time.GetTimeOfDay() >= 5.0f)
    {
        time.Update(1.0f / 60.0f);
        director.Update(1.0f / 60.0f, time);
    }
    EXPECT_EQ(time.GetWeather(), e.nightEvent) << "event night never engaged";

    while (time.GetTimeOfDay() < 6.0f || time.GetTimeOfDay() >= 20.0f)
    {
        time.Update(1.0f / 60.0f);
        director.Update(1.0f / 60.0f, time);
    }
    EXPECT_NE(time.GetWeather(), e.nightEvent) << "event never released at dawn";
}

// session-constant gust phases keep wind continuous across day rollover.
TEST(WeatherDirector, GustsContinuousAcrossDayRollover)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    director.SetAutoWeather(false);  // isolate gusts from forecast changes

    time.SetTime(23.9f);  // just before midnight
    float prev = -1.0f;
    for (int i = 0; i < 240; ++i)  // 4 s real = 4 game hours, crosses midnight
    {
        time.Update(1.0f / 60.0f);
        director.Update(1.0f / 60.0f, time);
        float cur = director.GetWindStrength();
        if (prev >= 0.0f)
        {
            EXPECT_LT(std::abs(cur - prev), 0.03f) << "gust stepped at frame " << i;
        }
        prev = cur;
    }
}

TEST(WeatherDirector, GetTransitionFreshAfterHardCut)
{
    TimeManager time;
    time.Initialize();
    WeatherDirector director;
    director.SetEnabled(true);
    director.SetAutoWeather(false);

    director.RequestWeather(time, WeatherState::Fog, 5.0f);       // transition
    director.RequestWeather(time, WeatherState::Blizzard, 0.0f);  // hard cut
    WeatherDirector::Transition t = director.GetTransition();
    EXPECT_FALSE(t.active);
    EXPECT_EQ(t.from, WeatherState::Blizzard);
    EXPECT_EQ(t.to, WeatherState::Blizzard);
}
