#include "EntityStore.hpp"

#include "AmbienceConfig.hpp"
#include "AnimationState.hpp"
#include "Appearance.hpp"
#include "AssetRegistry.hpp"
#include "CharacterConstants.hpp"
#include "Dialogue.hpp"
#include "DialogueStore.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "Hitbox.hpp"
#include "Identity.hpp"
#include "IRenderer.hpp"
#include "Logger.hpp"
#include "Motor.hpp"
#include "NpcIdle.hpp"
#include "NpcRecord.hpp"
#include "NpcSprite.hpp"
#include "NpcTag.hpp"
#include "Patrol.hpp"
#include "PatrolRoute.hpp"
#include "PlayerInputState.hpp"
#include "PlayerModes.hpp"
#include "PlayerMovementState.hpp"
#include "PlayerSprite.hpp"
#include "PlayerTag.hpp"
#include "Speed.hpp"
#include "TextureStore.hpp"
#include "Transform.hpp"
#include "WorldServices.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "NPC";

// Default greeting seeded into a spawned NPC's simple dialogue when the record carries no text.
constexpr const char* DEFAULT_NPC_TEXT = "Hello! How are you today?";

// Session-local NPC ID source; spawn only on the game thread.
std::uint64_t NextNpcInstanceId()
{
    static std::uint64_t s_Next = 1;
    return s_Next++;
}

std::vector<entt::entity> CollectNpcEntities(const entt::registry& world)
{
    const auto view = world.view<const NpcTag>();
    std::vector<entt::entity> entities;
    entities.reserve(view.size());
    for (const entt::entity entity : view)
    {
        entities.push_back(entity);
    }
    return entities;
}
}  // namespace

