#include "NpcRender.hpp"

#include "AnimationState.hpp"
#include "CameraFacing.hpp"
#include "CharacterConstants.hpp"
#include "CharacterRender.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "NpcSprite.hpp"
#include "SceneMath.hpp"
#include "Texture.hpp"
#include "TextureStore.hpp"
#include "Transform.hpp"
#include "WorldServices.hpp"

#include <algorithm>
#include <cctype>

namespace
{

const Texture& EmptyNpcTexture()
{
    static const Texture empty;
    return empty;
}

const Texture& NpcSheet(const entt::registry& world, const NpcSprite& sprite)
{
    const WorldServices* svc = world.ctx().find<WorldServices>();
    return (svc != nullptr && svc->textures != nullptr) ? svc->textures->Get(sprite.sheet)
                                                        : EmptyNpcTexture();
}
}  // namespace

glm::vec2 NpcRender::SpriteCoords(int frame, CharacterDirection dir)
{
    int spriteX = (frame % CharacterConstants::WALK_FRAME_COUNT) * CharacterConstants::SPRITE_WIDTH;
    int spriteY = 0;

    switch (dir)
    {
        case CharacterDirection::DOWN:
            spriteY = 2 * CharacterConstants::SPRITE_HEIGHT;
            break;
        case CharacterDirection::UP:
            spriteY = 3 * CharacterConstants::SPRITE_HEIGHT;
            break;
        case CharacterDirection::LEFT:
            spriteY = 1 * CharacterConstants::SPRITE_HEIGHT;
            break;
        case CharacterDirection::RIGHT:
            spriteY = 0 * CharacterConstants::SPRITE_HEIGHT;
            break;
        default:
            spriteY = 2 * CharacterConstants::SPRITE_HEIGHT;
            break;
    }

    return glm::vec2(static_cast<float>(spriteX), static_cast<float>(spriteY));
}

const Texture& NpcRender::ResolveRenderSheet(const entt::registry& world,
                                             const NpcSprite& sprite,
                                             glm::vec2& spriteCoords)
{
    // Atlas offsets use the standalone flipped row convention; retain flipY = false.
    if (sprite.atlas != nullptr)
    {
        spriteCoords += sprite.atlasOffset;
        return *sprite.atlas;
    }
    return NpcSheet(world, sprite);
}

// Pipeline: pick UVs (SpriteCoords) -> resolve sheet + fold atlas offset
// (ResolveRenderSheet) -> place with elevation (ComputeRenderPos) -> draw the
// requested half (DrawPart).
void NpcRender::DrawHalf(const entt::registry& world,
                         IRenderer& renderer,
                         glm::vec2 cameraPos,
                         bool topHalf,
                         const Transform& xf,
                         const Elevation& elev,
                         const Facing& facing,
                         const AnimationState& anim,
                         const NpcSprite& sprite)
{
    const glm::vec2 spriteSize(static_cast<float>(CharacterConstants::SPRITE_WIDTH),
                               static_cast<float>(CharacterConstants::SPRITE_HEIGHT));
    glm::vec2 spriteCoords = SpriteCoords(anim.currentFrame, facing.dir);
    const Texture& sheet = ResolveRenderSheet(world, sprite, spriteCoords);
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

void NpcRender::Draw3D(const entt::registry& world,
                       IRenderer& renderer,
                       const billboard::Orientation& orientation,
                       const Transform& xf,
                       const Elevation& elev,
                       const Facing& facing,
                       const AnimationState& anim,
                       const NpcSprite& sprite)
{
    const glm::vec2 spriteSize(static_cast<float>(CharacterConstants::SPRITE_WIDTH),
                               static_cast<float>(CharacterConstants::SPRITE_HEIGHT));

    // Rotate the row lookup by billboard yaw without changing stored facing.
    const CharacterDirection screenDir =
        cameraFacing::ScreenFacing(facing.dir, orientation.yawRadians);

    glm::vec2 spriteCoords = SpriteCoords(anim.currentFrame, screenDir);
    const Texture& sheet = ResolveRenderSheet(world, sprite, spriteCoords);

    const glm::vec3 footCenter = sceneMath::ToScene(xf.position, elev.offset);

    CharacterRender::DrawBillboard(
        renderer, sheet, footCenter, spriteCoords, spriteSize, orientation);
}

std::string NpcType::FromSpritePath(const std::string& path)
{
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

    if (filename.size() > 4)
    {
        std::string ext = filename.substr(filename.size() - 4);
        std::transform(ext.begin(),
                       ext.end(),
                       ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".png")
        {
            return filename.substr(0, filename.size() - 4);
        }
    }

    return filename;
}
