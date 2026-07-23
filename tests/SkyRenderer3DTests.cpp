// identify sprites by their recorded atlas offsets. MockRenderer records UVs,
// not texture identity; compare surviving draw-list entries with submitted quads.

#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/ParticleCards.hpp"
#include "../src/SceneMath.hpp"
#include "../src/SkyCards.hpp"
#include "../src/SkyDrawList.hpp"
#include "../src/SkyRenderer.hpp"
#include "../src/TextureStore.hpp"
#include "../src/TimeManager.hpp"
#include "../src/WeatherDefinitions.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace
{

constexpr int kViewW = 320;
constexpr int kViewH = 180;
constexpr glm::vec2 kVisible{320.0f, 180.0f};
constexpr float kAtlasDim = 1024.0f;

// multiples of four in a power-of-two atlas make uvMin exact.
constexpr float kOffsetStep = 100.0f;

float OffsetXFor(skyDraw::Sprite sprite)
{
    return static_cast<float>(static_cast<std::size_t>(sprite)) * kOffsetStep;
}

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

cameraRig::RigParams MakeRig(cameraRig::Preset preset, float pitchDeg = 0.0f, float yawDeg = 0.0f)
{
    cameraRig::RigParams params;
    params.target = {1000.0f, 500.0f};
    params.visibleWorldSize = kVisible;
    params.sceneRadius = 4096.0f;
    params.pitchRadians = Degrees(pitchDeg);
    params.yawRadians = Degrees(yawDeg);
    cameraRig::ApplyPreset(params, preset);
    return params;
}

void MakeAtlasTexture(Texture& out, std::vector<unsigned char>& storage)
{
    const int dim = static_cast<int>(kAtlasDim);
    storage.assign(static_cast<std::size_t>(dim) * dim * 4, 255);
    out.LoadFromData(storage.data(), dim, dim, 4, false);
}

void BindAtlas(SkyRenderer& sky, const Texture& atlas)
{
    SkyAtlasOffsets offsets;
    for (std::size_t i = 0; i < skyDraw::SPRITE_COUNT; ++i)
    {
        const auto sprite = static_cast<skyDraw::Sprite>(i);
        offsets[sprite] = glm::vec2(OffsetXFor(sprite), 0.0f);
    }
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

glm::vec3 QuadCentre(const MockRenderer::Quad3D& quad)
{
    return (quad.corners[0] + quad.corners[2]) * 0.5f;
}

std::size_t CountSprite(const MockRenderer& mock, skyDraw::Sprite sprite)
{
    std::size_t n = 0;
    for (const MockRenderer::Quad3D& q : mock.quads3D)
    {
        if (q.texCoord.x == OffsetXFor(sprite))
        {
            ++n;
        }
    }
    return n;
}

// region size comes from the sprite texture, including the 0x0 unlinked aurora mote.
glm::vec2 ExpectedRegionSize(const SkyRenderer& sky, skyDraw::Sprite sprite)
{
    const Texture* texture = nullptr;
    switch (sprite)
    {
        case skyDraw::Sprite::Ray:
            texture = &sky.GetRayTexture();
            break;
        case skyDraw::Sprite::Star:
            texture = &sky.GetStarTexture();
            break;
        case skyDraw::Sprite::StarGlow:
            texture = &sky.GetStarGlowTexture();
            break;
        case skyDraw::Sprite::ShootingStar:
            texture = &sky.GetShootingStarTexture();
            break;
        case skyDraw::Sprite::Glow:
            texture = &sky.GetGlowTexture();
            break;
        case skyDraw::Sprite::LightPool:
            texture = &sky.GetLightPoolTexture();
            break;
        case skyDraw::Sprite::AuroraCurtain:
            texture = &sky.GetAuroraCurtainTexture();
            break;
        case skyDraw::Sprite::AuroraSmall:
            texture = &sky.GetAuroraSmallTexture();
            break;
        case skyDraw::Sprite::AuroraBeam:
            texture = &sky.GetAuroraBeamTexture();
            break;
        case skyDraw::Sprite::Solid:
            texture = &sky.GetSolidTexture();
            break;
    }
    return {static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())};
}

skyDraw::Element MakeElement(skyDraw::Layer layer, skyDraw::Sprite sprite, glm::vec2 pos)
{
    skyDraw::Element e;
    e.layer = layer;
    e.sprite = sprite;
    e.pos = pos;
    e.size = glm::vec2(4.0f, 4.0f);
    e.color = glm::vec4(1.0f);
    e.additive = true;
    return e;
}

}  // namespace