namespace EntityStore
{
void SetNpcTile(Transform& xf,
                Patrol& patrol,
                PatrolRoute& route,
                int tileX,
                int tileY,
                int tileSize,
                bool preserveRoute)
{
    patrol.tileX = tileX;
    patrol.tileY = tileY;

    xf.position.x = tileX * tileSize + tileSize * 0.5f;
    xf.position.y = tileY * tileSize + static_cast<float>(tileSize);

    patrol.targetTileX = tileX;
    patrol.targetTileY = tileY;

    if (!preserveRoute)
    {
        route.Reset();
    }
}

entt::entity SpawnNpc(entt::registry& world, const NpcRecord& record, IRenderer* uploadVia)
{
    const WorldServices* svc = world.ctx().find<WorldServices>();
    TextureStore* textures = (svc != nullptr) ? svc->textures : nullptr;
    DialogueStore* dialogue = (svc != nullptr) ? svc->dialogue : nullptr;
    AssetRegistry* assets = (svc != nullptr) ? svc->assets : nullptr;

    const std::string spritePath = (assets != nullptr)
                                       ? assets->ResolveNpcAsset(record.type)
                                       : ("assets/non-player/" + record.type + ".png");

    TextureHandle sheet{};
    glm::vec3 accent = ambience::DIALOGUE_ACCENT_FALLBACK;
    if (textures != nullptr)
    {
        sheet = textures->Acquire(spritePath);
        if (!textures->IsValid(sheet))
        {
            Logger::ErrorF(LOG_SUBSYSTEM, "SpawnNpc: failed to load NPC sprite: {}", spritePath);
            return entt::null;
        }
        accent = textures->SampleAccent(sheet, ambience::DIALOGUE_ACCENT_FALLBACK);
    }

    DialogueHandle treeHandle{};
    if (dialogue != nullptr && record.hasTree)
    {
        treeHandle = dialogue->Add(record.tree);
    }

    Transform xf{};
    Patrol patrol{};
    PatrolRoute route{};
    SetNpcTile(xf, patrol, route, record.tileX, record.tileY, record.tileSize, false);

    Facing facing{};
    facing.dir = record.facing;

    Dialogue dialogueComp;
    dialogueComp.type = record.type;
    dialogueComp.name = record.name;
    dialogueComp.text = record.text.empty() ? DEFAULT_NPC_TEXT : record.text;
    dialogueComp.tree = treeHandle;

    NpcSprite sprite;
    sprite.sheet = sheet;
    sprite.atlas = nullptr;
    sprite.atlasOffset = glm::vec2(0.0f);
    sprite.accentColor = accent;

    Speed speed;
    speed.value = CharacterConstants::NPC_BASE_SPEED;

    Identity identity;
    identity.instanceId = (record.instanceId != 0) ? record.instanceId : NextNpcInstanceId();

    const entt::entity e = world.create();
    world.emplace<Transform>(e, std::move(xf));
    world.emplace<Elevation>(e);
    world.emplace<Facing>(e, facing);
    world.emplace<AnimationState>(e);
    world.emplace<Speed>(e, speed);
    world.emplace<Identity>(e, identity);
    world.emplace<NpcSprite>(e, std::move(sprite));
    world.emplace<Dialogue>(e, std::move(dialogueComp));
    world.emplace<NpcIdle>(e);
    world.emplace<Patrol>(e, std::move(patrol));
    world.emplace<PatrolRoute>(e, std::move(route));
    world.emplace<NpcTag>(e);

    if (uploadVia != nullptr && textures != nullptr)
    {
        uploadVia->UploadTexture(textures->Get(sheet));
    }
    return e;
}

NpcRecord SnapshotNpc(const entt::registry& world, entt::entity e)
{
    NpcRecord rec;
    const Dialogue& dialogue = world.get<Dialogue>(e);
    const Patrol& patrol = world.get<Patrol>(e);
    const Identity& identity = world.get<Identity>(e);
    const Facing& facing = world.get<Facing>(e);

    rec.type = dialogue.type;
    rec.name = dialogue.name;
    rec.text = dialogue.text;
    rec.tileX = patrol.tileX;
    rec.tileY = patrol.tileY;
    // Snapshot coordinates assume 16 px tiles; another tile size breaks the respawn round trip.
    rec.tileSize = 16;
    rec.instanceId = identity.instanceId;
    rec.facing = facing.dir;

    const WorldServices* svc = world.ctx().find<WorldServices>();
    if (svc != nullptr && svc->dialogue != nullptr && svc->dialogue->HasTree(dialogue.tree))
    {
        rec.tree = svc->dialogue->Get(dialogue.tree);
        rec.hasTree = true;
    }
    return rec;
}

entt::entity SpawnPlayer(entt::registry& world, glm::vec2 spawnPos)
{
    Transform xf{};
    xf.position = spawnPos;
    Speed speed{};
    speed.value = CharacterConstants::PLAYER_BASE_SPEED;
    Appearance appearance{};
    appearance.accentColor = ambience::DIALOGUE_ACCENT_FALLBACK;
    PlayerMovementState movement{};
    movement.lastSafeTileCenter = spawnPos;

    const entt::entity e = world.create();
    world.emplace<Transform>(e, std::move(xf));
    world.emplace<Elevation>(e);
    world.emplace<Facing>(e);
    world.emplace<AnimationState>(e);
    world.emplace<Speed>(e, speed);
    world.emplace<Appearance>(e, std::move(appearance));
    world.emplace<PlayerModes>(e);
    world.emplace<PlayerInputState>(e);
    world.emplace<PlayerMovementState>(e, std::move(movement));
    world.emplace<Motor>(e);
    world.emplace<PlayerSprite>(e);
    world.emplace<Hitbox>(e);
    world.emplace<PlayerTag>(e);
    return e;
}

void Remove(entt::registry& world, entt::entity e)
{
    if (world.valid(e))
    {
        world.destroy(e);
    }
}

void Clear(entt::registry& world)
{
    const std::vector<entt::entity> doomed = CollectNpcEntities(world);
    for (const entt::entity e : doomed)
    {
        world.destroy(e);
    }
}

std::size_t Count(entt::registry& world)
{
    return world.view<NpcTag>().size();
}

std::vector<entt::entity> Entities(const entt::registry& world)
{
    const auto view = world.view<const Identity, const NpcTag>();
    std::vector<entt::entity> entities;
    entities.reserve(view.size_hint());
    for (const entt::entity entity : view)
    {
        entities.push_back(entity);
    }
    std::ranges::sort(entities,
                      {},
                      [&world](const entt::entity entity)
                      { return world.get<Identity>(entity).instanceId; });
    return entities;
}

entt::entity FindById(entt::registry& world, std::uint64_t instanceId)
{
    if (instanceId == 0)
    {
        return entt::null;
    }
    for (auto [entity, identity] : world.view<const Identity, const NpcTag>().each())
    {
        if (identity.instanceId == instanceId)
        {
            return entity;
        }
    }
    return entt::null;
}

entt::entity FindById(const entt::registry& world, std::uint64_t instanceId)
{
    if (instanceId == 0)
    {
        return entt::null;
    }
    for (auto [entity, identity] : world.view<const Identity, const NpcTag>().each())
    {
        if (identity.instanceId == instanceId)
        {
            return entity;
        }
    }
    return entt::null;
}
}  // namespace EntityStore

void BuildNpcFeet(entt::registry& world, std::vector<glm::vec2>& out)
{
    out.clear();
    out.reserve(world.view<NpcTag>().size());
    world.view<const Transform, const NpcTag>().each([&out](const Transform& transform)
                                                     { out.push_back(transform.position); });
}

void BuildNpcCollisionBodies(entt::registry& world, std::vector<CharacterCollisionBody>& out)
{
    out.clear();
    out.reserve(world.view<NpcTag>().size());
    world.view<const Transform, const Elevation, const NpcTag>().each(
        [&out](const Transform& transform, const Elevation& elevation)
        {
            out.push_back(
                CharacterCollisionBody{transform.position, {elevation.surface, elevation.plane}});
        });
}
