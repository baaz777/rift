#pragma once

#include <cmath>
#include <glm/glm.hpp>

/**
 * @brief Defines world-to-tile row conventions.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * | Query           | Y adjustment                | Use                    |
 * |-----------------|-----------------------------|------------------------|
 * | TileIndex       | None.                       | Columns and bare cells |
 * | StandingTileRow | Subtract STANDING_EPS.      | Patrol and interaction |
 * | AnchorTileRow   | Subtract half a tile + eps. | Occupied body cell     |
 *
 * TileFeetCenter returns bottom-center feet coordinates. Coordinates and tile sizes are pixels;
 * tile sizes must be positive. The trace uses 16-pixel tiles with +Y down.
 *
 * @verbatim
 *   world Y
 *      0  +---------------------+  row 0
 *         |                     |
 *     16  +---------------------+  row 1   <- feet A (y = 16.0), on the boundary
 *     20  :  . . . . . . . . .  :          <- feet B (y = 20.0)
 *     24  :  . . . . . . . . .  :          <- feet C (y = 24.0), mid-tile
 *     32  +---------------------+  row 2   <- feet D (y = 32.0), on the boundary
 *
 *     feetY   TileIndex   StandingTileRow   AnchorTileRow
 *      16.0       1              0                0
 *      20.0       1              1                0
 *      24.0       1              1                1
 *      32.0       2              1                1
 * @endverbatim
 */

namespace TileMath
{
/// Pixel offset that assigns boundary-standing feet to the row above.
inline constexpr float STANDING_EPS = 0.1f;

/**
 * @fn int TileIndex(float coord, float tileSize)
 * @brief Bare floor index: tile column from world X, or a bare-floor row from Y.
 * @author Alex (<https://github.com/lextpf>)
 */
inline int TileIndex(float coord, float tileSize)
{
    return static_cast<int>(std::floor(coord / tileSize));
}

/**
 * @fn int StandingTileRow(float feetY, float tileHeight)
 * @brief Find the standing row with a small upward boundary offset.
 * @author Alex (<https://github.com/lextpf>)
 */
inline int StandingTileRow(float feetY, float tileHeight)
{
    return static_cast<int>(std::floor((feetY - STANDING_EPS) / tileHeight));
}

/**
 * @fn int AnchorTileRow(float feetY, float tileHeight, float eps = 0.0f)
 * @brief Find the occupied body row by shifting feet upward by half a tile.
 * @author Alex (<https://github.com/lextpf>)
 */
inline int AnchorTileRow(float feetY, float tileHeight, float eps = 0.0f)
{
    return static_cast<int>(std::floor((feetY - tileHeight * 0.5f - eps) / tileHeight));
}

/**
 * @fn glm::vec2 TileFeetCenter(int tileX, int tileY, float tileWidth, float tileHeight)
 * @brief Return the tile bottom-center feet anchor in world pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec2 TileFeetCenter(int tileX, int tileY, float tileWidth, float tileHeight)
{
    return glm::vec2(tileX * tileWidth + tileWidth * 0.5f, tileY * tileHeight + tileHeight);
}

inline glm::vec2 TileFeetCenter(int tileX, int tileY, float tileSize)
{
    return TileFeetCenter(tileX, tileY, tileSize, tileSize);
}
}  // namespace TileMath