TEST(SkyRenderer3DTest, AuroraWeatherEmitsCurtainQuads)
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
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer mock;
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(mock, tm, frame, kVisible);

    EXPECT_GT(mock.quads3D.size(), 0u);
    EXPECT_GT(CountSprite(mock, skyDraw::Sprite::AuroraCurtain), 0u);
    EXPECT_EQ(mock.quads3D.size(), sky.GetLastQuadCount3D());
}

TEST(SkyRenderer3DTest, ClearMiddayEmitsNoAuroraQuads)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);
    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 12.0f, WeatherState::Clear);
    sky.Update(0.1f, tm);

    MockRenderer mock;
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(mock, tm, frame, kVisible);

    EXPECT_EQ(CountSprite(mock, skyDraw::Sprite::AuroraCurtain), 0u);
    EXPECT_EQ(CountSprite(mock, skyDraw::Sprite::AuroraBeam), 0u);
}

TEST(SkyRenderer3DTest, EveryQuadCarriesTheSkyPassState)
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
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    for (cameraRig::Preset preset : {cameraRig::Preset::Classic, cameraRig::Preset::DS})
    {
        MockRenderer mock;
        const particleCards::Frame frame = particleCards::MakeFrame(MakeRig(preset));
        sky.Render3D(mock, tm, frame, kVisible);
        ASSERT_GT(mock.quads3D.size(), 0u);
        for (const MockRenderer::Quad3D& q : mock.quads3D)
        {
            EXPECT_EQ(q.blend, renderModes::BlendMode::Additive);
            EXPECT_EQ(q.depth, renderModes::DepthMode::None);
            EXPECT_EQ(q.light, renderModes::LightMode::SelfLit);
            EXPECT_FALSE(q.flipY);
        }
    }
}

// submission order must match the surviving subsequence of the draw list.
TEST(SkyRenderer3DTest, Submit3DMatchesTheDrawListAfterCulling)
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
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    const particleCards::Frame frame = particleCards::MakeFrame(MakeRig(cameraRig::Preset::DS));
    const skyDraw::List& list = sky.Build(tm, skyCards::ViewportTopLeft(frame), kViewW, kViewH);

    std::vector<const skyDraw::Element*> kept;
    for (const skyDraw::Element& e : list.items)
    {
        if (skyCards::KeepOnSheet(frame, e))
        {
            kept.push_back(&e);
        }
    }
    ASSERT_FALSE(kept.empty());
    ASSERT_LE(kept.size(), skyCards::MAX_SKY_QUADS_3D) << "no decimation expected for this scene";

    MockRenderer mock;
    sky.Submit3D(mock, list, frame);

    ASSERT_EQ(mock.quads3D.size(), kept.size());
    for (std::size_t i = 0; i < kept.size(); ++i)
    {
        const glm::vec3 expected = skyCards::SheetCentre(frame, kept[i]->pos, kept[i]->size);
        const glm::vec3 actual = QuadCentre(mock.quads3D[i]);
        EXPECT_NEAR(actual.x, expected.x, 1e-2f) << "quad " << i;
        EXPECT_NEAR(actual.y, expected.y, 1e-2f) << "quad " << i;
        EXPECT_NEAR(actual.z, expected.z, 1e-2f) << "quad " << i;
    }
}

