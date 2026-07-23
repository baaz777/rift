#pragma once

#include "SkyDrawList.hpp"
#include "WeatherDefinitions.hpp"

#include <glm/glm.hpp>

#include <vector>

/**
 * @brief Resolves shared light-pool intensity for both render paths.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Weather-resolved star visibility gates every pool. weather that hides stars also disables
 * lamps, including at midnight.
 */
namespace worldLights
{

/// Star visibility at or below which no pool contributes.
inline constexpr float NIGHT_GATE = 0.01f;

/// Resolved intensity below which a pool is skipped entirely.
inline constexpr float MIN_INTENSITY = 0.01f;

/// Fraction of a light's resolved intensity that reaches the pool alpha.
inline constexpr float POOL_ALPHA_SCALE = 0.6f;

/**
 * @fn void Build(const std::vector<WorldLight>& lights, float hour, float nightFactor, \
 * skyDraw::LightPoolList& out)
 * @brief Clears and rebuilds pools in map order.
 * @author Alex (<https://github.com/lextpf>)
 *
 * surfaceHeight remains zero; the 3D caller fills it afterwards.
 *
 * @param lights Map lights in the order used for submission.
 * @param hour Time of day in hours.
 * @param nightFactor Scene star visibility from 0 to 1.
 * @param out Receives the active pools; existing entries are cleared and capacity is retained.
 */
inline void Build(const std::vector<WorldLight>& lights,
                  float hour,
                  float nightFactor,
                  skyDraw::LightPoolList& out)
{
    out.clear();
    if (nightFactor <= NIGHT_GATE)
    {
        return;
    }
    for (const WorldLight& light : lights)
    {
        const float intensity = ComputeLightIntensity(light.schedule, hour) * nightFactor;
        if (intensity < MIN_INTENSITY)
        {
            continue;
        }
        skyDraw::LightPool pool;
        pool.centreWorld = light.position;
        pool.radius = light.radius;
        pool.surfaceHeight = 0.0f;
        pool.color = glm::vec4(light.color, intensity * POOL_ALPHA_SCALE);
        out.push_back(pool);
    }
}

}  // namespace worldLights
