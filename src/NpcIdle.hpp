#pragma once

/**
 * @struct NpcIdle
 * @brief NPC idle timers are in seconds and advanced by NpcAiSystem.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct NpcIdle
{
    float waitTimer{0.0f};  ///< Wait before resuming patrol, in seconds.
    /// overwritten each frame by player overlap; dialogue holds rely on skipping the speaker id.
    bool isStopped{false};
    bool standingStill{false};
    float lookAroundTimer{0.0f};             ///< Seconds until the next look-around step.
    float randomStandStillCheckTimer{0.0f};  ///< Seconds between stand-still rolls, about 5 to 10.
    float randomStandStillTimer{0.0f};  ///< Seconds left in the stand-still state, about 2 to 5.
};
