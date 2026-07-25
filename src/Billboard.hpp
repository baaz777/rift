#pragma once

#include "EnumTraits.hpp"
#include "SceneMath.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <cstddef>
#include <iterator>
#include <string_view>

/**
 * @brief Damped orientation for upright artwork.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * yawFollow and leanFollow range from 0 to 1: zero keeps the quad map-aligned and upright;
 * One follows camera yaw and pitch.
 *
 * A camera-facing quad has lean = pitch. Its top tips away from the camera.
 * The projected height is h * abs(cos(lean - pitch)); at pitch 51.3 degrees, an upright
 * quad retains 62.5% of its height and a fully leaning quad retains 100%.
 *
 * @verbatim
 *
 *      0           pitch           90        90 + lean        180
 *      |-------------|-------------|-------------|-------------|
 *    south          eye          v (up)       quad up        north
 *
 *                    |<------- must be 90 ------>|
 *                (90 + lean) - pitch = 90  =>  lean = pitch
 *
 * @endverbatim
 */
namespace billboard
{

/**
 * @enum Role
 * @brief Damping profile for upright artwork.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Ground artwork uses sceneMath::MakeGroundQuad.
 */
enum class Role
{
    Scenery = 0,
    Character = 1
};

/// Yaw and lean follow factors from 0 to 1, inclusive.
struct Damping
{
    float yawFollow = 1.0f;
    float leanFollow = 1.0f;
};

inline constexpr std::size_t ROLE_COUNT = 2;

/**
 * @brief Damping profiles indexed by Role.
 *
 * Scenery follows half the yaw to reduce fence rotation during camera orbits.
 * Characters follow full yaw to avoid edge-on sprites; both retain some upright lean.
 */
inline constexpr Damping DEFAULT_DAMPING[ROLE_COUNT] = {
    {0.50f, 0.80f},
    {1.00f, 0.90f},
};

/**
 * @fn constexpr Damping DefaultDamping(Role role)
 * @brief Damping profile for role.
 * @author Alex (<https://github.com/lextpf>)
 */
inline constexpr Damping DefaultDamping(Role role)
{
    return DEFAULT_DAMPING[static_cast<std::size_t>(role)];
}

/**
 * @fn float ApparentHeightScale(float leanRadians, float pitchRadians)
 * @brief Projected height ratio for leanRadians relative to camera pitchRadians.
 * @author Alex (<https://github.com/lextpf>)
 */
inline float ApparentHeightScale(float leanRadians, float pitchRadians)
{
    return std::abs(std::cos(leanRadians - pitchRadians));
}

/**
 * @struct Orientation
 * @brief The orthonormal in-plane axes of an oriented billboard quad.
 * @author Alex (<https://github.com/lextpf>)
 */
struct Orientation
{
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float yawRadians = 0.0f;
    float leanRadians = 0.0f;
};

/**
 * @fn Orientation Orient(float cameraYawRadians, float cameraPitchRadians, Damping damping)
 * @brief Resolve the damped orientation for a billboard under a given camera.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param cameraYawRadians   Camera yaw; 0 places the camera due map-south.
 * @param cameraPitchRadians Camera elevation above the horizon; pi/2 is top-down.
 * @param damping            Damping factors, normally `DefaultDamping`.
 */
inline Orientation Orient(float cameraYawRadians, float cameraPitchRadians, Damping damping)
{
    Orientation o;
    o.yawRadians = damping.yawFollow * cameraYawRadians;
    o.leanRadians = damping.leanFollow * cameraPitchRadians;

    const float sy = std::sin(o.yawRadians);
    const float cy = std::cos(o.yawRadians);
    const float sl = std::sin(o.leanRadians);
    const float cl = std::cos(o.leanRadians);

    // Horizontal axis across the quad, and the horizontal direction pointing
    // away from the camera (map-north at yaw 0). The top edge tips along the
    // latter, so a fully-leaning quad lies flat pointing north.
    o.right = {cy, 0.0f, -sy};
    const glm::vec3 awayFromCamera{-sy, 0.0f, -cy};

    o.up = cl * glm::vec3(0.0f, 1.0f, 0.0f) + sl * awayFromCamera;
    return o;
}

/**
 * @fn void MakeQuad(glm::vec3 footCenter, glm::vec2 size, const Orientation& o, glm::vec3 \
 * outCorners[sceneMath::QUAD_CORNER_COUNT], float rotationDegrees = 0.0f)
 * @brief Builds corners around a bottom-center scene anchor.
 * @author Alex (<https://github.com/lextpf>)
 *
 * `size` is in world pixels. rotationDegrees rotates artwork in its own plane.
 * outCorners uses sceneMath::QuadCorner order.
 */
inline void MakeQuad(glm::vec3 footCenter,
                     glm::vec2 size,
                     const Orientation& o,
                     glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT],
                     float rotationDegrees = 0.0f)
{
    // Routed through the shared primitive so per-tile rotation behaves the same
    // for upright artwork as for flat. note axisDown is -up: the quad's own
    // "down the image" points from its head toward its feet.
    const glm::vec3 centre = footCenter + o.up * (size.y * 0.5f);
    sceneMath::MakeOrientedQuad(centre, size, o.right, -o.up, rotationDegrees, outCorners);
}

/**
 * @fn void MakeQuad(glm::vec3 footCenter, glm::vec2 size, float cameraYawRadians, float \
 * cameraPitchRadians, Damping damping, glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
 * @brief Convenience overload orienting and building in one step.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void MakeQuad(glm::vec3 footCenter,
                     glm::vec2 size,
                     float cameraYawRadians,
                     float cameraPitchRadians,
                     Damping damping,
                     glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
{
    MakeQuad(footCenter, size, Orient(cameraYawRadians, cameraPitchRadians, damping), outCorners);
}

}  // namespace billboard

template <>
struct EnumTraits<billboard::Role> : EnumTraitsBase<billboard::Role, EnumTraits<billboard::Role>>
{
    static constexpr std::size_t Count = billboard::ROLE_COUNT;
    static constexpr std::string_view Names[] = {"Scenery", "Character"};
};

static_assert(std::size(EnumTraits<billboard::Role>::Names) == billboard::ROLE_COUNT,
              "billboard::Role names must stay in step with ROLE_COUNT");
