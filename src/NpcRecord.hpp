#pragma once

#include "CharacterDirection.hpp"
#include "DialogueTypes.hpp"

#include <cstdint>
#include <string>

/**
 * @struct NpcRecord
 * @brief Detached NPC data for map loading and undo; nonzero instanceId survives respawn. Routes
 * rebuild at runtime.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct NpcRecord
{
    std::string type;  ///< Sprite-path lookup key; becomes Dialogue::type.
    std::string name;
    std::string text;  ///< Simple dialogue line; becomes Dialogue::text, or a default if empty.
    int tileX = 0;
    int tileY = 0;
    /**
     * @brief Tile size in pixels; SnapshotNpc assumes 16 px, so round trips misplace NPCs on other
     * tile sizes.
     */
    int tileSize = 16;
    DialogueTree tree;
    bool hasTree = false;
    std::uint64_t instanceId = 0;  ///< 0 assigns a fresh Identity; nonzero preserves this one.
    CharacterDirection facing = CharacterDirection::DOWN;
};
