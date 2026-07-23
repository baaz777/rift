#pragma once

#include "SupportSurface.hpp"
#include "Tilemap.hpp"

#include <entt/entt.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

class IRenderer;

/**
 * @enum DrawableClass
 * @brief Selects the character or tile draw path.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
enum class DrawableClass : std::uint8_t
{
    Entity = 0,  ///< A complete character; drawn through Drawable::drawEntity.
    Tile = 1,    ///< A world tile; drawn through Tilemap::RenderSingleTile.
};

/**
 * @enum DrawablePhase
 * @brief Preserves authored background, Y-sort, and foreground roles.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Elevated artwork retains its authored phase. explicit Y-sort tiles remain peers of actors.
 */
enum class DrawablePhase : std::uint8_t
{
    Background = 0,  ///< Drawn before every actor.
    YSorted = 1,     ///< Depth-sorted as a peer of actors.
    Foreground = 2,  ///< Drawn after every actor.
};

/**
 * @struct Drawable
 * @brief One character or tile in the support-aware depth pass.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Phase sets the baseline painter order; sortY orders peers within a phase.
 * SortDrawables adds constraints only within a shared elevation footprint.
 *
 * Normal equal-depth ties draw higher tieBias first: tile, NPC, player.
 * ySortMinus pushes the tile compare point forward by half a tile and reverses the tie order.
 *
 * One character occupies one entry. Its draw thunk submits all sprite parts consecutively.
 *
 * @code
 *   Background -> YSorted -> Foreground
 *                     depth = sortY
 * @endcode
 *
 * @code
 *   TIE_TILE   (4)   <- back
 *   TIE_NPC    (3)
 *   TIE_PLAYER (1)   <- front
 * @endcode
 *
 * @verbatim
 *   normal tile vs entity          ysortMinus tile vs entity
 *
 *   tile base   @ 96               tile base   @ 96  (+8 offset)
 *   entity feet @ 100              entity feet @ 100
 *   96  < 100 -> tile behind       104 > 100 -> tile in front
 *                                  (feet up to 8px below the base
 *                                   still pass behind the tile)
 * @endverbatim
 */
struct Drawable
{
    float sortY = 0.0f;                         ///< Authored depth key; smaller sorts further back.
    float supportHeight = 0.0f;                 ///< Surface metadata; never a global Y-sort offset.
    DrawableClass cls = DrawableClass::Entity;  ///< Entity vs tile; draw-loop discriminant.
    DrawablePhase phase = DrawablePhase::YSorted;  ///< Authored baseline render role.
    int surfaceRegionId = -1;  ///< Local elevation footprint shared by tiles/actors.
    SupportSurface supportSurface{SupportSurface::Ground};  ///< Actor topology; ignored for tiles.
    bool isYSortMinus = false;              ///< Tile occlusion flag; false for entities.
    std::uint8_t tieBias = 0;               ///< Equal-depth order (see DrawableDepthLess).
    const entt::registry* world = nullptr;  ///< Registry for the thunk; null for tiles.
    entt::entity handle = entt::null;       ///< The player/NPC entity (none for tiles).
    /**
     * @brief Submits all character sprite parts consecutively; null for tiles.
     *
     * The thunk resolves components from world and handle at draw time.
     */
    void (*drawEntity)(const Drawable&, IRenderer&, glm::vec2) = nullptr;
    Tilemap::DepthSortedTile tile{};  ///< Tile payload; valid when cls is tile.
};

inline constexpr std::uint8_t TIE_PLAYER = 1;
inline constexpr std::uint8_t TIE_NPC = 3;
inline constexpr std::uint8_t TIE_TILE = 4;

/**
 * @fn DrawablePhase TileDrawablePhase(const Tilemap::DepthSortedTile& tile)
 * @brief Preserve the tile's authored fixed-pass or explicit Y-sort role.
 * @author Alex (<https://github.com/lextpf>)
 */
inline DrawablePhase TileDrawablePhase(const Tilemap::DepthSortedTile& tile)
{
    // ySortPlus/ySortMinus is an explicit authored rule, regardless of the
    // tile's fixed layer or elevation ownership. surface-local constraints may
    // override it only for an actor actually under/on the same structure.
    if (tile.authoredYSort)
    {
        return DrawablePhase::YSorted;
    }
    return tile.isBackground ? DrawablePhase::Background : DrawablePhase::Foreground;
}

inline constexpr float YSORT_MINUS_OFFSET = 8.0f;   ///< Half-tile front push for ySortMinus tiles.
inline constexpr float YSORT_MINUS_EPSILON = 0.1f;  ///< Near-tie band for ySortMinus vs entity.
inline constexpr float YSORT_DEPTH_EPSILON = 1.0f;  ///< ~1px depth-stability band before tieBias.

/**
 * @fn bool DrawableYSortLess(const Drawable& a, const Drawable& b)
 * @brief Orders Y-sort peers back to front with depth tolerances.
 * @author Alex (<https://github.com/lextpf>)
 *
 * For a ySortMinus tile against an entity, offset the tile by YSORT_MINUS_OFFSET.
 * Within YSORT_MINUS_EPSILON, lower tieBias draws first. other pairs use
 * YSORT_DEPTH_EPSILON and draw higher tieBias first.
 *
 * @code
 *   entity feet @ 100,  tile base @ 96
 *     normal tile:      96       < 100  ->  tile first   (behind entity)
 *     ySortMinus tile:  96+8=104 > 100  ->  entity first (tile in front)
 * @endcode
 *
 * @warning The epsilon bands can violate transitivity. At equal tieBias, depths 100.0,
 * 100.8, and 101.6 make adjacent pairs equivalent but order the outer pair.
 * Neither std::sort nor std::stable_sort guarantees valid results with this predicate.
 */
