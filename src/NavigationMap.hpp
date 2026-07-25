#pragma once

#include "BoolGrid.hpp"

/**
 * @class NavigationMap
 * @brief NPC navigation flags independent of player collision; storage and bounds behavior come
 * from BoolGrid.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * @code{.cpp}
 * NavigationMap<std::vector> nav;
 * nav.Resize(64, 64);
 * nav[10][20] = true;
 * nav.SetNavigation(10, 20, true);
 * if (nav.GetNavigation(10, 20)) { ... }
 * if (nav[10, 20]) { ... }  // C++23 multidimensional subscript
 * @endcode
 */
template <template <typename...> class Container>
    requires RandomAccessContainerOf<Container<bool>, bool> &&
             requires(Container<bool>& c, std::size_t i) {
                 c.resize(i, false);
                 { c.begin() };
                 { c.end() };
             }
class NavigationMap : public BoolGrid<Container>
{
public:
    using BoolGrid<Container>::BoolGrid;
    using BoolGrid<Container>::operator[];

    using NavigationColumn = typename BoolGrid<Container>::Column;

    using ConstNavigationColumn = typename BoolGrid<Container>::ConstColumn;

    /**
     * @fn void SetNavigation(int x, int y, bool walkable) noexcept
     * @brief Ignore writes outside the grid.
     * @author Alex (<https://github.com/lextpf>)
     */
    constexpr void SetNavigation(int x, int y, bool walkable) noexcept
    {
        this->Set(x, y, walkable);
    }

    /**
     * @fn bool GetNavigation(int x, int y) const noexcept
     * @brief False outside the grid.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] constexpr bool GetNavigation(int x, int y) const noexcept
    {
        return this->Get(x, y);
    }

    [[nodiscard]] std::vector<int> GetNavigationIndices() const { return this->GetTrueIndices(); }

    [[nodiscard]] int GetNavigationCount() const { return this->GetTrueCount(); }
};
