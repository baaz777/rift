#pragma once

#include "CharacterConstants.hpp"

/**
 * @struct Hitbox
 * @brief Player collision dimensions anchored at the feet bottom-center.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * Player collision uses these dimensions for both player and NPC boxes.
 * NpcAiSystem uses CharacterConstants dimensions directly.
 * Positive Y points down, so the box extends from feet.y - height to feet.y.
 *
 * @verbatim
 *                8px               8px
 *          |<--------------+-------------->|
 *          +-------------------------------+   y = feet.y - height   (top edge)
 *          |                               |
 *          |      16 x 16 collision box    |   height = 16
 *          |                               |
 *          +---------------X---------------+   y = feet.y            (bottom edge)
 *                          ^
 *                          feet anchor = Transform::position
 *                          x spans [feet.x - 8, feet.x + 8]
 * @endverbatim
 */
struct Hitbox
{
    float halfWidth = CharacterConstants::HALF_HITBOX_WIDTH;  ///< Half-width in world pixels.
    float height = CharacterConstants::HITBOX_HEIGHT;  ///< Height above the feet in world pixels.
};
