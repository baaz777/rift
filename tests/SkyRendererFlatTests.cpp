// sky positions vary with std::random_device. assert counts, modes, shapes,
// and submission order instead of absolute positions. distinct atlas offsets
// identify sprites because MockRenderer does not record texture identity.

#include <gtest/gtest.h>

#include "../src/SkyRenderer.hpp"
#include "../src/TextureStore.hpp"
#include "../src/TimeManager.hpp"
#include "../src/WeatherDefinitions.hpp"
#include "MockRenderer.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace
{

constexpr int kViewW = 320;
constexpr int kViewH = 180;
constexpr float kAtlasDim = 1024.0f;

// distinct offsets identify sprites; multiples of four in a power-of-two atlas keep UVs exact.
constexpr float kRayOffsetX = 0.0f;
constexpr float kStarOffsetX = 100.0f;
constexpr float kStarGlowOffsetX = 200.0f;
constexpr float kShootingStarOffsetX = 300.0f;
constexpr float kGlowOffsetX = 400.0f;
constexpr float kLightPoolOffsetX = 500.0f;
constexpr float kAuroraCurtainOffsetX = 600.0f;
constexpr float kAuroraSmallOffsetX = 700.0f;
constexpr float kAuroraBeamOffsetX = 800.0f;
constexpr float kSolidOffsetX = 900.0f;

struct FlatDraw
{
    std::size_t sequence = 0;
    bool isRect = false;
    bool fromAtlas = false;
    bool additive = false;
    float rotation = 0.0f;
    glm::vec2 position{0.0f};
    glm::vec2 size{0.0f};
    glm::vec2 uvMin{0.0f};
    glm::vec4 color{1.0f};
};

std::vector<FlatDraw> MergeFlat(const MockRenderer& mock)
{
    std::vector<FlatDraw> out;
    out.reserve(mock.sprites2D.size() + mock.rects.size());
    for (const MockRenderer::Sprite2D& s : mock.sprites2D)
    {
        FlatDraw d;
        d.sequence = s.sequence;
        d.isRect = false;
        d.fromAtlas = s.fromAtlas;
        d.additive = s.additive;
        d.rotation = s.rotation;
        d.position = s.position;
        d.size = s.size;
        d.uvMin = s.uvMin;
        d.color = s.color;
        out.push_back(d);
    }
    for (const MockRenderer::Rect& r : mock.rects)
    {
        FlatDraw d;
        d.sequence = r.sequence;
        d.isRect = true;
        d.additive = r.additive;
        d.position = r.position;
        d.size = r.size;
        d.color = r.color;
        out.push_back(d);
    }
    std::sort(out.begin(),
              out.end(),
              [](const FlatDraw& a, const FlatDraw& b) { return a.sequence < b.sequence; });
    return out;
}

// atlas pixel offset, or -1 for a standalone texture.
float AtlasOffsetX(const FlatDraw& d)
{
    return d.fromAtlas ? d.uvMin.x * kAtlasDim : -1.0f;
}

// DrawSkyElement reads only atlas dimensions, so opaque placeholder pixels are sufficient.
void MakeAtlasTexture(Texture& out, std::vector<unsigned char>& storage)
{
    const int dim = static_cast<int>(kAtlasDim);
    storage.assign(static_cast<std::size_t>(dim) * dim * 4, 255);
    out.LoadFromData(storage.data(), dim, dim, 4, false);
}

void BindAtlas(SkyRenderer& sky, const Texture& atlas)
{
    SkyAtlasOffsets offsets;
    offsets[skyDraw::Sprite::Ray] = glm::vec2(kRayOffsetX, 0.0f);
    offsets[skyDraw::Sprite::Star] = glm::vec2(kStarOffsetX, 0.0f);
    offsets[skyDraw::Sprite::StarGlow] = glm::vec2(kStarGlowOffsetX, 0.0f);
    offsets[skyDraw::Sprite::ShootingStar] = glm::vec2(kShootingStarOffsetX, 0.0f);
    offsets[skyDraw::Sprite::Glow] = glm::vec2(kGlowOffsetX, 0.0f);
    offsets[skyDraw::Sprite::LightPool] = glm::vec2(kLightPoolOffsetX, 0.0f);
    offsets[skyDraw::Sprite::AuroraCurtain] = glm::vec2(kAuroraCurtainOffsetX, 0.0f);
    offsets[skyDraw::Sprite::AuroraSmall] = glm::vec2(kAuroraSmallOffsetX, 0.0f);
    offsets[skyDraw::Sprite::AuroraBeam] = glm::vec2(kAuroraBeamOffsetX, 0.0f);
    offsets[skyDraw::Sprite::Solid] = glm::vec2(kSolidOffsetX, 0.0f);
    sky.SetAtlasBinding(&atlas, offsets);
}

// freeze time before settling weather so the loop cannot advance past night.
void SettleAt(TimeManager& tm, float hour, WeatherState weather)
{
    tm.Initialize();
    tm.SetTime(hour);
    tm.SetTimeScale(0.0f);
    tm.SetWeather(weather);
    for (int i = 0; i < 400; ++i)
    {
        tm.Update(0.1f);
    }
}

}  // namespace

