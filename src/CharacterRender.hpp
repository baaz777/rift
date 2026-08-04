#pragma once

#include "Billboard.hpp"

#include <glm/glm.hpp>

class IRenderer;
class Texture;

/**
 * @brief Shared sprite projection and draw helpers.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
namespace CharacterRender
{
/**
 * @enum Part
 * @brief Sprite slices used by the Y-sort pass.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Sheets are loaded bottom-up and drawn with flipY = false. BottomHalf samples source
 * offset 0; TopHalf samples half a cell above it.
 *
 * @verbatim
 *   one atlas cell (H = spriteSize.y), v increases upward in GL row space
 *
 *   v = coords.y + H     +------------------+  <- cell top
 *                        |    head/torso    |   Part::TopHalf
 *                        |  source +H/2     |   -> screen y = renderPos.y
 *   v = coords.y + H/2   +------------------+
 *                        |       feet       |   Part::BottomHalf
 *                        |  source +0       |   -> screen y = renderPos.y + H/2
 *   v = coords.y         +------------------+  <- cell bottom = the anchor row
 * @endverbatim
 */
enum class Part
{
    Full,
    BottomHalf,  ///< Feet: lower screen band, sampled at source offset +0.
    TopHalf,     ///< Head/torso: upper screen band, sampled at source offset +H/2.
};

/**
 * @fn glm::vec2 ComputeRenderPos(glm::vec2 feetWorld, glm::vec2 cameraPos, float \
 * elevationOffset, glm::vec2 spriteSize)
 * @brief Convert a feet anchor to the sprite's top-left position for the flat view.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param feetWorld       Bottom-center anchor in world pixels.
 * @param cameraPos       World position of the viewport's top-left corner.
 * @param elevationOffset Upward lift in pixels (positive = higher).
 * @param spriteSize      Sprite cell size in pixels.
 * @return Top-left view position in world pixels, before projection and viewport scaling.
 */
glm::vec2 ComputeRenderPos(glm::vec2 feetWorld,
                           glm::vec2 cameraPos,
                           float elevationOffset,
                           glm::vec2 spriteSize);

/**
 * @fn void DrawPart(IRenderer& renderer, const Texture& sheet, glm::vec2 renderPos, glm::vec2 \
 * spriteCoords, glm::vec2 spriteSize, Part part)
 * @brief Draws a sprite slice with flipY = false.
 * @author Alex (<https://github.com/lextpf>)
 *
 * renderPos is the top-left of the full sprite. spriteCoords is the cell origin in
 * bottom-up GL rows; spriteSize is the full cell size in pixels.
 */
void DrawPart(IRenderer& renderer,
              const Texture& sheet,
              glm::vec2 renderPos,
              glm::vec2 spriteCoords,
              glm::vec2 spriteSize,
              Part part);

/**
 * @fn void DrawBillboard(IRenderer& renderer, const Texture& sheet, glm::vec3 footCenter, \
 * glm::vec2 spriteCoords, glm::vec2 spriteSize, const billboard::Orientation& orientation)
 * @brief Draws one character billboard with depth writes.
 * @author Alex (<https://github.com/lextpf>)
 *
 * footCenter is scene-space feet, including elevation. spriteCoords is the cell
 * origin in bottom-up GL rows. spriteSize is the full cell size in world pixels.
 *
 * @pre Submit in opaque pass A1. alpha discard provides per-pixel occlusion.
 */
void DrawBillboard(IRenderer& renderer,
                   const Texture& sheet,
                   glm::vec3 footCenter,
                   glm::vec2 spriteCoords,
                   glm::vec2 spriteSize,
                   const billboard::Orientation& orientation);
}  // namespace CharacterRender
