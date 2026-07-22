#include "NavigationRecalc.hpp"

#include "EntityStore.hpp"
#include "Logger.hpp"
#include "NpcAiSystem.hpp"
#include "NpcIdle.hpp"
#include "NpcRecord.hpp"
#include "Patrol.hpp"
#include "PatrolRoute.hpp"
#include "Tilemap.hpp"
#include "WorldServices.hpp"

#include <random>
#include <utility>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Nav";
}  // namespace

std::vector<NpcRecord> SnapshotAndEraseNPCsOnNonWalkable(const Tilemap& tilemap,
                                                         entt::registry& npcs)
{
    // Collect before erasing to keep registry iteration valid.
    std::vector<entt::entity> doomed;
    for (const entt::entity entity : EntityStore::Entities(npcs))
    {
        const Patrol& patrol = npcs.get<Patrol>(entity);
        if (!tilemap.GetNavigation(patrol.tileX, patrol.tileY))
        {
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Removing NPC at tile ({}, {}) - no longer on navigation",
                          patrol.tileX,
                          patrol.tileY);
            doomed.push_back(entity);
        }
    }

    std::vector<NpcRecord> snapshot;
    snapshot.reserve(doomed.size());
    for (const entt::entity e : doomed)
    {
        snapshot.push_back(EntityStore::SnapshotNpc(npcs, e));
        EntityStore::Remove(npcs, e);
    }
    return snapshot;
}

void RestoreErasedNPCs(entt::registry& npcs, std::vector<NpcRecord>& snapshot)
{
    for (const NpcRecord& rec : snapshot)
        EntityStore::SpawnNpc(npcs, rec);
    snapshot.clear();
}

void RebuildPatrolRoutes(Tilemap& tilemap, entt::registry& npcs)
{
    // Use the world RNG when published; headless worlds use a local fallback.
    const WorldServices* svc = npcs.ctx().find<WorldServices>();
    std::mt19937 fallback;
    std::mt19937& rng = (svc != nullptr && svc->npcRng != nullptr) ? *svc->npcRng : fallback;

    for (const entt::entity entity : EntityStore::Entities(npcs))
    {
        NpcIdle& idle = npcs.get<NpcIdle>(entity);
        Patrol& patrol = npcs.get<Patrol>(entity);
        PatrolRoute& route = npcs.get<PatrolRoute>(entity);
        if (!NpcAiSystem::ReinitializePatrolRoute(idle, patrol, route, &tilemap, rng))
        {
            Logger::WarnF(LOG_SUBSYSTEM,
                          "NPC at ({}, {}) could not find valid patrol route",
                          patrol.tileX,
                          patrol.tileY);
        }
    }
}
