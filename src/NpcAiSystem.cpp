#include "NpcAiSystem.hpp"

#include "AnimationState.hpp"
#include "CharacterConstants.hpp"
#include "CharacterKinematics.hpp"
#include "CollisionGeometry.hpp"
#include "Elevation.hpp"
#include "EntityStore.hpp"
#include "Facing.hpp"
#include "Identity.hpp"
#include "NpcIdle.hpp"
#include "NpcTag.hpp"
#include "Patrol.hpp"
#include "PatrolRoute.hpp"
#include "Speed.hpp"
#include "SurfaceSystem.hpp"
#include "Tilemap.hpp"
#include "TileMath.hpp"
#include "Transform.hpp"

#include <cmath>
#include <random>

namespace
{
constexpr float WAYPOINT_REACH_THRESHOLD = 0.5f;  // pixels to count as "reached".
constexpr float MIN_MOVEMENT_DIST = 0.001f;

constexpr Direction ALL_DIRECTIONS[] = {
    Direction::LEFT, Direction::RIGHT, Direction::UP, Direction::DOWN};

void UpdateLookAround(NpcIdle& idle, Facing& facing, float dt, std::mt19937& rng)
{
    idle.lookAroundTimer -= dt;
    if (idle.lookAroundTimer <= 0.0f)
    {
        facing.dir = ALL_DIRECTIONS[std::uniform_int_distribution<int>(0, 3)(rng)];
        idle.lookAroundTimer = 2.0f;
    }
}

void EnterStandingStill(NpcIdle& idle,
                        Facing& facing,
                        AnimationState& anim,
                        bool isRandom,
                        float duration,
                        std::mt19937& rng)
{
    idle.standingStill = true;
    // Zero duration holds indefinitely until a route rebuild succeeds.
    idle.randomStandStillTimer = isRandom ? duration : 0.0f;
    idle.lookAroundTimer = 2.0f;
    CharacterKinematics::ResetAnimation(anim);

    facing.dir = ALL_DIRECTIONS[std::uniform_int_distribution<int>(0, 3)(rng)];
}

// Diagonal ties face vertically; zero delta preserves facing.
void UpdateDirectionFromMovement(Facing& facing, int dx, int dy)
{
    if (dx != 0 || dy != 0)
    {
        facing.dir = CardinalFromDelta(static_cast<float>(dx), static_cast<float>(dy));
    }
}

bool CheckPlayerCollision(glm::vec2 newPosition,
                          SupportState support,
                          const CharacterCollisionBody* playerBody)
{
    if (!playerBody || support != playerBody->support)
    {
        return false;
    }

    return CollisionGeometry::FeetBoxesOverlap(newPosition,
                                               playerBody->feet,
                                               CharacterConstants::HALF_HITBOX_WIDTH,
                                               CharacterConstants::HITBOX_HEIGHT,
                                               CharacterConstants::COLLISION_EPS);
}
}  // namespace