// bind an atlas to exercise ResolveSprite; an unbound atlas takes the fallback path.
TEST(SkyRenderer3DTest, AtlasUvsMatchTheBoundRegion)
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
    for (int i = 0; i < 40; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer mock;
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(mock, tm, frame, kVisible);
    ASSERT_GT(mock.quads3D.size(), 0u);

    // each atlas offset must pair with that sprite's own texture dimensions.
    std::size_t distinctSprites = 0;
    std::array<bool, skyDraw::SPRITE_COUNT> seen{};
    for (const MockRenderer::Quad3D& q : mock.quads3D)
    {
        const float step = q.texCoord.x / kOffsetStep;
        ASSERT_FLOAT_EQ(step, std::floor(step)) << "offset is not one of the bound slots";
        ASSERT_LT(step, static_cast<float>(skyDraw::SPRITE_COUNT));
        EXPECT_FLOAT_EQ(q.texCoord.y, 0.0f);

        const auto sprite = static_cast<skyDraw::Sprite>(static_cast<std::size_t>(step));
        const glm::vec2 expected = ExpectedRegionSize(sky, sprite);
        EXPECT_FLOAT_EQ(q.texSize.x, expected.x)
            << "region width for "
            << EnumTraits<skyDraw::Sprite>::Names[static_cast<std::size_t>(step)];
        EXPECT_FLOAT_EQ(q.texSize.y, expected.y);

        if (!seen[static_cast<std::size_t>(step)])
        {
            seen[static_cast<std::size_t>(step)] = true;
            ++distinctSprites;
        }
    }
    EXPECT_GE(distinctSprites, 3u) << "an aurora night must exercise several sprites";
}

// the 3D halo and beam use the atlas to keep the pass on one texture.
TEST(SkyRenderer3DTest, Submit3DIgnoresTheStandaloneFlag)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);
    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::List list;
    skyDraw::Element beam =
        MakeElement(skyDraw::Layer::Aurora, skyDraw::Sprite::AuroraBeam, glm::vec2(160.0f, 90.0f));
    beam.standalone = true;
    list.items.push_back(beam);

    MockRenderer mock;
    sky.Submit3D(mock, list, frame);

    ASSERT_EQ(mock.quads3D.size(), 1u);
    EXPECT_FLOAT_EQ(mock.quads3D[0].texCoord.x, OffsetXFor(skyDraw::Sprite::AuroraBeam));
}

// without an atlas, sample each sprite's whole texture.
TEST(SkyRenderer3DTest, WithoutAnAtlasTheStandaloneRegionIsUsed)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::List list;
    list.items.push_back(
        MakeElement(skyDraw::Layer::Star, skyDraw::Sprite::Star, glm::vec2(160.0f, 90.0f)));

    MockRenderer mock;
    sky.Submit3D(mock, list, frame);

    ASSERT_EQ(mock.quads3D.size(), 1u);
    EXPECT_FLOAT_EQ(mock.quads3D[0].texCoord.x, 0.0f);
    EXPECT_FLOAT_EQ(mock.quads3D[0].texCoord.y, 0.0f);
    EXPECT_FLOAT_EQ(mock.quads3D[0].texSize.x, static_cast<float>(sky.GetStarTexture().GetWidth()));
}

TEST(SkyRenderer3DTest, AtmosphericBandsBecomeSolidQuadsIn3DAndStayRectsOnFlat)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);
    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Clear);
    sky.Update(0.016f, tm);
    ASSERT_GE(tm.GetStarVisibility(), 0.2f);

    MockRenderer flat;
    sky.Render(flat, tm, glm::vec2(0.0f), kViewW, kViewH);
    EXPECT_GE(flat.rects.size(), 1u) << "the flat path still uses colour-only rects";

    MockRenderer world;
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(world, tm, frame, kVisible);
    EXPECT_TRUE(world.rects.empty()) << "the 3D path uses no 2D primitive at all";
    EXPECT_GE(CountSprite(world, skyDraw::Sprite::Solid), 1u);
}

