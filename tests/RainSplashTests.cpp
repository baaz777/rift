#include <gtest/gtest.h>

#include "../src/ParticleSystem.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

// impact tests drive the real particle update path without loading a renderer or sprite atlas.
namespace
{
int CountOfType(const ParticleSystem& ps, ParticleType type)
{
    int count = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == type)
        {
            ++count;
        }
    }
    return count;
}

void PlaceRainZone(ParticleSystem& ps, std::vector<ParticleZone>& zones)
{
    zones.emplace_back(glm::vec2(100.0f, 100.0f), glm::vec2(64.0f, 64.0f), ParticleType::Rain);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);  // deep night: global ambient spawning is gated off.
}

void PlaceSnowZone(ParticleSystem& ps, std::vector<ParticleZone>& zones)
{
    zones.emplace_back(glm::vec2(100.0f, 100.0f), glm::vec2(64.0f, 64.0f), ParticleType::Snow);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
}

constexpr glm::vec2 kCameraPos{0.0f, 0.0f};
constexpr glm::vec2 kViewSize{640.0f, 480.0f};
}  // namespace

// impact spawning has a ~30% random throttle; observe many impacts over 20 s.
TEST(RainSplash, EditorRainZoneProducesSplashes)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    PlaceRainZone(ps, zones);

    bool sawSplash = false;
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.1f, kCameraPos, kViewSize);
        if (CountOfType(ps, ParticleType::RainSplash) > 0)
        {
            sawSplash = true;
        }
    }
    EXPECT_TRUE(sawSplash);
}

TEST(RainSplash, RainImpactsNoLongerEmitSparkles)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    PlaceRainZone(ps, zones);

    int maxSparkles = 0;
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.1f, kCameraPos, kViewSize);
        maxSparkles = std::max(maxSparkles, CountOfType(ps, ParticleType::Sparkles));
    }
    EXPECT_EQ(maxSparkles, 0);
}

TEST(RainSplash, SpawnOneAppendsFiniteSplash)
{
    ParticleSystem ps;
    ps.SpawnOne(ParticleType::RainSplash, glm::vec2(320.0f, 240.0f));

    ASSERT_EQ(CountOfType(ps, ParticleType::RainSplash), 1);
    const Particle& p = ps.GetParticles().front();
    EXPECT_TRUE(std::isfinite(p.position.x) && std::isfinite(p.position.y));
    EXPECT_GT(p.size, 0.0f);
    EXPECT_GT(p.lifetime, 0.0f);
    EXPECT_FALSE(p.additive);
}

TEST(RainSplash, SplashFadesAndRecycles)
{
    ParticleSystem ps;
    ps.SpawnOne(ParticleType::RainSplash, glm::vec2(320.0f, 240.0f));
    ASSERT_EQ(CountOfType(ps, ParticleType::RainSplash), 1);

    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.05f, kCameraPos, kViewSize);
        for (const auto& p : ps.GetParticles())
        {
            if (p.type == ParticleType::RainSplash)
            {
                EXPECT_TRUE(std::isfinite(p.color.a));
                EXPECT_GE(p.color.a, 0.0f);
                EXPECT_LE(p.color.a, 1.0f);
            }
        }
    }
    // 3 s exceeds the splash lifetime of 0.30-0.40 s.
    EXPECT_EQ(CountOfType(ps, ParticleType::RainSplash), 0);
}

TEST(SnowSplash, EditorSnowZoneProducesImpacts)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    PlaceSnowZone(ps, zones);

    bool sawSplash = false;
    for (int i = 0; i < 300; ++i)
    {
        ps.Update(0.1f, kCameraPos, kViewSize);
        if (CountOfType(ps, ParticleType::SnowSplash) > 0)
        {
            sawSplash = true;
        }
    }
    EXPECT_TRUE(sawSplash);
}

TEST(SnowSplash, SnowImpactsNoLongerEmitSparkles)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    PlaceSnowZone(ps, zones);

    int maxSparkles = 0;
    for (int i = 0; i < 300; ++i)
    {
        ps.Update(0.1f, kCameraPos, kViewSize);
        maxSparkles = std::max(maxSparkles, CountOfType(ps, ParticleType::Sparkles));
    }
    EXPECT_EQ(maxSparkles, 0);
}

TEST(SnowSplash, SpawnOneAppendsFiniteImpact)
{
    ParticleSystem ps;
    ps.SpawnOne(ParticleType::SnowSplash, glm::vec2(320.0f, 240.0f));

    ASSERT_EQ(CountOfType(ps, ParticleType::SnowSplash), 1);
    const Particle& p = ps.GetParticles().front();
    EXPECT_TRUE(std::isfinite(p.position.x) && std::isfinite(p.position.y));
    EXPECT_GT(p.size, 0.0f);
    EXPECT_GT(p.lifetime, 0.0f);
    EXPECT_FALSE(p.additive);
}

// impact sprites dim with night factor so white artwork does not glare.
TEST(SplashVisibility, ImpactAlphaIsDimmerAtNight)
{
    auto peakAlpha = [](ParticleType type, float nightFactor)
    {
        ParticleSystem ps;
        ps.SetSceneNightFactor(nightFactor);
        ps.SpawnOne(type, glm::vec2(320.0f, 240.0f));
        ps.Update(0.01f, kCameraPos, kViewSize);  // one tick: Update sets alpha from scene darkness
        float a = 0.0f;
        for (const auto& p : ps.GetParticles())
        {
            if (p.type == type)
            {
                a = std::max(a, p.color.a);
            }
        }
        return a;
    };
    for (const ParticleType t : {ParticleType::RainSplash, ParticleType::SnowSplash})
    {
        const float dayAlpha = peakAlpha(t, 0.0f);
        const float nightAlpha = peakAlpha(t, 1.0f);
        EXPECT_GT(dayAlpha, 0.0f);
        EXPECT_LT(nightAlpha, dayAlpha);
    }
}

// pending splashes render before their first update. spawn alpha must already
// include the night factor to prevent a one-frame flash.
TEST(SplashVisibility, NoBrightSpawnFrameFlashAtNight)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2(100.0f, 100.0f), glm::vec2(64.0f, 64.0f), ParticleType::Rain);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    ps.SetSceneNightFactor(1.0f);  // deep night: splashes should stay very faint

    float maxSplashAlpha = 0.0f;
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.1f, kCameraPos, kViewSize);
        for (const auto& p : ps.GetParticles())
        {
            if (p.type == ParticleType::RainSplash)
            {
                maxSplashAlpha = std::max(maxSplashAlpha, p.color.a);
            }
        }
    }
    EXPECT_GT(maxSplashAlpha, 0.0f);   // splashes did appear
    EXPECT_LT(maxSplashAlpha, 0.25f);  // ~0.15 at steady state
}
