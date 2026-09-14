#include <gtest/gtest.h>

#include "../src/ParticleSystem.hpp"
#include "../src/WeatherDefinitions.hpp"

#include <glm/glm.hpp>

// particle checks cover shared weather caps, wind response, and simultaneous transition streams.
namespace
{
int CountType(const ParticleSystem& ps, ParticleType type)
{
    int n = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == type && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            ++n;
        }
    }
    return n;
}
}  // namespace

TEST(ParticleWind, HoistedCapStillBindsExactly)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Fog), 1.0f);

    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
        EXPECT_LE(CountType(ps, ParticleType::Fog),
                  GetWeatherDefinition(WeatherState::Fog).maxWeatherParticles);
    }
    // allow 5% below the cap as particles expire, but never above it.
    EXPECT_GE(CountType(ps, ParticleType::Fog),
              static_cast<int>(GetWeatherDefinition(WeatherState::Fog).maxWeatherParticles * 0.95));
}

namespace
{

struct VelocityRange
{
    float minX{1e9f}, maxX{-1e9f}, minY{1e9f}, maxY{-1e9f};
};

VelocityRange SpawnSnowBurst(float windStrength, glm::vec2 windDir)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind(windDir, windStrength);
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Blizzard), 1.0f);
    ps.Update(0.5f, {0.0f, 0.0f}, {640.0f, 480.0f});

    VelocityRange r;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type != ParticleType::Snow || p.zoneIndex != ParticleSystem::WEATHER_ZONE_INDEX)
        {
            continue;
        }
        r.minX = std::min(r.minX, p.velocity.x);
        r.maxX = std::max(r.maxX, p.velocity.x);
        r.minY = std::min(r.minY, p.velocity.y);
        r.maxY = std::max(r.maxY, p.velocity.y);
    }
    return r;
}
}  // namespace

// at full wind, Snow velocity scales base X in +/-6 by 7 and Y in 12-22 by 3.5;
// X follows the wind sign.
TEST(ParticleWind, SnowBoostMatchesBlizzardAtFullStrength)
{
    VelocityRange r = SpawnSnowBurst(1.0f, {-1.0f, 0.0f});
    EXPECT_LT(r.maxX, 0.0f) << "wind blows -X: all flakes must drift left";
    EXPECT_GE(r.minX, -6.0f * 7.0f - 1e-3f);
    EXPECT_GE(r.minY, 12.0f * 3.5f - 1e-3f);
    EXPECT_LE(r.maxY, 22.0f * 3.5f + 1e-3f);
}

TEST(ParticleWind, SnowBoostHasNoCliff)
{
    VelocityRange below = SpawnSnowBurst(0.69f, {-1.0f, 0.0f});
    VelocityRange above = SpawnSnowBurst(0.71f, {-1.0f, 0.0f});

    EXPECT_LT(std::abs(above.minX - below.minX), std::abs(below.minX) * 0.15f);
    EXPECT_LT(std::abs(above.maxY - below.maxY), below.maxY * 0.15f);
}

// wind below 0.3 keeps the spawn-velocity multiplier at one.
TEST(ParticleWind, SnowCalmWindIsUnboosted)
{
    VelocityRange r = SpawnSnowBurst(0.2f, {-1.0f, 0.0f});
    EXPECT_GE(r.minX, -6.0f - 1e-3f);
    EXPECT_LE(r.maxX, 6.0f + 1e-3f);
    EXPECT_LE(r.maxY, 22.0f + 1e-3f);
}

TEST(ParticleWind, SnowFollowsWindSign)
{
    VelocityRange r = SpawnSnowBurst(1.0f, {1.0f, 0.0f});
    EXPECT_GT(r.minX, 0.0f);
}

