#pragma once

#include "Billboard.hpp"
#include "CharacterDirection.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>

#include <string>

class IRenderer;
class Texture;
struct Transform;
struct Elevation;
struct Facing;
struct AnimationState;
struct NpcSprite;

/**
 * @brief NPC rendering resolves TextureStore through WorldServices; absent services yield an empty
 * texture.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * The Y-sort pass supplies granular components from one entity. Helpers resolve a sheet and cell,
 * then submit through IRenderer without retaining entity or texture references across frames.
 */
namespace NpcRender
{
/**
 * @fn glm::vec2 SpriteCoords(int frame, CharacterDirection dir)
 * @brief Cell origin in pixels, with rows counted from the flipped texture bottom.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Rows are RIGHT=0, LEFT=1, DOWN=2, UP=3; unknown directions use DOWN.
 * Frame wraps to WALK_FRAME_COUNT. Pass flipY = false to CharacterRender.
 *
 * The sheet rows already use the flipped artwork order, so there is no requiresYFlip remapping
 * step here. PlayerRender starts from a different logical direction order.
 */
glm::vec2 SpriteCoords(int frame, CharacterDirection dir);

/**
 * @fn const Texture& ResolveRenderSheet(const entt::registry& world, const NpcSprite& sprite, \
 * glm::vec2& spriteCoords)
 * @brief Borrow the atlas or NPC sheet for this draw only.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Add the atlas pixel offset to spriteCoords when bound. Missing TextureStore returns
 * an empty texture. Atlas repacks and map reloads invalidate the borrowed contents.
 */
const Texture& ResolveRenderSheet(const entt::registry& world,
                                  const NpcSprite& sprite,
                                  glm::vec2& spriteCoords);

/**
 * @fn void DrawHalf(const entt::registry& world, IRenderer& renderer, glm::vec2 cameraPos, bool \
 * topHalf, const Transform& xf, const Elevation& elev, const Facing& facing, const \
 * AnimationState& anim, const NpcSprite& sprite)
 * @brief Draw the selected half of a character in the flat view.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Submit both halves consecutively from one render-list entry to prevent interleaved tiles.
 * `cameraPos` is the viewport top-left in world pixels. `topHalf` selects the upper screen
 * band. Only elev.offset supplies visual height.
 *
 * xf.position is the feet anchor. facing selects the sheet row and anim selects the walk frame;
 * CharacterRender applies projection and half selection.
 */
void DrawHalf(const entt::registry& world,
              IRenderer& renderer,
              glm::vec2 cameraPos,
              bool topHalf,
              const Transform& xf,
              const Elevation& elev,
              const Facing& facing,
              const AnimationState& anim,
              const NpcSprite& sprite);

/**
 * @fn void Draw3D(const entt::registry& world, IRenderer& renderer, const \
 * billboard::Orientation& orientation, const Transform& xf, const Elevation& elev, const Facing& \
 * facing, const AnimationState& anim, const NpcSprite& sprite)
 * @brief Draw one world-space billboard from the feet anchor.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Use elev.offset for visual height and the damped Character-role orientation. Depth testing
 * replaces the flat top/bottom split; facing and animation select the same sheet cell as DrawHalf.
 */
void Draw3D(const entt::registry& world,
            IRenderer& renderer,
            const billboard::Orientation& orientation,
            const Transform& xf,
            const Elevation& elev,
            const Facing& facing,
            const AnimationState& anim,
            const NpcSprite& sprite);
}  // namespace NpcRender

/**
 * @brief NPC type names derived from sprite filenames.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
namespace NpcType
{
/**
 * @fn std::string FromSpritePath(const std::string& path)
 * @brief Strip either directory separator and a case-insensitive .png suffix; keep other suffixes
 * and bare .png.
 * @author Alex (<https://github.com/lextpf>)
 */
std::string FromSpritePath(const std::string& path);
}  // namespace NpcType
