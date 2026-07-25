#pragma once

#include "ColumnProxy.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <vector>

/**
 * @class BoolGrid
 * @brief Row-major tile flags with bounds-checked access.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * X increases right; Y increases down. Dimensions are tile counts.
 * Out-of-bounds reads return false; writes do nothing. Synchronize concurrent writes, including
 * writes to different cells: a packed boolean container can share storage between cells.
 *
 * @code{.cpp}
 * BoolGrid<std::vector> grid;
 * grid.Resize(64, 64);
 * grid[10][20] = true;          // ColumnProxy access
 * bool v = grid[10, 20];        // C++23 multidimensional subscript
 * grid.Set(10, 20, false);      // Named setter
 * if (grid.Get(10, 20)) { ... } // Named getter
 * @endcode
 *
 * @verbatim
 *     Column:  0   1   2   3
 *            +---+---+---+---+
 *   Row 0:   | 0 | 1 | 2 | 3 |
 *            +---+---+---+---+
 *   Row 1:   | 4 | 5 | 6 | 7 |
 *            +---+---+---+---+
 * @endverbatim
 */
template <template <typename...> class Container>
    requires RandomAccessContainerOf<Container<bool>, bool>
             // Second constraint: verify std::ranges::begin/end work
             && requires(Container<bool>& c, std::size_t i) {
                    c.resize(i, false);
                    { c.begin() };
                    { c.end() };
                }
class BoolGrid
{
public:
    /// The concrete container type (Container<bool>).
    using container_type = Container<bool>;

    /// element type (always bool).
    using value_type = bool;

    /// Mutable proxy for grid[x][y] reads and writes.
    using Column = ColumnProxy<container_type, value_type, false>;

    using ConstColumn = ColumnProxy<container_type, value_type, false, false>;

    constexpr BoolGrid() noexcept = default;
    ~BoolGrid() = default;

    BoolGrid(BoolGrid&&) noexcept = default;
    BoolGrid& operator=(BoolGrid&&) noexcept = default;
    BoolGrid(const BoolGrid&) = default;
    BoolGrid& operator=(const BoolGrid&) = default;

    /**
     * @fn void BoolGrid::Resize(int width, int height)
     * @brief Clears all flags; negative tile dimensions become zero.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Resize(int width, int height)
    {
        if (width < 0)
            width = 0;
        if (height < 0)
            height = 0;
        m_Width = width;
        m_Height = height;
        m_Data.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), false);
    }

    constexpr void Set(int x, int y, bool value) noexcept
    {
        if (x >= 0 && x < m_Width && y >= 0 && y < m_Height)
            m_Data[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_Width) +
                   static_cast<std::size_t>(x)] = value;
    }

    [[nodiscard]] constexpr bool Get(int x, int y) const noexcept
    {
        if (x >= 0 && x < m_Width && y >= 0 && y < m_Height)
            return static_cast<bool>(
                m_Data[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_Width) +
                       static_cast<std::size_t>(x)]);
        return false;
    }

    /**
     * @fn std::vector<int> BoolGrid::GetTrueIndices() const
     * @brief Indices of true cells in row-major order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Convert with `x = i % width` and `y = i / width`. An empty grid returns an empty vector.
     *
     * @return A copy of the set indices, in ascending order.
     */
    [[nodiscard]] std::vector<int> GetTrueIndices() const
    {
        std::vector<int> indices;
        indices.reserve(m_Data.size());
        for (auto [i, val] : std::views::enumerate(m_Data))
            if (val)
                indices.push_back(static_cast<int>(i));
        return indices;
    }

    /**
     * @fn void BoolGrid::Clear()
     * @brief Clear all flags to false.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Clear() { std::ranges::fill(m_Data, false); }

    [[nodiscard]] constexpr int GetWidth() const noexcept { return m_Width; }

    [[nodiscard]] constexpr int GetHeight() const noexcept { return m_Height; }

    [[nodiscard]] int GetTrueCount() const
    {
        return static_cast<int>(std::ranges::count(m_Data, true));
    }

    [[nodiscard]] constexpr const container_type& GetData() const noexcept { return m_Data; }

    /**
     * @fn bool BoolGrid::SetData(const container_type& data, int width, int height)
     * @brief Replace the grid when dimensions and data size agree.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Validation failure leaves the grid unchanged. Successful replacement invalidates element
     * proxies from earlier lookups. The data copy can allocate; if it throws, the dimensions
     * have already changed.
     *
     * @param data Row-major flags with exactly width * height entries.
     * @param width Column count; zero is allowed.
     * @param height Row count; zero is allowed.
     * @return False for negative dimensions or a mismatched size; true after replacement.
     */
    bool SetData(const container_type& data, int width, int height)
    {
        if (width < 0 || height < 0)
            return false;
        if (data.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
            return false;
        m_Width = width;
        m_Height = height;
        m_Data = data;
        return true;
    }

    [[nodiscard]] constexpr bool operator[](int x, int y) const noexcept { return Get(x, y); }

    /**
     * @fn constexpr Column BoolGrid::operator[](int x) noexcept
     * @brief Borrow a column for bounds-checked row access.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The grid must outlive the column. Use mutable element proxies before a resize.
     */
    [[nodiscard]] constexpr Column operator[](int x) noexcept
    {
        return Column(&m_Data, &m_Width, &m_Height, x);
    }

    /**
     * @fn constexpr ConstColumn BoolGrid::operator[](int x) const noexcept
     * @brief Borrow a read-only column for bounds-checked row access.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The grid must outlive the column. Use mutable element proxies before a resize.
     */
    [[nodiscard]] constexpr ConstColumn operator[](int x) const noexcept
    {
        return ConstColumn(&m_Data, &m_Width, &m_Height, x);
    }

protected:
    container_type m_Data{};
    int m_Width{0};
    int m_Height{0};
};
