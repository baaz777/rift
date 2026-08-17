#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

class Tilemap;

/**
 * @brief Four-connected BFS over NPC navigation, without collision checks.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Patrol routes also reject collision, so these paths do not guarantee NPC traversal.
 * Searches allocate a whole-map visited mask; FindPath also allocates one predecessor
 * index per cell. Invalid endpoints and a valid equal start/goal return before allocation.
 */
namespace Pathfinding
{
/**
 * @fn std::vector<glm::ivec2> FindPath(const Tilemap& tilemap, glm::ivec2 start, glm::ivec2 goal)
 * @brief Find a shortest path through cardinally adjacent navigation cells.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Ties use +X, -X, +Y, -Y order. The returned path includes both endpoints.
 * An empty result means an invalid endpoint or no route.
 *
 * Start and goal are tile coordinates on the NPC navigation grid. Start == goal returns one
 * coordinate when that tile is navigable. Out-of-bounds coordinates count as non-navigable.
 */
[[nodiscard]] std::vector<glm::ivec2> FindPath(const Tilemap& tilemap,
                                               glm::ivec2 start,
                                               glm::ivec2 goal);

/**
 * @fn std::size_t FloodReachable(const Tilemap& tilemap, glm::ivec2 start, glm::ivec2& \
 *     outBoundsMin, glm::ivec2& outBoundsMax)
 * @brief Count the four-connected navigable region from start.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Write inclusive minimum and maximum tile bounds only when the count is nonzero. An invalid start
 * returns zero and leaves both output arguments unchanged.
 */
[[nodiscard]] std::size_t FloodReachable(const Tilemap& tilemap,
                                         glm::ivec2 start,
                                         glm::ivec2& outBoundsMin,
                                         glm::ivec2& outBoundsMax);
}  // namespace Pathfinding
