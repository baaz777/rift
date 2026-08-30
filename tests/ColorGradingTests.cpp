// grading checks cover the neutral midday transform and separate shadow and highlight tints.

#include "AmbienceConfig.hpp"
#include "PostFXParams.hpp"

#include <gtest/gtest.h>

namespace
{
constexpr float kTolerance = 1e-4f;

TEST(GradingParams, IdentityAtMidday)
{
    auto p = ComputeGradingParams(12.0f, 0.0f);
    EXPECT_NEAR(p.lift.r, 0.0f, kTolerance);
    EXPECT_NEAR(p.lift.g, 0.0f, kTolerance);
    EXPECT_NEAR(p.lift.b, 0.0f, kTolerance);
    EXPECT_NEAR(p.gamma.r, 1.0f, kTolerance);
    EXPECT_NEAR(p.gamma.g, 1.0f, kTolerance);
    EXPECT_NEAR(p.gamma.b, 1.0f, kTolerance);
    EXPECT_NEAR(p.gain.r, 1.0f, kTolerance);
    EXPECT_NEAR(p.gain.g, 1.0f, kTolerance);
    EXPECT_NEAR(p.gain.b, 1.0f, kTolerance);
}

TEST(GradingParams, WarmGainAtGoldenHour)
{
    auto dawn = ComputeGradingParams(6.0f, 0.0f);
    EXPECT_GT(dawn.gain.r, 1.0f) << "Dawn highlights should be warmer (red boost)";
    EXPECT_LT(dawn.gain.b, 1.0f) << "Dawn highlights should be warmer (blue suppress)";

    auto dusk = ComputeGradingParams(19.0f, 0.0f);
    EXPECT_GT(dusk.gain.r, 1.0f) << "Dusk highlights should be warmer";
    EXPECT_LT(dusk.gain.b, 1.0f) << "Dusk highlights should be warmer";
}

TEST(GradingParams, CoolLiftAtNight)
{
    auto night = ComputeGradingParams(1.0f, 1.0f);
    EXPECT_GT(night.lift.b, night.lift.r) << "Night shadows should lean blue";
    EXPECT_LT(night.lift.r, 0.0f) << "Night shadows should crush red";

    EXPECT_GT(night.gain.b, night.gain.r);
}

TEST(GradingParams, DawnDuskAreDistinctlyDifferent)
{
    // dawn and dusk use distinct shadow tints even when both have warm highlights.
    auto dawn = ComputeGradingParams(6.0f, 0.0f);
    auto dusk = ComputeGradingParams(19.0f, 0.0f);
    bool liftDiffers = std::abs(dawn.lift.r - dusk.lift.r) > kTolerance ||
                       std::abs(dawn.lift.g - dusk.lift.g) > kTolerance ||
                       std::abs(dawn.lift.b - dusk.lift.b) > kTolerance;
    EXPECT_TRUE(liftDiffers) << "Dawn vs dusk shadow tint should differ";
}

TEST(GradingParams, BoundedByAmplitude)
{
    // combined golden-hour and night inputs must stay within twice the configured amplitude.
    constexpr float kBound = ambience::GRADING_TINT_AMPLITUDE * 2.0f;
    for (float t = 0.0f; t < 24.0f; t += 0.25f)
    {
        for (float n = 0.0f; n <= 1.0f; n += 0.1f)
        {
            auto p = ComputeGradingParams(t, n);
            EXPECT_LE(std::abs(p.lift.r), kBound + kTolerance) << "t=" << t << " n=" << n;
            EXPECT_LE(std::abs(p.lift.g), kBound + kTolerance) << "t=" << t << " n=" << n;
            EXPECT_LE(std::abs(p.lift.b), kBound + kTolerance) << "t=" << t << " n=" << n;
            EXPECT_LE(std::abs(p.gain.r - 1.0f), kBound + kTolerance) << "t=" << t << " n=" << n;
            EXPECT_LE(std::abs(p.gain.g - 1.0f), kBound + kTolerance) << "t=" << t << " n=" << n;
            EXPECT_LE(std::abs(p.gain.b - 1.0f), kBound + kTolerance) << "t=" << t << " n=" << n;
        }
    }
}

TEST(GradingParams, ContinuousAcrossDayBoundary)
{
    // warmth starts at zero at each transition boundary to prevent color jumps.
    auto preDawn = ComputeGradingParams(4.99f, 0.0f);
    auto atFive = ComputeGradingParams(5.0f, 0.0f);
    EXPECT_NEAR(preDawn.gain.r, atFive.gain.r, 0.01f);
    EXPECT_NEAR(preDawn.lift.b, atFive.lift.b, 0.01f);
}

}  // namespace