// SetAmbientColor flushes the 2D batch only. the 3D sky must use LightMode::SelfLit
// without changing world ambient.
TEST(SkyRenderer3DTest, Submit3DLeavesTheSceneAmbientAlone)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Aurora);
    for (int i = 0; i < 20; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer mock;
    mock.ambient = glm::vec3(0.3f, 0.3f, 0.45f);
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(mock, tm, frame, kVisible);
    EXPECT_EQ(mock.ambient, glm::vec3(0.3f, 0.3f, 0.45f));

    sky.Render(mock, tm, glm::vec2(0.0f), kViewW, kViewH);
    EXPECT_EQ(mock.ambient, glm::vec3(1.0f));
}

TEST(SkyRenderer3DTest, QuadCountIsBoundedByTheSkyBudget)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::List list;
    for (std::size_t i = 0; i < 4000; ++i)
    {
        list.items.push_back(
            MakeElement(skyDraw::Layer::Star, skyDraw::Sprite::Star, glm::vec2(160.0f, 90.0f)));
    }

    MockRenderer mock;
    const std::size_t submitted = sky.Submit3D(mock, list, frame);
    EXPECT_LE(mock.quads3D.size(), skyCards::MAX_SKY_QUADS_3D);
    EXPECT_EQ(submitted, mock.quads3D.size());
    EXPECT_EQ(sky.GetLastQuadCount3D(), mock.quads3D.size());
}

TEST(SkyRenderer3DTest, BudgetNeverDropsTheBoltOrTheFlash)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);
    // atlas binding makes sprite offsets observable; unbound draws all report (0, 0).
    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::List list;
    for (std::size_t i = 0; i < 4000; ++i)
    {
        list.items.push_back(MakeElement(
            skyDraw::Layer::Aurora, skyDraw::Sprite::AuroraCurtain, glm::vec2(160.0f, 90.0f)));
    }
    list.items.push_back(
        MakeElement(skyDraw::Layer::Flash, skyDraw::Sprite::Glow, glm::vec2(160.0f, 90.0f)));
    for (std::size_t i = 0; i < 25; ++i)
    {
        list.items.push_back(
            MakeElement(skyDraw::Layer::Bolt, skyDraw::Sprite::Glow, glm::vec2(160.0f, 90.0f)));
    }

    MockRenderer mock;
    sky.Submit3D(mock, list, frame);

    // flash and bolt share the glow sprite: preserve 1 flash + 25 bolt segments.
    EXPECT_EQ(CountSprite(mock, skyDraw::Sprite::Glow), 26u);
    EXPECT_LT(CountSprite(mock, skyDraw::Sprite::AuroraCurtain), 4000u)
        << "the elastic layer must be the one that gives way";
    EXPECT_LE(mock.quads3D.size(), skyCards::MAX_SKY_QUADS_3D);
}

// MeteorShower combines the full star field with meteors, exercising the largest sky count.
TEST(SkyRenderer3DTest, MeteorShowerAtNoonStillDrawsStarsAndFitsTheBudget)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);
    Texture atlas;
    std::vector<unsigned char> pixels;
    MakeAtlasTexture(atlas, pixels);
    BindAtlas(sky, atlas);

    TimeManager tm;
    SettleAt(tm, 12.0f, WeatherState::MeteorShower);
    ASSERT_GT(tm.GetStarVisibility(), 0.9f) << "MeteorShower imposes night at any hour";

    MockRenderer seed;
    sky.Render(seed, tm, glm::vec2(1000.0f, 500.0f), kViewW, kViewH);
    for (int i = 0; i < 100; ++i)
    {
        sky.Update(0.1f, tm);
    }

    MockRenderer mock;
    const particleCards::Frame frame = particleCards::MakeFrame(MakeRig(cameraRig::Preset::DS));
    sky.Render3D(mock, tm, frame, kVisible);

    EXPECT_GT(CountSprite(mock, skyDraw::Sprite::Star), 0u);
    EXPECT_LE(mock.quads3D.size(), skyCards::MAX_SKY_QUADS_3D);
}

