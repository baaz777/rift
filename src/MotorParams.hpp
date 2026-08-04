#pragma once

/**
 * @struct MotorParams
 * @brief Free-stop prediction uses decel; grid landing solves deceleration within the resolved
 * bounds.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct MotorParams
{
    float accel = 800.0f;  ///< Acceleration in px/s^2.
    /// Free-stop prediction deceleration in px/s^2.
    float decel = 200.0f;
    float minResolvedDecel = 40.0f;    ///< Lower clamp for grid-resolved deceleration, in px/s^2.
    float maxResolvedDecel = 5000.0f;  ///< Upper clamp for grid-resolved deceleration, in px/s^2.
    float settleSpeed = 60.0f;         ///< Gentle pull of an idle axis onto its grid line, in px/s.
    float stopEpsilon = 0.5f;          ///< Distance to target that counts as arrived, in pixels.
    float speedEpsilon = 1.5f;         ///< Speed that counts as stopped, in px/s.
};