namespace
{

float DriftDistanceX(ParticleType type, float windStrength, float seconds)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, windStrength);
    // track one live particle because pool removal can reorder entries.
    ps.SetMaxParticlesPerZone(1);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    std::vector<ParticleZone> zones;
    zones.emplace_back(cameraPos, viewSize, type);
    ps.SetZones(&zones);

    float startX = 0.0f;
    bool found = false;
    for (int i = 0; i < 100 && !found; ++i)  // up to 5 s of spawn attempts
    {
        ps.Update(0.05f, cameraPos, viewSize);
        for (const auto& p : ps.GetParticles())
        {
            if (p.type == type)
            {
                startX = p.position.x;
                found = true;
                break;
            }
        }
    }
    if (!found)
    {
        return -1.0f;  // caller asserts >= 0 as the vacuity guard
    }
    for (int i = 0; i < static_cast<int>(seconds / 0.05f); ++i)
    {
        ps.Update(0.05f, cameraPos, viewSize);
    }
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == type)
        {
            return std::abs(p.position.x - startX);
        }
    }
    return -1.0f;
}
}  // namespace

// at wind strength 0.5, leaf and pollen drift are 18 and 8 px/s.
TEST(ParticleWind, LeafAndPollenDriftScaleWithStrength)
{
    const float leafCalm = DriftDistanceX(ParticleType::DriftingLeaf, 0.5f, 1.0f);
    const float leafGust = DriftDistanceX(ParticleType::DriftingLeaf, 1.0f, 1.0f);
    ASSERT_GE(leafCalm, 0.0f) << "test vacuous: no leaf spawned";
    ASSERT_GE(leafGust, 0.0f);
    // 18 px/s baseline (with a little slack for the sine sway on Y only - X is pure wind).
    EXPECT_NEAR(leafCalm, 18.0f, 2.5f);
    EXPECT_GT(leafGust, leafCalm * 1.25f) << "gust must visibly push leaves";

    const float pollenCalm = DriftDistanceX(ParticleType::Pollen, 0.5f, 1.0f);
    const float pollenGust = DriftDistanceX(ParticleType::Pollen, 1.0f, 1.0f);
    ASSERT_GE(pollenCalm, 0.0f) << "test vacuous: no pollen spawned";
    EXPECT_NEAR(pollenCalm, 8.0f, 1.5f);
    EXPECT_GT(pollenGust, pollenCalm * 1.25f);
}

// sand speed scales around strength 0.5; its spawn edge requires positive X drift.
TEST(ParticleWind, SandSpeedScalesWithStrength)
{
    ParticleSystem psCalm;
    psCalm.SetTimeOfDay(12.0f);
    psCalm.SetNightFactor(0.0f);
    psCalm.SetWind({-1.0f, 0.0f}, 0.5f);
    psCalm.SetWeatherState(&GetWeatherDefinition(WeatherState::Sandstorm), 1.0f);
    psCalm.Update(0.5f, {0.0f, 0.0f}, {640.0f, 480.0f});

    ParticleSystem psGust;
    psGust.SetTimeOfDay(12.0f);
    psGust.SetNightFactor(0.0f);
    psGust.SetWind({-1.0f, 0.0f}, 1.0f);
    psGust.SetWeatherState(&GetWeatherDefinition(WeatherState::Sandstorm), 1.0f);
    psGust.Update(0.5f, {0.0f, 0.0f}, {640.0f, 480.0f});

    auto maxSandVx = [](const ParticleSystem& ps)
    {
        float m = -1.0f;
        for (const auto& p : ps.GetParticles())
        {
            if (p.type == ParticleType::Sand)
            {
                m = std::max(m, p.velocity.x);
            }
        }
        return m;
    };
    const float calm = maxSandVx(psCalm);
    const float gust = maxSandVx(psGust);
    ASSERT_GT(calm, 0.0f) << "test vacuous: no sand spawned";
    EXPECT_LE(calm, 200.0f + 1e-3f);  // spawn range at wind strength 0.5
    EXPECT_GT(gust, calm * 1.2f);
    EXPECT_GT(maxSandVx(psGust), 0.0f) << "sand keeps +X axis";
}

