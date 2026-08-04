#pragma once

#include "AnimationType.hpp"

/**
 * @struct PlayerModes
 * @brief Bicycle takes priority over run; apply the mode speed multiplier before speedMultiplier.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct PlayerModes
{
    bool isRunning{false};
    bool isBicycling{false};
    /// Game disables world and NPC blocking by passing null collision inputs.
    bool noClip{false};
    float speedMultiplier{1.0f};  ///< developer speed multiplier; 1.0 = normal.

    AnimationType animationType{AnimationType::IDLE};
};
