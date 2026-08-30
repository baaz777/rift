// the saturation gate must be independent of brightness and stable near black.

#include "PostFXParams.hpp"

#include <glm/glm.hpp>

#include <gtest/gtest.h>

namespace
{
constexpr float kTolerance = 1e-5f;

TEST(HsvSaturation, PureRedFullySaturated)
{
    EXPECT_NEAR(HsvSaturation(glm::vec3(1.0f, 0.0f, 0.0f)), 1.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.0f, 1.0f, 0.0f)), 1.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.0f, 0.0f, 1.0f)), 1.0f, kTolerance);
}

TEST(HsvSaturation, WhiteIsAchromatic)
{
    EXPECT_NEAR(HsvSaturation(glm::vec3(1.0f, 1.0f, 1.0f)), 0.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.5f, 0.5f, 0.5f)), 0.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.25f, 0.25f, 0.25f)), 0.0f, kTolerance);
}

TEST(HsvSaturation, DimRedReadsFullySaturated)
{
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.3f, 0.0f, 0.0f)), 1.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.05f, 0.0f, 0.0f)), 1.0f, kTolerance);
}

TEST(HsvSaturation, NearBlackReturnsZeroViaEpsilon)
{
    // the near-black epsilon prevents division noise from producing false saturation.
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.0f, 0.0f, 0.0f)), 0.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(1e-5f, 1e-5f, 1e-5f)), 0.0f, kTolerance);
    EXPECT_NEAR(HsvSaturation(glm::vec3(1e-5f, 0.0f, 0.0f)), 0.0f, kTolerance);
}

TEST(HsvSaturation, MidPaletteValuesAreCorrect)
{
    // a pixel that's half-saturated (e.g., dim red mixed with grey) returns
    // a fractional value: (max - min) / max.
    //   (1.0, 0.5, 0.5) -> (1.0 - 0.5) / 1.0 = 0.5
    EXPECT_NEAR(HsvSaturation(glm::vec3(1.0f, 0.5f, 0.5f)), 0.5f, kTolerance);
    //   (0.8, 0.4, 0.2) -> (0.8 - 0.2) / 0.8 = 0.75
    EXPECT_NEAR(HsvSaturation(glm::vec3(0.8f, 0.4f, 0.2f)), 0.75f, kTolerance);
}

TEST(HsvSaturation, HdrInputIsWellDefined)
{
    // the RGB16F scene buffer can exceed 1.0:
    //   (2.0, 0.5, 0.5) -> (2.0 - 0.5) / 2.0 = 0.75
    EXPECT_NEAR(HsvSaturation(glm::vec3(2.0f, 0.5f, 0.5f)), 0.75f, kTolerance);
    //   (3.0, 3.0, 3.0) -> 0 (still achromatic)
    EXPECT_NEAR(HsvSaturation(glm::vec3(3.0f, 3.0f, 3.0f)), 0.0f, kTolerance);
}

TEST(HsvSaturation, NeverNaNOrInf)
{
    EXPECT_FALSE(std::isnan(HsvSaturation(glm::vec3(0.0f))));
    EXPECT_FALSE(std::isnan(HsvSaturation(glm::vec3(1e-9f))));
    EXPECT_FALSE(std::isinf(HsvSaturation(glm::vec3(1e6f, 0.0f, 0.0f))));
}

}  // namespace
