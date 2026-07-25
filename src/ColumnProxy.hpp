#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

/**
 * @brief Random-access storage with readable and writable element proxies.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
template <typename C, typename T>
concept RandomAccessContainerOf = requires(C& c, const C& cc, std::size_t i, T val) {
    { c[i] = val };             // assignable from T
    { static_cast<T>(cc[i]) };  // convertible to T
    { cc.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @class RefProxy
 * @brief Bounds-aware element proxy.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Out-of-bounds writes do nothing; reads return `DefaultValue`. Supports real references and
 * `std::vector<bool>` proxies. The validity flag and flat index are captured at construction;
 * use the proxy before resizing or replacing the backing storage.
 */
template <typename C, typename T, T DefaultValue>
class RefProxy
{
public:
    /**
     * @fn constexpr RefProxy::RefProxy(C* data, std::size_t index, bool valid) noexcept
     * @brief Borrows an element slot; the container must outlive the proxy.
     * @author Alex (<https://github.com/lextpf>)
     *
     * When `valid` is false, `index` is ignored and the container is never dereferenced.
     * Otherwise, supply a non-null container and an index below its size.
     */
    constexpr RefProxy(C* data, std::size_t index, bool valid) noexcept
        : m_Data(data),
          m_Index(index),
          m_Valid(valid)
    {
    }

    /**
     * @fn constexpr RefProxy& RefProxy::operator=(const T& value) noexcept
     * @brief Write through to the element; a no-op when out-of-bounds.
     * @author Alex (<https://github.com/lextpf>)
     */
    constexpr RefProxy& operator=(const T& value) noexcept
    {
        if (m_Valid)
            (*m_Data)[m_Index] = value;
        return *this;
    }

    /**
     * @fn constexpr RefProxy::operator T() const noexcept
     * @brief Read the element, or DefaultValue when out-of-bounds.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] constexpr operator T() const noexcept
    {
        return m_Valid ? static_cast<T>((*m_Data)[m_Index]) : DefaultValue;
    }

private:
    C* m_Data;            ///< non-owning; the caller guarantees it outlives the proxy.
    std::size_t m_Index;  ///< Flat row-major index; meaningless unless m_Valid.
    bool m_Valid;
};

/**
 * @class ColumnProxy
 * @brief Row access for a borrowed column in flat storage.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * The container and dimension pointers must be non-null and outlive the proxy. Dimensions must
 * match the flat storage size. Each row lookup uses the current dimensions; an element proxy
 * already returned from a lookup retains its captured index and validity.
 * Out-of-bounds reads return `DefaultValue`; writes do nothing. `Mutable` controls write access.
 *
 * @code{.cpp}
 * std::vector<bool> flags(64 * 64, false);
 * int w = 64, h = 64;
 *
 * ColumnProxy<std::vector<bool>, bool, false> boolCol(&flags, &w, &h, 10);
 * boolCol[20] = true;                         // Write (Mutable=true)
 * if (boolCol[20]) {}                         // Read
 *
 * ColumnProxy<std::vector<bool>, bool, false, false> readOnly(&flags, &w, &h, 10);
 * bool v = readOnly[20];                      // Read-only (Mutable=false)
 * @endcode
 */
template <typename C, typename T, T DefaultValue = T{}, bool Mutable = true>
    requires RandomAccessContainerOf<C, T>
class ColumnProxy
{
public:
    using container_type = C;
    using value_type = T;

    /// pointer type: C* when Mutable, const C* otherwise.
    using data_ptr = std::conditional_t<Mutable, C*, const C*>;

    /**
     * @fn constexpr ColumnProxy::ColumnProxy(data_ptr data, const int* width, const int* height, \
     *     int x) noexcept
     * @brief Borrows the container and dimensions; all pointers must outlive the proxy.
     * @author Alex (<https://github.com/lextpf>)
     */
    constexpr ColumnProxy(data_ptr data, const int* width, const int* height, int x) noexcept
        : m_Data(data),
          m_Width(width),
          m_Height(height),
          m_X(x)
    {
    }

    /**
     * @fn constexpr RefProxy<C, T, DefaultValue> ColumnProxy::operator[](int y) noexcept
     * @brief Access element at row y (mutable).
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param y Row index.
     * @return proxy that can be assigned to; out-of-bounds assignments are discarded.
     */
    [[nodiscard]] constexpr RefProxy<C, T, DefaultValue> operator[](int y) noexcept
        requires Mutable
    {
        const bool valid = (m_X >= 0 && m_X < *m_Width && y >= 0 && y < *m_Height);
        const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(*m_Width) +
                           static_cast<std::size_t>(m_X);
        return RefProxy<C, T, DefaultValue>(m_Data, valid ? index : 0, valid);
    }

    /**
     * @fn constexpr T ColumnProxy::operator[](int y) const noexcept
     * @brief Access element at row y (read-only).
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param y Row index.
     * @return element value, or DefaultValue if out-of-bounds.
     */
    [[nodiscard]] constexpr T operator[](int y) const noexcept
    {
        if (m_X >= 0 && m_X < *m_Width && y >= 0 && y < *m_Height)
            return static_cast<T>(
                (*m_Data)[static_cast<std::size_t>(y) * static_cast<std::size_t>(*m_Width) +
                          static_cast<std::size_t>(m_X)]);
        return DefaultValue;
    }

private:
    data_ptr m_Data;  ///< non-owning pointer to the flat container.
    /**
     * @brief Pointers, not values, so the proxy sees a live resize of the owning grid.
     *
     * All three pointers must outlive the proxy; it is meant to be a temporary within one
     * expression.
     *
     */
    const int* m_Width;
    const int* m_Height;
    int m_X;
};

/// Read-only column proxy with the same out-of-bounds default as ColumnProxy.
template <typename C, typename T, T DefaultValue = T{}>
using ConstColumnProxy = ColumnProxy<C, T, DefaultValue, false>;
