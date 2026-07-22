#include <gtest/gtest.h>

#include "../src/EntityStore.hpp"
#include "../src/Identity.hpp"
#include "../src/NpcRecord.hpp"

#include <entt/entt.hpp>

#include <cstdint>

// instanceId must survive undo and remain stable when other registry entries are removed.
TEST(NpcIdentity, InstanceIdsAreUniqueAndNonZero)
{
    entt::registry world;
    const entt::entity a = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity b = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity c = EntityStore::SpawnNpc(world, NpcRecord{});

    const std::uint64_t ia = world.get<Identity>(a).instanceId;
    const std::uint64_t ib = world.get<Identity>(b).instanceId;
    const std::uint64_t ic = world.get<Identity>(c).instanceId;

    EXPECT_NE(ia, 0u);
    EXPECT_NE(ib, 0u);
    EXPECT_NE(ic, 0u);

    EXPECT_NE(ia, ib);
    EXPECT_NE(ib, ic);
    EXPECT_NE(ia, ic);
}

TEST(NpcIdentity, SnapshotRespawnPreservesId)
{
    // undo and redo preserve instanceId so dialogue references still resolve.
    entt::registry world;
    const entt::entity e = EntityStore::SpawnNpc(world, NpcRecord{});
    const std::uint64_t id = world.get<Identity>(e).instanceId;

    const NpcRecord snap = EntityStore::SnapshotNpc(world, e);
    EntityStore::Remove(world, e);

    const entt::entity respawned = EntityStore::SpawnNpc(world, snap);
    EXPECT_EQ(world.get<Identity>(respawned).instanceId, id);
}

TEST(NpcIdentity, IdSurvivesRegistryRemove)
{
    // removing another NPC must not retarget a reference stored by instanceId.
    entt::registry world;
    const entt::entity e0 = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity e1 = EntityStore::SpawnNpc(world, NpcRecord{});
    EntityStore::SpawnNpc(world, NpcRecord{});

    const std::uint64_t removedId = world.get<Identity>(e0).instanceId;
    const std::uint64_t targetId = world.get<Identity>(e1).instanceId;

    EntityStore::Remove(world, e0);

    EXPECT_EQ(EntityStore::FindById(world, targetId), e1);

    EXPECT_EQ(EntityStore::FindById(world, removedId), entt::null);
}
