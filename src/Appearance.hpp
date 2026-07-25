#pragma once

#include "CharacterType.hpp"

#include <glm/glm.hpp>

/**
 * @struct Appearance
 * @brief Player sprite variant and dialogue accent.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * RestoreOriginalAppearance clears usingCopiedAppearance only when the switch succeeds.
 * accentColor is sampled from the walking sheet on each appearance change.
 */
struct Appearance
{
    CharacterType characterType{CharacterType::BW1_MALE};
    bool usingCopiedAppearance{false};
    glm::vec3 accentColor{0.0f};
};
