#pragma once

#include "Billboard.hpp"
#include "CameraRig.hpp"
#include "Frustum.hpp"
#include "SceneMath.hpp"
#include "TileRole.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

/**
 * @brief Place flat particle artwork on scene-space cards.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Full yaw and lean damping keeps cards perpendicular to the view. leanFollow must
 * remain 1 to avoid foreshortening at steep pitch. The focus sheet has constant camera
 * depth, so world-pixel offsets retain flat-screen scale and positive Y stays screen-down.
 *
 * FACADE_DEPTH_BIAS shifts decals toward the eye by 1 world pixel. The facade leans
 * 10.27 degrees less than the sheet, giving cos(10.27 degrees) = 0.984 px wall clearance.
 *
 * @verbatim
 *  side view, yaw 0, DS pitch 51.34 deg           eye
 *                                                  \
 *    scene +Y (up)                                  \  view ray
 *      |                sheet, lean = pitch          \
 *      |   dy = -90 --> C = F + 90*up                 \
 *      |                     \                         \
 *      |                      \                         \
 *      |                       F  focus, depth D = 683.6 \
 *      |  ground ---------------\-------------------------\--- scene +Z (south)
 *      |                         \
 *      |   dy = +90 --> C = F - 90*up   (below ground; drawn with DepthMode::None)
 *
 *  a structure facade leans 0.8 * pitch = 41.07 deg, 10.27 deg less than the sheet
 * @endverbatim
 *
 * @verbatim
 *  worked example: preset DS, visible 320x180, screen 1520x855
 *
 *    sp = sin 51.34 deg = 0.7808668     cp = cos 51.34 deg = 0.6246973
 *    tan 7.5 deg = 0.1316525            D  = 90 / tan 7.5 deg = 683.6179
 *
 *    a particle at F + (30, -50) lands at F + (30, +31.2349, -39.0433), at
 *    depth exactly D, on pixel (902.5, 190.0) - the flat frame's pixel.
 *
 *    rain velocity (0, +200) becomes dC/dt = (0, -124.9, +156.2): the card
 *    point loses height and advances toward the camera, and on screen it
 *    still moves straight down at 200 px per second.
 * @endverbatim
 *
 * | Card        | anchor A                 | centre                         |
 * |-------------|--------------------------|--------------------------------|
 * | sheet       | the rig focus            | CardPoint(A, focusHeight, P)   |
 * | ground prop | the particle itself      | CardPoint(P, surfaceHeight, P) |
 * | zone card   | ClampToRect(focus, zone) | CardPoint(A, surfaceHeight, P) |
 * | facade      | the body's runFoot       | FacadePoint(...)               |
 */