namespace NpcAiSystem
{
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
            std::mt19937& rng)
{
    if (!tilemap)
        return;

    // Smooth elevation even when patrol is stopped.
    CharacterKinematics::UpdateElevation(elev, dt);

    bool isCollidingWithPlayer = false;
    if (playerBody && CharacterKinematics::GetSupport(elev) == playerBody->support)
    {
        if (CollisionGeometry::FeetBoxesOverlap(xf.position,
                                                playerBody->feet,
                                                CharacterConstants::HALF_HITBOX_WIDTH,
                                                CharacterConstants::HITBOX_HEIGHT,
                                                CharacterConstants::COLLISION_EPS))
        {
            isCollidingWithPlayer = true;
            idle.waitTimer = 0.5f;
        }
    }

    if (idle.isStopped || isCollidingWithPlayer)
    {
        CharacterKinematics::ResetAnimation(anim);
        return;
    }

    if (idle.standingStill)
    {
        CharacterKinematics::ResetAnimation(anim);

        if (idle.randomStandStillTimer > 0.0f)
        {
            idle.randomStandStillTimer -= dt;
            if (idle.randomStandStillTimer <= 0.0f)
            {
                idle.standingStill = false;
                idle.randomStandStillTimer = 0.0f;
            }
            else
            {
                UpdateLookAround(idle, facing, dt, rng);
                return;
            }
        }
        else
        {
            UpdateLookAround(idle, facing, dt, rng);
            return;
        }
    }

    const int tileWidth = tilemap->GetTileWidth();
    const int tileHeight = tilemap->GetTileHeight();
    if (tileWidth <= 0 || tileHeight <= 0)
        return;

    patrol.tileX = TileMath::TileIndex(xf.position.x, static_cast<float>(tileWidth));
    // Boundary feet belong to the tile above.
    patrol.tileY = TileMath::StandingTileRow(xf.position.y, static_cast<float>(tileHeight));

    if (idle.waitTimer > 0.0f)
    {
        idle.waitTimer -= dt;
        if (idle.waitTimer < 0.0f)
            idle.waitTimer = 0.0f;
    }

    if (idle.waitTimer > 0.0f)
        return;

    anim.animationTime += dt;
    if (anim.animationTime >= CharacterConstants::ANIM_FRAME_DURATION)
    {
        anim.animationTime -= CharacterConstants::ANIM_FRAME_DURATION;
        CharacterKinematics::AdvanceWalkAnimation(anim);
    }

    if (route.IsValid() && idle.randomStandStillCheckTimer > 0.0f)
    {
        idle.randomStandStillCheckTimer -= dt;
    }

    glm::vec2 targetPos = TileMath::TileFeetCenter(patrol.targetTileX,
                                                   patrol.targetTileY,
                                                   static_cast<float>(tileWidth),
                                                   static_cast<float>(tileHeight));

    glm::vec2 toTarget = targetPos - xf.position;
    float dist = glm::length(toTarget);

    if (dist < WAYPOINT_REACH_THRESHOLD)
    {
        int targetTileX = TileMath::TileIndex(targetPos.x, static_cast<float>(tileWidth));
        int targetTileY = TileMath::AnchorTileRow(targetPos.y, static_cast<float>(tileHeight));
        const SurfaceTransition transition =
            CharacterKinematics::ResolveSupport(elev, xf.position, targetPos, *tilemap);
        if (!transition.connected || (tilemap->GetTileCollision(targetTileX, targetTileY) &&
                                      SurfaceSystem::CollisionBelongsTo(
                                          *tilemap, targetTileX, targetTileY, transition.support)))
        {
            EnterStandingStill(idle, facing, anim, false, 0.0f, rng);
            route = PatrolRoute();
            return;
        }

        xf.position = targetPos;
        CharacterKinematics::CommitSupport(elev, transition.support);

        // Cap route collection at 100 tiles to keep patrols local.
        if (!route.IsValid())
        {
            if (!route.Initialize(patrol.tileX, patrol.tileY, tilemap, 100))
            {
                EnterStandingStill(idle, facing, anim, false, 0.0f, rng);
                return;
            }
            else
            {
                idle.standingStill = false;
                idle.randomStandStillTimer = 0.0f;
                // Stagger pause checks by 5-9.99 seconds.
                idle.randomStandStillCheckTimer =
                    5.0f + std::uniform_int_distribution<int>(0, 499)(rng) / 100.0f;
            }
        }

        // 30% pause chance per eligible waypoint.
        if (route.IsValid() && idle.randomStandStillCheckTimer <= 0.0f)
        {
            idle.randomStandStillCheckTimer =
                5.0f + std::uniform_int_distribution<int>(0, 499)(rng) / 100.0f;
            if (std::uniform_int_distribution<int>(0, 99)(rng) < 30)
            {
                float duration = 2.0f + std::uniform_int_distribution<int>(0, 299)(rng) / 100.0f;
                EnterStandingStill(idle, facing, anim, true, duration, rng);
                return;
            }
        }

        int nextX, nextY;
        if (route.GetNextWaypoint(nextX, nextY))
        {
            patrol.targetTileX = nextX;
            patrol.targetTileY = nextY;
            UpdateDirectionFromMovement(
                facing, patrol.targetTileX - patrol.tileX, patrol.targetTileY - patrol.tileY);
        }
        else
        {
            idle.waitTimer = 1.0f;
        }
        return;
    }

    if (dist > MIN_MOVEMENT_DIST)
    {
        glm::vec2 dir = toTarget / dist;
        glm::vec2 newPosition = xf.position + dir * speed.value * dt;

        const SurfaceTransition transition =
            CharacterKinematics::ResolveSupport(elev, xf.position, newPosition, *tilemap);
        bool wouldCollide = !transition.connected ||
                            CheckPlayerCollision(newPosition, transition.support, playerBody);

        if (!wouldCollide)
        {
            xf.position = newPosition;
            CharacterKinematics::CommitSupport(elev, transition.support);
            UpdateDirectionFromMovement(facing,
                                        static_cast<int>(dir.x > 0) - static_cast<int>(dir.x < 0),
                                        static_cast<int>(dir.y > 0) - static_cast<int>(dir.y < 0));
        }
        else
        {
            idle.waitTimer = 0.5f;
        }
    }
}

