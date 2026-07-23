#pragma once

/**
 * @enum AnimationType
 * @brief Walk-cycle mode.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * PlayerRender::ResolveRenderSheet gives PlayerModes::isBicycling priority over this mode.
 * PlayerMovementSystem::UpdateAnimation derives frame cadence from velocity.
 */
enum class AnimationType
{
    IDLE = 0,  ///< Standing still; the walk cycle resets to frame 0 instead of advancing.
    WALK = 1,  ///< Walk cycle advancing over the sequence [1,0,2,0], drawn from the walking sheet.
    /// Uses WALK timing for both running and bicycling.
    RUN = 2
};
