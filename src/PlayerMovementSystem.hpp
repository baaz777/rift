#pragma once

#include "SupportSurface.hpp"

#include <glm/glm.hpp>

#include <vector>

struct Transform;
struct Motor;
struct Facing;
struct AnimationState;
struct Elevation;
struct PlayerModes;
struct PlayerInputState;
struct PlayerMovementState;
struct Speed;
struct Hitbox;
class Tilemap;

/**
 * @brief Player input, motor and collision orchestration over component references.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
namespace PlayerMovementSystem
{
/**
 * @fn void UpdateFacing( Facing& facing, PlayerInputState& input, int moveDx, int moveDy, \
 *     glm::vec2 normalizedDir)
 * @brief Prefer the most recently pressed axis for facing.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Simultaneous rising edges use the larger normalizedDir component. Otherwise keep
 * the current facing axis while held, then use the remaining axis. No input preserves facing.
 * moveDx and moveDy are signs (-1, 0, 1); positive Y points down. Update both axis-edge latches.
 */
void UpdateFacing(
    Facing& facing, PlayerInputState& input, int moveDx, int moveDy, glm::vec2 normalizedDir);

/**
 * @fn void UpdateAnimation(AnimationState& anim, const PlayerModes& modes, const Motor& motor, \
 *     float dt, float animationSpeed)
 * @brief Scale animation cadence by velocity, including glide frames.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Frame duration is animationSpeed * clamp(PLAYER_BASE_SPEED / speed, 0.4, 2.5).
 * dt and animationSpeed are in seconds; animationType selects idle or walk frames.
 */
void UpdateAnimation(AnimationState& anim,
                     const PlayerModes& modes,
                     const Motor& motor,
                     float dt,
                     float animationSpeed);

/**
 * @fn void Stop(AnimationState& anim, PlayerInputState& input, PlayerModes& modes, Motor& motor)
 * @brief Reset animation to idle, clear input.isMoving, and reset velocity and the stop target.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Retain position, facing, elevation and input-edge latches. Preserve isRunning, isBicycling and
 * speedMultiplier; stopping does not change the selected movement mode.
 */
void Stop(AnimationState& anim, PlayerInputState& input, PlayerModes& modes, Motor& motor);

/**
 * @fn glm::vec2 CurrentTileCenter(glm::vec2 position, float tileSize)
 * @brief Return the tile bottom-center in world pixels; boundary feet belong to the tile above.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Nonpositive size returns zero.
 *
 * tileSize is in world pixels. The bottom-edge convention uses the tile above an exact Y boundary
 * so standing feet do not snap into the next row.
 */
glm::vec2 CurrentTileCenter(glm::vec2 position, float tileSize);

/**
 * @fn void Step(Transform& xf, Motor& motor, Facing& facing, AnimationState& anim, Elevation& \
 *     elev, PlayerModes& modes, PlayerInputState& input, PlayerMovementState& movement, const \
 *     Speed& speed, const Hitbox& hitbox, glm::vec2 direction, float dt, const Tilemap* tilemap, \
 *     const std::vector<CharacterCollisionBody>* npcBodies)
 * @brief Apply input, collision and support as one movement step.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The final accepted probe commits feet position and support together. Null tilemap
 * uses 16 px tiles and applies raw displacement without support; stuck recovery also
 * changes only position and motor. The caller must derive support after no-clip movement.
 *
 * Clear blocked-axis velocity except after a successful corner slide, which redirects
 * that momentum. direction is normalized internally; magnitude below 0.1 is idle.
 * dt is in seconds; speed is in px/s. Null npcBodies skips NPC blocking.
 *
 * Probe the full step before trying a corner slide or axis split. Re-probe after lane snapping so
 * alignment cannot move into blocking geometry. NPC blocking cancels the step; tile blocking can
 * retain motion on the clear axis.
 *
 * Speed is the base walking speed before run, bicycle and speedMultiplier scaling.
 * ```mermaid
 * flowchart TD
 *     IN["input direction"] --> FA["UpdateFacing<br/>Facing, PlayerInputState"]
 *     FA --> MO["ComputeDisplacement<br/>Motor"]
 *     MO --> AN["animation gate<br/>AnimationState, PlayerModes"]
 *     AN --> NC{"tilemap null?"}
 *     NC -->|no-clip| RAW["position += disp<br/>Transform only, support not committed"]
 *     NC -->|world| ID{"displacement ~ 0?"}
 *     ID -->|idle| SR["HandleStuckRecovery<br/>Transform + Motor only if embedded"]
 *     ID -->|moving| P1{"direct probe"}
 *     P1 -->|clear| LS
 *     P1 -->|npc blocked| ZE["displacement = 0"]
 *     P1 -->|tile blocked| CS["corner slide<br/>PlayerMovementState slideDir"]
 *     CS -->|no slide, diagonal| AX["axis split, keep X or Y"]
 *     ZE --> LS
 *     CS --> LS
 *     AX --> LS["lane snap<br/>skipped after a slide or a block"]
 *     LS --> P2{"re-probe"}
 *     P2 -->|blocked| AF["axis fallback<br/>PlayerMovementState axisPref"]
 *     P2 -->|clear| KI["zero blocked axis<br/>Motor"]
 *     AF --> KI
 *     KI --> CM["commit<br/>Transform + Elevation support"]
 * ```
 */
void Step(Transform& xf,
          Motor& motor,
          Facing& facing,
          AnimationState& anim,
          Elevation& elev,
          PlayerModes& modes,
          PlayerInputState& input,
          PlayerMovementState& movement,
          const Speed& speed,
          const Hitbox& hitbox,
          glm::vec2 direction,
          float dt,
          const Tilemap* tilemap,
          const std::vector<CharacterCollisionBody>* npcBodies);
}  // namespace PlayerMovementSystem
