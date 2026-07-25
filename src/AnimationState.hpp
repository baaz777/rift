#pragma once

/**
 * @struct AnimationState
 * @brief Walk-cycle accumulator.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct AnimationState
{
    int currentFrame{0};
    float animationTime{0.0f};  ///< Seconds accumulated since the last frame advance.
    int walkSequenceIndex{0};
};
