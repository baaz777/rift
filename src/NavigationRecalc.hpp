#pragma once

#include <entt/entt.hpp>

#include <vector>

class Tilemap;
struct NpcRecord;

/**
 * @fn std::vector<NpcRecord> SnapshotAndEraseNPCsOnNonWalkable(const Tilemap& tilemap, \
 *     entt::registry& npcs)
 * @brief Remove NPCs whose patrol tile is no longer navigable.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Returns spawn records in instance-ID order for undo. Only navigation flags select NPCs;
 * collision changes alone do not remove them. Store the records before rebuilding routes.
 *
 * ```mermaid
 * flowchart LR
 *     classDef apply fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef snap  fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef rebuild fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *
 *     subgraph Apply
 *         direction LR
 *         A1[Flip nav flags]:::apply --> A2[SnapshotAndEraseNPCsOnNonWalkable]:::snap
 *         A2 --> A3[RebuildPatrolRoutes]:::rebuild
 *         A3 --> A4[Cmd keeps snapshot]
 *     end
 *
 *     subgraph Revert
 *         direction LR
 *         R1[Revert nav flags]:::apply --> R2[RestoreErasedNPCs]:::snap
 *         R2 --> R3[RebuildPatrolRoutes]:::rebuild
 *     end
 *
 *     Apply -. undo .-> Revert
 * ```
 *
 * @code{.cpp}
 * // Apply: command has already written new navigation flags.
 * auto displaced = SnapshotAndEraseNPCsOnNonWalkable(tilemap, npcs);
 * RebuildPatrolRoutes(tilemap, npcs);
 * cmd.storedDisplaced = std::move(displaced);
 *
 * // Revert: command has already restored the old navigation flags.
 * RestoreErasedNPCs(npcs, cmd.storedDisplaced);
 * RebuildPatrolRoutes(tilemap, npcs);
 * @endcode
 *
 * @pre NPCs selected by EntityStore::Entities have the components required by SnapshotNpc.
 */
std::vector<NpcRecord> SnapshotAndEraseNPCsOnNonWalkable(const Tilemap& tilemap,
                                                         entt::registry& npcs);

/**
 * @fn void RestoreErasedNPCs(entt::registry& npcs, std::vector<NpcRecord>& snapshot)
 * @brief Consume displaced NPC records after restoring their navigation cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Attempts each spawn with its saved instance ID, then clears the snapshot. A failed sprite
 * load does not retain that record for retry. Call RebuildPatrolRoutes after restoration.
 */
void RestoreErasedNPCs(entt::registry& npcs, std::vector<NpcRecord>& snapshot);

/**
 * @fn void RebuildPatrolRoutes(Tilemap& tilemap, entt::registry& npcs)
 * @brief Rebuild NPC routes after the navigation grid and roster are final.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Failed routes leave NPCs standing and looking around, with a warning.
 * Uses WorldServices::npcRng when present, otherwise a local default-seeded RNG.
 * NPCs are processed in instance-ID order so the shared RNG draw order is stable.
 *
 * @pre Every NPC selected by EntityStore::Entities has NpcIdle, Patrol, and PatrolRoute.
 */
void RebuildPatrolRoutes(Tilemap& tilemap, entt::registry& npcs);
