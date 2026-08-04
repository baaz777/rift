#pragma once

#include <glm/glm.hpp>

/**
 * @brief Feet-anchored AABB helpers.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * World pixels use downward Y. Boxes extend halfWidth to either side of the feet and
 * boxHeight above them; eps shrinks each edge inward.
 */
namespace CollisionGeometry
{
/**
 * @struct Aabb
 * @brief World-pixel bounds with downward Y.
 *
 * Bounds are not validated. An epsilon that inverts a box does not guarantee no overlap.
 */
struct Aabb
{
    float minX;  ///< Left edge, in world pixels.
    float maxX;  ///< Right edge, in world pixels.
    float minY;  ///< Top edge, the smaller Y, in world pixels.
    float maxY;  ///< Bottom edge, the larger Y, in world pixels.
};

/**
 * @struct Hitbox
 * @brief Local hitbox dimensions.
 *
 * Entity collision uses the distinct global Hitbox component; the helpers take dimensions
 * directly.
 */
struct Hitbox
{
    float halfWidth;  ///< Half-width to each side of the feet, in world pixels.
    float height;     ///< Box height above the feet, in world pixels.
};

/**
 * @fn Aabb MakeFeetAabb(glm::vec2 feet, float halfWidth, float boxHeight, float eps = 0.0f)
 * @brief Build a feet-anchored AABB, anchored at bottom-center.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param feet       Feet position in world pixels.
 * @param halfWidth  Half-width to each side of the feet.
 * @param boxHeight  Box height above the feet.
 * @param eps        Amount to shrink the box inward on every side, which avoids edge-on-edge
 *                   false positives. Pass 0 for an exact box.
 */
inline Aabb MakeFeetAabb(glm::vec2 feet, float halfWidth, float boxHeight, float eps = 0.0f)
{
    return Aabb{
        feet.x - halfWidth + eps, feet.x + halfWidth - eps, feet.y - boxHeight + eps, feet.y - eps};
}

/**
 * @fn bool AabbOverlap(const Aabb& a, const Aabb& b)
 * @brief Test for positive overlap on both axes.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Exact edge contact is not overlap. Supply ordered bounds; inverted boxes are not rejected.
 */
inline bool AabbOverlap(const Aabb& a, const Aabb& b)
{
    return a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY && a.maxY > b.minY;
}

/**
 * @fn bool FeetBoxesOverlap(glm::vec2 a, glm::vec2 b, float halfWidth, float boxHeight, float \
 *     eps)
 * @brief Test two same-size feet-anchored boxes for overlap.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param a          Feet position of the first box.
 * @param b          Feet position of the second box.
 * @param halfWidth  Half-width shared by both boxes.
 * @param boxHeight  Height shared by both boxes.
 * @param eps        Inward shrink applied to both boxes before the test.
 * @return           True when the shrunk boxes overlap.
 */
inline bool FeetBoxesOverlap(glm::vec2 a, glm::vec2 b, float halfWidth, float boxHeight, float eps)
{
    return AabbOverlap(MakeFeetAabb(a, halfWidth, boxHeight, eps),
                       MakeFeetAabb(b, halfWidth, boxHeight, eps));
}
}  // namespace CollisionGeometry
