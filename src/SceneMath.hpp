#pragma once

#include <glm/glm.hpp>

#include <algorithm>

/**
 * @brief Maps world pixels to the right-handed 3D scene.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * One world pixel equals one scene unit. At yaw 0, the camera looks north from the south.
 * At pitch 90 degrees, projected world axes match the flat view.
 *
 * ElevationRole selects which tile layers inherit the cell height. This keeps water
 * below a bridge at ground height. Characters use Elevation::offset without rescaling.
 *
 * @verbatim
 *   world (2D, Y-down)              scene (3D, Y-up, right-handed)
 *
 *      +--------> +X (east)              +Y (up)
 *      |                                  |
 *      |                                  |    +Z (south, toward the camera
 *      v                                  |   /      at yaw 0)
 *     +Y (south)                          |  /
 *                                         | /
 *                                         +-------> +X (east)
 *
 *   scene = (worldX, height, worldY)
 * @endverbatim
 */
namespace sceneMath
{

/**
 * @fn glm::vec3 ToScene(glm::vec2 world, float height = 0.0f)
 * @brief Lift a world position onto the scene ground plane (or to height).
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 ToScene(glm::vec2 world, float height = 0.0f)
{
    return {world.x, height, world.y};
}

/**
 * @fn glm::vec2 ToWorld(glm::vec3 scene)
 * @brief Drop a scene position back to world pixels, discarding its height.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec2 ToWorld(glm::vec3 scene)
{
    return {scene.x, scene.z};
}

/// Quad corners run top-left, top-right, bottom-right, bottom-left.
enum QuadCorner
{
    QUAD_TOP_LEFT = 0,
    QUAD_TOP_RIGHT = 1,
    QUAD_BOTTOM_RIGHT = 2,
    QUAD_BOTTOM_LEFT = 3,
    QUAD_CORNER_COUNT = 4
};

/**
 * @fn void MakeOrientedQuad(glm::vec3 centre, glm::vec2 size, glm::vec3 axisRight, glm::vec3 \
 * axisDown, float rotationDegrees, glm::vec3 outCorners[QUAD_CORNER_COUNT])
 * @brief Rotates artwork about its centre in a screen-oriented basis.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Supply perpendicular unit axes; this function does not normalize them.
 * `axisDown` points along the artwork's downward Y axis. Reversing it mirrors rotations.
 *
 * $$
 *   \vec{u}' = \cos\theta\,\vec{u} + \sin\theta\,\vec{v}, \qquad
 *   \vec{v}' = -\sin\theta\,\vec{u} + \cos\theta\,\vec{v}
 * $$
 *
 * @param centre Scene units.
 * @param size Width and height in world pixels.
 * @param axisRight Unit vector along artwork +X.
 * @param axisDown Unit vector along artwork +Y.
 * @param rotationDegrees Same rotation convention as the flat path.
 * @param outCorners Receives four corners in QuadCorner order.
 */
inline void MakeOrientedQuad(glm::vec3 centre,
                             glm::vec2 size,
                             glm::vec3 axisRight,
                             glm::vec3 axisDown,
                             float rotationDegrees,
                             glm::vec3 outCorners[QUAD_CORNER_COUNT])
{
    glm::vec3 u = axisRight;
    glm::vec3 v = axisDown;

    if (std::abs(rotationDegrees) > 1e-6f)
    {
        const float rad = rotationDegrees * 3.14159265f / 180.0f;
        const float cosR = std::cos(rad);
        const float sinR = std::sin(rad);
        const glm::vec3 rotatedU = cosR * axisRight + sinR * axisDown;
        const glm::vec3 rotatedV = -sinR * axisRight + cosR * axisDown;
        u = rotatedU;
        v = rotatedV;
    }

    const glm::vec3 halfW = u * (size.x * 0.5f);
    const glm::vec3 halfH = v * (size.y * 0.5f);

    outCorners[QUAD_TOP_LEFT] = centre - halfW - halfH;
    outCorners[QUAD_TOP_RIGHT] = centre + halfW - halfH;
    outCorners[QUAD_BOTTOM_RIGHT] = centre + halfW + halfH;
    outCorners[QUAD_BOTTOM_LEFT] = centre - halfW + halfH;
}

/**
 * @fn void MakeGroundQuad(glm::vec2 worldTopLeft, glm::vec2 size, float height, float \
 * rotationDegrees, glm::vec3 outCorners[QUAD_CORNER_COUNT])
 * @brief Places artwork +X east and +Y south on the ground plane.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Unrotated adjacent tiles share edge coordinates when `worldTopLeft` comes from tile indices.
 * Rotation changes artwork geometry only; it does not select terrain heights.
 *
 * @param worldTopLeft Tile top-left in world pixels.
 * @param size Tile size in world pixels.
 * @param height Scene units.
 * @param rotationDegrees Artwork rotation about the centre.
 * @param outCorners Receives four corners in QuadCorner order.
 */
inline void MakeGroundQuad(glm::vec2 worldTopLeft,
                           glm::vec2 size,
                           float height,
                           float rotationDegrees,
                           glm::vec3 outCorners[QUAD_CORNER_COUNT])
{
    const glm::vec3 centre = ToScene(worldTopLeft + size * 0.5f, height);
    MakeOrientedQuad(centre,
                     size,
                     glm::vec3(1.0f, 0.0f, 0.0f),  // Artwork +X = world east
                     glm::vec3(0.0f, 0.0f, 1.0f),  // Artwork +Y = world south
                     rotationDegrees,
                     outCorners);
}

/**
 * @fn void ApplySlopeHeights(glm::vec2 worldTopLeft, glm::vec2 size, float heightMinus, float \
 * heightPlus, bool alongZ, glm::vec3 outCorners[QUAD_CORNER_COUNT])
 * @brief Samples slope heights at each corner's world position.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Call after MakeGroundQuad so artwork rotation does not rotate the slope axis.
 * Rotated corners outside the cell extrapolate beyond the endpoint heights.
 * A zero span assigns `heightMinus` to every corner. Only scene Y changes; X and Z are retained.
 *
 * @param worldTopLeft Cell top-left in world pixels.
 * @param size Cell size in world pixels.
 * @param heightMinus Scene height at the west or north edge.
 * @param heightPlus Scene height at the east or south edge.
 * @param alongZ True for north-south slopes; false for east-west.
 * @param outCorners Updated in place.
 */
inline void ApplySlopeHeights(glm::vec2 worldTopLeft,
                              glm::vec2 size,
                              float heightMinus,
                              float heightPlus,
                              bool alongZ,
                              glm::vec3 outCorners[QUAD_CORNER_COUNT])
{
    const float span = alongZ ? size.y : size.x;
    const float origin = alongZ ? worldTopLeft.y : worldTopLeft.x;
    for (int i = 0; i < QUAD_CORNER_COUNT; ++i)
    {
        const float along = alongZ ? outCorners[i].z : outCorners[i].x;
        const float t = (span != 0.0f) ? (along - origin) / span : 0.0f;
        outCorners[i].y = heightMinus + (heightPlus - heightMinus) * t;
    }
}

}  // namespace sceneMath
