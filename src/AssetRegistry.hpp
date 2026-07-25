#pragma once

#include "CharacterType.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class AssetRegistry
 * @brief Sprite paths keyed by player variant or NPC type.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Game owns the registry and publishes it through WorldServices::assets.
 * The project manifest fills the paths; TextureStore owns the pixels.
 */
class AssetRegistry
{
public:
    /**
     * @fn void AssetRegistry::SetCharacterAsset(CharacterType type, const std::string& \
     *     spriteType, const std::string& path)
     * @brief Register a player character sprite path, keyed by (type, spriteType).
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetCharacterAsset(CharacterType type,
                           const std::string& spriteType,
                           const std::string& path);

    /**
     * @fn std::string AssetRegistry::ResolveCharacterAsset(CharacterType type, const std::string& \
     *     spriteType) const
     * @brief Resolve a player character sprite path, or "" if unregistered.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::string ResolveCharacterAsset(CharacterType type,
                                                    const std::string& spriteType) const;

    /**
     * @fn void AssetRegistry::SetNpcAsset(const std::string& type, const std::string& path)
     * @brief Register an NPC sprite path for a type id (filename without extension).
     * @author Alex (<https://github.com/lextpf>)
     *
     * An empty `type` is ignored silently; a known type id has its path overwritten.
     */
    void SetNpcAsset(const std::string& type, const std::string& path);

    /**
     * @fn std::string AssetRegistry::ResolveNpcAsset(const std::string& type) const
     * @brief Returns the registered path or a working-directory-relative fallback.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Registered paths are returned unchanged. Unknown types resolve to
     * `assets/non-player/<type>.png`, independent of the manifest base directory.
     */
    [[nodiscard]] std::string ResolveNpcAsset(const std::string& type) const;

    /**
     * @fn std::vector<std::string> AssetRegistry::AvailableNpcTypes() const
     * @brief Registered NPC type ids, unspecified order (npc.spawn autocomplete).
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::vector<std::string> AvailableNpcTypes() const;

private:
    /// Key for the player character asset table.
    struct CharacterAssetKey
    {
        CharacterType type;
        std::string spriteType;

        bool operator<(const CharacterAssetKey& other) const
        {
            if (type != other.type)
                return type < other.type;
            return spriteType < other.spriteType;
        }
    };

    std::map<CharacterAssetKey, std::string> m_CharacterAssets;
    std::unordered_map<std::string, std::string> m_NpcAssets;
};
