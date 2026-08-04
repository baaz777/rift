#include "MotionSystem.hpp"

#include <algorithm>
#include <cmath>

namespace
{

float ApproachScalar(float value, float target, float maxDelta)
{
    float diff = target - value;
    if (std::abs(diff) <= maxDelta)
    {
        return target;
    }
    return value + (diff > 0.0f ? maxDelta : -maxDelta);
}

// Bottom-center feet rest at X tile centers (8, 24, 40, ...) and Y bottom edges (0, 16, 32, ...).

float AlignedRestX(float x, float tileSize)
{
    return std::round((x - tileSize * 0.5f) / tileSize) * tileSize + tileSize * 0.5f;
}

float AlignedRestY(float y, float tileSize)
{
    return std::round(y / tileSize) * tileSize;
}

// Integrate one axis to its grid target; update velocity and return displacement.
float ResolveAxisStop(float pos, float target, float& vel, const MotorParams& p, float dt)
{
    float toTarget = target - pos;

    if (std::abs(vel) <= p.speedEpsilon)
    {
        vel = 0.0f;
        if (std::abs(toTarget) <= p.stopEpsilon)
        {
            return 0.0f;
        }
        float settleStep = p.settleSpeed * dt;
        return std::clamp(toTarget, -settleStep, settleStep);
    }

    float dir = (vel > 0.0f) ? 1.0f : -1.0f;
    float remaining = toTarget * dir;

    if (remaining <= p.stopEpsilon)
    {
        vel = 0.0f;
        return toTarget;
    }

    // Solve v^2 / 2d, bounded by the configured deceleration limits.
    float speed = std::abs(vel);
    float effDecel =
        std::clamp((speed * speed) / (2.0f * remaining), p.minResolvedDecel, p.maxResolvedDecel);
    float newSpeed = std::max(0.0f, speed - effDecel * dt);
    float disp = (speed + newSpeed) * 0.5f * dt * dir;

    if (disp * dir > remaining)
    {
        disp = toTarget;
        newSpeed = 0.0f;
    }

    vel = newSpeed * dir;
    return disp;
}
}  // namespace

namespace MotionSystem
{
glm::vec2 ComputeDisplacement(Motor& motor,
                              glm::vec2 position,
                              glm::vec2 inputDir,
                              float targetSpeed,
                              float tileSize,
                              float dt)
{
    if (dt <= 0.0f)
    {
        return glm::vec2(0.0f);
    }

    const bool hasInput = glm::length(inputDir) > 1e-4f;

    if (hasInput)
    {
        motor.hasStopTarget = false;
        glm::vec2 targetVel = glm::normalize(inputDir) * targetSpeed;
        float step = motor.params.accel * dt;
        motor.velocity.x = ApproachScalar(motor.velocity.x, targetVel.x, step);
        motor.velocity.y = ApproachScalar(motor.velocity.y, targetVel.y, step);
        return motor.velocity * dt;
    }

    // Stopping uses remaining distance and MotorParams, independent of targetSpeed.
    (void)targetSpeed;

    if (!motor.hasStopTarget)
    {
        // Latch once; recalculating the free-stop prediction each frame would prevent convergence.
        if (std::abs(motor.velocity.x) > motor.params.speedEpsilon)
        {
            float dir = (motor.velocity.x > 0.0f) ? 1.0f : -1.0f;
            float natural = (motor.velocity.x * motor.velocity.x) / (2.0f * motor.params.decel);
            motor.stopTarget.x = AlignedRestX(position.x + dir * natural, tileSize);
        }
        else
        {
            motor.stopTarget.x = AlignedRestX(position.x, tileSize);
        }

        if (std::abs(motor.velocity.y) > motor.params.speedEpsilon)
        {
            float dir = (motor.velocity.y > 0.0f) ? 1.0f : -1.0f;
            float natural = (motor.velocity.y * motor.velocity.y) / (2.0f * motor.params.decel);
            motor.stopTarget.y = AlignedRestY(position.y + dir * natural, tileSize);
        }
        else
        {
            motor.stopTarget.y = AlignedRestY(position.y, tileSize);
        }

        motor.hasStopTarget = true;
    }

    glm::vec2 disp(0.0f);
    disp.x = ResolveAxisStop(position.x, motor.stopTarget.x, motor.velocity.x, motor.params, dt);
    disp.y = ResolveAxisStop(position.y, motor.stopTarget.y, motor.velocity.y, motor.params, dt);

    if (!IsMoving(motor) && glm::length(disp) < 1e-4f)
    {
        motor.hasStopTarget = false;
    }
    return disp;
}

bool IsMoving(const Motor& motor)
{
    return glm::length(motor.velocity) > motor.params.speedEpsilon;
}
}  // namespace MotionSystem
