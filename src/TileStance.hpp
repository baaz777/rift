#pragma once

#include "EnumTraits.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

/**
 * @brief Authored geometry role, independent of per-tile sorting flags.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Props turn around their cell anchor. Walls keep grid yaw so their endpoints stay fixed.
 *
 * @verbatim
 *   Flat        Prop          Wall            Structure
 *   grass       lantern       fence panel     house facade
 *   path        bush          hedge           bridge railing
 *   water       signpost      low retaining   cliff face
 *
 *   lies on     stands,       stands,         stands, and is one
 *   the         turns to      locked to       tile of a taller body
 *   ground      the camera    the grid        anchored on its base row
 * @endverbatim
 */
enum class TileStance : std::uint8_t
{
    Flat = 0,  ///< Ground artwork: lies on the ground plane, drawn depth-free.
    Prop,      ///< Upright pole: one tile tall on its own row, always turns to the camera.
    Wall,      ///< Upright surface: one tile tall on its own row, locked to the grid.
    Structure  ///< Upright surface: one tile of a multi-tile-tall body on its base row.
};

inline constexpr std::size_t TILE_STANCE_COUNT = 4;

template <>
struct EnumTraits<TileStance> : EnumTraitsBase<TileStance, EnumTraits<TileStance>>
{
    static constexpr std::size_t Count = TILE_STANCE_COUNT;
    static constexpr std::string_view Names[] = {"Flat", "Prop", "Wall", "Structure"};
};

static_assert(std::size(EnumTraits<TileStance>::Names) == TILE_STANCE_COUNT,
              "TileStance names must stay in step with TILE_STANCE_COUNT");

static_assert(std::to_underlying(TileStance::Flat) == 0,
              "Flat must be the zero value: it is the default of every per-tile stance array, "
              "so a freshly resized map would otherwise stand its whole floor on edge");
