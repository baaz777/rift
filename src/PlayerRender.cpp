#include "PlayerRender.hpp"

#include "AnimationState.hpp"
#include "AnimationType.hpp"
#include "CameraFacing.hpp"
#include "CharacterConstants.hpp"
#include "CharacterRender.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "IRenderer.hpp"
#include "PlayerModes.hpp"
#include "PlayerSprite.hpp"
#include "PlayerSystem.hpp"
#include "SceneMath.hpp"
#include "Texture.hpp"
#include "Transform.hpp"

#include <algorithm>

glm::vec2 PlayerRender::SpriteCoords(int frame, CharacterDirection dir, bool requiresYFlip)
{
    int clampedFrame = frame % CharacterConstants::WALK_FRAME_COUNT;
    int spriteX = clampedFrame * CharacterConstants::SPRITE_WIDTH;

    int dirRow = 0;
    switch (dir)
    {
        case CharacterDirection::DOWN:
            dirRow = 0;
            break;
        case CharacterDirection::UP:
            dirRow = 1;
            break;
        case CharacterDirection::LEFT:
            dirRow = 2;
            break;
        case CharacterDirection::RIGHT:
            dirRow = 3;
            break;
    }

    if (requiresYFlip)
    {
        // Logical row -> physical GL row (rows counted up from the bottom of the
        // stbi-flipped sheet). This is a permutation, not the `3 - row` inversion
        // a pure bottom-up flip would give: DOWN and UP come out swapped relative
        // to that, because the artwork's row order is not the logical order.
        //
        //   logical  DOWN(0)  UP(1)  LEFT(2)  RIGHT(3)
        //   GL row      2       3       1        0
        //
        // The result matches the row order NpcRender::SpriteCoords hard-codes
        // (RIGHT=0, LEFT=1, DOWN=2, UP=3) - both sheet families share a layout.
        static const int glRowMap[] = {2, 3, 1, 0};
        dirRow = glRowMap[dirRow];
    }

    return glm::vec2(static_cast<float>(spriteX),
                     static_cast<float>(dirRow * CharacterConstants::SPRITE_HEIGHT));
}

const Texture& PlayerRender::ResolveRenderSheet(const entt::registry& world,
                                                const PlayerModes& modes,
                                                const PlayerSprite& sprite,
                                                glm::vec2& spriteCoords)
{
    const Texture& localSheet = modes.isBicycling
                                    ? PlayerSystem::GetBicycleSpriteSheet(world, sprite)
                                : (modes.animationType == AnimationType::RUN)
                                    ? PlayerSystem::GetRunningSpriteSheet(world, sprite)
                                    : PlayerSystem::GetSpriteSheet(world, sprite);

    if (sprite.atlas != nullptr)
    {
        spriteCoords += modes.isBicycling                             ? sprite.atlasBicycleOffset
                        : (modes.animationType == AnimationType::RUN) ? sprite.atlasRunOffset
                                                                      : sprite.atlasWalkOffset;
        return *sprite.atlas;
    }
    return localSheet;
}

// Pipeline: pick UVs (SpriteCoords, with the renderer's flip convention) -> resolve
// sheet + fold the mode's atlas offset (ResolveRenderSheet) -> place with elevation
// (ComputeRenderPos) -> draw the requested half (DrawPart).
void PlayerRender::DrawHalf(const entt::registry& world,
                            IRenderer& renderer,
                            glm::vec2 cameraPos,
                            bool topHalf,
                            const Transform& xf,
                            const Elevation& elev,
                            const Facing& facing,
                            const AnimationState& anim,
                            const PlayerModes& modes,
                            const PlayerSprite& sprite)
{
    glm::vec2 spriteCoords = SpriteCoords(anim.currentFrame, facing.dir, renderer.RequiresYFlip());
    const Texture& sheet = ResolveRenderSheet(world, modes, sprite, spriteCoords);
    const glm::vec2 spriteSize(CharacterConstants::SPRITE_WIDTH_F,
                               CharacterConstants::SPRITE_HEIGHT_F);
    glm::vec2 renderPos =
        CharacterRender::ComputeRenderPos(xf.position, cameraPos, elev.offset, spriteSize);
    CharacterRender::DrawPart(
        renderer,
        sheet,
        renderPos,
        spriteCoords,
        spriteSize,
        topHalf ? CharacterRender::Part::TopHalf : CharacterRender::Part::BottomHalf);
}

void PlayerRender::Draw3D(const entt::registry& world,
                          IRenderer& renderer,
                          const billboard::Orientation& orientation,
                          const Transform& xf,
                          const Elevation& elev,
                          const Facing& facing,
                          const AnimationState& anim,
                          const PlayerModes& modes,
                          const PlayerSprite& sprite)
{
    // Rotate the row lookup by billboard yaw to match the visible side; retain stored facing.
    const CharacterDirection screenDir =
        cameraFacing::ScreenFacing(facing.dir, orientation.yawRadians);

    glm::vec2 spriteCoords = SpriteCoords(anim.currentFrame, screenDir, renderer.RequiresYFlip());
    const Texture& sheet = ResolveRenderSheet(world, modes, sprite, spriteCoords);
    const glm::vec2 spriteSize(CharacterConstants::SPRITE_WIDTH_F,
                               CharacterConstants::SPRITE_HEIGHT_F);

    // Visual height is elev.offset; the plane index is for collision and sorting.
    const glm::vec3 footCenter = sceneMath::ToScene(xf.position, elev.offset);

    CharacterRender::DrawBillboard(
        renderer, sheet, footCenter, spriteCoords, spriteSize, orientation);
}
