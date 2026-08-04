#pragma once

#include "MotorParams.hpp"

#include <glm/glm.hpp>

/**
 * @struct Motor
 * @brief Velocity and latched grid-stop state consumed by MotionSystem.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct Motor
{
    glm::vec2 velocity{0.0f, 0.0f};  ///< Current velocity in pixels/second.
    MotorParams params{};
    glm::vec2 stopTarget{0.0f, 0.0f};
    /// Collision on either axis must invalidate the stop target.
    bool hasStopTarget{false};
};
