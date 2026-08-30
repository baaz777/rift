// the CPU grading helper evaluates pow(c * gain + lift, 1 / gamma); shader execution is outside
// this suite.

#include "PostFXParams.hpp"

#include <gtest/gtest.h>

namespace
{
constexpr float kTolerance = 1e-4f;

TEST(ApplyLGG, IdentityWhenNeutral)
{
    // identity: lift=(0,0,0), gamma=(1,1,1), gain=(1,1,1) -> output == input.
    glm::vec3 in(0.25f, 0.50f, 0.75f);
    glm::vec3 out = ApplyLGG(in, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
    EXPECT_NEAR(out.r, in.r, kTolerance);
    EXPECT_NEAR(out.g, in.g, kTolerance);
    EXPECT_NEAR(out.b, in.b, kTolerance);
}

TEST(ApplyLGG, GainAffectsHighlightsMost)
{
    // gain=2, lift=0, gamma=1 gives output = c * 2; the change scales with brightness.
    glm::vec3 shadow(0.1f);
    glm::vec3 highlight(0.9f);
    glm::vec3 gain(2.0f);
    glm::vec3 lift(0.0f);
    glm::vec3 gamma(1.0f);

    glm::vec3 outS = ApplyLGG(shadow, lift, gamma, gain);
    glm::vec3 outH = ApplyLGG(highlight, lift, gamma, gain);

    float deltaS = outS.r - shadow.r;     // 0.1
    float deltaH = outH.r - highlight.r;  // 0.9
    EXPECT_GT(deltaH, deltaS);
}

TEST(ApplyLGG, LiftAffectsShadowsMost)
{
    // constant lift has a larger relative effect on shadows than highlights.
    glm::vec3 shadow(0.10f);
    glm::vec3 highlight(0.90f);
    glm::vec3 lift(0.05f);
    glm::vec3 gamma(1.0f);
    glm::vec3 gain(1.0f);

    glm::vec3 outS = ApplyLGG(shadow, lift, gamma, gain);
    glm::vec3 outH = ApplyLGG(highlight, lift, gamma, gain);

    float relS = (outS.r - shadow.r) / shadow.r;
    float relH = (outH.r - highlight.r) / highlight.r;
    EXPECT_GT(relS, relH);
}

TEST(ApplyLGG, GammaCurvesMidtones)
{
    // gamma above one brightens midtones more than values near zero or one.
    glm::vec3 mid(0.5f);
    glm::vec3 lift(0.0f);
    glm::vec3 gamma(1.4f);
    glm::vec3 gain(1.0f);
    glm::vec3 out = ApplyLGG(mid, lift, gamma, gain);
    EXPECT_GT(out.r, mid.r) << "Gamma > 1 should brighten midtones";
}

TEST(ApplyLGG, ASCCDLOrder)
{
    glm::vec3 in(0.3f, 0.5f, 0.7f);
    glm::vec3 lift(0.05f, 0.0f, -0.05f);
    glm::vec3 gamma(1.1f, 1.0f, 0.9f);
    glm::vec3 gain(1.05f, 1.0f, 0.95f);
    glm::vec3 out = ApplyLGG(in, lift, gamma, gain);

    glm::vec3 expected;
    for (int c = 0; c < 3; ++c)
    {
        float step = std::max(in[c] * gain[c] + lift[c], 0.0f);
        expected[c] = std::pow(step, 1.0f / gamma[c]);
    }
    EXPECT_NEAR(out.r, expected.r, kTolerance);
    EXPECT_NEAR(out.g, expected.g, kTolerance);
    EXPECT_NEAR(out.b, expected.b, kTolerance);
}

TEST(ApplyLGG, ClampsNegativeBeforePow)
{
    // clamp the power base to zero to avoid NaN for negative channel values.
    glm::vec3 in(0.0f);
    glm::vec3 lift(-1.0f);  // forces c*gain+lift to be negative
    glm::vec3 gamma(2.2f);
    glm::vec3 gain(1.0f);
    glm::vec3 out = ApplyLGG(in, lift, gamma, gain);
    EXPECT_FLOAT_EQ(out.r, 0.0f);
    EXPECT_FLOAT_EQ(out.g, 0.0f);
    EXPECT_FLOAT_EQ(out.b, 0.0f);
}

}  // namespace
