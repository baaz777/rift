#pragma once

#include "SupportSurface.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

class IRenderer;
class PatrolRoute;
struct NpcRecord;
struct Patrol;
struct Transform;

/**
 * @brief NPC lifecycle and stable-ID queries.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Spawn reads nullable WorldServices and uses defaults for missing services. NPCs carry
 * Identity independently of their entity handles so undo can retain identity.
 * BuildNpcFeet and BuildNpcCollisionBodies are declared at global scope.
 */
namespace EntityStore
{

/**
 * @fn entt::entity SpawnNpc(entt::registry& world, const NpcRecord& record, IRenderer* uploadVia \
 *     = nullptr)
 * @brief Spawns an NPC blueprint, resolving assets through WorldServices.
 * @author Alex (<https://github.com/lextpf>)
 *
 * A nonzero `record.instanceId` is retained without uniqueness checks; remove the original
 * NPC before restoring its ID. The generated ID counter does not advance for retained IDs.
 * When a DialogueStore is available, `record.hasTree` adds a fresh tree copy on each spawn.
 *
 * A missing TextureStore creates the NPC with an invalid sheet handle and fallback accent.
 * A null `uploadVia` skips GPU upload. Spawn calls must run on the game thread.
 *
 * @return The new entity, or `entt::null` if a requested sprite load fails before creation.
 */
entt::entity SpawnNpc(entt::registry& world,
                      const NpcRecord& record,
                      IRenderer* uploadVia = nullptr);

/**
 * @fn NpcRecord SnapshotNpc(const entt::registry& world, entt::entity e)
 * @brief Copy NPC spawn data while preserving its instance identity.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Captures the Patrol tile cursor, facing, and available dialogue tree. The record uses
 * 16 px tiles. It omits fractional position, support, animation, idle timers, and route state;
 * respawn initializes those components from their defaults.
 *
 * @pre The entity is alive with Dialogue, Patrol, Identity, and Facing; reads are unchecked.
 */
NpcRecord SnapshotNpc(const entt::registry& world, entt::entity e);

/**
 * @fn entt::entity SpawnPlayer(entt::registry& world, glm::vec2 spawnPos = glm::vec2(200.0f, \
 *     150.0f))
 * @brief Creates player components without resolving assets.
 * @author Alex (<https://github.com/lextpf>)
 *
 * PlayerSystem::SwitchCharacter binds sheets afterward. The spawnPos argument is the feet anchor in
 * world pixels.
 */
entt::entity SpawnPlayer(entt::registry& world, glm::vec2 spawnPos = glm::vec2(200.0f, 150.0f));

/**
 * @fn void SetNpcTile(Transform& xf, Patrol& patrol, PatrolRoute& route, int tileX, int tileY, \
 *     int tileSize, bool preserveRoute = false)
 * @brief Moves feet to the tile bottom-center and updates Patrol cursors.
 * @author Alex (<https://github.com/lextpf>)
 *
 * tileSize is pixels. Resets the route unless preserveRoute is true.
 */
void SetNpcTile(Transform& xf,
                Patrol& patrol,
                PatrolRoute& route,
                int tileX,
                int tileY,
                int tileSize,
                bool preserveRoute = false);

/**
 * @fn void Remove(entt::registry& world, entt::entity e)
 * @brief Destroy entity e, doing nothing if it is not alive.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Accepts any live entity, including the player. NPC-only callers must enforce the tag.
 * Destroying an entity does not remove its shared textures or dialogue trees.
 */
void Remove(entt::registry& world, entt::entity e);

/**
 * @fn void Clear(entt::registry& world)
 * @brief Destroys all NpcTag entities.
 * @author Alex (<https://github.com/lextpf>)
 */
void Clear(entt::registry& world);

std::size_t Count(entt::registry& world);

/**
 * @fn std::vector<entt::entity> Entities(const entt::registry& world)
 * @brief NPC handles sorted by instanceId.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Indices can change when lower IDs are added or removed.
 */
std::vector<entt::entity> Entities(const entt::registry& world);

/**
 * @fn entt::entity FindById(entt::registry& world, std::uint64_t instanceId)
 * @brief Returns entt::null for an absent instance ID; zero never matches.
 * @author Alex (<https://github.com/lextpf>)
 */
entt::entity FindById(entt::registry& world, std::uint64_t instanceId);
entt::entity FindById(const entt::registry& world, std::uint64_t instanceId);

}  // namespace EntityStore

/**
 * @fn void BuildNpcFeet(entt::registry& world, std::vector<glm::vec2>& out)
 * @brief Replace the output with current NPC feet anchors in world pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
void BuildNpcFeet(entt::registry& world, std::vector<glm::vec2>& out);

/**
 * @fn void BuildNpcCollisionBodies(entt::registry& world, std::vector<CharacterCollisionBody>& \
 *     out)
 * @brief Replace the output with NPC feet anchors and committed collision support.
 * @author Alex (<https://github.com/lextpf>)
 */
void BuildNpcCollisionBodies(entt::registry& world, std::vector<CharacterCollisionBody>& out);
