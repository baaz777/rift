// the CPU saturation helper uses mix(vec3(luma), c, s); these checks do not execute the GLSL path.

#include "PostFXParams.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

namespace
{
constexpr float kTolerance = 1e-5f;
const glm::vec3 kLuma{0.2126f, 0.7152f, 0.0722f};

TEST(ApplySaturation, IdentityAtOne)
{
    const std::array<glm::vec3, 3> samples{
        glm::vec3(0.9f, 0.1f, 0.1f),     // saturated red
        glm::vec3(0.4f, 0.4f, 0.4f),     // mid gray
        glm::vec3(0.95f, 0.92f, 0.85f),  // off white
    };
    for (const glm::vec3& in : samples)
    {
        glm::vec3 out = ApplySaturation(in, 1.0f);
        EXPECT_NEAR(out.r, in.r, kTolerance);
        EXPECT_NEAR(out.g, in.g, kTolerance);
        EXPECT_NEAR(out.b, in.b, kTolerance);
    }
}

TEST(ApplySaturation, GrayscaleAtZero)
{
    // s = 0 gives vec3(dot(c, LUMA)); this checks the CPU luma weights.
    const std::array<glm::vec3, 3> samples{
        glm::vec3(0.9f, 0.1f, 0.1f),
        glm::vec3(0.2f, 0.7f, 0.3f),
        glm::vec3(0.1f, 0.4f, 0.95f),
    };
    for (const glm::vec3& in : samples)
    {
        float expected = glm::dot(in, kLuma);
        glm::vec3 out = ApplySaturation(in, 0.0f);
        EXPECT_NEAR(out.r, expected, kTolerance);
        EXPECT_NEAR(out.g, expected, kTolerance);
        EXPECT_NEAR(out.b, expected, kTolerance);
    }
}

TEST(ApplySaturation, PumpAboveOne)
{
    const glm::vec3 in(0.8f, 0.3f, 0.5f);
    const float s = 1.5f;
    glm::vec3 out = ApplySaturation(in, s);

    float lumIn = glm::dot(in, kLuma);
    float lumOut = glm::dot(out, kLuma);
    // luma is preserved (algebraic identity of mix(vec3(L), c, s) with weights LUMA).
    EXPECT_NEAR(lumOut, lumIn, kTolerance);

    for (int c = 0; c < 3; ++c)
    {
        float devIn = in[c] - lumIn;
        float devOut = out[c] - lumIn;

        EXPECT_TRUE((devIn >= 0.0f) == (devOut >= 0.0f))
            << "Channel " << c << ": deviation flipped sign";

        EXPECT_GT(std::abs(devOut), std::abs(devIn)) << "Channel " << c << ": chroma did not pump";
    }
}

TEST(ApplySaturation, PreservesLuma)
{
    // mix(vec3(L), c, s) is constructed so dot(out, LUMA) = L * (1 - s) + dot(c, LUMA) * s.
    // substituting L = dot(c, LUMA) makes that = L for any s.
    const glm::vec3 in(0.42f, 0.71f, 0.29f);
    float lumIn = glm::dot(in, kLuma);
    for (float s : {0.0f, 0.25f, 1.0f, 1.5f, 2.0f})
    {
        glm::vec3 out = ApplySaturation(in, s);
        float lumOut = glm::dot(out, kLuma);
        EXPECT_NEAR(lumOut, lumIn, kTolerance) << "Luma drifted at s=" << s;
    }
}

TEST(ApplySaturation, AchromaticInputInvariant)
{
    const glm::vec3 gray(0.4f, 0.4f, 0.4f);
    for (float s : {0.0f, 0.5f, 1.0f, 1.5f, 2.0f})
    {
        glm::vec3 out = ApplySaturation(gray, s);
        EXPECT_NEAR(out.r, gray.r, kTolerance) << "s=" << s;
        EXPECT_NEAR(out.g, gray.g, kTolerance) << "s=" << s;
        EXPECT_NEAR(out.b, gray.b, kTolerance) << "s=" << s;
    }
}

}  // namespace