// both endpoint particle types coexist during a weather transition.
TEST(ParticleWind, TransitionCrossFadesTypes)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 0.5f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    const WeatherDefinition& storm = GetWeatherDefinition(WeatherState::Thunderstorm);
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);

    ps.SetWeatherState(&storm, 1.0f);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    ASSERT_GT(CountType(ps, ParticleType::Rain), 0) << "test vacuous: no rain";

    // early transition (w = 0.15): rain still dominant, snow appearing.
    ps.SetWeatherState(&blizzard, 1.0f);  // effective def reports destination
    ps.SetWeatherTransition(&storm, &blizzard, 0.15f);
    for (int i = 0; i < 30; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    const int rainEarly = CountType(ps, ParticleType::Rain);
    const int snowEarly = CountType(ps, ParticleType::Snow);
    EXPECT_GT(rainEarly, 0) << "outgoing stream stopped spawning too early";
    EXPECT_GT(snowEarly, 0) << "incoming stream not spawning";

    ps.SetWeatherTransition(&storm, &blizzard, 0.9f);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountType(ps, ParticleType::Snow), CountType(ps, ParticleType::Rain));

    ps.SetWeatherTransition(nullptr, nullptr, 0.0f);
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_EQ(CountType(ps, ParticleType::Rain), 0) << "rain never fully decayed";
    EXPECT_GT(CountType(ps, ParticleType::Snow), 0);
}

// use disjoint synthetic size ranges to identify each stream's definition
// reliably even when Wisp spawning yields few samples.
TEST(ParticleWind, OutgoingStreamUsesOwnSizeScale)
{
    ParticleSystem psTransition;
    psTransition.SetTimeOfDay(12.0f);  // Aurora streams are time-of-day independent.
    psTransition.SetNightFactor(0.0f);
    psTransition.SetWind({-1.0f, 0.0f}, 0.5f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    WeatherDefinition aurora = GetWeatherDefinition(WeatherState::Aurora);
    const WeatherDefinition& clear = GetWeatherDefinition(WeatherState::Clear);
    aurora.particleSizeScale = 0.25f;
    aurora.secondaryBaseSpawnRate = 20.0f;

    // outgoing Wisp size [3, 5] scales to at most 1.25. using Clear's scale
    // would produce sizes of at least 3.
    psTransition.SetWeatherState(&clear, 1.0f);
    psTransition.SetWeatherTransition(&aurora, &clear, 0.1f);
    psTransition.Update(1.0f, cameraPos, viewSize);
    float maxSize = 0.0f;
    for (const auto& p : psTransition.GetParticles())
    {
        if (p.type == ParticleType::Wisp && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            maxSize = std::max(maxSize, p.size);
        }
    }
    ASSERT_GT(maxSize, 0.0f) << "test vacuous: outgoing stream spawned no wisps";
    EXPECT_LE(maxSize, 5.0f * aurora.particleSizeScale + 1e-4f)
        << "outgoing stream used the wrong def's size scale";
}

// Fog -> Blizzard shares the smaller Fog cap of 2500 across both streams.
TEST(ParticleWind, DualStreamSharedTypeCapFloors)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 0.5f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    const WeatherDefinition& fog = GetWeatherDefinition(WeatherState::Fog);
    const WeatherDefinition& blizzard = GetWeatherDefinition(WeatherState::Blizzard);

    ps.SetWeatherState(&fog, 1.0f);
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
    }
    ASSERT_GT(CountType(ps, ParticleType::Fog), 2000) << "test vacuous: fog not cap-bound";

    ps.SetWeatherState(&blizzard, 1.0f);
    ps.SetWeatherTransition(&fog, &blizzard, 0.5f);
    for (int i = 0; i < 100; ++i)
    {
        ps.Update(0.2f, cameraPos, viewSize);
        EXPECT_LE(CountType(ps, ParticleType::Fog), 2500) << "shared cap breached at frame " << i;
    }
}

namespace
{

int CountTypeInBottomHalf(const ParticleSystem& ps,
                          ParticleType type,
                          glm::vec2 cameraPos,
                          glm::vec2 viewSize)
{
    int n = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == type && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX &&
            p.position.y > cameraPos.y + viewSize.y * 0.5f &&
            p.position.y < cameraPos.y + viewSize.y)
        {
            ++n;
        }
    }
    return n;
}
}  // namespace

