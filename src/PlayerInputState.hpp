#pragma once

#include <glm/glm.hpp>

/**
 * @struct PlayerInputState
 * @brief Input edges for facing; collision hysteresis lives in PlayerMovementState.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct PlayerInputState
{
    /**
     * @brief Last committed unit direction; blocked frames retain it for wall-slide axis-change
     * detection.
     */
    glm::vec2 lastMovementDirection{0.0f, 0.0f};
    /// Tracks motor velocity, including glide after key release.
    bool isMoving{false};
    bool prevAxisXActive{false};
    bool prevAxisYActive{false};
};
