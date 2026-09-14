#include <gtest/gtest.h>

#include "../src/AmbienceConfig.hpp"
#include "../src/WeatherBlend.hpp"
#include "../src/WeatherDefinitions.hpp"

#include <set>

namespace
{
// keep this seed: it covers the distribution bounds and the truncated first
// front. arbitrary seeds can fail the lower bound for that partial run.
constexpr uint64_t kSeed = 0x51F7C0DEULL;
}  // namespace

TEST(Forecast, Deterministic)
{
    for (int64_t d = -10; d < 50; ++d)
    {
        ForecastEntry a = ForecastForDay(kSeed, d);
        ForecastEntry b = ForecastForDay(kSeed, d);
        EXPECT_EQ(a.front, b.front);
        EXPECT_EQ(a.hasNightEvent, b.hasNightEvent);
        EXPECT_EQ(a.nightEvent, b.nightEvent);
    }

    bool diverged = false;
    for (int64_t d = 0; d < 50 && !diverged; ++d)
    {
        diverged = ForecastForDay(kSeed, d).front != ForecastForDay(kSeed + 1, d).front;
    }
    EXPECT_TRUE(diverged);
}

// each day belongs to one front; front indices advance by one and full runs last 2-6 days.
TEST(Forecast, FrontPartitionIsTotalAndBounded)
{
    int64_t prev = ForecastFrontIndex(kSeed, -500);
    int runLength = 1;
    bool firstRun = true;  // the scan starts mid-front: the first observed
                           // run is window-truncated, exempt from the lower bound
    for (int64_t d = -499; d < 500; ++d)
    {
        int64_t cur = ForecastFrontIndex(kSeed, d);
        ASSERT_TRUE(cur == prev || cur == prev + 1) << "gap/overlap at day " << d;
        if (cur == prev)
        {
            ++runLength;
        }
        else
        {
            if (!firstRun)
            {
                EXPECT_GE(runLength, ambience::WEATHER_FRONT_LENGTH_DAYS - 2)
                    << "short front at " << d;
            }
            EXPECT_LE(runLength, ambience::WEATHER_FRONT_LENGTH_DAYS + 2) << "long front at " << d;
            firstRun = false;
            runLength = 1;
        }
        prev = cur;
    }
}

TEST(Forecast, FrontWeatherConstantAndBootClear)
{
    EXPECT_EQ(ForecastForDay(kSeed, 0).front, WeatherState::Clear);
    for (int64_t d = 0; d < 300; ++d)
    {
        if (ForecastFrontIndex(kSeed, d) == ForecastFrontIndex(kSeed, d + 1))
        {
            EXPECT_EQ(ForecastForDay(kSeed, d).front, ForecastForDay(kSeed, d + 1).front)
                << "front weather changed mid-front at day " << d;
        }
    }
}

// forecasts exclude console-only states; night events use a separate pool.
TEST(Forecast, PoolMembership)
{
    const std::set<WeatherState> frontPool = {WeatherState::Clear,
                                              WeatherState::LightRain,
                                              WeatherState::HeavyRain,
                                              WeatherState::Thunderstorm,
                                              WeatherState::Blizzard,
                                              WeatherState::Fog,
                                              WeatherState::Sandstorm,
                                              WeatherState::FallingLeaves,
                                              WeatherState::CherryBlossoms,
                                              WeatherState::PollenStorm};
    const std::set<WeatherState> eventPool = {
        WeatherState::Aurora, WeatherState::MeteorShower, WeatherState::FireflySwarm};

    for (int64_t d = 0; d < 2000; ++d)
    {
        ForecastEntry e = ForecastForDay(kSeed, d);
        EXPECT_TRUE(frontPool.count(e.front)) << "day " << d << " front outside pool";
        if (e.hasNightEvent)
        {
            EXPECT_TRUE(eventPool.count(e.nightEvent)) << "day " << d << " bad event";
        }
    }
}

// loose bounds over 4000 days check the weighted pool without fixing an exact sequence.
TEST(Forecast, DistributionSanity)
{
    int clearDays = 0;
    int eventNights = 0;
    for (int64_t d = 0; d < 4000; ++d)
    {
        ForecastEntry e = ForecastForDay(kSeed, d);
        clearDays += (e.front == WeatherState::Clear) ? 1 : 0;
        eventNights += e.hasNightEvent ? 1 : 0;
    }
    EXPECT_GT(clearDays, 4000 / 5) << "Clear should be the heaviest front";
    EXPECT_GT(eventNights, static_cast<int>(4000 * ambience::WEATHER_EVENT_NIGHT_CHANCE * 0.6f));
    EXPECT_LT(eventNights, static_cast<int>(4000 * ambience::WEATHER_EVENT_NIGHT_CHANCE * 1.5f));
}

// moon phases 4 and 0 favor aurora/meteors and fireflies, respectively.
TEST(Forecast, MoonPhaseWeighting)
{
    int auroraFull = 0;
    int auroraNew = 0;
    for (int64_t d = 0; d < 16000; ++d)
    {
        ForecastEntry e = ForecastForDay(kSeed, d);
        if (!e.hasNightEvent || e.nightEvent != WeatherState::Aurora)
        {
            continue;
        }
        const int phase = static_cast<int>(((d % 8) + 8) % 8);
        auroraFull += (phase >= 3 && phase <= 5) ? 1 : 0;  // around full
        auroraNew += (phase <= 1 || phase == 7) ? 1 : 0;   // around new
    }
    EXPECT_GT(auroraFull, auroraNew) << "aurora should favor full-ish moons";
}
