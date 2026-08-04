#pragma once

#include "AnimationState.hpp"
#include "Elevation.hpp"
#include "ElevationAxis.hpp"
#include "SupportSurface.hpp"

#include <glm/glm.hpp>

class Tilemap;

/**
 * @brief Character support commits and visual transitions.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 *
 * SurfaceSystem resolves support; these functions apply accepted transitions to components.
 */
namespace CharacterKinematics
{
/**
 * @fn void SetElevationTarget(Elevation& elev, float offset)
 * @brief Starts smoothstep toward the target height in pixels.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Starts from the current visual offset, so retargeting during a transition stays continuous.
 * An unchanged target preserves the current interpolation progress.
 */
void SetElevationTarget(Elevation& elev, float offset);

/**
 * @fn void UpdatePlane(Elevation& elev, int destTileElev, ElevationAxis tileAxis, int moveDx, int \
 *     moveDy)
 * @brief Update support from a single destination tile.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Movement must use SurfaceSystem::ResolveMove to validate each crossed boundary.
 *
 * None commits destTileElev without a step gate. X or Y commits when the matching movement
 * component is nonzero and the height delta passes MAX_STEP_HEIGHT. Other cases do nothing.
 * Unlike ResolveMove, this accepts diagonal ramp entry.
 *
 * destTileElev is in pixels. moveDx and moveDy are step signs (-1, 0, +1);
 * positive moveDy is south.
 */
void UpdatePlane(Elevation& elev, int destTileElev, ElevationAxis tileAxis, int moveDx, int moveDy);

SupportState GetSupport(const Elevation& elev);

/**
 * @fn SurfaceTransition ResolveSupport(const Elevation& elev, glm::vec2 before, glm::vec2 after, \
 *     const Tilemap& tilemap)
 * @brief Probes support without changing the component.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Before and after are feet positions in world pixels. Reject disconnected transitions.
 */
SurfaceTransition ResolveSupport(const Elevation& elev,
                                 glm::vec2 before,
                                 glm::vec2 after,
                                 const Tilemap& tilemap);

/**
 * @fn void CommitSupport(Elevation& elev, SupportState support)
 * @brief Commits support and starts its visual transition after an accepted move.
 * @author Alex (<https://github.com/lextpf>)
 */
void CommitSupport(Elevation& elev, SupportState support);

/**
 * @fn void DerivePlane(Elevation& elev, glm::vec2 before, glm::vec2 after, const Tilemap& \
 *     tilemap)
 * @brief Updates support after an external or no-clip move.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Before and after are feet positions in world pixels. Disconnected paths leave support
 * unchanged. Collision-aware movement must probe before moving.
 */
void DerivePlane(Elevation& elev, glm::vec2 before, glm::vec2 after, const Tilemap& tilemap);

/**
 * @fn void UpdateElevation(Elevation& elev, float deltaTime)
 * @brief Advances the visual offset over 0.15 s; logical support is unchanged.
 * @author Alex (<https://github.com/lextpf>)
 *
 * deltaTime is frame time in seconds.
 */
void UpdateElevation(Elevation& elev, float deltaTime);

/**
 * @fn void AdvanceWalkAnimation(AnimationState& anim)
 * @brief Advances the repeating 1, 0, 2, 0 frame sequence.
 * @author Alex (<https://github.com/lextpf>)
 */
void AdvanceWalkAnimation(AnimationState& anim);

/**
 * @fn void ResetAnimation(AnimationState& anim)
 * @brief Resets the frame, sequence index and timing accumulator to zero.
 * @author Alex (<https://github.com/lextpf>)
 */
void ResetAnimation(AnimationState& anim);
}  // namespace CharacterKinematics
