#pragma once

#include "Motor.hpp"

#include <glm/glm.hpp>

/**
 * @brief Momentum kinematics without collision; callers apply the requested displacement and clear
 * blocked axes.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * ComputeDisplacement updates velocity and stop-target state before collision. The caller applies
 * its accepted displacement to Transform, then clears velocity on blocked axes; this keeps the
 * motor independent of tilemap and actor collision.
 */
namespace MotionSystem
{
/**
 * @fn glm::vec2 ComputeDisplacement(Motor& motor, glm::vec2 position, glm::vec2 inputDir, float \
 *     targetSpeed, float tileSize, float dt)
 * @brief Advance velocity and request a world-pixel displacement.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The first no-input frame predicts v^2 / 2a and latches a grid-aligned stop.
 * Keep that target until input resumes, an axis is cleared, Reset runs, or motion ends.
 * Trapezoidal integration and overshoot clamping land on the target; below speedEpsilon,
 * settleSpeed eases the remaining distance. d in the formula is distance along the heading.
 *
 * position is the feet anchor in world pixels; tileSize is in pixels.
 * inputDir must be normalized or zero. targetSpeed is in px/s and ignored without input.
 * dt is in seconds; non-positive dt returns zero without changing motor.
 * Apply collision before moving and clear blocked axes to prevent velocity accumulation.
 *
 * With input, approach each component of the requested velocity using the configured acceleration.
 * After release, use the latched stopping distance to choose deceleration within the configured
 * bounds. The different X/Y rest grids follow the bottom-center feet anchor in the diagram.
 * $$
 * a_{eff} = \mathrm{clamp}\!\left(\frac{v^{2}}{2d},\; a_{min},\; a_{max}\right),
 * \qquad v' = \max\!\left(0,\; v - a_{eff}\,\Delta t\right),
 * \qquad \Delta x = \frac{v + v'}{2}\,\Delta t
 * $$
 *
 * @verbatim
 *   One 16x16 tile. The feet anchor is bottom-center, so each axis rests on a
 *   different family of grid lines. The asymmetry follows from the anchor
 *   convention, so do not change one axis to match the other.
 *
 *        x=16              x=32
 *          +---------------+       y=16
 *          |               |
 *          |       .       |       . = tile center, x = 24
 *          |               |
 *          +-------X-------+       y=32
 *                  ^
 *                  feet anchor rests at x = 24 (tile center)
 *                                   and y = 32 (tile bottom edge)
 *
 *     AlignedRestX(x) = round((x - w/2) / w) * w + w/2   ->  8, 24, 40, ...
 *     AlignedRestY(y) = round( y        / h) * h         ->  0, 16, 32, ...
 * @endverbatim
 */
glm::vec2 ComputeDisplacement(Motor& motor,
                              glm::vec2 position,
                              glm::vec2 inputDir,
                              float targetSpeed,
                              float tileSize,
                              float dt);

/**
 * @fn bool IsMoving(const Motor& motor)
 * @brief True when velocity magnitude exceeds motor.params.speedEpsilon.
 * @author Alex (<https://github.com/lextpf>)
 */
bool IsMoving(const Motor& motor);

/**
 * @fn void ZeroAxisX(Motor& motor)
 * @brief Clear X velocity and invalidate both latched stop coordinates.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void ZeroAxisX(Motor& motor)
{
    motor.velocity.x = 0.0f;
    motor.hasStopTarget = false;
}

/**
 * @fn void ZeroAxisY(Motor& motor)
 * @brief Clear Y velocity and invalidate both latched stop coordinates.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void ZeroAxisY(Motor& motor)
{
    motor.velocity.y = 0.0f;
    motor.hasStopTarget = false;
}

/**
 * @fn void Reset(Motor& motor)
 * @brief Clear velocity and the stop target after teleports, tile snaps or stuck recovery.
 * @author Alex (<https://github.com/lextpf>)
 */
inline void Reset(Motor& motor)
{
    motor.velocity = glm::vec2(0.0f);
    motor.hasStopTarget = false;
}
}  // namespace MotionSystem
