#pragma once

#include "CharacterDirection.hpp"

#include <cmath>

/**
 * @brief Maps world facing to a sprite row under an orbit camera.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Facing remains world-absolute for dialogue, AI and interaction. rotate only the render
 * lookup. yaw zero gives the identity mapping.
 *
 * @verbatim
 *   right = ( cos yaw, -sin yaw)    screen-right
 *   toCam = ( sin yaw,  cos yaw)    screen-down (toward the viewer)
 *
 *   sx = facing . right      sy = facing . toCam
 *   |sy| >= |sx|  ->  sy > 0 ? DOWN : UP
 *   otherwise     ->  sx > 0 ? RIGHT : LEFT
 * @endverbatim
 */
namespace cameraFacing
{

/**
 * @fn void FacingVector(CharacterDirection dir, float& outX, float& outY)
 * @brief World-space unit vector a facing points along (x east, y south).
 * @author Alex (<https://github.com/lextpf>)
 */
inline void FacingVector(CharacterDirection dir, float& outX, float& outY)
{
    switch (dir)
    {
        case CharacterDirection::UP:
            outX = 0.0f;
            outY = -1.0f;
            break;
        case CharacterDirection::DOWN:
            outX = 0.0f;
            outY = 1.0f;
            break;
        case CharacterDirection::LEFT:
            outX = -1.0f;
            outY = 0.0f;
            break;
        case CharacterDirection::RIGHT:
            outX = 1.0f;
            outY = 0.0f;
            break;
    }
}

/**
 * @fn CharacterDirection ScreenFacing(CharacterDirection facing, float yawRadians)
 * @brief Sprite row for the camera-relative facing.
 * @author Alex (<https://github.com/lextpf>)
 *
 * yawRadians is camera yaw in radians; zero places the camera south of the map.
 */
inline CharacterDirection ScreenFacing(CharacterDirection facing, float yawRadians)
{
    float fx = 0.0f;
    float fy = 0.0f;
    FacingVector(facing, fx, fy);

    const float sinYaw = std::sin(yawRadians);
    const float cosYaw = std::cos(yawRadians);

    const float screenX = fx * cosYaw - fy * sinYaw;  // Dot with screen-right
    const float screenY = fx * sinYaw + fy * cosYaw;  // Dot with screen-down

    // ties favour the vertical rows, matching the flat pipeline on exact diagonals.
    if (std::abs(screenY) >= std::abs(screenX))
    {
        return screenY > 0.0f ? CharacterDirection::DOWN : CharacterDirection::UP;
    }
    return screenX > 0.0f ? CharacterDirection::RIGHT : CharacterDirection::LEFT;
}

}  // namespace cameraFacing