inline bool DrawableYSortLess(const Drawable& a, const Drawable& b)
{
    const bool aIsEntity = (a.cls == DrawableClass::Entity);
    const bool bIsEntity = (b.cls == DrawableClass::Entity);

    if ((a.isYSortMinus && bIsEntity) || (b.isYSortMinus && aIsEntity))
    {
        const float aSortY = a.sortY + (a.isYSortMinus ? YSORT_MINUS_OFFSET : 0.0f);
        const float bSortY = b.sortY + (b.isYSortMinus ? YSORT_MINUS_OFFSET : 0.0f);
        if (std::abs(aSortY - bSortY) > YSORT_MINUS_EPSILON)
        {
            return aSortY < bSortY;
        }
        return a.tieBias < b.tieBias;
    }

    if (std::abs(a.sortY - b.sortY) > YSORT_DEPTH_EPSILON)
    {
        return a.sortY < b.sortY;
    }

    return a.tieBias > b.tieBias;
}

/**
 * @fn bool DrawableDepthLess(const Drawable& a, const Drawable& b)
 * @brief Baseline order: authored phase first, then ordinary Y-sort rules.
 * @author Alex (<https://github.com/lextpf>)
 */
inline bool DrawableDepthLess(const Drawable& a, const Drawable& b)
{
    if (a.phase != b.phase)
    {
        return a.phase < b.phase;
    }
    return DrawableYSortLess(a, b);
}

/**
 * @fn void AddNpcDrawable(std::vector<Drawable>& list, const entt::registry& world, entt::entity \
 * npc, glm::vec2 feetPos, SupportSurface supportSurface, float supportHeight, int \
 * surfaceRegionId, std::uint8_t tieBias)
 * @brief Queues an NPC without caching component references.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param list Receives one atomic character entry; existing entries remain queued.
 * @param world Borrowed registry used to resolve components when the entry is drawn.
 * @param npc NPC entity whose required components remain valid until drawing.
 * @param feetPos Feet anchor in world pixels.
 * @param supportSurface Committed ground or elevated support for local occlusion constraints.
 * @param supportHeight Committed physical elevation; metadata only.
 * @param surfaceRegionId Elevation footprint under the feet, or -1.
 * @param tieBias Equal-depth priority, normally TIE_NPC or TIE_PLAYER.
 * @pre World, npc, and its transform, elevation, facing, AnimationState, and NpcSprite
 * components must remain valid until the list is drawn or cleared.
 */
void AddNpcDrawable(std::vector<Drawable>& list,
                    const entt::registry& world,
                    entt::entity npc,
                    glm::vec2 feetPos,
                    SupportSurface supportSurface,
                    float supportHeight,
                    int surfaceRegionId,
                    std::uint8_t tieBias);

/**
 * @fn void AddPlayerDrawable(std::vector<Drawable>& list, const entt::registry& world, \
 * entt::entity player, glm::vec2 feetPos, SupportSurface supportSurface, float supportHeight, \
 * int surfaceRegionId, std::uint8_t tieBias)
 * @brief Queues a player without caching component references.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param list Receives one atomic character entry; existing entries remain queued.
 * @param world Borrowed registry used to resolve components when the entry is drawn.
 * @param player Player entity whose required components remain valid until drawing.
 * @param feetPos Feet anchor in world pixels.
 * @param supportSurface Committed ground or elevated support for local occlusion constraints.
 * @param supportHeight Committed physical elevation; metadata only.
 * @param surfaceRegionId Elevation footprint under the feet, or -1.
 * @param tieBias Equal-depth priority, normally TIE_NPC or TIE_PLAYER.
 * @pre World, player, and its transform, elevation, facing, AnimationState, PlayerModes,
 * and PlayerSprite components must remain valid until the list is drawn or cleared.
 */
void AddPlayerDrawable(std::vector<Drawable>& list,
                       const entt::registry& world,
                       entt::entity player,
                       glm::vec2 feetPos,
                       SupportSurface supportSurface,
                       float supportHeight,
                       int surfaceRegionId,
                       std::uint8_t tieBias);

/**
 * @fn void SortDrawables(std::vector<Drawable>& list)
 * @brief Applies structure-local constraints after the baseline painter sort.
 * @author Alex (<https://github.com/lextpf>)
 *
 * A ground actor precedes tiles in its elevation footprint. background surface tiles
 * precede elevated actors in that footprint. negative region IDs add no constraints.
 *
 * Indices are invalidated. A min-heap kahn pass preserves baseline order for unconstrained
 * pairs. A cycle leaves the baseline order unchanged without reporting an error.
 *
 * ```mermaid
 * flowchart LR
 * GA["Ground actor"]:::node
 * EA["Elevated actor"]:::node
 * RT["Region tile"]:::node
 * BT["Background surface tile"]:::node
 * YT["Authored Y-sort tile"]:::node
 * OUT["Actor with surfaceRegionId &lt; 0
 * (no edges)"]:::loose
 * classDef node fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 * classDef loose fill:#3f3f46,stroke:#71717a,color:#e2e8f0
 * GA -->|draws before: underpass| RT
 * BT -->|draws before: deck| EA
 * YT <-->|direction from DrawableYSortLess| EA
 * ```
 *
 * @note Allocates scratch storage each call. edge count is actors times tiles per region.
 */
void SortDrawables(std::vector<Drawable>& list);