namespace particleCards
{

/// Full yaw and lean keep the card perpendicular to the view ray.
inline constexpr billboard::Damping SHEET_DAMPING{1.0f, 1.0f};

/// Eye-ward offset of a facade decal from its wall, in world pixels.
inline constexpr float FACADE_DEPTH_BIAS = 1.0f;

/// Constant term of the sheet cull pad, in world pixels.
inline constexpr float SHEET_CULL_PAD = 50.0f;

/// Cache orientation and frustum calculations once per frame.
struct Frame
{
    billboard::Orientation sheet;
    billboard::Orientation wall;   ///< Facade axes for a body wider than one tile.
    billboard::Orientation pivot;  ///< Facade axes for a one-tile-wide body.
    glm::vec2 focusWorld{0.0f};    ///< Sheet anchor: the rig's ground focus, world pixels.
    float focusHeight = 0.0f;      ///< Scene height of the ground under the focus.
    glm::vec2 halfVisible{0.0f};   ///< Half the rig's visible world extent, world pixels.
    glm::vec3 towardEye{0.0f};     ///< Unit vector from the focus toward the eye.
    frustum::Frustum view;
};

inline Frame MakeFrame(const cameraRig::RigParams& rig)
{
    Frame frame;
    frame.sheet = billboard::Orient(rig.yawRadians, rig.pitchRadians, SHEET_DAMPING);
    frame.wall = billboard::Orient(rig.yawRadians, rig.pitchRadians, tileRole::DampingForWidth(2));
    frame.pivot = billboard::Orient(rig.yawRadians, rig.pitchRadians, tileRole::DampingForWidth(1));
    frame.focusWorld = rig.target;
    frame.focusHeight = rig.focusHeight;
    frame.halfVisible = rig.visibleWorldSize * 0.5f;
    frame.towardEye = cameraRig::EyeDirection(rig.yawRadians, rig.pitchRadians);
    // Use the rig matrix; Vulkan clip-corrects its renderer copy, which would tilt cull planes.
    frame.view = frustum::FromViewProjection(cameraRig::BuildViewProjection(rig));
    return frame;
}

/**
 * @fn glm::vec3 CardPoint(const Frame& frame, glm::vec2 anchorWorld, float anchorHeight, \
 * glm::vec2 world)
 * @brief Offset world pixels from anchorWorld along card right and negative up; anchorHeight is
 * scene height.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 CardPoint(const Frame& frame,
                           glm::vec2 anchorWorld,
                           float anchorHeight,
                           glm::vec2 world)
{
    const glm::vec2 offset = world - anchorWorld;
    return sceneMath::ToScene(anchorWorld, anchorHeight) + frame.sheet.right * offset.x -
           frame.sheet.up * offset.y;
}

/**
 * @fn glm::vec2 ClampToRect(glm::vec2 point, glm::vec2 rectPos, glm::vec2 rectSize)
 * @brief Negative extents collapse to a point.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec2 ClampToRect(glm::vec2 point, glm::vec2 rectPos, glm::vec2 rectSize)
{
    const glm::vec2 hi = rectPos + glm::max(rectSize, glm::vec2(0.0f));
    return {std::clamp(point.x, rectPos.x, hi.x), std::clamp(point.y, rectPos.y, hi.y)};
}

/**
 * @fn bool InsideSheetView(const Frame& frame, glm::vec2 world, glm::vec2 size)
 * @brief Flat-view cull padded by twice the largest raw sprite dimension plus SHEET_CULL_PAD;
 * Units are pixels.
 * @author Alex (<https://github.com/lextpf>)
 */
inline bool InsideSheetView(const Frame& frame, glm::vec2 world, glm::vec2 size)
{
    const float padding = std::max(size.x, size.y) * 2.0f + SHEET_CULL_PAD;
    const glm::vec2 offset = world - frame.focusWorld;
    return std::abs(offset.x) <= frame.halfVisible.x + padding &&
           std::abs(offset.y) <= frame.halfVisible.y + padding;
}

/**
 * @fn const billboard::Orientation& FacadeAxes(const Frame& frame, int widthTiles)
 * @brief Multi-tile bodies hold the grid; single-tile bodies rotate with the view.
 * @author Alex (<https://github.com/lextpf>)
 */
inline const billboard::Orientation& FacadeAxes(const Frame& frame, int widthTiles)
{
    return tileRole::IsGridLocked(TileStance::Structure, widthTiles) ? frame.wall : frame.pivot;
}

/**
 * @fn glm::vec3 FacadePoint(const Frame& frame, const billboard::Orientation& axes, glm::vec3 \
 * foot, float runCentreX, float baseSouthEdgeY, glm::vec2 world)
 * @brief Measure from runCentreX/baseSouthEdgeY in world pixels; offset the facade point toward
 * the eye.
 * @author Alex (<https://github.com/lextpf>)
 */
inline glm::vec3 FacadePoint(const Frame& frame,
                             const billboard::Orientation& axes,
                             glm::vec3 foot,
                             float runCentreX,
                             float baseSouthEdgeY,
                             glm::vec2 world)
{
    return foot + axes.right * (world.x - runCentreX) + axes.up * (baseSouthEdgeY - world.y) +
           frame.towardEye * FACADE_DEPTH_BIAS;
}

/**
 * @fn void MakeSpriteQuad(glm::vec3 centre, glm::vec2 size, const billboard::Orientation& o, \
 * float rotationDegrees, glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
 * @brief Build a center-anchored quad in sceneMath::QuadCorner order.
 * @author Alex (<https://github.com/lextpf>)
 *
 * `size` is in world pixels; retain negative width, which mirrors Snow artwork.
 * rotationDegrees turns within the card plane using negative up for its downward axis.
 */
inline void MakeSpriteQuad(glm::vec3 centre,
                           glm::vec2 size,
                           const billboard::Orientation& o,
                           float rotationDegrees,
                           glm::vec3 outCorners[sceneMath::QUAD_CORNER_COUNT])
{
    sceneMath::MakeOrientedQuad(centre, size, o.right, -o.up, rotationDegrees, outCorners);
}

}  // namespace particleCards
