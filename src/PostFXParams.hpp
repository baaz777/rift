#pragma once

#include "AmbienceConfig.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

/**
 * @brief OpenGL post-processing parameters and CPU equivalents of shader math.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */

/**
 * @struct GradingParams
 * @brief Per-channel lift, gamma and gain for HDR scene colors.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Out = pow(max(in * gain + lift, 0), 1 / gamma).
 * Identity is lift = 0, gamma = 1, gain = 1. The RGB16F scene may exceed 1;
 * Grading runs after bloom and before saturation, vignette, grain and tonemapping.
 */
struct GradingParams
{
    /// Additive shadow offset; 0 is identity. ApplyLGG floors the adjusted color at 0.
    glm::vec3 lift{0.0f};
    /// Strictly positive midtone exponent denominator; 1 is identity.
    glm::vec3 gamma{1.0f};
    /// Highlight multiplier; 1 is identity, 0 removes the channel.
    glm::vec3 gain{1.0f};
};

/**
 * @fn glm::vec3 ApplyLGG(const glm::vec3& c, const glm::vec3& lift, const glm::vec3& gamma, \
 * const glm::vec3& gain)
 * @brief Floor the adjusted color at 0 before the power step.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 ApplyLGG(const glm::vec3& c,
                          const glm::vec3& lift,
                          const glm::vec3& gamma,
                          const glm::vec3& gain)
{
    glm::vec3 step = c * gain + lift;
    step = glm::max(step, glm::vec3(0.0f));
    return glm::vec3(std::pow(step.r, 1.0f / gamma.r),
                     std::pow(step.g, 1.0f / gamma.g),
                     std::pow(step.b, 1.0f / gamma.b));
}

/**
 * @fn float KarisBloomWeight(float lum, float threshold)
 * @brief Soft-knee luminance threshold; return 0 below threshold.
 * @author Alex (<https://github.com/lextpf>)
 */
inline float KarisBloomWeight(float lum, float threshold)
{
    float over = std::max(lum - threshold, 0.0f);
    return over / (1.0f + over);
}

/**
 * @fn float HsvSaturation(const glm::vec3& c)
 * @brief HSV saturation with a 1e-4 near-black guard; matches BloomPrefilter.frag.
 * @author Alex (<https://github.com/lextpf>)
 *
 * (max - min) / max gives 1 for both dim red (0.3, 0, 0) and bright red (1, 0, 0).
 */
inline float HsvSaturation(const glm::vec3& c)
{
    float maxC = std::max({c.r, c.g, c.b});
    float minC = std::min({c.r, c.g, c.b});
    return (maxC > 1e-4f) ? (maxC - minC) / maxC : 0.0f;
}

/**
 * @fn float KarisBloomChromaWeight(float sat, float threshold)
 * @brief Soft-knee HSV saturation threshold; only colored pixels feed bloom.
 * @author Alex (<https://github.com/lextpf>)
 */
inline float KarisBloomChromaWeight(float sat, float threshold)
{
    float over = std::max(sat - threshold, 0.0f);
    return over / (1.0f + over);
}

/**
 * @fn glm::vec3 ApplySaturation(const glm::vec3& c, float s)
 * @brief Luma-preserving saturation: 0 is grayscale, 1 is identity; weights match
 * PostFXComposite.frag.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 ApplySaturation(const glm::vec3& c, float s)
{
    const glm::vec3 LUMA{0.2126f, 0.7152f, 0.0722f};
    float lum = glm::dot(c, LUMA);
    return glm::mix(glm::vec3(lum), c, s);
}

/**
 * @fn GradingParams ComputeGradingParams(float timeOfDay, float nightFactor)
 * @brief Blend golden-hour warmth and night grading.
 * @author Alex (<https://github.com/lextpf>)
 *
 * timeOfDay is in hours from 0 inclusive to 24 exclusive; warmth occurs only at 5-7 and 18-20.
 * nightFactor ranges from 0 to 1 and is independent; callers must keep it consistent
 * with timeOfDay. midday with zero nightFactor gives identity grading.
 */
inline GradingParams ComputeGradingParams(float timeOfDay, float nightFactor)
{
    constexpr float A = ambience::GRADING_TINT_AMPLITUDE;

    float warmth = 0.0f;
    bool isDusk = false;
    if (timeOfDay >= 5.0f && timeOfDay <= 7.0f)
    {
        warmth = 1.0f - std::abs(timeOfDay - 6.0f);
    }
    else if (timeOfDay >= 18.0f && timeOfDay <= 20.0f)
    {
        warmth = 1.0f - std::abs(timeOfDay - 19.0f);
        isDusk = true;
    }

    GradingParams p;

    // Gain (highlights): warm at dawn, orange at dusk, cool muted at night.
    if (isDusk)
    {
        p.gain =
            glm::vec3(1.0f + 1.2f * A * warmth, 1.0f + 0.6f * A * warmth, 1.0f - 0.8f * A * warmth);
    }
    else
    {
        p.gain =
            glm::vec3(1.0f + 1.0f * A * warmth, 1.0f + 0.4f * A * warmth, 1.0f - 0.6f * A * warmth);
    }
    p.gain += glm::vec3(-0.8f, -0.4f, +0.8f) * A * nightFactor;

    // Lift (shadows): cool at dawn, purple at dusk, navy at night.
    if (isDusk)
    {
        p.lift = glm::vec3(+0.1f, -0.1f, +0.2f) * A * warmth;
    }
    else
    {
        p.lift = glm::vec3(-0.1f, 0.0f, +0.3f) * A * warmth;
    }
    p.lift += glm::vec3(-0.2f, -0.1f, +0.4f) * A * nightFactor;

    // Gamma (midtones): slight lift at golden hour, slight crush at night.
    if (isDusk)
    {
        p.gamma = glm::vec3(1.0f + 0.2f * A * warmth, 1.0f, 1.0f - 0.2f * A * warmth);
    }
    else
    {
        p.gamma = glm::vec3(1.0f + 0.4f * A * warmth, 1.0f + 0.2f * A * warmth, 1.0f);
    }
    p.gamma += glm::vec3(-0.6f, -0.4f, 0.0f) * A * nightFactor;

    return p;
}

/**
 * @struct PostFXParams
 * @brief OpenGL composite parameters; Vulkan ignores every field, including postFXEnabled.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
struct PostFXParams
{
    /// Hours from 0 to 24.
    float timeOfDay{12.0f};

    /// 0 is full day; 1 is deep night.
    float nightFactor{0.0f};

    /// Vignette intensity scalar (0 disables vignette this frame).
    float vignetteIntensity{ambience::VIGNETTE_INTENSITY};

    float grainIntensity{ambience::GRAIN_INTENSITY};

    float bloomIntensity{ambience::BLOOM_INTENSITY};

    /// Saturation multiplier: 0 is grayscale, 1 is identity.
    float saturation{ambience::COLOR_SATURATION};

    GradingParams gradingParams{};

    /// Time accumulator (seconds) - drives the grain noise seed.
    float time{0.0f};

    /// False returns the raw scene texel without composite effects.
    bool postFXEnabled{true};
};
