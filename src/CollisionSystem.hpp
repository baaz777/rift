#pragma once

#include "Hitbox.hpp"
#include "SupportSurface.hpp"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

struct PlayerMovementState;
class Tilemap;

/**
 * @brief Player collision probes, wall sliding and lane snapping.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * Geometry and tolerance budgets assume 16 px tiles and a 16x16 hitbox for every movement
 * mode. Tiles and NPCs block only matching support surface and height. Null tilemap and
 * npcBodies skip their respective blocking tests; each movement helper defines its fallback.
 *
 * forceCollision in phase 3 bypasses the 1% overlap floor.
 *
 * @verbatim
 *                   +----------------------------------------+
 *   moveDx,moveDy   | CollidesWithTilesStrict                |
 *   diagonalInput   |   for each overlapping tile:           |
 *   feetPos         |     1) ShouldSkipDiagonalTile          |--+
 *                   |        cardinal grazing past a corner  |  |
 *                   |     2) ShouldTolerateWallPenetration   |  |
 *                   |        sliding along a wall face       |  +--> "no
 *                   |     3) ShouldAllowCornerCut            |  |     collision"
 *                   |        exposed convex corner with an   |  |     (tile skipped)
 *                   |        escape route, or <=15% overlap  |  |
 *                   |        with a side wall during pure    |  |
 *                   |        cardinal motion                 |  |
 *                   |     4) overlap <= 1% of hitbox area    |--+
 *                   |                                        |
 *                   |     3a) forceCollision: closed convex  |--+
 *                   |         corner under diagonal input    |  +--> "blocked"
 *                   |     5) overlap > 1% of hitbox area     |--+
 *                   +----------------------------------------+
 * @endverbatim
 */
namespace CollisionSystem
{

/**
 * @fn bool CollidesWithNPC(const Hitbox& hitbox, const glm::vec2& feetPos, SupportState support, \
 *     const std::vector<CharacterCollisionBody>* npcBodies)
 * @brief Tests NPC overlap on the same support.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Both boxes use the supplied hitbox dimensions and shrink by COLLISION_EPS.
 * feetPos is in world pixels. Null or empty npcBodies skips NPC blocking.
 */
bool CollidesWithNPC(const Hitbox& hitbox,
                     const glm::vec2& feetPos,
                     SupportState support,
                     const std::vector<CharacterCollisionBody>* npcBodies);

/**
 * @fn bool CollidesWithTilesStrict(const Hitbox& hitbox, const glm::vec2& feetPos, const Tilemap* \
 *     tilemap, int moveDx, int moveDy, bool diagonalInput, SupportState support = {})
 * @brief Tests tile overlap after the documented tolerance cascade.
 * @author Alex (<https://github.com/lextpf>)
 *
 * feetPos is in world pixels. moveDx and moveDy are signs (-1, 0, +1), with Y down.
 * Use the actual movement direction: zero signs disable directional tolerance.
 * diagonalInput disables cardinal-only tolerance. Null tilemap returns false.
 * Support must match the character; the default tests ground at height zero only.
 */
bool CollidesWithTilesStrict(const Hitbox& hitbox,
                             const glm::vec2& feetPos,
                             const Tilemap* tilemap,
                             int moveDx,
                             int moveDy,
                             bool diagonalInput,
                             SupportState support = {});

/**
 * @fn bool CollidesAt(const Hitbox& hitbox, const glm::vec2& feetPos, const Tilemap* tilemap, \
 *     const std::vector<CharacterCollisionBody>* npcBodies, int moveDx = 0, int moveDy = 0, bool \
 *     diagonalInput = false, SupportState support = {})
 * @brief Tests a position against tiles and NPCs on the supplied support.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Does not validate support transitions; use ProbeMovement before committing movement.
 * feetPos is in world pixels. Movement signs feed tile tolerance; diagonalInput disables
 * cardinal-only tolerance. Null inputs skip their respective tests. The default support
 * tests ground at height zero only.
 */
bool CollidesAt(const Hitbox& hitbox,
                const glm::vec2& feetPos,
                const Tilemap* tilemap,
                const std::vector<CharacterCollisionBody>* npcBodies,
                int moveDx = 0,
                int moveDy = 0,
                bool diagonalInput = false,
                SupportState support = {});

/**
 * @struct MovementProbeResult
 * @brief Complete result of a movement probe before any state is committed.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct MovementProbeResult
{
    /**
     * @brief Support the move would reach. When transition.connected is false the move crossed
     * an unsupported step/drop and `tileBlocked` is set too, so IsBlocked() covers it.
     */
    SurfaceTransition transition{};
    bool tileBlocked{false};  ///< A world tile on the resolved support blocks the target.
    bool npcBlocked{false};

