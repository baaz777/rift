#pragma once

#include "EnumTraits.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

/**
 * @brief Per-layer artwork height rule.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Cell elevation drives support and collision. This role selects which layer artwork rises.
 * Ramp marks sloped artwork because a single cell height cannot express a slope.
 *
 * @verbatim
 *   one cell, elevation 6
 *
 *   layer 2  [deck]   Raised  -> height 6
 *   layer 0  [water]  Ground  -> height 0
 *   ===================================== scene ground plane
 * @endverbatim
 */
enum class ElevationRole : std::uint8_t
{
    Ground = 0,  ///< Sits at height 0 whatever the cell's elevation says.
    Raised,      ///< Sits flat at the cell's elevation (deck, plateau).
    Ramp
};

inline constexpr std::size_t ELEVATION_ROLE_COUNT = 3;

template <>
struct EnumTraits<ElevationRole> : EnumTraitsBase<ElevationRole, EnumTraits<ElevationRole>>
{
    static constexpr std::size_t Count = ELEVATION_ROLE_COUNT;
    static constexpr std::string_view Names[] = {"Ground", "Raised", "Ramp"};
};

static_assert(std::size(EnumTraits<ElevationRole>::Names) == ELEVATION_ROLE_COUNT,
              "ElevationRole names must stay in step with ELEVATION_ROLE_COUNT");

static_assert(std::to_underlying(ElevationRole::Ground) == 0,
              "Ground must be the zero value: it is the default of every per-tile role array, "
              "so a freshly resized map would otherwise come up as a grid of ramps");

/**
 * @brief Artwork height and ramp-edge rules.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
namespace elevationRole
{

/**
 * @fn float SurfaceHeight(int cellElevation, ElevationRole role)
 * @brief Scene height this layer's artwork occupies at a cell, in world pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
inline constexpr float SurfaceHeight(int cellElevation, ElevationRole role)
{
    return (role == ElevationRole::Ground) ? 0.0f : static_cast<float>(cellElevation);
}

/// One neighbouring cell as seen from the same layer.
struct NeighbourSurface
{
    int elevation = 0;  ///< That cell's elevation, in pixels.
    ElevationRole role = ElevationRole::Ground;
};

/**
 * @fn float EdgeHeight(int myElevation, NeighbourSurface neighbour)
 * @brief Ramp edge height from a same-layer neighbour.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Snap to Ground or Raised heights; average adjoining ramps so the deck join has no step.
 *
 * @verbatim
 *   x:      39      40      41      42
 *   elev:    0       2       4       6
 *   role: Ground   Ramp    Ramp   Raised
 *   span:    0    0 -> 3  3 -> 6     6
 *                                 ^ exact, no seam
 * @endverbatim
 */
inline constexpr float EdgeHeight(int myElevation, NeighbourSurface neighbour)
{
    return (neighbour.role == ElevationRole::Ramp)
               ? (static_cast<float>(myElevation) + static_cast<float>(neighbour.elevation)) * 0.5f
               : SurfaceHeight(neighbour.elevation, neighbour.role);
}

}  // namespace elevationRole
