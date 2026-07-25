#pragma once

#include "BoolGrid.hpp"

/**
 * @class CollisionMap
 * @brief Authored tile collision flags backed by BoolGrid.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Collision and NPC navigation are independent grids. Inherited bounds handling returns
 * false on reads and ignores writes outside the map.
 *
 * ```mermaid
 * classDiagram
 *     class BoolGrid~Container~ {
 *         +Resize(w, h)
 *         +Set(x, y, value)
 *         +Get(x, y) bool
 *         +operator[](x) Column
 *     }
 *     class CollisionMap~Container~ {
 *         +SetCollision(x, y, blocking)
 *         +HasCollision(x, y) bool
 *         +GetCollisionIndices() vector~int~
 *     }
 *     class NavigationMap~Container~ {
 *         +SetNavigation(x, y, walkable)
 *         +GetNavigation(x, y) bool
 *         +GetNavigationIndices() vector~int~
 *     }
 *     BoolGrid <|-- CollisionMap
 *     BoolGrid <|-- NavigationMap
 * ```
 *
 * @code{.cpp}
 * CollisionMap<std::vector> col;
 * col.Resize(64, 64);
 * col[10][20] = true;
 * col.SetCollision(10, 20, true);
 * if (col.HasCollision(10, 20)) { ... }
 * if (col[10, 20]) { ... }  // C++23 multidimensional subscript
 * @endcode
 */
template <template <typename...> class Container>
    requires RandomAccessContainerOf<Container<bool>, bool> &&
             requires(Container<bool>& c, std::size_t i) {
                 c.resize(i, false);
                 { c.begin() };
                 { c.end() };
             }
class CollisionMap : public BoolGrid<Container>
{
public:
    using BoolGrid<Container>::BoolGrid;
    using BoolGrid<Container>::operator[];

    /// Mutable proxy into one collision-grid column.
    using CollisionColumn = typename BoolGrid<Container>::Column;

    using ConstCollisionColumn = typename BoolGrid<Container>::ConstColumn;

    /**
     * @fn void SetCollision(int x, int y, bool collision) noexcept
     * @brief Set collision flag for a tile.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param x         Column (out-of-bounds ignored).
     * @param y         Row (out-of-bounds ignored).
     * @param collision `true` if blocking, `false` if passable.
     */
    constexpr void SetCollision(int x, int y, bool collision) noexcept
    {
        this->Set(x, y, collision);
    }

    /**
     * @fn bool HasCollision(int x, int y) const noexcept
     * @brief Query if a tile blocks movement.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return `true` if blocking, `false` if passable or out-of-bounds.
     */
    [[nodiscard]] constexpr bool HasCollision(int x, int y) const noexcept
    {
        return this->Get(x, y);
    }

    [[nodiscard]] std::vector<int> GetCollisionIndices() const { return this->GetTrueIndices(); }

    [[nodiscard]] int GetCollisionCount() const { return this->GetTrueCount(); }
};
