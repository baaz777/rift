#pragma once

#include <cstddef>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

/**
 * @brief Enum name lookup from Count and an index-parallel Names array.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Values must be contiguous from zero to `Count - 1`, with one name per value in `Names`.
 * `ToString` returns `Unknown` outside that range. `FromString` matches exact case and returns
 * `nullopt` if absent. Name spelling is part of console and manifest input.
 */
template <typename E, typename Derived>
struct EnumTraitsBase
{
    static constexpr std::string_view ToString(E value)
    {
        auto i = std::to_underlying(value);
        return static_cast<size_t>(i) < Derived::Count ? Derived::Names[i] : "Unknown";
    }

    static constexpr std::optional<E> FromString(std::string_view name)
    {
        for (size_t i = 0; i < Derived::Count; ++i)
            if (Derived::Names[i] == name)
                return static_cast<E>(i);
        return std::nullopt;
    }
};

/**
 * @brief Specialize with Count and Names in underlying-value order.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * @code{.cpp}
 * enum class Color { Red = 0, Green = 1, Blue = 2 };
 *
 * template<>
 * struct EnumTraits<Color> : EnumTraitsBase<Color, EnumTraits<Color>> {
 *     static constexpr size_t Count = 3;
 *     static constexpr std::string_view Names[] = { "Red", "Green", "Blue" };
 * };
 * @endcode
 */
template <typename E>
struct EnumTraits;

/**
 * @fn constexpr E NextEnum(E value)
 * @brief Advances through contiguous zero-based values, wrapping at Count.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * @code{.cpp}
 * auto next = NextEnum(CharacterType::BW1_MALE);  // BW1_FEMALE
 * auto wrap = NextEnum(CharacterType::CC_FEMALE);  // BW1_MALE
 * @endcode
 */
template <typename E>
    requires requires { EnumTraits<E>::Count; }
constexpr E NextEnum(E value)
{
    return static_cast<E>((std::to_underlying(value) + 1) % EnumTraits<E>::Count);
}

/**
 * @fn constexpr auto EnumValues()
 * @brief Iterates contiguous zero-based enum values below Count.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * @code{.cpp}
 * for (auto pt : EnumValues<ParticleType>())
 *     std::cout << EnumTraits<ParticleType>::ToString(pt) << "\n";
 * @endcode
 */
template <typename E>
    requires requires { EnumTraits<E>::Count; }
constexpr auto EnumValues()
{
    return std::views::iota(size_t{0}, EnumTraits<E>::Count) |
           std::views::transform([](size_t i) { return static_cast<E>(i); });
}

/**
 * @fn constexpr void ForEachEnum(Fn&& fn)
 * @brief Call once per trait value in underlying-value order.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Discard callback results. If the callback throws, iteration stops and earlier calls remain
 * applied. The named callback is invoked as an lvalue on every iteration.
 */
template <typename E, typename Fn>
    requires requires { EnumTraits<E>::Count; }
constexpr void ForEachEnum(Fn&& fn)
{
    for (auto v : EnumValues<E>())
        fn(v);
}
