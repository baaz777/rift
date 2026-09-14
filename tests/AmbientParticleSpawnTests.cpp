#include <gtest/gtest.h>

#include "../src/AmbienceConfig.hpp"
#include "../src/ParticleSystem.hpp"
#include "../src/WeatherDefinitions.hpp"

#include <glm/glm.hpp>

#include <cmath>

class AmbientParticleSpawnTest : public ::testing::Test
{
protected:
    ParticleSystem ps;
    glm::vec2 cameraPos{0.0f, 0.0f};
    glm::vec2 viewSize{640.0f, 480.0f};

    int CountAmbient() const
    {
        int count = 0;
        for (const auto& p : ps.GetParticles())
        {
            switch (p.type)
            {
                case ParticleType::DriftingLeaf:
                case ParticleType::DustMote:
                case ParticleType::Pollen:
                    ++count;
                    break;
                default:
                    break;
            }
        }
        return count;
    }

    int CountOfType(ParticleType type) const
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
};

TEST_F(AmbientParticleSpawnTest, RespectsTotalCapAtPeakBias)
{
    ps.SetTimeOfDay(12.0f);  // peak leaf and dust bias.
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.5f, cameraPos, viewSize);
        EXPECT_LE(CountAmbient(), ambience::AMBIENT_PARTICLE_TOTAL_CAP);
    }
}

TEST_F(AmbientParticleSpawnTest, NoAmbientSpawnAtDeepNight)
{
    ps.SetTimeOfDay(2.0f);  // all three biases are zero at this hour.
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.5f, cameraPos, viewSize);
    }
    EXPECT_EQ(CountAmbient(), 0);
}

TEST_F(AmbientParticleSpawnTest, PollenAbsentAtMidday)
{
    // midday: leaves and dust spawn, but pollen's golden-hour bias is zero.
    ps.SetTimeOfDay(12.0f);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_EQ(CountOfType(ParticleType::Pollen), 0);
    EXPECT_GT(CountOfType(ParticleType::DustMote), 0);
}

TEST_F(AmbientParticleSpawnTest, PollenSpawnsAtDawnGoldenHour)
{
    ps.SetTimeOfDay(6.5f);  // peak pollen bias at dawn golden hour.
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::Pollen), 0);
}

TEST_F(AmbientParticleSpawnTest, PollenSpawnsAtDuskGoldenHour)
{
    ps.SetTimeOfDay(19.0f);  // peak pollen bias at dusk golden hour.
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::Pollen), 0);
}

TEST_F(AmbientParticleSpawnTest, AmbientParticlesDieAndRecycle)
{
    ps.SetTimeOfDay(12.0f);
    // advance past the 15 s leaf lifetime to exercise pool recycling.
    for (int i = 0; i < 200; ++i)
    {
        ps.Update(0.5f, cameraPos, viewSize);
    }
    int count = static_cast<int>(ps.GetParticles().size());
    // zones not set, so all particles come through the global ambient cap.
    EXPECT_LE(count, ambience::AMBIENT_PARTICLE_TOTAL_CAP);
}

TEST_F(AmbientParticleSpawnTest, BlizzardSpawnsBothSnowAndFog)
{
    // 02:00 suppresses ambient spawning, so the pool contains only weather particles.
    ps.SetTimeOfDay(2.0f);
    ps.SetWeatherState(&GetWeatherDefinition(WeatherState::Blizzard), 1.0f);
    for (int i = 0; i < 240; ++i)
    {
        ps.Update(1.0f / 60.0f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::Snow), 0);
    EXPECT_GT(CountOfType(ParticleType::Fog), 0);
}

// placed zones spawn independently of the ambient cap and time-of-day bias.

class AmbientParticleZoneTest : public AmbientParticleSpawnTest
{
protected:
    std::vector<ParticleZone> zones;

    void PlaceZone(ParticleType type)
    {
        // position inside the camera rect so the visibility check passes.
        ParticleZone z(glm::vec2(100.0f, 100.0f), glm::vec2(64.0f, 64.0f), type);
        zones.push_back(z);
        ps.SetZones(&zones);
    }
};

TEST_F(AmbientParticleZoneTest, DriftingLeafZoneSpawnsLeaves)
{
    ps.SetTimeOfDay(2.0f);  // deep night: global ambient gating zero.
    PlaceZone(ParticleType::DriftingLeaf);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::DriftingLeaf), 0);
}

TEST_F(AmbientParticleZoneTest, DustMoteZoneSpawnsMotes)
{
    ps.SetTimeOfDay(2.0f);
    PlaceZone(ParticleType::DustMote);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::DustMote), 0);
}

TEST_F(AmbientParticleZoneTest, PollenZoneSpawnsOutsideGoldenHour)
{
    // midday: global pollen bias is zero, but a placed zone must still spawn.
    ps.SetTimeOfDay(12.0f);
    PlaceZone(ParticleType::Pollen);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.1f, cameraPos, viewSize);
    }
    EXPECT_GT(CountOfType(ParticleType::Pollen), 0);
}

