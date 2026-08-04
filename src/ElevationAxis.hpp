#pragma once

#include <cstdint>

/**
 * @enum ElevationAxis
 * @brief Axis along which a tile engages elevation support.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Tilemap chooses the stronger elevation gradient, then the longer elevated span, then X.
 * Ground connects to a ramp only through an axis-aligned low-end crossing. Diagonal or
 * perpendicular ground crossings stay on ground beneath the elevation. An elevated actor
 * can cross to adjacent elevation within the step limit without matching the ramp axis.
 * None denotes height zero. Exits to ground use the source axis and must pass the step limit.
 *
 * @verbatim
 *   Top-down tile elevations for an east-west (X-axis) bridge; 0 = bare ground.
 *
 *        +---+---+---+---+---+
 *   N    | 0 | 0 | 0 | 0 | 0 |   A north/south traveller has moveDy != 0, so an
 *   ^    +---+---+---+---+---+   X-axis cell never engages: it keeps its Ground
 *   |    | 0 | 4 | 8 | 4 | 0 |   support and walks under the bridge.
 *   v    +---+---+---+---+---+
 *   S    | 0 | 0 | 0 | 0 | 0 |   Every nonzero cell in that row reports axis X.
 *        +---+---+---+---+---+
 *           ramp  top  ramp
 *
 *   Side view, west <-> east travel (moveDy == 0, so the axis does engage):
 *
 *     8                _______
 *     4          _____/       \_____
 *     0   ______/                   \______   <- ground, and the under-bridge lane
 *              |     |       |     |
 *              enter climb  descend exit
 *              0->4  4->8    8->4   4->0
 * @endverbatim
 */
enum class ElevationAxis : uint8_t
{
    None = 0,
    X = 1,
    Y = 2,
};
