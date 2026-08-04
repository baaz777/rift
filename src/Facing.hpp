#pragma once

#include "CharacterDirection.hpp"

/**
 * @struct Facing
 * @brief World-absolute cardinal facing.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * cameraFacing::ScreenFacing derives the render row without changing world-facing state.
 */
struct Facing
{
    CharacterDirection dir{CharacterDirection::DOWN};
};
