#pragma once

#include "TextureHandle.hpp"

#include <glm/glm.hpp>

class Texture;

/**
 * @struct PlayerSprite
 * @brief Runtime sheet handles and a borrowed Tilemap atlas.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 *
 * Offsets are in pixels from the atlas bottom, valid only while atlas is non-null.
 * PackAdditionalSheets stacks sheets vertically at X = 0; repacking replaces all offsets.
 */
struct PlayerSprite
{
    TextureHandle walk;
    TextureHandle run;
    TextureHandle bicycle;

    const Texture* atlas{nullptr};
    glm::vec2 atlasWalkOffset{0.0f};
    glm::vec2 atlasRunOffset{0.0f};
    glm::vec2 atlasBicycleOffset{0.0f};
};