TEST_F(AmbientParticleZoneTest, ZoneSpawnedParticlesAreInsideZoneBounds)
{
    ps.SetTimeOfDay(2.0f);
    PlaceZone(ParticleType::DriftingLeaf);
    // the spawn interval is 1 / 2.5 = 0.4 s. spawning follows the update pass,
    // so this single spawn has not moved yet.
    ps.Update(0.5f, cameraPos, viewSize);
    ASSERT_GT(CountOfType(ParticleType::DriftingLeaf), 0);
    const auto& z = zones.front();
    for (const auto& p : ps.GetParticles())
    {
        if (p.type != ParticleType::DriftingLeaf)
        {
            continue;
        }
        EXPECT_GE(p.position.x, z.position.x);
        EXPECT_LE(p.position.x, z.position.x + z.size.x);
        EXPECT_GE(p.position.y, z.position.y);
        EXPECT_LE(p.position.y, z.position.y + z.size.y);
    }
}

// dust stays neutral; pollen supplies the colored ambient particles.

TEST_F(AmbientParticleZoneTest, DustMoteColorsAreNeutralGreyOnly)
{
    ps.SetTimeOfDay(2.0f);
    PlaceZone(ParticleType::DustMote);
    for (int i = 0; i < 60; ++i)
    {
        ps.Update(0.5f, cameraPos, viewSize);
    }
    int sampled = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type != ParticleType::DustMote)
        {
            continue;
        }
        ++sampled;
        // pure grey: R == G == B (within float tolerance from accumulated math).
        EXPECT_NEAR(p.color.r, p.color.g, 1e-4f) << "DustMote has non-neutral RGB: (" << p.color.r
                                                 << ", " << p.color.g << ", " << p.color.b << ")";
        EXPECT_NEAR(p.color.g, p.color.b, 1e-4f) << "DustMote has non-neutral RGB: (" << p.color.r
                                                 << ", " << p.color.g << ", " << p.color.b << ")";
    }
    EXPECT_GT(sampled, 0);
}

TEST_F(AmbientParticleZoneTest, PollenIsNeverWhitish)
{
    ps.SetTimeOfDay(12.0f);
    PlaceZone(ParticleType::Pollen);
    for (int i = 0; i < 80; ++i)
    {
        ps.Update(0.5f, cameraPos, viewSize);
    }
    int sampled = 0;
    for (const auto& p : ps.GetParticles())
    {
        if (p.type != ParticleType::Pollen)
        {
            continue;
        }
        ++sampled;
        // exclude near-white pollen: all three channels above 0.95 with little separation.
        const bool whitish = p.color.r >= 0.9f && p.color.g >= 0.9f && p.color.b >= 0.9f &&
                             std::abs(p.color.r - p.color.g) <= 0.10f &&
                             std::abs(p.color.g - p.color.b) <= 0.10f;
        EXPECT_FALSE(whitish) << "Pollen rendered nearly white: (" << p.color.r << ", " << p.color.g
                              << ", " << p.color.b << ")";
    }
    EXPECT_GT(sampled, 0);
}

TEST(ParticleType, EnumLayoutInvariant)
{
    // particle indices are serialized in saved zones; keep their numeric values stable.
    EXPECT_EQ(static_cast<int>(ParticleType::Pollen), 10);
    EXPECT_EQ(static_cast<int>(ParticleType::CherryBlossom), 11);
    EXPECT_EQ(static_cast<int>(ParticleType::Ash), 12);
    EXPECT_EQ(static_cast<int>(ParticleType::Ember), 13);
    EXPECT_EQ(static_cast<int>(ParticleType::Sand), 14);
    EXPECT_EQ(static_cast<int>(ParticleType::Smoke), 15);
    EXPECT_EQ(static_cast<int>(ParticleType::Ink), 42);

    EXPECT_EQ(static_cast<int>(ParticleType::RainSplash), 43);
    EXPECT_EQ(static_cast<int>(ParticleType::SnowSplash), 44);
    EXPECT_EQ(EnumTraits<ParticleType>::Count, 45u);
}

TEST(ParticleType, AllTypesSpawnAndSurviveUpdate)
{
    // no textures are loaded, so this also checks the fallback variant count of one.
    ParticleSystem ps;
    const glm::vec2 cameraPos{0.0f, 0.0f};
    const glm::vec2 viewSize{640.0f, 480.0f};

    for (size_t i = 0; i < EnumTraits<ParticleType>::Count; ++i)
    {
        const auto type = static_cast<ParticleType>(i);
        ps.SpawnOne(type, glm::vec2(320.0f, 240.0f));
    }
    // some types, including Butterfly and Confetti, spawn more than one particle.
    EXPECT_GE(ps.GetParticles().size(), EnumTraits<ParticleType>::Count);

    for (int step = 0; step < 5; ++step)
    {
        ps.Update(0.016f, cameraPos, viewSize);
    }
    for (const auto& p : ps.GetParticles())
    {
        EXPECT_TRUE(std::isfinite(p.position.x) && std::isfinite(p.position.y))
            << "type=" << EnumTraits<ParticleType>::ToString(p.type);
        EXPECT_TRUE(std::isfinite(p.color.a))
            << "type=" << EnumTraits<ParticleType>::ToString(p.type);
        EXPECT_TRUE(std::isfinite(p.size) && p.size >= 0.0f)
            << "type=" << EnumTraits<ParticleType>::ToString(p.type);
        EXPECT_LT(static_cast<size_t>(p.variant), ParticleSystem::MAX_PARTICLE_VARIANTS)
            << "type=" << EnumTraits<ParticleType>::ToString(p.type);
    }
}