    /**
     * @fn bool IsBlocked() const
     * @brief True when the move must be rejected for any reason.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Callers keep the exact probe that accepted a movement so its resolved support can be
     * committed with the position.
     */
    bool IsBlocked() const { return !transition.connected || tileBlocked || npcBlocked; }
};

/**
 * @fn MovementProbeResult ProbeMovement(const Hitbox& hitbox, glm::vec2 origin, glm::vec2 target, \
 *     SupportState currentSupport, const Tilemap* tilemap, const \
 *     std::vector<CharacterCollisionBody>* npcBodies, int moveDx, int moveDy, bool diagonalInput)
 * @brief Resolves support before testing tile and NPC blocking.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Origin and target are feet positions in world pixels. A disconnected transition blocks
 * the move and skips NPC testing. Null tilemap passes currentSupport through as connected;
 * null npcBodies skips NPC testing. Commit position and support together only if unblocked.
 * moveDx and moveDy are movement signs; diagonalInput disables cardinal-only tolerance.
 */
MovementProbeResult ProbeMovement(const Hitbox& hitbox,
                                  glm::vec2 origin,
                                  glm::vec2 target,
                                  SupportState currentSupport,
                                  const Tilemap* tilemap,
                                  const std::vector<CharacterCollisionBody>* npcBodies,
                                  int moveDx,
                                  int moveDy,
                                  bool diagonalInput);

/**
 * @fn glm::vec2 FindClosestSafeTileCenter(const Hitbox& hitbox, glm::vec2 playerPos, SupportState \
 *     support, const Tilemap* tilemap, const std::vector<CharacterCollisionBody>* npcBodies)
 * @brief Searches a 5x5 tile window for the nearest safe feet anchor.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Keeps elevated characters on tiles with the same authored height. playerPos is in world
 * pixels. Null tilemap or a fully blocked window returns playerPos unchanged; null npcBodies
 * skips NPC blocking.
 */
glm::vec2 FindClosestSafeTileCenter(const Hitbox& hitbox,
                                    glm::vec2 playerPos,
                                    SupportState support,
                                    const Tilemap* tilemap,
                                    const std::vector<CharacterCollisionBody>* npcBodies);

/**
 * @fn float CalculateFollowAlpha(float deltaTime, float settleTime, float epsilon = 0.01f)
 * @brief Frame-independent lane-snap blend factor.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param deltaTime Frame seconds; negative values clamp to zero.
 * @param settleTime Seconds to reach the residual fraction; clamped above zero.
 * @param epsilon Residual fraction after settleTime.
 * @return Blend factor in [0, 1].
 */
float CalculateFollowAlpha(float deltaTime, float settleTime, float epsilon = 0.01f);

/**
 * @fn glm::vec2 TrySlideMovement(const Hitbox& hitbox, glm::vec2 playerPos, PlayerMovementState& \
 *     movement, SupportState support, glm::vec2 desiredMovement, glm::vec2 normalizedDir, float \
 *     deltaTime, float currentSpeed, const Tilemap* tilemap, const \
 *     std::vector<CharacterCollisionBody>* npcBodies, int moveDx, int moveDy, bool diagonalInput)
 * @brief Finds a slide displacement and updates hysteresis when tiles block movement.
 * @author Alex (<https://github.com/lextpf>)
 *
 * An unblocked probe returns desiredMovement. An NPC block stops movement and clears
 * hysteresis. Probe both sides up to 16 px; cap by currentSpeed * deltaTime and 75% of
 * forward distance, then binary-search the safe forward fraction. Prefer a collision-free
 * 35% blend toward the slide; return a perpendicular-only step if forward motion stays blocked.
 *
 * Positions and displacements use world pixels; deltaTime is seconds and currentSpeed is px/s.
 * normalizedDir is unused. Null tilemap skips tile blocking; null npcBodies skips NPC blocking.
 * Movement signs and diagonalInput select the tile tolerances. The result can be zero.
 */
