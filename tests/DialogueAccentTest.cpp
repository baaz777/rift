// sprite assets are needed for spawn wiring; these cases compare the CPU accent samplers.

#include <gtest/gtest.h>

#include "../src/Texture.hpp"
#include "../src/TextureStore.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace
{
constexpr int kSize = 4;      // 4x4 = 16 pixels - plenty for filter tests
constexpr int kChannels = 4;  // RGBA

using Pixel = std::array<unsigned char, 4>;

// disable flipY to preserve the test buffer's pixel layout.
Texture MakeTexture(const std::vector<unsigned char>& pixels)
{
    Texture tex;
    auto buffer = pixels;  // LoadFromData takes non-const pointer
    EXPECT_TRUE(tex.LoadFromData(buffer.data(), kSize, kSize, kChannels, false));
    return tex;
}

std::vector<unsigned char> Uniform(Pixel p)
{
    std::vector<unsigned char> out(kSize * kSize * kChannels);
    for (int i = 0; i < kSize * kSize; ++i)
    {
        out[i * 4 + 0] = p[0];
        out[i * 4 + 1] = p[1];
        out[i * 4 + 2] = p[2];
        out[i * 4 + 3] = p[3];
    }
    return out;
}

std::vector<unsigned char> WithOnePixel(Pixel bg, int x, int y, Pixel one)
{
    auto out = Uniform(bg);
    const int idx = (y * kSize + x) * kChannels;
    out[idx + 0] = one[0];
    out[idx + 1] = one[1];
    out[idx + 2] = one[2];
    out[idx + 3] = one[3];
    return out;
}

constexpr glm::vec3 kFallback{0.85f, 0.75f, 0.40f};

bool ColorsClose(glm::vec3 a, glm::vec3 b, float eps = 1e-3f)
{
    return std::fabs(a.r - b.r) < eps && std::fabs(a.g - b.g) < eps && std::fabs(a.b - b.b) < eps;
}
}  // namespace

TEST(DialogueAccentTest, AllTransparent_ReturnsFallback)
{
    auto tex = MakeTexture(Uniform({255, 100, 100, 0}));  // saturated but alpha=0
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, AllSkinTone_ReturnsFallback)
{
    // #d2a07c: hue ~25 degrees, saturation ~0.40, value ~0.82.
    // inside hue [0, 30] and saturation [0.20, 0.60], so the skin filter rejects it.
    auto tex = MakeTexture(Uniform({0xd2, 0xa0, 0x7c, 255}));
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, AllGrey_ReturnsFallback)
{
    // sat = 0 -> below the 0.30 saturation threshold.
    auto tex = MakeTexture(Uniform({128, 128, 128, 255}));
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, AllDarkShadow_ReturnsFallback)
{
    // value ~0.23 -> below the 0.25 threshold.
    auto tex = MakeTexture(Uniform({0x1a, 0x1a, 0x3a, 255}));
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, AllNearWhiteHighlight_ReturnsFallback)
{
    // value > 0.95 -> filtered as highlight.
    auto tex = MakeTexture(Uniform({250, 250, 250, 255}));
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, BrightRedAmongGrey_PicksRed)
{
    // single saturated red pixel among 15 grey pixels -> red wins.
    auto tex = MakeTexture(WithOnePixel({128, 128, 128, 255}, 2, 1, {220, 30, 30, 255}));
    const glm::vec3 result = tex.SampleDominantNonSkinColor(kFallback);
    EXPECT_NEAR(result.r, 220.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(result.g, 30.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(result.b, 30.0f / 255.0f, 1e-3f);
}

TEST(DialogueAccentTest, MixedSaturated_PicksMostVibrant)
{
    // cyan has the larger saturation * value score: ~0.96 * 0.85 versus ~0.50 * 0.40.
    std::vector<unsigned char> pixels(kSize * kSize * kChannels, 0);

    for (int i = 0; i < kSize * kSize; ++i)
    {
        pixels[i * 4 + 0] = 100;
        pixels[i * 4 + 1] = 100;
        pixels[i * 4 + 2] = 100;
        pixels[i * 4 + 3] = 255;
    }

    pixels[0] = 10;
    pixels[1] = 220;
    pixels[2] = 220;
    pixels[3] = 255;

    const int dimIdx = (3 * kSize + 3) * 4;
    pixels[dimIdx + 0] = 80;
    pixels[dimIdx + 1] = 40;
    pixels[dimIdx + 2] = 100;
    pixels[dimIdx + 3] = 255;

    auto tex = MakeTexture(pixels);
    const glm::vec3 result = tex.SampleDominantNonSkinColor(kFallback);
    EXPECT_NEAR(result.r, 10.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(result.g, 220.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(result.b, 220.0f / 255.0f, 1e-3f);
}

TEST(DialogueAccentTest, MidSaturationOrange_NotMisclassifiedAsSkin)
{
    // #ff8030: hue ~17 degrees is inside the skin band, but saturation ~0.81
    // exceeds the 0.60 cap, so the color remains eligible.
    auto tex = MakeTexture(Uniform({0xff, 0x80, 0x30, 255}));
    const glm::vec3 result = tex.SampleDominantNonSkinColor(kFallback);
    EXPECT_NEAR(result.r, 1.0f, 1e-3f);
    EXPECT_NEAR(result.g, 0x80 / 255.0f, 1e-3f);
    EXPECT_NEAR(result.b, 0x30 / 255.0f, 1e-3f);
}

TEST(DialogueAccentTest, EmptyTexture_ReturnsFallback)
{
    Texture tex;  // never loaded
    EXPECT_TRUE(ColorsClose(tex.SampleDominantNonSkinColor(kFallback), kFallback));
}

TEST(DialogueAccentTest, StoreSampleAccent_MatchesTextureSampler)
{
    // the store sampler must match the texture sampler used to set NpcSprite::accentColor.
    auto pixels = WithOnePixel({128, 128, 128, 255}, 0, 0, {220, 30, 30, 255});
    Texture tex = MakeTexture(pixels);

    TextureStore store;
    const TextureHandle handle = store.Adopt(std::move(tex));
    const glm::vec3 accent = store.SampleAccent(handle, kFallback);
    EXPECT_NEAR(accent.r, 220.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(accent.g, 30.0f / 255.0f, 1e-3f);
    EXPECT_NEAR(accent.b, 30.0f / 255.0f, 1e-3f);
}