// Initialize builds CPU textures; LoadFromData skips GL without a current context.
TEST(SkyRendererFlatTest, InitializeIsHeadlessAndAdoptsEverySprite)
{
    TextureStore store;
    EXPECT_EQ(store.Count(), 0u);

    SkyRenderer sky;
    sky.Initialize(store);

    EXPECT_EQ(store.Count(), 10u);
    EXPECT_EQ(sky.GetSolidTexture().GetWidth(), 4);
    EXPECT_EQ(sky.GetGlowTexture().GetWidth(), 256);
    EXPECT_EQ(sky.GetStarTexture().GetWidth(), 64);
    EXPECT_EQ(sky.GetStarGlowTexture().GetWidth(), 128);
}

// rendering consumes no RNG, so repeated renders of the same state must agree.
TEST(SkyRendererFlatTest, EmissionSequenceIsStable)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Aurora);
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer first;
    MockRenderer second;
    sky.Render(first, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
    sky.Render(second, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);

    const std::vector<FlatDraw> a = MergeFlat(first);
    const std::vector<FlatDraw> b = MergeFlat(second);
    ASSERT_FALSE(a.empty());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        EXPECT_EQ(a[i].isRect, b[i].isRect) << "at " << i;
        EXPECT_EQ(a[i].position, b[i].position) << "at " << i;
        EXPECT_EQ(a[i].size, b[i].size) << "at " << i;
        EXPECT_EQ(a[i].color, b[i].color) << "at " << i;
        EXPECT_FLOAT_EQ(a[i].rotation, b[i].rotation) << "at " << i;
    }
}

// every sky draw is additive, matching the 3D pass blend mode.
TEST(SkyRendererFlatTest, EveryFlatDrawIsAdditive)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Aurora);
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer mock;
    sky.Render(mock, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);

    const std::vector<FlatDraw> draws = MergeFlat(mock);
    ASSERT_FALSE(draws.empty());
    for (const FlatDraw& d : draws)
    {
        EXPECT_TRUE(d.additive);
    }
    EXPECT_EQ(mock.ambient, glm::vec3(1.0f));
}

