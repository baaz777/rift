#pragma once

#include "Billboard.hpp"
#include "TileStance.hpp"

/**
 * @brief Derives geometry from authored TileStance.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Sorting flags, layer indices, and neighbours do not determine runtime stance.
 * Tilemap::LoadMapFromJSON uses them only when importing maps without stance data.
 */
namespace tileRole
{

inline constexpr bool IsUpright(TileStance stance)
{
    return stance != TileStance::Flat;
}

/**
 * @fn bool StacksVertically(TileStance stance)
 * @brief Only Structure tiles stack above a shared base row.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Walls and props stay on their own rows, so north-south fences do not become towers.
 *
 * @verbatim
 *   Structure (stacks)             Wall / Prop going north (does not)
 *   rows y..y+2 are one image      rows y..y+2 are three props
 *
 *      [roof ]                        [#]        <- own row, 1 tall
 *      [wall ]   all on row y+2       [#]        <- own row, 1 tall
 *      [door ]                        [#]        <- own row, 1 tall
 *     ========= ground               ============= ground, receding north
 * @endverbatim
 */
inline constexpr bool StacksVertically(TileStance stance)
{
    return stance == TileStance::Structure;
}

/**
 * @fn bool IsSurface(TileStance stance)
 * @brief Walls and structures are surfaces; props can turn around their pivot.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Surfaces keep grid yaw to retain their footprint. They appear edge-on at a perpendicular view.
 *
 * @verbatim
 *   pole (Prop)                 surface (Wall / wide Structure)
 *   pivot == the tile           pivot at the center
 *
 *        [#]                    [#][#][#][#]      authored
 *         ^  spins in place          |
 *         |  anchor held             v
 *                              [#][#]                 far ends swing
 *                                    [#][#]           off their anchors
 * @endverbatim
 */
inline constexpr bool IsSurface(TileStance stance)
{
    return stance == TileStance::Wall || stance == TileStance::Structure;
}

/**
 * @fn bool IsGridLocked(TileStance stance, int structureWidthInTiles)
 * @brief Fixes wall yaw and structure yaw for bodies wider than one tile.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Structure width comes from authored bounds or the body's contiguous run.
 * Prop neighbours do not affect orientation.
 */
inline constexpr bool IsGridLocked(TileStance stance, int structureWidthInTiles)
{
    return stance == TileStance::Wall ||
           (stance == TileStance::Structure && structureWidthInTiles > 1);
}

/**
 * @fn billboard::Damping UprightDamping()
 * @brief Uses scenery damping, which follows only part of the camera yaw.
 * @author Alex (<https://github.com/lextpf>)
 */
inline billboard::Damping UprightDamping()
{
    return billboard::DefaultDamping(billboard::Role::Scenery);
}

/**
 * @fn billboard::Damping DampingFor(TileStance stance, int structureWidthInTiles)
 * @brief Suppresses yaw for grid-locked artwork while retaining common lean.
 * @author Alex (<https://github.com/lextpf>)
 */
inline billboard::Damping DampingFor(TileStance stance, int structureWidthInTiles)
{
    billboard::Damping damping = UprightDamping();
    if (IsGridLocked(stance, structureWidthInTiles))
    {
        damping.yawFollow = 0.0f;
    }
    return damping;
}

/**
 * @fn billboard::Damping DampingForWidth(int widthInTiles)
 * @brief Widths below two tiles retain scenery yaw.
 * @author Alex (<https://github.com/lextpf>)
 */
inline billboard::Damping DampingForWidth(int widthInTiles)
{
    return DampingFor(TileStance::Structure, widthInTiles);
}

}  // namespace tileRole
