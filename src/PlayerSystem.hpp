#pragma once

#include "SupportSurface.hpp"

#include "CharacterType.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>

class IRenderer;
class Texture;
class Tilemap;
struct PlayerSprite;

/**
 * @brief Player appearance, movement, and animation operations over registry components.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * services come from world.ctx().find<WorldServices>(). entity components are updated in place;
 * stateless PlayerMovementSystem handles movement and stopping. Missing services are tolerated,
 * but callers must supply a valid player with the required components.
 */
namespace PlayerSystem
{
/**
 * @fn bool PlayerSystem::SwitchCharacter(entt::registry& world, entt::entity player, \
 *     CharacterType type)
 * @brief Load walk and run sheets; bicycle is optional.
 * @author Alex (<https://github.com/lextpf>)
 *
 * On failure return false and preserve the appearance.
 *
 * Resolve paths through AssetRegistry and load through TextureStore. success updates PlayerSprite
 * and Appearance and resamples the dialogue accent. If the optional bicycle sheet cannot load,
 * retain the previous bicycle sheet. Failed switches can still populate the texture cache.
 */
bool SwitchCharacter(entt::registry& world, entt::entity player, CharacterType type);

/**
 * @fn bool PlayerSystem::CopyAppearanceFrom(entt::registry& world, entt::entity player, const \
 *     std::string& spritePath)
 * @brief Copy an NPC walking sheet, set disguise and resample accent; failure preserves the
 * appearance.
 * @author Alex (<https://github.com/lextpf>)
 */
bool CopyAppearanceFrom(entt::registry& world, entt::entity player, const std::string& spritePath);

/**
 * @fn void PlayerSystem::RestoreOriginalAppearance(entt::registry& world, entt::entity player)
 * @brief Reload original sheets and clear disguise on success; do nothing when not disguised.
 * @author Alex (<https://github.com/lextpf>)
 */
void RestoreOriginalAppearance(entt::registry& world, entt::entity player);

/**
 * @fn void PlayerSystem::UploadTextures(const entt::registry& world, entt::entity player, \
 *     IRenderer& renderer)
 * @brief Upload walk, run and bicycle sheets after an appearance change or renderer switch; missing
 * TextureStore makes this a no-op.
 * @author Alex (<https://github.com/lextpf>)
 */
void UploadTextures(const entt::registry& world, entt::entity player, IRenderer& renderer);

/**
 * @fn void PlayerSystem::SetAtlasBinding(entt::registry& world, entt::entity player, const \
 *     Texture* atlasTex, glm::vec2 walkOffset, glm::vec2 runOffset, glm::vec2 bicycleOffset)
 * @brief Borrow atlasTex; null restores individual sheets.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Offsets are in atlas pixels.
 *
 */
void SetAtlasBinding(entt::registry& world,
                     entt::entity player,
                     const Texture* atlasTex,
                     glm::vec2 walkOffset,
                     glm::vec2 runOffset,
                     glm::vec2 bicycleOffset);

/**
 * @fn const Texture& PlayerSystem::GetSpriteSheet(const entt::registry& world, const \
 *     PlayerSprite& sprite)
 * @brief Borrow the walking sheet, or a shared empty texture if no TextureStore is published.
 * @author Alex (<https://github.com/lextpf>)
 */
const Texture& GetSpriteSheet(const entt::registry& world, const PlayerSprite& sprite);

/**
 * @fn const Texture& PlayerSystem::GetRunningSpriteSheet(const entt::registry& world, const \
 *     PlayerSprite& sprite)
 * @brief Borrow the running sheet, or a shared empty texture without TextureStore.
 * @author Alex (<https://github.com/lextpf>)
 */
const Texture& GetRunningSpriteSheet(const entt::registry& world, const PlayerSprite& sprite);

/**
 * @fn const Texture& PlayerSystem::GetBicycleSpriteSheet(const entt::registry& world, const \
 *     PlayerSprite& sprite)
 * @brief Borrow the bicycle sheet, or a shared empty texture without TextureStore.
 * @author Alex (<https://github.com/lextpf>)
 */
const Texture& GetBicycleSpriteSheet(const entt::registry& world, const PlayerSprite& sprite);

/**
 * @fn void PlayerSystem::Update(entt::registry& world, entt::entity player, float deltaTime)
 * @brief Advance velocity-driven walk cadence and smooth visual elevation; deltaTime is in seconds.
 * @author Alex (<https://github.com/lextpf>)
 *
 * position changes belong to Move. Animation follows actual motor speed during the glide after
 * input ends.
 */
void Update(entt::registry& world, entt::entity player, float deltaTime);

/**
 * @fn void PlayerSystem::Move(entt::registry& world, entt::entity player, glm::vec2 direction, \
 *     float deltaTime, const Tilemap* tilemap, const std::vector<CharacterCollisionBody>* \
 *     npcBodies)
 * @brief Apply normalized direction for deltaTime seconds.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Null tilemap skips world blocking and support commit; derive the plane afterwards
 * with CharacterKinematics::DerivePlane. Null npcBodies skips NPC blocking.
 */
void Move(entt::registry& world,
          entt::entity player,
          glm::vec2 direction,
          float deltaTime,
          const Tilemap* tilemap,
          const std::vector<CharacterCollisionBody>* npcBodies);

/**
 * @fn void PlayerSystem::Stop(entt::registry& world, entt::entity player)
 * @brief Reset movement to idle, including the motor stop target.
 * @author Alex (<https://github.com/lextpf>)
 */
void Stop(entt::registry& world, entt::entity player);

/**
 * @fn void PlayerSystem::SetTilePosition(entt::registry& world, entt::entity player, int tileX, \
 *     int tileY)
 * @brief Snap feet to tile bottom-center and reset the motor; this path assumes 16 px tiles.
 * @author Alex (<https://github.com/lextpf>)
 *
 * resetting the motor clears the old grid stop target so it cannot pull the player back after
 * teleporting. projects with another tile size must avoid this snapping path.
 */
void SetTilePosition(entt::registry& world, entt::entity player, int tileX, int tileY);

/**
 * @fn void PlayerSystem::SetPositionRaw(entt::registry& world, entt::entity player, glm::vec2 \
 *     pos)
 * @brief Set feet in world pixels without snapping and reset the motor stop target.
 * @author Alex (<https://github.com/lextpf>)
 */
void SetPositionRaw(entt::registry& world, entt::entity player, glm::vec2 pos);
}  // namespace PlayerSystem
