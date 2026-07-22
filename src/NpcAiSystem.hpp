#pragma once

#include "SupportSurface.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <random>

struct Transform;
struct Elevation;
struct Facing;
struct AnimationState;
struct NpcIdle;
struct Patrol;
struct Speed;
class PatrolRoute;
class Tilemap;

/**
 * @brief NPC patrol and idle state transitions with an explicit RNG.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * PatrolRoute is a regenerable runtime cache; navigation edits rebuild it in place.
 * Retain the Patrol cursor, not cached route references. Patrol tiles require navigation
 * without collision; Pathfinding checks navigation alone.
 *
 * Stopped takes priority over waiting and standing states. Stalled looks around
 * indefinitely until ReinitializePatrolRoute succeeds.
 *
 * Game owns the shared std::mt19937 published as WorldServices::npcRng. Callers can seed it for
 * deterministic idle transitions. Each update works on the supplied components; the system keeps
 * no hidden per-NPC state.
 * ```mermaid
 * stateDiagram-v2
 *     state "Patrolling" as Pat
 *     state "Stopped (isStopped)" as Stop
 *     state "Waiting (waitTimer &gt; 0)" as Wait
 *     state "RandomPause (standingStill, randomStandStillTimer &gt; 0)" as Pause
 *     state "Stalled (standingStill, randomStandStillTimer == 0)" as Stall
 *
 *     [*] --> Pat
 *     Pat --> Stop: ApplyPlayerOverlapStop sees an overlap
 *     Stop --> Pat: overlap ends (reassigned every frame)
 *     Pat --> Wait: player overlap, blocked step, or route exhausted
 *     Wait --> Pat: timer reaches 0
 *     Pat --> Pause: 30% roll at a waypoint, 2 to 5 s
 *     Pause --> Pat: timer reaches 0
 *     Pat --> Stall: waypoint blocked, or Initialize failed
 *     Stall --> Pat: ReinitializePatrolRoute succeeds
 * ```
 */
namespace NpcAiSystem
{
/**
 * @fn void Update(Transform& xf, Elevation& elev, Facing& facing, AnimationState& anim, NpcIdle& \
 *     idle, Patrol& patrol, PatrolRoute& route, const Speed& speed, float dt, const Tilemap* \
 *     tilemap, const CharacterCollisionBody* playerBody, std::mt19937& rng)
 * @brief Advance patrol and idle state; dt is in seconds and speed is in px/s.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Null tilemap leaves state unchanged. Null playerBody skips overlap avoidance.
 * Smooth elevation even while idle. Blocked waypoints and failed route rebuilds
 * enter indefinite standstill; only a successful ReinitializePatrolRoute resumes patrol.
 *
 * Walk toward the current waypoint, advance on arrival, and roll random pauses only at eligible
 * waypoints. Temporary overlap with the player enters a timed wait. Random pause and stalled
 * look-around behavior share NpcIdle timers but have different resume conditions.
 */
void Update(Transform& xf,
            Elevation& elev,
            Facing& facing,
            AnimationState& anim,
            NpcIdle& idle,
            Patrol& patrol,
            PatrolRoute& route,
            const Speed& speed,
            float dt,
            const Tilemap* tilemap,
            const CharacterCollisionBody* playerBody,
            std::mt19937& rng);

/**
 * @fn bool ReinitializePatrolRoute( NpcIdle& idle, Patrol& patrol, PatrolRoute& route, const \
 *     Tilemap* tilemap, std::mt19937& rng)
 * @brief Rebuild from the Patrol tile without correcting that cursor.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Null tilemap returns false without changing idle or route. Otherwise reset the route;
 * success resumes patrol and seeds the pause cooldown, while failure leaves standstill.
 *
 * Navigation edits call this through RebuildPatrolRoutes. Use the saved tile
 * cursor as the start; rebuilding does not move the NPC or correct that cursor.
 */
bool ReinitializePatrolRoute(
    NpcIdle& idle, Patrol& patrol, PatrolRoute& route, const Tilemap* tilemap, std::mt19937& rng);

/**
 * @fn void UpdateAll(entt::registry& world, const Tilemap& tilemap, CharacterCollisionBody \
 *     playerBody, std::mt19937& rng, std::uint64_t frozenNpcId, float dt)
 * @brief Visit complete NPC component sets in ascending Identity::instanceId order.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Stable ordering preserves shared RNG draws. Commit each accepted position and support
 * together. frozenNpcId skips one speaker; 0 skips none. dt is in seconds.
 */
void UpdateAll(entt::registry& world,
               const Tilemap& tilemap,
               CharacterCollisionBody playerBody,
               std::mt19937& rng,
               std::uint64_t frozenNpcId,
               float dt);

/**
 * @fn void ApplyPlayerOverlapStop(entt::registry& world, CharacterCollisionBody playerBody)
 * @brief Assign isStopped from exact feet overlap on matching support, after UpdateAll.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Overwrite every NPC flag each frame, including console or dialogue holds.
 * Dialogue freezing relies on UpdateAll skipping the speaker id.
 */
void ApplyPlayerOverlapStop(entt::registry& world, CharacterCollisionBody playerBody);
}  // namespace NpcAiSystem
