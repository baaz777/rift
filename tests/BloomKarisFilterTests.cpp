// CPU bloom weights mirror the shader formulas. these tests check the math without executing GLSL.

#include "PostFXParams.hpp"

#include <gtest/gtest.h>

namespace
{
constexpr float kTolerance = 1e-5f;

TEST(KarisBloomWeight, ZeroAtAndBelowThreshold)
{
    EXPECT_NEAR(KarisBloomWeight(0.0f, 0.85f), 0.0f, kTolerance);
    EXPECT_NEAR(KarisBloomWeight(0.50f, 0.85f), 0.0f, kTolerance);
    EXPECT_NEAR(KarisBloomWeight(0.85f, 0.85f), 0.0f, kTolerance);
}

TEST(KarisBloomWeight, RampsSmoothlyAboveThreshold)
{
    float prev = 0.0f;
    for (float lum = 0.851f; lum < 5.0f; lum += 0.01f)
    {
        float w = KarisBloomWeight(lum, 0.85f);
        EXPECT_GE(w, prev) << "Weight non-monotonic at lum=" << lum;
        EXPECT_GE(w, 0.0f);
        EXPECT_LE(w, 1.0f);
        prev = w;
    }
}

TEST(KarisBloomWeight, ContinuousAtThreshold)
{
    // continuity at the threshold prevents bloom from switching abruptly.
    float justBelow = KarisBloomWeight(0.85f - 1e-4f, 0.85f);
    float justAbove = KarisBloomWeight(0.85f + 1e-4f, 0.85f);
    EXPECT_NEAR(justBelow, 0.0f, 1e-3f);
    EXPECT_NEAR(justAbove, 0.0f, 1e-3f);
}

TEST(KarisBloomWeight, AsymptoticToOne)
{
    // as lum -> infinity, over / (1 + over) -> 1.
    EXPECT_GT(KarisBloomWeight(100.0f, 0.85f), 0.99f);
    EXPECT_LT(KarisBloomWeight(100.0f, 0.85f), 1.0f);
}

TEST(KarisBloomWeight, NeverNaNOrInf)
{
    EXPECT_FALSE(std::isnan(KarisBloomWeight(0.0f, 0.0f)));
    EXPECT_FALSE(std::isnan(KarisBloomWeight(0.0f, 1.0f)));
    EXPECT_FALSE(std::isinf(KarisBloomWeight(1e6f, 0.85f)));
}

TEST(KarisBloomChromaWeight, ZeroAtAndBelowThreshold)
{
    EXPECT_NEAR(KarisBloomChromaWeight(0.0f, 0.30f), 0.0f, kTolerance);
    EXPECT_NEAR(KarisBloomChromaWeight(0.20f, 0.30f), 0.0f, kTolerance);
    EXPECT_NEAR(KarisBloomChromaWeight(0.30f, 0.30f), 0.0f, kTolerance);
}

TEST(KarisBloomChromaWeight, MatchesExpectedRampValues)
{
    EXPECT_NEAR(KarisBloomChromaWeight(0.50f, 0.30f), 0.20f / 1.20f, 1e-4f);
    EXPECT_NEAR(KarisBloomChromaWeight(0.80f, 0.30f), 0.50f / 1.50f, 1e-4f);
    EXPECT_NEAR(KarisBloomChromaWeight(1.00f, 0.30f), 0.70f / 1.70f, 1e-4f);
}

TEST(KarisBloomChromaWeight, RampsMonotonically)
{
    // HSV saturation = (max - min) / max lies in [0, 1]; the ramp must be monotonic.
    float prev = 0.0f;
    for (float s = 0.301f; s <= 1.0f; s += 0.01f)
    {
        float w = KarisBloomChromaWeight(s, 0.30f);
        EXPECT_GE(w, prev) << "Weight non-monotonic at sat=" << s;
        EXPECT_GE(w, 0.0f);
        EXPECT_LT(w, 1.0f);
        prev = w;
    }
}

TEST(KarisBloomChromaWeight, ContinuousAtThreshold)
{
    float justBelow = KarisBloomChromaWeight(0.30f - 1e-4f, 0.30f);
    float justAbove = KarisBloomChromaWeight(0.30f + 1e-4f, 0.30f);
    EXPECT_NEAR(justBelow, 0.0f, 1e-3f);
    EXPECT_NEAR(justAbove, 0.0f, 1e-3f);
}

TEST(KarisBloomChromaWeight, NeverNaNOrInf)
{
    EXPECT_FALSE(std::isnan(KarisBloomChromaWeight(0.0f, 0.0f)));
    EXPECT_FALSE(std::isnan(KarisBloomChromaWeight(0.0f, 1.0f)));
    EXPECT_FALSE(std::isinf(KarisBloomChromaWeight(100.0f, 0.30f)));
}

}  // namespace
