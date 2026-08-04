#pragma once

#include "TextureHandle.hpp"

#include <glm/glm.hpp>

class Texture;

/**
 * @struct NpcSprite
 * @brief Runtime NPC sheet and atlas binding; atlas is borrowed from Tilemap and never persisted.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct NpcSprite
{
    TextureHandle sheet;
    const Texture* atlas{nullptr};
    glm::vec2 atlasOffset{0.0f};  ///< Pixel offset of this NPC's sheet within the atlas.
    /// sampled once at spawn; absent TextureStore uses DIALOGUE_ACCENT_FALLBACK.
    glm::vec3 accentColor{0.0f};
};
