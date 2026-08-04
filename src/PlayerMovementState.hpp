#pragma once

#include <glm/glm.hpp>

/**
 * @struct PlayerMovementState
 * @brief Collision hysteresis prevents alternating slide and axis decisions at corners.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct PlayerMovementState
{
    glm::vec2 slideDir{0.0f};  ///< Last chosen wall-slide direction, held to damp jitter.
    /// Seconds holding slideDir; a chosen direction stays fixed for about 120 ms.
    float slideTimer{0.0f};
    int axisPref{0};        ///< Axis preference: -1 prefers Y, +1 prefers X, 0 is no preference.
    float axisTimer{0.0f};  ///< Seconds remaining before the axis preference may change.
    glm::vec2 snapStart{0.0f};   ///< Unused.
    glm::vec2 snapTarget{0.0f};  ///< Unused.
    float snapProgress{1.0f};    ///< Unused.
    /**
     * @brief Write-only safe feet anchor; initial spawn may be off-center. Stuck recovery computes
     * its own target.
     */
    glm::vec2 lastSafeTileCenter{0.0f};
    int lastInputX{0};  ///< Sign of the last non-zero horizontal input: -1 left, +1 right.
    int lastInputY{0};  ///< Sign of the last non-zero vertical input: -1 up, +1 down.
};