TEST(SkyRenderer3DTest, Render3DIsInertBeforeInitialize)
{
    SkyRenderer sky;
    TimeManager tm;
    SettleAt(tm, 2.0f, WeatherState::Aurora);

    MockRenderer mock;
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    sky.Render3D(mock, tm, frame, kVisible);

    EXPECT_TRUE(mock.quads3D.empty());
    EXPECT_TRUE(sky.GetLastDrawList().items.empty());
}

TEST(SkyRenderer3DTest, LightPoolsDrawAsGroundQuadsInTheirOwnPass)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::LightPoolList pools;
    skyDraw::LightPool pool;
    pool.centreWorld = glm::vec2(1000.0f, 500.0f);
    pool.radius = 64.0f;
    pool.surfaceHeight = 8.0f;
    pool.color = glm::vec4(1.0f, 0.85f, 0.55f, 0.48f);
    pools.push_back(pool);

    MockRenderer mock;
    EXPECT_EQ(sky.SubmitLightPools3D(mock, pools, frame), 1u);

    ASSERT_EQ(mock.quads3D.size(), 1u);
    const MockRenderer::Quad3D& q = mock.quads3D[0];
    EXPECT_EQ(q.blend, renderModes::BlendMode::Additive);
    EXPECT_EQ(q.depth, renderModes::DepthMode::None);
    EXPECT_EQ(q.light, renderModes::LightMode::SelfLit);
    EXPECT_FALSE(q.flipY);
    for (const glm::vec3& corner : q.corners)
    {
        EXPECT_FLOAT_EQ(corner.y, 8.0f) << "the quad lies flat at the lamp's surface height";
    }
    EXPECT_FLOAT_EQ(q.color.a, 0.48f);
}

// keep a light if its radius reaches the view even when its center lies outside.
TEST(SkyRenderer3DTest, LightPoolsOutsideTheFrustumAreCulledConservatively)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));

    skyDraw::LightPool far;
    far.centreWorld = glm::vec2(9000.0f, 9000.0f);
    far.radius = 64.0f;
    far.color = glm::vec4(1.0f);

    skyDraw::LightPool edge;
    edge.centreWorld = glm::vec2(1000.0f - 160.0f - 40.0f, 500.0f);
    edge.radius = 64.0f;
    edge.color = glm::vec4(1.0f);

    MockRenderer mock;
    EXPECT_EQ(sky.SubmitLightPools3D(mock, {far}, frame), 0u);

    mock.ClearRecorded();
    EXPECT_EQ(sky.SubmitLightPools3D(mock, {edge}, frame), 1u)
        << "a pool reaching into view must survive";
}

TEST(SkyRenderer3DTest, LightPoolCountIsCapped)
{
    TextureStore store;
    SkyRenderer sky;
    sky.Initialize(store);

    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    skyDraw::LightPoolList pools;
    for (std::size_t i = 0; i < skyCards::MAX_LIGHT_POOL_QUADS_3D + 144; ++i)
    {
        skyDraw::LightPool pool;
        pool.centreWorld = glm::vec2(1000.0f, 500.0f);
        pool.radius = 64.0f;
        pool.color = glm::vec4(1.0f);
        pools.push_back(pool);
    }

    MockRenderer mock;
    EXPECT_EQ(sky.SubmitLightPools3D(mock, pools, frame), skyCards::MAX_LIGHT_POOL_QUADS_3D);
    EXPECT_EQ(mock.quads3D.size(), skyCards::MAX_LIGHT_POOL_QUADS_3D);
}
