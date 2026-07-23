#pragma once

#include "Billboard.hpp"
#include "CharacterDirection.hpp"

#include <entt/entt.hpp>

#include <glm/glm.hpp>

class IRenderer;
class Texture;
struct Transform;
struct Elevation;
struct Facing;
struct AnimationState;
struct PlayerModes;
struct PlayerSprite;

/**
 * @brief Player rendering selects sheets in bicycle, run, walk priority order through the registry
 * TextureStore.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Resolve shared textures through WorldServices for each draw. The Y-sort pass passes component
 * references from one entity; these helpers do not retain them.
 */
namespace PlayerRender
{
/**
 * @fn glm::vec2 SpriteCoords(int frame, CharacterDirection dir, bool requiresYFlip = false)
 * @brief Cell origin in pixels; frame wraps to WALK_FRAME_COUNT.
 * @author Alex (<https://github.com/lextpf>)
 *
 * requiresYFlip remaps logical rows to the shared flipped artwork layout.
 * Both backends request this mapping. false returns the logical rows.
 *
 * @verbatim
 *     logical  DOWN(0) UP(1) LEFT(2) RIGHT(3)
 *     GL row      2      3      1       0
 * @endverbatim
 */
glm::vec2 SpriteCoords(int frame, CharacterDirection dir, bool requiresYFlip = false);

/**
 * @fn const Texture& ResolveRenderSheet(const entt::registry& world, const PlayerModes& modes, \
 * const PlayerSprite& sprite, glm::vec2& spriteCoords)
 * @brief Borrow the atlas or active mode sheet for this draw only.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Select bicycle, run, walk in that order. Add the mode atlas pixel offset to spriteCoords
 * when bound. Missing TextureStore returns an empty texture. Repacks and map reloads
 * invalidate the borrowed contents.
 */
const Texture& ResolveRenderSheet(const entt::registry& world,
                                  const PlayerModes& modes,
                                  const PlayerSprite& sprite,
                                  glm::vec2& spriteCoords);

/**
 * @fn void DrawHalf(const entt::registry& world, IRenderer& renderer, glm::vec2 cameraPos, bool \
 * topHalf, const Transform& xf, const Elevation& elev, const Facing& facing, const \
 * AnimationState& anim, const PlayerModes& modes, const PlayerSprite& sprite)
 * @brief Draw the selected half of a character in the flat view.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Submit both halves consecutively from one render-list entry to prevent interleaved tiles.
 * `cameraPos` is the viewport top-left in world pixels. `topHalf` selects the upper screen
 * band. Only elev.offset supplies visual height.
 *
 * Resolve the active sheet and animation cell, then delegate projection and half selection to
 * CharacterRender. facing chooses the row and anim chooses the walk frame; xf.position is the feet
 * anchor.
 */
void DrawHalf(const entt::registry& world,
              IRenderer& renderer,
              glm::vec2 cameraPos,
              bool topHalf,
              const Transform& xf,
              const Elevation& elev,
              const Facing& facing,
              const AnimationState& anim,
              const PlayerModes& modes,
              const PlayerSprite& sprite);

/**
 * @fn void Draw3D(const entt::registry& world, IRenderer& renderer, const \
 * billboard::Orientation& orientation, const Transform& xf, const Elevation& elev, const Facing& \
 * facing, const AnimationState& anim, const PlayerModes& modes, const PlayerSprite& sprite)
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
            const PlayerModes& modes,
            const PlayerSprite& sprite);
}  // namespace PlayerRender
