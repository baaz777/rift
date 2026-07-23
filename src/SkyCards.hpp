#pragma once

#include "ParticleCards.hpp"
#include "SceneMath.hpp"
#include "SkyDrawList.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Places sky elements on the camera-facing particle sheet.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Element positions are viewport-relative world pixels. The sheet offset is
 * pos + size/2 - halfVisible; camera translation cancels. Both projections frame the
 * visible world extent at the sheet depth, preserving flat-path placement.
 *
 * @note The lightning flash is centred only at zoom 1: its size uses the unzoomed
 * viewport while halfVisible includes zoom.
 *
 * $$ centreWorld = (focus - halfVisible) + pos + size/2 $$
 *
 * $ d = pos + size/2 - halfVisible $
 *
 * @verbatim
 *  the lightning flash, twice the visible extent, at zoom 1
 *
 *    pos  = (-sw/2, -sh/2) = (-160, -90)      sw, sh = 320, 180
 *    size = ( 2*sw,  2*sh) = ( 640, 360)      halfVisible = (160, 90)
 *
 *    d = pos + size/2 - halfVisible
 *      = (-160 + 320 - 160, -90 + 180 - 90)
 *      = (0, 0)                     <- exactly on the focus
 *
 *    so the quad spans 2*halfVisible each way: the viewport plus a half
 *    viewport of margin on every side, which is its flat coverage. Under DS
 *    on a 1520x855 framebuffer it projects to (-760, -427.5)..(2280, 1282.5).
 *
 *    it is one quad, never split - the sheet has no extent of its own - and
 *    never depth-clipped, because the sheet plane passes through the focus.
 *
 *  and a worked ordinary element, DS, visible 320x180, screen 1520x855
 *
 *    atmospheric bottom band: pos = (0, 158.4), size = (320, 21.6)
 *      d        = (0, 79.2)
 *      screen y = 427.5 + 79.2 * (855/180) = 803.70
 *      flat     = 169.2 * (855/180)        = 803.70    identical
 * @endverbatim
 */
namespace skyCards
{

/// Bounds sky submissions to protect the shared vertex buffer.
inline constexpr std::size_t MAX_SKY_QUADS_3D = 2048;

/// Ceiling on world light pool quads submitted in one frame.
inline constexpr std::size_t MAX_LIGHT_POOL_QUADS_3D = 256;

/**
 * @brief Reserves essential effects before thinning stars and aurora.
 *
 * Emission order puts flashes and bolts near the tail. A flat cap would remove them first.
 * Aurora and star receive the remaining budget.
 */
inline constexpr std::array<std::uint16_t, skyDraw::LAYER_COUNT> LAYER_RESERVE{
    4,   // DawnWash: two gradient quads plus two horizon quads
    2,   // AtmosphericWash: the bottom band and the top shimmer
    0,   // Aurora: elastic
    0,   // Star: elastic
    18,  // Meteor: the MeteorShower concurrent ceiling
    4,   // Dew: DEW_SPARKLE_COUNT
    12,  // Ray: three sun and three moon rays, two quads each
    1,   // Flash: one full-screen wash
    49   // Bolt: 25 main segments plus three branches of up to eight
};

static_assert(LAYER_RESERVE[0] + LAYER_RESERVE[1] + LAYER_RESERVE[2] + LAYER_RESERVE[3] +
                      LAYER_RESERVE[4] + LAYER_RESERVE[5] + LAYER_RESERVE[6] + LAYER_RESERVE[7] +
                      LAYER_RESERVE[8] <
                  MAX_SKY_QUADS_3D,
              "reserved layers must leave room for the elastic aurora and star layers");

inline std::size_t ReserveFor(skyDraw::Layer layer)
{
    return LAYER_RESERVE[static_cast<std::size_t>(layer)];
}

/**
 * @fn glm::vec2 ViewportTopLeft(const particleCards::Frame& frame)
 * @brief World position of the viewport's top-left corner, which is the origin sky element
 * positions are measured from.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec2 ViewportTopLeft(const particleCards::Frame& frame)
{
    return frame.focusWorld - frame.halfVisible;
}

/**
 * @fn glm::vec2 ElementCentreWorld(const particleCards::Frame& frame, glm::vec2 pos, glm::vec2 \
 * size)
 * @brief World position of an element's centre, from its top-left pos.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec2 ElementCentreWorld(const particleCards::Frame& frame,
                                    glm::vec2 pos,
                                    glm::vec2 size)
{
    return ViewportTopLeft(frame) + pos + size * 0.5f;
}

/**
 * @fn glm::vec3 SheetCentre(const particleCards::Frame& frame, glm::vec2 pos, glm::vec2 size)
 * @brief Scene point an element's centre occupies on the sheet.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 SheetCentre(const particleCards::Frame& frame, glm::vec2 pos, glm::vec2 size)
{
    return particleCards::CardPoint(
        frame, frame.focusWorld, frame.focusHeight, ElementCentreWorld(frame, pos, size));
}

/**
 * @fn void MakeSkyQuad(const particleCards::Frame& frame, const skyDraw::Element& e, glm::vec3 \
 * outCorners[sceneMath::QUAD_CORNER_COUNT])
 * @brief Rotates around the element centre with the flat path's sign convention.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param frame Camera sheet axes and focus resolved for this frame.
 * @param e Sky element with viewport-relative position and size in world pixels.
 * @param outCorners Receives four corners in sceneMath::QuadCorner order.
 */
inline void MakeSkyQuad(const particleCards::Frame& frame,
                        const skyDraw::Element& e,
                        glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
{
    particleCards::MakeSpriteQuad(
        SheetCentre(frame, e.pos, e.size), e.size, frame.sheet, e.rotation, outCorners);
}

/**
 * @fn bool KeepOnSheet(const particleCards::Frame& frame, const skyDraw::Element& e)
 * @brief Culls sheet elements with a margin that preserves viewport intersections.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Absolute size prevents mirrored artwork from culling itself.
 */
inline bool KeepOnSheet(const particleCards::Frame& frame, const skyDraw::Element& e)
{
    return particleCards::InsideSheetView(
        frame, ElementCentreWorld(frame, e.pos, e.size), glm::abs(e.size));
}

/**
 * @fn void MakeLightPoolQuad(glm::vec2 centreWorld, float radius, float surfaceHeight, glm::vec3 \
 * outCorners[sceneMath::QUAD_CORNER_COUNT])
 * @brief Aligns a light pool with the ground under its lamp.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param centreWorld World pixels.
 * @param radius World pixels.
 * @param surfaceHeight Scene units.
 * @param outCorners Receives four corners in sceneMath::QuadCorner order.
 */
inline void MakeLightPoolQuad(glm::vec2 centreWorld,
                              float radius,
                              float surfaceHeight,
                              glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
{
    sceneMath::MakeGroundQuad(
        centreWorld - glm::vec2(radius), glm::vec2(radius * 2.0f), surfaceHeight, 0.0f, outCorners);
}

/**
 * @fn bool StrideKeep(std::size_t index, std::size_t count, std::size_t allowance)
 * @brief Keeps an evenly distributed subset of elements.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Keeps exactly allowance of count when allowance is smaller. This thins aurora
 * uniformly. The maximum discarded run is ceil(count / allowance) - 1.
 *
 * $ \lceil count / allowance \rceil - 1 $
 */
inline bool StrideKeep(std::size_t index, std::size_t count, std::size_t allowance)
{
    if (allowance >= count)
    {
        return true;
    }
    if (allowance == 0 || count == 0)
    {
        return false;
    }
    return ((index + 1) * allowance) / count > (index * allowance) / count;
}

}  // namespace skyCards