// the atmospheric bands are the only untextured sky draws.
TEST(SkyRendererFlatTest, GlowBandsAreColoredRects)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Clear);
    sky.Update(0.016f, tm);
    ASSERT_GE(tm.GetStarVisibility(), 0.2f);

    MockRenderer mock;
    sky.Render(mock, tm, glm::vec2(0.0f), kViewW, kViewH);

    // the bottom band appears above 0.2; the top band also depends on a sine threshold.
    ASSERT_GE(mock.rects.size(), 1u);
    ASSERT_LE(mock.rects.size(), 2u);

    const MockRenderer::Rect& band = mock.rects[0];
    const float glowHeight = static_cast<float>(kViewH) * 0.12f;
    EXPECT_FLOAT_EQ(band.position.x, 0.0f);
    EXPECT_FLOAT_EQ(band.position.y, static_cast<float>(kViewH) - glowHeight);
    EXPECT_FLOAT_EQ(band.size.x, static_cast<float>(kViewW));
    EXPECT_FLOAT_EQ(band.size.y, glowHeight);
    EXPECT_FLOAT_EQ(band.color.r, 0.08f);
    EXPECT_FLOAT_EQ(band.color.g, 0.12f);
    EXPECT_FLOAT_EQ(band.color.b, 0.25f);
    EXPECT_TRUE(band.additive);
}

// the flat halo and beam sample standalone textures.
TEST(SkyRendererFlatTest, HaloAndBeamBindTheirOwnTexture)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Aurora);
    ASSERT_GT(tm.GetAuroraFade(), 0.01f);

    // halo pulse and beam age can suppress frame-zero draws; step until each appears.
    bool sawAtlas = false;
    bool sawStandalone = false;
    for (int i = 0; i < 600 && !(sawAtlas && sawStandalone); ++i)
    {
        sky.Update(0.1f, tm);
        MockRenderer mock;
        sky.Render(mock, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
        for (const MockRenderer::Sprite2D& s : mock.sprites2D)
        {
            if (s.fromAtlas)
            {
                sawAtlas = true;
            }
            else
            {
                sawStandalone = true;
            }
        }
    }
    EXPECT_TRUE(sawAtlas) << "aurora curtains must batch through the atlas";
    EXPECT_TRUE(sawStandalone) << "the ribbon halo and beam must bypass it";
}

// Update needs the viewport published by Render before it can spawn meteors.
TEST(SkyRendererFlatTest, RenderPublishesTheViewportForTheSpawners)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::MeteorShower);
    ASSERT_GT(tm.GetStarVisibility(), 0.3f);

    auto meteorCount = [&](const MockRenderer& mock)
    {
        std::size_t n = 0;
        for (const MockRenderer::Sprite2D& s : mock.sprites2D)
        {
            if (s.fromAtlas && s.uvMin.x * kAtlasDim == kShootingStarOffsetX)
            {
                ++n;
            }
        }
        return n;
    };

    for (int i = 0; i < 300; ++i)
    {
        sky.Update(0.1f, tm);
    }
    MockRenderer blind;
    sky.Render(blind, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
    EXPECT_EQ(meteorCount(blind), 0u);

    std::size_t seen = 0;
    for (int i = 0; i < 300 && seen == 0; ++i)
    {
        sky.Update(0.1f, tm);
        MockRenderer mock;
        sky.Render(mock, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
        seen = meteorCount(mock);
    }
    EXPECT_GT(seen, 0u) << "meteors must spawn once a viewport has been published";
}

// check the bolt span rather than jittered endpoints to verify viewport sizing.
TEST(SkyRendererFlatTest, LightningBoltSpansTheBuiltViewport)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Thunderstorm);

    // publish the viewport before the storm interval; main segments are 6 px thick, branches 4 px.
    MockRenderer seed;
    sky.Render(seed, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);

    float minY = 1e9f;
    float maxY = -1e9f;
    bool sawBolt = false;
    for (int i = 0; i < 600 && !sawBolt; ++i)
    {
        sky.Update(0.1f, tm);
        MockRenderer mock;
        sky.Render(mock, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
        for (const MockRenderer::Sprite2D& s : mock.sprites2D)
        {
            if (s.fromAtlas && s.uvMin.x * kAtlasDim == kGlowOffsetX && s.size.x == 6.0f)
            {
                sawBolt = true;
                minY = std::min(minY, s.position.y);
                maxY = std::max(maxY, s.position.y);
            }
        }
    }
    ASSERT_TRUE(sawBolt) << "a thunderstorm must produce a bolt within the interval";
    EXPECT_GT(maxY - minY, 0.7f * static_cast<float>(kViewH));
}
