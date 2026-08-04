#pragma once

#include "EnumTraits.hpp"

/**
 * @enum CharacterType
 * @brief Available player character sprite variants.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * Each character type owns three sprite sheets: walking, which also holds the idle pose;
 * running; and bicycle.
 */
enum class CharacterType
{
    BW1_MALE = 0,    ///< black and white 1 male protagonist.
    BW1_FEMALE = 1,  ///< black and white 1 female protagonist.
    BW2_MALE = 2,    ///< black and white 2 male protagonist.
    BW2_FEMALE = 3,  ///< black and white 2 female protagonist.
    CC_FEMALE = 4
};

/**
 * @brief Names indexed by CharacterType value.
 * @ingroup Entities
 *
 * Keep Names in enum order; renaming a string changes console and asset lookup spelling.
 */
template <>
struct EnumTraits<CharacterType> : EnumTraitsBase<CharacterType, EnumTraits<CharacterType>>
{
    static constexpr size_t Count = 5;
    /**
     * @brief Identifier spelling per enumerator, in declaration order.
     *
     * Console and asset-registry lookups round-trip through these, so renaming one is a
     * user-visible change.
     *
     */
    static constexpr std::string_view Names[] = {
        "BW1_MALE", "BW1_FEMALE", "BW2_MALE", "BW2_FEMALE", "CC_FEMALE"};

    // pins the last enumerator to Count - 1, which only holds while the values stay a dense
    // 0-based run.
    static_assert(std::to_underlying(CharacterType::CC_FEMALE) == Count - 1,
                  "Update EnumTraits<CharacterType> when adding new CharacterType values");
};
