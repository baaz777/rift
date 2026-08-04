#pragma once

#include <cmath>

/**
 * @enum CharacterDirection
 * @brief World-facing direction and logical sprite row.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * PlayerRender permutes rows when RequiresYFlip is true. NpcRender uses the same physical
 * row order directly.
 *
 * | Direction | value (logical row) | physical sheet row |
 * |-----------|---------------------|--------------------|
 * | DOWN      | 0                   | 2                  |
 * | UP        | 1                   | 3                  |
 * | LEFT      | 2                   | 1                  |
 * | RIGHT     | 3                   | 0                  |
 */
enum class CharacterDirection
{
    DOWN = 0,
    UP = 1,
    LEFT = 2,
    RIGHT = 3
};

using Direction = CharacterDirection;

/**
 * @fn CharacterDirection CardinalFromDelta(float dx, float dy)
 * @brief Cardinal facing with vertical priority on equal magnitudes.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @ingroup Entities
 *
 * dy is positive downward. A zero delta returns UP; guard it to retain the current facing.
 */
inline CharacterDirection CardinalFromDelta(float dx, float dy)
{
    if (std::abs(dx) > std::abs(dy))
    {
        return (dx > 0.0f) ? CharacterDirection::RIGHT : CharacterDirection::LEFT;
    }
    return (dy > 0.0f) ? CharacterDirection::DOWN : CharacterDirection::UP;
}
