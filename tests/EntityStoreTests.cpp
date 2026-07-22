#include <gtest/gtest.h>

#include "../src/AnimationState.hpp"
#include "../src/Dialogue.hpp"
#include "../src/Elevation.hpp"
#include "../src/EntityStore.hpp"
#include "../src/Facing.hpp"
#include "../src/Identity.hpp"
#include "../src/NpcIdle.hpp"
#include "../src/NpcRecord.hpp"
#include "../src/NpcSprite.hpp"
#include "../src/NpcTag.hpp"
#include "../src/Patrol.hpp"
#include "../src/PatrolRoute.hpp"
#include "../src/Speed.hpp"
#include "../src/Transform.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

// without WorldServices, SpawnNpc leaves invalid sprite handles but must still attach the data
// components.
TEST(EntityStore, SpawnCreatesEntities)
{
    entt::registry world;
    EntityStore::SpawnNpc(world, NpcRecord{});
    EntityStore::SpawnNpc(world, NpcRecord{});
    EXPECT_EQ(EntityStore::Count(world), static_cast<std::size_t>(2));
}

TEST(EntityStore, SpawnAttachesFullComponentSet)
{
    // an NPC without one required component does not appear in the system view.
    // SpawnNpc must attach the complete tuple and NpcTag.
    entt::registry world;
    const entt::entity e = EntityStore::SpawnNpc(world, NpcRecord{});
    ASSERT_TRUE(world.valid(e));
    EXPECT_TRUE((world.all_of<Transform,
                              Elevation,
                              Facing,
                              AnimationState,
                              Speed,
                              Identity,
                              NpcSprite,
                              Dialogue,
                              NpcIdle,
                              Patrol,
                              PatrolRoute>(e)));
    EXPECT_TRUE(world.all_of<NpcTag>(e));
}

TEST(EntityStore, SpawnPositionsAtTileAndAssignsFreshIdentity)
{
    entt::registry world;
    NpcRecord rec;
    rec.tileX = 3;
    rec.tileY = 4;
    rec.tileSize = 16;
    const entt::entity e = EntityStore::SpawnNpc(world, rec);
    EXPECT_EQ(world.get<Patrol>(e).tileX, 3);
    EXPECT_EQ(world.get<Patrol>(e).tileY, 4);

    EXPECT_FLOAT_EQ(world.get<Transform>(e).position.x, 3 * 16 + 8.0f);
    EXPECT_FLOAT_EQ(world.get<Transform>(e).position.y, 4 * 16 + 16.0f);
    EXPECT_NE(world.get<Identity>(e).instanceId, 0u);
}

TEST(EntityStore, SpawnPreservesNonzeroInstanceId)
{
    entt::registry world;
    NpcRecord rec;
    rec.instanceId = 4242;  // undo/redo round-trips a nonzero id
    const entt::entity e = EntityStore::SpawnNpc(world, rec);
    EXPECT_EQ(world.get<Identity>(e).instanceId, static_cast<std::uint64_t>(4242));
}

TEST(EntityStore, RemoveDestroysOneKeepsOthers)
{
    entt::registry world;
    EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity mid = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity last = EntityStore::SpawnNpc(world, NpcRecord{});
    const std::uint64_t keepId = world.get<Identity>(last).instanceId;

    EntityStore::Remove(world, mid);

    EXPECT_EQ(EntityStore::Count(world), static_cast<std::size_t>(2));
    EXPECT_FALSE(world.valid(mid));

    EXPECT_NE(EntityStore::FindById(world, keepId), entt::null);
}

TEST(EntityStore, RemoveDeadEntityIsNoOp)
{
    entt::registry world;
    const entt::entity e = EntityStore::SpawnNpc(world, NpcRecord{});
    EntityStore::Remove(world, e);
    EntityStore::Remove(world, e);  // second remove must not crash
    EXPECT_EQ(EntityStore::Count(world), static_cast<std::size_t>(0));
}

TEST(EntityStore, ClearRemovesNpcsOnly)
{
    entt::registry world;
    const entt::entity player = EntityStore::SpawnPlayer(world);
    const entt::entity unrelated = world.create();
    EntityStore::SpawnNpc(world, NpcRecord{});
    EntityStore::SpawnNpc(world, NpcRecord{});

    EntityStore::Clear(world);

    EXPECT_EQ(EntityStore::Count(world), static_cast<std::size_t>(0));
    EXPECT_TRUE(world.valid(player));
    EXPECT_TRUE(world.valid(unrelated));
}

