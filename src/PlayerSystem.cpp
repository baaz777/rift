#include "PlayerSystem.hpp"

#include "AmbienceConfig.hpp"
#include "AnimationState.hpp"
#include "Appearance.hpp"
#include "AssetRegistry.hpp"
#include "CharacterConstants.hpp"
#include "CharacterKinematics.hpp"
#include "Elevation.hpp"
#include "EnumTraits.hpp"
#include "Facing.hpp"
#include "Hitbox.hpp"
#include "IRenderer.hpp"
#include "Logger.hpp"
#include "MotionSystem.hpp"
#include "Motor.hpp"
#include "PlayerInputState.hpp"
#include "PlayerModes.hpp"
#include "PlayerMovementState.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerSprite.hpp"
#include "Speed.hpp"
#include "Texture.hpp"
#include "TextureStore.hpp"
#include "TileMath.hpp"
#include "Transform.hpp"
#include "WorldServices.hpp"

#include <string>
#include <vector>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Player";

// static fallback keeps borrowed texture references valid without services.
const Texture& EmptyPlayerTexture()
{
    static const Texture empty;
    return empty;
}

// registry services are optional in headless worlds; resolve a nullable store for each operation.
TextureStore* TexturesOf(const entt::registry& world)
{
    const WorldServices* svc = world.ctx().find<WorldServices>();
    return (svc != nullptr) ? svc->textures : nullptr;
}

// character paths come from the optional registry AssetRegistry.
AssetRegistry* AssetsOf(const entt::registry& world)
{
    const WorldServices* svc = world.ctx().find<WorldServices>();
    return (svc != nullptr) ? svc->assets : nullptr;
}
}  // namespace