bool ReinitializePatrolRoute(
    NpcIdle& idle, Patrol& patrol, PatrolRoute& route, const Tilemap* tilemap, std::mt19937& rng)
{
    if (!tilemap)
        return false;

    route.Reset();

    bool success = route.Initialize(patrol.tileX, patrol.tileY, tilemap, 100);

    if (success)
    {
        idle.standingStill = false;
        idle.randomStandStillTimer = 0.0f;
        idle.randomStandStillCheckTimer =
            5.0f + std::uniform_int_distribution<int>(0, 499)(rng) / 100.0f;
    }
    else
    {
        idle.standingStill = true;
        idle.randomStandStillTimer = 0.0f;
        idle.lookAroundTimer = 2.0f;
    }

    return success;
}

void UpdateAll(entt::registry& world,
               const Tilemap& tilemap,
               CharacterCollisionBody playerBody,
               std::mt19937& rng,
               std::uint64_t frozenNpcId,
               float dt)
{
    for (const entt::entity entity : EntityStore::Entities(world))
    {
        if (!world.all_of<Transform,
                          Elevation,
                          Facing,
                          AnimationState,
                          NpcIdle,
                          Patrol,
                          PatrolRoute,
                          Speed>(entity))
        {
            continue;
        }

        Identity& identity = world.get<Identity>(entity);
        if (frozenNpcId != 0 && identity.instanceId == frozenNpcId)
        {
            continue;
        }

        Update(world.get<Transform>(entity),
               world.get<Elevation>(entity),
               world.get<Facing>(entity),
               world.get<AnimationState>(entity),
               world.get<NpcIdle>(entity),
               world.get<Patrol>(entity),
               world.get<PatrolRoute>(entity),
               world.get<Speed>(entity),
               dt,
               &tilemap,
               &playerBody,
               rng);
    }
}

void ApplyPlayerOverlapStop(entt::registry& world, CharacterCollisionBody playerBody)
{
    world.view<Transform, Elevation, NpcIdle, NpcTag>().each(
        [&](const Transform& transform, const Elevation& elevation, NpcIdle& idle)
        {
            idle.isStopped =
                CharacterKinematics::GetSupport(elevation) == playerBody.support &&
                CollisionGeometry::FeetBoxesOverlap(playerBody.feet,
                                                    transform.position,
                                                    CharacterConstants::HALF_HITBOX_WIDTH,
                                                    CharacterConstants::HITBOX_HEIGHT,
                                                    0.0f);
        });
}
}  // namespace NpcAiSystem
