#pragma once

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <vector>

/**
 * @brief Containers with resize and resetToDefault operations.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
template <typename F>
concept resettable_container = requires(F& f, size_t n) {
    f.resize(n);
    f.resetToDefault();
};

/**
 * @class defaulted_vector
 * @brief Vector with a compile-time fill value.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * New slots and `resetToDefault` use `Default`. Resizing preserves the retained prefix; resetting
 * fills existing slots without changing the size. Subscript forwards `std::vector<bool>` proxies.
 *
 * @code{.cpp}
 * defaulted_vector<int, -1> tiles;
 * tiles.resize(100);       // 100 elements, all -1
 * tiles[42] = 7;
 * tiles.resetToDefault();  // all 100 elements back to -1
 * @endcode
 */
template <typename T, auto Default>
class defaulted_vector
{
public:
    using value_type = T;
    static constexpr auto default_value = static_cast<T>(Default);

    /**
     * @fn decltype(auto) defaulted_vector::operator[](this auto&& self, size_t i)
     * @brief Unchecked access; the index must be below size.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] decltype(auto) operator[](this auto&& self, size_t i) { return self.m_Data[i]; }

    [[nodiscard]] size_t size() const noexcept { return m_Data.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_Data.empty(); }

    /**
     * @fn auto defaulted_vector::begin(this auto&& self) noexcept
     * @brief Iterator access.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Deducing this forwards const-ness automatically.
     *
     */
    [[nodiscard]] auto begin(this auto&& self) noexcept { return self.m_Data.begin(); }
    [[nodiscard]] auto end(this auto&& self) noexcept { return self.m_Data.end(); }

    /**
     * @fn void defaulted_vector::resize(size_t n)
     * @brief Resize to n elements; new slots initialized to default_value.
     * @author Alex (<https://github.com/lextpf>)
     */
    void resize(size_t n) { m_Data.resize(n, default_value); }

    /**
     * @fn void defaulted_vector::assign(size_t n, const T& value)
     * @brief Replace contents with n copies of value.
     * @author Alex (<https://github.com/lextpf>)
     */
    void assign(size_t n, const T& value) { m_Data.assign(n, value); }

    /**
     * @fn void defaulted_vector::resetToDefault()
     * @brief Fill every element with default_value (size unchanged).
     * @author Alex (<https://github.com/lextpf>)
     */
    void resetToDefault() { std::ranges::fill(m_Data, default_value); }

private:
    std::vector<T> m_Data;
};

/**
 * @fn template <resettable_container... Containers> void resize_all(size_t n, Containers&... \
 *     containers)
 * @brief Resize parallel containers to the same size.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @ingroup Core
 *
 * Containers resize in argument order. If one resize throws, earlier containers stay resized;
 * the operation does not restore their original contents or sizes.
 *
 * @code{.cpp}
 * defaulted_vector<int, -1> tiles;
 * defaulted_vector<float, 0.0f> rotation;
 * resize_all(100, tiles, rotation);  // both resized to 100
 * @endcode
 */
template <resettable_container... Containers>
void resize_all(size_t n, Containers&... containers)
{
    (containers.resize(n), ...);
}

/**
 * @fn template <resettable_container... Containers> void reset_all(Containers&... containers)
 * @brief Fills each container with its own default; sizes remain unchanged.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @ingroup Core
 */
template <resettable_container... Containers>
void reset_all(Containers&... containers)
{
    (containers.resetToDefault(), ...);
}
