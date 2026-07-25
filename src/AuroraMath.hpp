#pragma once

#include <cmath>

#include <glm/glm.hpp>

/**
 * @brief Aurora palette and ribbon math.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
namespace AuroraMath
{
/**
 * @fn glm::vec3 AuroraColor(float phase)
 * @brief Interpolates the aurora RGB palette; phase repeats every 1.0.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 AuroraColor(float phase)
{
    float p = phase - std::floor(phase);
    constexpr int kStops = 8;
    const glm::vec3 stops[kStops] = {
        {0.15f, 1.00f, 0.45f},  // Emerald
        {0.10f, 0.95f, 0.80f},  // Teal
        {0.25f, 0.85f, 1.00f},  // Cyan
        {0.35f, 0.55f, 1.00f},  // Azure blue
        {0.60f, 0.35f, 1.00f},  // Violet
        {0.95f, 0.45f, 1.00f},  // Magenta
        {1.00f, 0.50f, 0.70f},  // Rose
        {1.00f, 0.82f, 0.50f},  // Warm gold
    };
    float pos = p * static_cast<float>(kStops);
    int i = static_cast<int>(pos) % kStops;
    int j = (i + 1) % kStops;
    float f = pos - std::floor(pos);
    return glm::mix(stops[i], stops[j], f);
}

/**
 * @fn float TangentAngleDeg(glm::vec2 prev, glm::vec2 next)
 * @brief Ribbon tangent angle in degrees; zero for a zero-length segment.
 * @author Alex (<https://github.com/lextpf>)
 */
inline float TangentAngleDeg(glm::vec2 prev, glm::vec2 next)
{
    const float dx = next.x - prev.x;
    const float dy = next.y - prev.y;
    if (dx == 0.0f && dy == 0.0f)
    {
        return 0.0f;
    }
    constexpr float kRadToDeg = 57.29577951308232f;
    return std::atan2(dy, dx) * kRadToDeg;
}

/**
 * @fn float SweepBoost(float segNorm, float t, float speed, float width, float seed)
 * @brief Gaussian brightness sweep along a ribbon.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The center wraps; distance to it does not. The spot fades out at the end and re-enters
 * at the start.
 *
 * $$
 * B(s,t) = \exp(-((s - \mathrm{frac}(vt+\sigma))/w)^2)
 * $$
 *
 * @param segNorm Ribbon position in [0, 1].
 * @param t Elapsed seconds.
 * @param speed Band lengths per second.
 * @param width Gaussian 1/e radius in band lengths.
 * @param seed Per-band phase offset.
 * @return Brightness in [0, 1], subject to floating-point underflow.
 * @pre Width > 0.
 */
inline float SweepBoost(float segNorm, float t, float speed, float width, float seed)
{
    float center = t * speed + seed;
    center -= std::floor(center);  // Cycle 0..1 along the band
    const float d = (segNorm - center) / width;
    return std::exp(-d * d);
}
}  // namespace AuroraMath