namespace PlayerSystem
{
bool SwitchCharacter(entt::registry& world, entt::entity player, CharacterType type)
{
    const auto typeName = EnumTraits<CharacterType>::ToString(type);

    // Validate services before changing appearance.
    AssetRegistry* assets = AssetsOf(world);
    TextureStore* textures = TexturesOf(world);
    if (assets == nullptr)
    {
        Logger::Error(LOG_SUBSYSTEM,
                      "SwitchCharacter called with no AssetRegistry in the registry context");
        return false;
    }
    if (textures == nullptr)
    {
        Logger::Error(LOG_SUBSYSTEM,
                      "SwitchCharacter called with no TextureStore in the registry context");
        return false;
    }

    auto getAssetPath = [&](const std::string& spriteType) -> std::string
    {
        std::string path = assets->ResolveCharacterAsset(type, spriteType);
        if (path.empty())
        {
            Logger::ErrorF(LOG_SUBSYSTEM, "No asset registered for {} {}", typeName, spriteType);
        }
        return path;
    };

    // acquire into temporary handles; commit only after validation.
    auto acquire = [&](const std::string& path) -> TextureHandle
    { return path.empty() ? TextureHandle{} : textures->Acquire(path); };

    TextureHandle newWalking = acquire(getAssetPath("Walking"));
    TextureHandle newRunning = acquire(getAssetPath("Running"));
    TextureHandle newBicycle = acquire(getAssetPath("Bicycle"));

    if (!textures->IsValid(newWalking) || !textures->IsValid(newRunning))
    {
        Logger::ErrorF(LOG_SUBSYSTEM, "Failed to load character sprites for {}", typeName);
        return false;
    }

    // bicycle is optional; retain the previous sheet if loading fails.
    if (!textures->IsValid(newBicycle))
    {
        Logger::WarnF(LOG_SUBSYSTEM, "Bicycle sprite not found for {}", typeName);
    }

    auto& sprite = world.get<PlayerSprite>(player);
    auto& appearance = world.get<Appearance>(player);
    sprite.walk = newWalking;
    sprite.run = newRunning;
    if (textures->IsValid(newBicycle))
    {
        sprite.bicycle = newBicycle;
    }
    appearance.characterType = type;

    appearance.accentColor =
        textures->SampleAccent(sprite.walk, ambience::DIALOGUE_ACCENT_FALLBACK);

    Logger::InfoF(LOG_SUBSYSTEM, "Switched to {}", typeName);
    return true;
}

bool CopyAppearanceFrom(entt::registry& world, entt::entity player, const std::string& spritePath)
{
    TextureStore* textures = TexturesOf(world);
    if (textures == nullptr)
    {
        Logger::Error(LOG_SUBSYSTEM,
                      "CopyAppearanceFrom called with no TextureStore in the registry context");
        return false;
    }

    // NPC disguise supplies walking only; run and bicycle restore the original sheets.
    const TextureHandle disguise = textures->Acquire(spritePath);
    if (!textures->IsValid(disguise))
    {
        Logger::ErrorF(LOG_SUBSYSTEM, "Failed to copy appearance from: {}", spritePath);
        return false;
    }

    auto& sprite = world.get<PlayerSprite>(player);
    auto& appearance = world.get<Appearance>(player);
    sprite.walk = disguise;
    appearance.usingCopiedAppearance = true;
    appearance.accentColor =
        textures->SampleAccent(sprite.walk, ambience::DIALOGUE_ACCENT_FALLBACK);
    Logger::InfoF(LOG_SUBSYSTEM, "Copied appearance from: {}", spritePath);
    return true;
}

void RestoreOriginalAppearance(entt::registry& world, entt::entity player)
{
    auto& appearance = world.get<Appearance>(player);
    if (!appearance.usingCopiedAppearance)
    {
        return;
    }

    if (SwitchCharacter(world, player, appearance.characterType))
    {
        world.get<Appearance>(player).usingCopiedAppearance = false;
        Logger::Info(LOG_SUBSYSTEM, "Restored original appearance");
    }
    else
    {
        Logger::Error(LOG_SUBSYSTEM, "Failed to restore original appearance");
    }
}

void UploadTextures(const entt::registry& world, entt::entity player, IRenderer& renderer)
{
    TextureStore* textures = TexturesOf(world);
    if (textures == nullptr)
    {
        return;
    }
    const auto& sprite = world.get<PlayerSprite>(player);
    renderer.UploadTexture(textures->Get(sprite.walk));
    renderer.UploadTexture(textures->Get(sprite.run));
    renderer.UploadTexture(textures->Get(sprite.bicycle));
}

void SetAtlasBinding(entt::registry& world,
                     entt::entity player,
                     const Texture* atlasTex,
                     glm::vec2 walkOffset,
                     glm::vec2 runOffset,
                     glm::vec2 bicycleOffset)
{
    auto& sprite = world.get<PlayerSprite>(player);
    sprite.atlas = atlasTex;
    sprite.atlasWalkOffset = walkOffset;
    sprite.atlasRunOffset = runOffset;
    sprite.atlasBicycleOffset = bicycleOffset;
}

const Texture& GetSpriteSheet(const entt::registry& world, const PlayerSprite& sprite)
{
    TextureStore* textures = TexturesOf(world);
    return (textures != nullptr) ? textures->Get(sprite.walk) : EmptyPlayerTexture();
}

const Texture& GetRunningSpriteSheet(const entt::registry& world, const PlayerSprite& sprite)
{
    TextureStore* textures = TexturesOf(world);
    return (textures != nullptr) ? textures->Get(sprite.run) : EmptyPlayerTexture();
}

const Texture& GetBicycleSpriteSheet(const entt::registry& world, const PlayerSprite& sprite)
{
    TextureStore* textures = TexturesOf(world);
    return (textures != nullptr) ? textures->Get(sprite.bicycle) : EmptyPlayerTexture();
}

void Update(entt::registry& world, entt::entity player, float deltaTime)
{
    // Advance cosmetic state here; Move applies positional movement separately.
    auto& anim = world.get<AnimationState>(player);
    auto& elev = world.get<Elevation>(player);
    PlayerMovementSystem::UpdateAnimation(anim,
                                          world.get<PlayerModes>(player),
                                          world.get<Motor>(player),
                                          deltaTime,
                                          CharacterConstants::ANIM_FRAME_DURATION);
    CharacterKinematics::UpdateElevation(elev, deltaTime);
}

void Move(entt::registry& world,
          entt::entity player,
          glm::vec2 direction,
          float deltaTime,
          const Tilemap* tilemap,
          const std::vector<CharacterCollisionBody>* npcBodies)
{
    // The component retains collision hysteresis between calls; this wrapper owns no movement
    // State.
    PlayerMovementSystem::Step(world.get<Transform>(player),
                               world.get<Motor>(player),
                               world.get<Facing>(player),
                               world.get<AnimationState>(player),
                               world.get<Elevation>(player),
                               world.get<PlayerModes>(player),
                               world.get<PlayerInputState>(player),
                               world.get<PlayerMovementState>(player),
                               world.get<Speed>(player),
                               world.get<Hitbox>(player),
                               direction,
                               deltaTime,
                               tilemap,
                               npcBodies);
}

void Stop(entt::registry& world, entt::entity player)
{
    PlayerMovementSystem::Stop(world.get<AnimationState>(player),
                               world.get<PlayerInputState>(player),
                               world.get<PlayerModes>(player),
                               world.get<Motor>(player));
}

void SetTilePosition(entt::registry& world, entt::entity player, int tileX, int tileY)
{
    // Clear the latched motor target so a teleport cannot pull the player back toward its old grid
    // stop.
    world.get<Transform>(player).position = TileMath::TileFeetCenter(tileX, tileY, 16.0f);
    MotionSystem::Reset(world.get<Motor>(player));
}

void SetPositionRaw(entt::registry& world, entt::entity player, glm::vec2 pos)
{
    // Dialogue alignment needs exact feet positions without tile snapping.
    world.get<Transform>(player).position = pos;
    MotionSystem::Reset(world.get<Motor>(player));
}
}  // namespace PlayerSystem
