#pragma once

#include "SupportSurface.hpp"

#include <glm/glm.hpp>

class Tilemap;

/**
 * @brief Resolves connections between overlapping ground and elevation surfaces.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Ground remains under every cell. Only ramp low-end edges connect it to elevation.
 * Probes within one tile keep their support. Each crossed boundary tests one edge.
 *
 * MAX_STEP is CharacterConstants::MAX_STEP_HEIGHT. h and src are destination and source
 * heights; dh is the difference from current support height. A ramp low-end requires
 * bare ground on the other side.
 *
 * X ramps require pure east-west movement; Y ramps require pure north-south movement.
 * Diagonal crossings never engage a connector. ElevationAxis::None has no connector.
 *
 * ```mermaid
 * stateDiagram-v2
 *     direction LR
 *
 *     state "Ground (height 0)" as Ground
 *     state "Elevation (height h)" as Elev
 *     state "connected = false" as Blocked
 *
 *     Ground --> Ground: any crossing; ground runs under everything
 *     Ground --> Elev: ramp low-end, axis-aligned, abs(h) at most MAX_STEP
 *     Elev --> Elev: adjacent elevation, abs(dh) at most MAX_STEP
 *     Elev --> Ground: ramp low-end, axis-aligned, abs(src) at most MAX_STEP
 *     Elev --> Blocked: deck edge with no ramp, or step too large
 * ```
 */
namespace SurfaceSystem
{
/**
 * @fn SurfaceTransition ResolveMove(SupportState current, glm::vec2 from, glm::vec2 to, const \
 *     Tilemap& tilemap)
 * @brief Probes support without changing entity state.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Splits each axis into steps of at most max(1 px, 45% of the smaller tile dimension).
 * Stops at the first disconnected boundary so long probes cannot skip ramp connectors.
 * It does not test collision flags or actors, and it does not clamp positions to map bounds.
 *
 * @param current Committed support at the starting feet position.
 * @param from Feet position in world pixels, with +Y down.
 * @param to Candidate feet position in world pixels.
 * @param tilemap Authored cell heights and tile dimensions used by the support probe.
 * @return Last connected support and a connectivity flag. A false flag rejects the whole
 *         move, even when earlier steps reached a different support.
 */
SurfaceTransition ResolveMove(SupportState current,
                              glm::vec2 from,
                              glm::vec2 to,
                              const Tilemap& tilemap);

/**
 * @fn bool CollisionBelongsTo(const Tilemap& tilemap, int tileX, int tileY, SupportState support)
 * @brief Tests exact support identity and height for a collision cell.
 * @author Alex (<https://github.com/lextpf>)
 *
 * MAX_STEP_HEIGHT does not apply here; it gates transitions.
 * The caller must check the cell's collision flag separately.
 */
bool CollisionBelongsTo(const Tilemap& tilemap, int tileX, int tileY, SupportState support);
}  // namespace SurfaceSystem
