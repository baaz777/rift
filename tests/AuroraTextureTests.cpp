// alpha must fade at every texture border so adjacent ribbon quads have no visible seams.

#include <gtest/gtest.h>

#include <algorithm>

#include "AuroraTextures.hpp"

namespace
{
// Alpha (channel 3) of pixel (x,y) in a width*height*4 RGBA8 buffer.
int A(const std::vector<unsigned char>& px, int width, int x, int y)
{
    return px[(static_cast<size_t>(y) * width + x) * 4 + 3];
}

// the outer pixel ring must fade to zero to hide seams between sky quads.
int MaxBorderAlpha(const std::vector<unsigned char>& px, int width, int height)
{
    int maxA = 0;
    for (int x = 0; x < width; ++x)
    {
        maxA = std::max(maxA, std::max(A(px, width, x, 0), A(px, width, x, height - 1)));
    }
    for (int y = 0; y < height; ++y)
    {
        maxA = std::max(maxA, std::max(A(px, width, 0, y), A(px, width, width - 1, y)));
    }
    return maxA;
}
}  // namespace

TEST(AuroraTextureTests, CurtainSizeAndOpaqueWhiteRGB)
{
    auto px = AuroraTextures::BuildCurtainPixels(128, 256);
    ASSERT_EQ(px.size(), static_cast<size_t>(128) * 256 * 4);
    EXPECT_EQ(px[0], 255);  // R
    EXPECT_EQ(px[1], 255);  // G
    EXPECT_EQ(px[2], 255);  // B
}

TEST(AuroraTextureTests, CurtainHorizontalEdgesAreFeathered)
{
    const int w = 128, h = 256;
    auto px = AuroraTextures::BuildCurtainPixels(w, h);
    const int midRow = h * 6 / 10;                            // near the vertical peak
    EXPECT_LT(A(px, w, 0, midRow), 12);                       // left border ~transparent
    EXPECT_LT(A(px, w, w - 1, midRow), 12);                   // right border ~transparent
    EXPECT_GT(A(px, w, w / 2, midRow), A(px, w, 2, midRow));  // center brighter than edge
}

TEST(AuroraTextureTests, BeamIsSoftFeatheredVerticalOval)
{
    const int w = 64, h = 256;
    auto px = AuroraTextures::BuildBeamPixels(w, h);

    const int center = A(px, w, w / 2, h / 2);
    EXPECT_GT(center, A(px, w, w / 2, 4));      // fades toward the top
    EXPECT_GT(center, A(px, w, w / 2, h - 5));  // fades toward the bottom (soft base)
    EXPECT_LT(A(px, w, w / 2, 4), 40);          // top feathered
    EXPECT_LT(A(px, w, w / 2, h - 5), 40);      // base feathered (no hard cut)
    EXPECT_LT(A(px, w, 0, h / 2), 12);          // oval side ~transparent
    EXPECT_LT(A(px, w, w - 1, h / 2), 12);
}

TEST(AuroraTextureTests, CurtainEntireBorderFeathersToZero)
{
    const int w = 128, h = 256;
    auto px = AuroraTextures::BuildCurtainPixels(w, h);

    EXPECT_LE(MaxBorderAlpha(px, w, h), 3);
    EXPECT_LT(A(px, w, w / 2, 0), 4);       // top-center feathered (was a hard ~25)
    EXPECT_LT(A(px, w, w / 2, h - 1), 4);   // bottom-center feathered
    EXPECT_GT(A(px, w, w / 2, h / 2), 60);  // interior stays bright (feather didn't blank it)
}

TEST(AuroraTextureTests, CurtainHasVisibleCoreAndSoftOuterHalo)
{
    const int w = 128, h = 256;
    auto px = AuroraTextures::BuildCurtainPixels(w, h);
    const int center = A(px, w, w / 2, h / 2);
    const int verticalOuter = A(px, w, w / 2, h / 8);
    const int verticalInner = A(px, w, w / 2, h / 4);
    const int horizontalOuter = A(px, w, w / 8, h / 2);
    const int horizontalInner = A(px, w, w / 4, h / 2);

    // the core stays translucent; the outer envelope retains a faint tail before the border.
    EXPECT_GT(center, 155);
    EXPECT_LT(center, 185);
    EXPECT_GT(verticalOuter, 4);
    EXPECT_GT(horizontalOuter, 3);
    EXPECT_LT(verticalOuter, verticalInner);
    EXPECT_LT(verticalInner, center);
    EXPECT_LT(horizontalOuter, horizontalInner);
    EXPECT_LT(horizontalInner, center);
    EXPECT_LT(verticalOuter, center / 5);
    EXPECT_LT(horizontalOuter, center / 5);
}

TEST(AuroraTextureTests, BeamEntireBorderFeathersToZero)
{
    const int w = 64, h = 256;
    auto px = AuroraTextures::BuildBeamPixels(w, h);

    EXPECT_LE(MaxBorderAlpha(px, w, h), 3);
    EXPECT_LT(A(px, w, w / 2, 0), 4);       // top-center feathered (was a hard ~15)
    EXPECT_LT(A(px, w, w / 2, h - 1), 4);   // bottom-center feathered
    EXPECT_GT(A(px, w, w / 2, h / 2), 60);  // interior stays bright
    EXPECT_LT(A(px, w, w / 2, h / 2), 90);  // soft glow, never a dense core
}
