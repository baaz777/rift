#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cmath>

/**
 * @brief Scene-space frustum culling.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Gribb-Hartmann extraction forms planes from matrix row sums and differences.
 * Planes store (a, b, c, d); ax + by + cz + d >= 0 is inside. Tests may retain objects
 * outside a corner. Sky, weather and ambient sheets use the flat rectangle cull.
 */
namespace frustum
{

/// Indices into a Frustum's plane array.
enum PlaneIndex
{
    PLANE_LEFT = 0,
    PLANE_RIGHT = 1,
    PLANE_BOTTOM = 2,
    PLANE_TOP = 3,
    PLANE_NEAR = 4,
    PLANE_FAR = 5,
    PLANE_COUNT = 6
};

/// Six normalized clip planes, inside-positive.
struct Frustum
{
    std::array<glm::vec4, PLANE_COUNT> planes{};
};

/**
 * @fn Frustum FromViewProjection(const glm::mat4& viewProj)
 * @brief Extracts normalized planes from an OpenGL clip volume.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Supply the column-major projection * view matrix before backend clip correction.
 * Clip Z must range from -w to w. A Vulkan-corrected zero-to-w matrix uses a different near plane.
 * Nondegenerate normals are normalized so sphere radii use scene units.
 *
 * @pre The matrix must describe a finite, nondegenerate viewing volume.
 */
inline Frustum FromViewProjection(const glm::mat4& viewProj)
{
    // GLM is column-major (m[column][row]), so row i is the i-th component of
    // each of the four columns.
    const glm::vec4 rowX{viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]};
    const glm::vec4 rowY{viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]};
    const glm::vec4 rowZ{viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]};
    const glm::vec4 rowW{viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]};

    Frustum f;
    f.planes[PLANE_LEFT] = rowW + rowX;
    f.planes[PLANE_RIGHT] = rowW - rowX;
    f.planes[PLANE_BOTTOM] = rowW + rowY;
    f.planes[PLANE_TOP] = rowW - rowY;
    f.planes[PLANE_NEAR] = rowW + rowZ;
    f.planes[PLANE_FAR] = rowW - rowZ;

    for (glm::vec4& p : f.planes)
    {
        const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        if (len > 1e-6f)
        {
            p /= len;
        }
    }
    return f;
}

/**
 * @fn float SignedDistance(const glm::vec4& plane, glm::vec3 point)
 * @brief Evaluate a point against an inside-positive plane.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The result is a distance in scene units only when the plane normal has unit length.
 */
inline float SignedDistance(const glm::vec4& plane, glm::vec3 point)
{
    return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
}

/**
 * @fn bool ContainsPoint(const Frustum& f, glm::vec3 point)
 * @brief Whether a point lies inside all six planes.
 * @author Alex (<https://github.com/lextpf>)
 */
inline bool ContainsPoint(const Frustum& f, glm::vec3 point)
{
    for (const glm::vec4& p : f.planes)
    {
        if (SignedDistance(p, point) < 0.0f)
        {
            return false;
        }
    }
    return true;
}

/**
 * @fn bool IntersectsSphere(const Frustum& f, glm::vec3 center, float radius)
 * @brief Whether a sphere intersects the frustum.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Reject only when the center is farther outside a plane than the radius.
 * Tangency counts as intersection. A sphere outside a frustum corner can be retained.
 *
 * @param f Scene-space frustum with normalized inward-facing planes.
 * @param center Sphere center in scene units, without a camera offset.
 * @param radius Nonnegative sphere radius in scene units.
 * @pre Plane normals have unit length.
 */
inline bool IntersectsSphere(const Frustum& f, glm::vec3 center, float radius)
{
    for (const glm::vec4& p : f.planes)
    {
        if (SignedDistance(p, center) < -radius)
        {
            return false;
        }
    }
    return true;
}

/**
 * @fn bool IntersectsAabb(const Frustum& f, glm::vec3 minCorner, glm::vec3 maxCorner)
 * @brief Whether an axis-aligned box intersects the frustum.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Test the box corner farthest along each inward plane normal. If even that corner is
 * outside the plane, the entire box is outside. Passing all planes is conservative near corners.
 *
 * @pre Each component of `minCorner` is no greater than the corresponding `maxCorner` component.
 */
inline bool IntersectsAabb(const Frustum& f, glm::vec3 minCorner, glm::vec3 maxCorner)
{
    for (const glm::vec4& p : f.planes)
    {
        const glm::vec3 positiveVertex{p.x >= 0.0f ? maxCorner.x : minCorner.x,
                                       p.y >= 0.0f ? maxCorner.y : minCorner.y,
                                       p.z >= 0.0f ? maxCorner.z : minCorner.z};
        if (SignedDistance(p, positiveVertex) < 0.0f)
        {
            return false;
        }
    }
    return true;
}

}  // namespace frustum