TEST(EntityStore, EntitiesSortByInstanceIdAndRestoreRespawnedRank)
{
    entt::registry world;
    NpcRecord highestRecord;
    highestRecord.instanceId = 30;
    NpcRecord middleRecord;
    middleRecord.instanceId = 20;
    NpcRecord lowestRecord;
    lowestRecord.instanceId = 10;

    const entt::entity highest = EntityStore::SpawnNpc(world, highestRecord);
    const entt::entity middle = EntityStore::SpawnNpc(world, middleRecord);
    const entt::entity lowest = EntityStore::SpawnNpc(world, lowestRecord);
    const NpcRecord middleSnapshot = EntityStore::SnapshotNpc(world, middle);

    EntityStore::Remove(world, middle);
    const entt::entity respawned = EntityStore::SpawnNpc(world, middleSnapshot);

    const std::vector<entt::entity> all = EntityStore::Entities(world);
    ASSERT_EQ(all.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(all[0], lowest);
    EXPECT_EQ(all[1], respawned);
    EXPECT_EQ(all[2], highest);
    EXPECT_TRUE(world.valid(lowest));
    EXPECT_TRUE(world.valid(respawned));
    EXPECT_TRUE(world.valid(highest));
}

TEST(EntityStore, FindByIdResolvesAndMisses)
{
    entt::registry world;
    const entt::entity e = EntityStore::SpawnNpc(world, NpcRecord{});
    const std::uint64_t id = world.get<Identity>(e).instanceId;

    EXPECT_EQ(EntityStore::FindById(world, id), e);
    EXPECT_EQ(EntityStore::FindById(world, id + 99999u), entt::null);  // unknown id
    EXPECT_EQ(EntityStore::FindById(world, 0u), entt::null);           // id 0 never resolves
}

TEST(EntityStore, SnapshotRoundTripsAuthoredState)
{
    entt::registry world;
    NpcRecord rec;
    rec.type = "guard";
    rec.name = "Bob";
    rec.text = "Halt!";
    rec.tileX = 5;
    rec.tileY = 6;
    rec.instanceId = 77;
    const entt::entity e = EntityStore::SpawnNpc(world, rec);

    const NpcRecord snap = EntityStore::SnapshotNpc(world, e);
    EXPECT_EQ(snap.type, "guard");
    EXPECT_EQ(snap.name, "Bob");
    EXPECT_EQ(snap.text, "Halt!");
    EXPECT_EQ(snap.tileX, 5);
    EXPECT_EQ(snap.tileY, 6);
    EXPECT_EQ(snap.instanceId, static_cast<std::uint64_t>(77));
}

TEST(BuildNpcFeet, CollectsFeetPositions)
{
    entt::registry world;
    const entt::entity a = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity b = EntityStore::SpawnNpc(world, NpcRecord{});
    world.get<Transform>(a).position = glm::vec2(10.0f, 20.0f);
    world.get<Transform>(b).position = glm::vec2(-5.0f, 7.5f);

    std::vector<glm::vec2> feet;
    BuildNpcFeet(world, feet);

    ASSERT_EQ(feet.size(), static_cast<std::size_t>(2));
    // registry iteration order is unspecified here; check membership only.
    bool has10 = false;
    bool hasNeg5 = false;
    for (const glm::vec2& f : feet)
    {
        if (f.x == 10.0f && f.y == 20.0f)
        {
            has10 = true;
        }
        if (f.x == -5.0f && f.y == 7.5f)
        {
            hasNeg5 = true;
        }
    }
    EXPECT_TRUE(has10);
    EXPECT_TRUE(hasNeg5);
}

TEST(BuildNpcFeet, ClearsStaleOutputBeforeFilling)
{
    entt::registry world;
    EntityStore::SpawnNpc(world, NpcRecord{});

    std::vector<glm::vec2> feet{glm::vec2(99.0f), glm::vec2(99.0f), glm::vec2(99.0f)};
    BuildNpcFeet(world, feet);

    EXPECT_EQ(feet.size(), static_cast<std::size_t>(1));
}

TEST(BuildNpcCollisionBodies, IncludesCommittedSupportSurface)
{
    entt::registry world;
    const entt::entity groundNpc = EntityStore::SpawnNpc(world, NpcRecord{});
    const entt::entity deckNpc = EntityStore::SpawnNpc(world, NpcRecord{});
    world.get<Transform>(groundNpc).position = glm::vec2(10.0f, 20.0f);
    world.get<Transform>(deckNpc).position = glm::vec2(30.0f, 40.0f);
    world.get<Elevation>(deckNpc).surface = SupportSurface::Elevation;
    world.get<Elevation>(deckNpc).plane = 10;

    std::vector<CharacterCollisionBody> bodies;
    BuildNpcCollisionBodies(world, bodies);

    ASSERT_EQ(bodies.size(), static_cast<std::size_t>(2));
    bool foundGround = false;
    bool foundDeck = false;
    for (const CharacterCollisionBody& body : bodies)
    {
        foundGround |= body.feet == glm::vec2(10.0f, 20.0f) &&
                       body.support == SupportState{SupportSurface::Ground, 0};
        foundDeck |= body.feet == glm::vec2(30.0f, 40.0f) &&
                     body.support == SupportState{SupportSurface::Elevation, 10};
    }
    EXPECT_TRUE(foundGround);
    EXPECT_TRUE(foundDeck);
}