glm::vec2 TrySlideMovement(const Hitbox& hitbox,
                           glm::vec2 playerPos,
                           PlayerMovementState& movement,
                           SupportState support,
                           glm::vec2 desiredMovement,
                           glm::vec2 normalizedDir,
                           float deltaTime,
                           float currentSpeed,
                           const Tilemap* tilemap,
                           const std::vector<CharacterCollisionBody>* npcBodies,
                           int moveDx,
                           int moveDy,
                           bool diagonalInput);

/**
 * @fn glm::vec2 ApplyLaneSnapping(const Hitbox& hitbox, glm::vec2 playerPos, glm::vec2 \
 *     tileCenter, SupportState support, glm::vec2 desiredMovement, glm::vec2 normalizedDir, float \
 *     deltaTime, const Tilemap* tilemap, const std::vector<CharacterCollisionBody>* npcBodies, \
 *     int moveDx, int moveDy)
 * @brief Adds a collision-checked correction toward tile-center lanes.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Corrects only perpendicular to cardinal travel; diagonal movement is unchanged.
 * Settles over 0.3 s, capped at 1.2 px per frame. A blocked correction retries as a
 * perpendicular-only step. Null tilemap returns desiredMovement unchanged.
 *
 * Positions and displacement use world pixels; deltaTime is seconds. normalizedDir selects
 * the travel axis. Null npcBodies skips NPC blocking; movement signs select tile tolerance.
 */
glm::vec2 ApplyLaneSnapping(const Hitbox& hitbox,
                            glm::vec2 playerPos,
                            glm::vec2 tileCenter,
                            SupportState support,
                            glm::vec2 desiredMovement,
                            glm::vec2 normalizedDir,
                            float deltaTime,
                            const Tilemap* tilemap,
                            const std::vector<CharacterCollisionBody>* npcBodies,
                            int moveDx,
                            int moveDy);

/**
 * @fn std::optional<glm::vec2> HandleStuckRecovery(const Hitbox& hitbox, glm::vec2 playerPos, \
 *     PlayerMovementState& movement, SupportState support, glm::vec2 currentTileCenter, const \
 *     Tilemap* tilemap, const std::vector<CharacterCollisionBody>* npcBodies)
 * @brief Reports a safe feet anchor when embedded in a blocking tile.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Does not move the entity. The caller applies the position and resets its motor.
 * Use TileMath::AnchorTileRow on the returned bottom-center anchor; floor selects the row below.
 *
 * Positions use world pixels. Updates movement.lastSafeTileCenter while safe. Null tilemap
 * returns nullopt without that update; null npcBodies skips NPC blocking.
 */
std::optional<glm::vec2> HandleStuckRecovery(const Hitbox& hitbox,
                                             glm::vec2 playerPos,
                                             PlayerMovementState& movement,
                                             SupportState support,
                                             glm::vec2 currentTileCenter,
                                             const Tilemap* tilemap,
                                             const std::vector<CharacterCollisionBody>* npcBodies);

/**
 * @fn glm::vec2 GetCornerSlideDirection(const Hitbox& hitbox, glm::vec2 playerPos, \
 *     PlayerMovementState& movement, SupportState support, const glm::vec2& testPos, const \
 *     Tilemap* tilemap, int moveDirX, int moveDirY)
 * @brief Chooses a unit perpendicular slide direction and updates hysteresis.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Returns zero for a flat wall without an opening, a corner beyond 0.75 tiles, a cut-blocked
 * corner, failed 10 px probes on both sides, or null tilemap.
 *
 * testPos - playerPos selects the forward axis; both positions use world pixels. moveDirX
 * and moveDirY are ignored. A changed direction starts a roughly 120 ms commit timer.
 * Flat-wall or distant-corner cases clear the direction only after that timer expires.
 */
glm::vec2 GetCornerSlideDirection(const Hitbox& hitbox,
                                  glm::vec2 playerPos,
                                  PlayerMovementState& movement,
                                  SupportState support,
                                  const glm::vec2& testPos,
                                  const Tilemap* tilemap,
                                  int moveDirX,
                                  int moveDirY);

}  // namespace CollisionSystem