// weather must fill newly exposed viewport space and move its impact band with the camera.
TEST(ParticleWind, FallingWeatherSurvivesDownwardCameraSprint)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 1.0f);  // full Blizzard boost: fall 42-77 px/s

    glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Blizzard), 1.0f);

    for (int i = 0; i < 100; ++i)
    {
        ps.Update(0.05f, cameraPos, viewSize);
    }
    ASSERT_GT(CountTypeInBottomHalf(ps, ParticleType::Snow, cameraPos, viewSize), 0)
        << "test vacuous: no snow at rest";

    // sprint downward at 150 px/s (faster than boosted snowfall) for 5 s.
    for (int i = 0; i < 100; ++i)
    {
        cameraPos.y += 150.0f * 0.05f;
        ps.Update(0.05f, cameraPos, viewSize);
    }

    EXPECT_GT(CountTypeInBottomHalf(ps, ParticleType::Snow, cameraPos, viewSize), 20)
        << "bottom half starved: weather outrun by the camera";

    // each ground line must remain inside the moving impact band.
    const float bandFloor = cameraPos.y + viewSize.y * 0.10f - 1.0f;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Snow && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX &&
            p.bakedGroundY > 0.0f)
        {
            EXPECT_GE(p.bakedGroundY, bandFloor) << "splash band lagged the camera";
        }
    }
}

TEST(ParticleWind, RainSurvivesDownwardCameraSprint)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 0.5f);

    glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Thunderstorm), 1.0f);

    for (int i = 0; i < 100; ++i)
    {
        ps.Update(0.05f, cameraPos, viewSize);
    }
    ASSERT_GT(CountTypeInBottomHalf(ps, ParticleType::Rain, cameraPos, viewSize), 0)
        << "test vacuous: no rain at rest";

    for (int i = 0; i < 100; ++i)
    {
        cameraPos.y += 300.0f * 0.05f;  // rain falls faster; sprint harder
        ps.Update(0.05f, cameraPos, viewSize);
    }
    EXPECT_GT(CountTypeInBottomHalf(ps, ParticleType::Rain, cameraPos, viewSize), 20)
        << "bottom half starved: rain outrun by the camera";

    // check rain's ground line directly; its short lifetime can hide a stale band in count checks.
    const float bandFloor = cameraPos.y + viewSize.y * 0.10f - 1.0f;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Rain && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX &&
            p.bakedGroundY > 0.0f)
        {
            EXPECT_GE(p.bakedGroundY, bandFloor) << "rain splash band lagged the camera";
        }
    }
}

// with a static camera, flakes stop at the impact band and cannot accumulate below it.
TEST(ParticleWind, StaticCameraGroundImpactsStillFire)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 1.0f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Blizzard), 1.0f);

    for (int i = 0; i < 400; ++i)
    {
        ps.Update(0.05f, cameraPos, viewSize);
    }
    // spawning follows the per-particle band check. one zero-dt update checks the
    // last pre-warmed particles without moving them or spawning another interval.
    ps.Update(0.0f, cameraPos, viewSize);
    int belowBand = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Snow && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX &&
            p.position.y > cameraPos.y + viewSize.y * 1.05f + 30.0f)
        {
            ++belowBand;
        }
    }
    EXPECT_EQ(belowBand, 0) << "snow fell through the ground band";
}

// pre-aging must populate the fall column immediately.
TEST(ParticleWind, AshPreAgesAcrossTheColumn)
{
    ParticleSystem ps;
    ps.SetTimeOfDay(12.0f);
    ps.SetNightFactor(0.0f);
    ps.SetWind({-1.0f, 0.0f}, 0.5f);

    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::AshFall), 1.0f);
    ps.Update(1.0f, cameraPos, viewSize);  // one burst of spawns

    // the spawn strip occupies the top ~10% of the overspray rectangle; check below it.
    int belowStrip = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type == ParticleType::Ash && p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX &&
            p.position.y > cameraPos.y + viewSize.y * 0.3f)
        {
            ++belowStrip;
        }
    }
    EXPECT_GT(belowStrip, 0) << "ash not pre-aged: column fills only at fall speed";
}
