#pragma once

#include "EnumTraits.hpp"
#include "MathConstants.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <iterator>
#include <optional>
#include <string_view>

/**
 * @brief Orbit-camera matrices, unprojection and ground queries.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Game::BuildCameraRig converts the flat viewport corner to a ground focus each frame.
 * Scene +X is east, +Z is south, and +Y is up. yaw is measured around +Y;
 * Pitch is above the horizon. eye offsets are distance * cos(pitch) horizontally and
 * distance * sin(pitch) vertically.
 *
 * Orthographic yaw 0 and pitch pi/2 reproduce the flat view. ScreenToGround inverts the
 * render transform. Picking uses framebuffer pixels with a top-left origin; convert window
 * coordinates to that scale before calling when the framebuffer size differs.
 * The editor still uses flat picking and is incorrect at nonzero yaw.
 *
 * ```mermaid
 * flowchart TD
 *     G["Game: m_CameraPreset, m_CameraYaw, m_CameraPitch"]
 *     C["CameraController: position, zoom"]
 *     B["Game::BuildCameraRig"]
 *     R["RigParams<br/>rebuilt every frame, never stored"]
 *     G --> B
 *     C --> B
 *     B --> R
 *     R --> VP["BuildViewProjection"]
 *     VP --> SVP["IRenderer::SetViewProjection"]
 *     VP --> FF["frustum::FromViewProjection"]
 *     FF --> IS["per-tile IntersectsSphere"]
 *     R --> GF["GroundFootprintAabb"]
 *     GF --> TR["Tilemap::ComputeTileRange"]
 *     R --> OR["billboard::Orient"]
 *     OR --> DR["character and upright-tile quads"]
 * ```
 *
 * @verbatim
 *                       yaw = pi
 *                    north (map -Y)
 *                          |
 *                          |
 *   yaw = -pi/2  ----------+----------  yaw = +pi/2
 *   west (map -X)          |            east (map +X)
 *                          |
 *                    south (map +Y)
 *                       yaw = 0
 * @endverbatim
 */
namespace cameraRig
{

/// Whether the projection converges (perspective) or not (orthographic).
enum class ProjectionKind
{
    Orthographic = 0,
    Perspective = 1
};

/// Named camera configurations.
enum class Preset
{
    /// Flat top-down orthographic view.
    Classic = 0,
    /// Long-distance perspective keeps tile distortion small.
    DS = 1,
    /// User-driven orbit; angles come from mouse drag rather than the preset.
    Free = 2
};

inline constexpr std::size_t PROJECTION_KIND_COUNT = 2;

inline constexpr std::size_t PRESET_COUNT = 3;

/// Minimum pitch in radians; avoids near-horizontal ground rays.
inline constexpr float MIN_PITCH_RADIANS = 10.0f * rift::PiF / 180.0f;
/// Straight overhead. also the Preset::Classic value.
inline constexpr float MAX_PITCH_RADIANS = rift::PiF * 0.5f;
/// Elevation of the real DS overworld camera: atan(35 / 28).
inline constexpr float DS_PITCH_RADIANS = 51.34f * rift::PiF / 180.0f;
/// Vertical FOV of the real DS overworld camera.
inline constexpr float DS_FOV_RADIANS = 15.0f * rift::PiF / 180.0f;
/// Editor FOV in radians; ApplyPreset does not select this value.
inline constexpr float EDITOR_FOV_RADIANS = 30.0f * rift::PiF / 180.0f;

/**
 * @struct RigParams
 * @brief Everything needed to build a view and a projection.
 * @author Alex (<https://github.com/lextpf>)
 */
struct RigParams
{
    /// Ground focus point in world pixels (look-at point).
    glm::vec2 target{0.0f};
    float focusHeight = 0.0f;
    float yawRadians = 0.0f;  ///< 0 places the eye due map-south of the focus.
    /// Elevation above the horizon; pi/2 is straight down. see ClampPitch.
    float pitchRadians = MAX_PITCH_RADIANS;
    /// Vertical FOV in radians; also controls orbit distance under orthographic projection.
    float fovYRadians = DS_FOV_RADIANS;
    ProjectionKind kind = ProjectionKind::Orthographic;
    /**
     * @brief Zoom-adjusted visible extent in world pixels.
     *
     * Sets orthographic extents and the perspective distance at the focus.
     */
    glm::vec2 visibleWorldSize{320.0f, 180.0f};
    /**
     * @brief Half-extent of scene content to keep inside the depth range, in world pixels.
     *
     * normally about the map diagonal.
     */
    float sceneRadius = 4096.0f;
};

/**
 * @struct Basis
 * @brief Derived camera frame in scene space.
 * @author Alex (<https://github.com/lextpf>)
 */
struct Basis
{
    glm::vec3 eye{0.0f};
    glm::vec3 focus{0.0f};
    glm::vec3 forward{0.0f};
    glm::vec3 right{0.0f};
    glm::vec3 up{0.0f};
    float distance = 0.0f;
};

/// A scene-space ray, used for picking.
struct Ray
{
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  ///< Normalized.
};

/**
 * @struct GroundBounds
 * @brief Axis-aligned world-pixel bounds of the camera's ground footprint.
 * @author Alex (<https://github.com/lextpf>)
 */
struct GroundBounds
{
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};
    /**
     * @brief Whether min / max are the true footprint or a stand-in.
     *
     * The function must return a finite box, but with the horizon on screen the
     * visible ground runs to infinity, so corner rays that miss the
     * plane are cut off.
     */
    bool complete = true;
};

/// Orbit angles a drag produces.
struct OrbitAngles
{
    float yawRadians = 0.0f;
    float pitchRadians = MAX_PITCH_RADIANS;
};

/// Rotation in radians for a full-window drag; 100 degrees per axis.
inline constexpr float DRAG_SWEEP_RADIANS = 100.0f * rift::PiF / 180.0f;

/**
 * @fn OrbitAngles ApplyOrbitDrag(OrbitAngles current, glm::vec2 dragPixels, glm::vec2 \
 * viewportSize)
 * @brief Dragging right moves the eye west; dragging down raises it.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Wraps yaw and clamps pitch. dragPixels and viewportSize are framebuffer pixels, with Y down.
 */
OrbitAngles ApplyOrbitDrag(OrbitAngles current, glm::vec2 dragPixels, glm::vec2 viewportSize);

/**
 * @fn float ClampPitch(float pitchRadians)
 * @brief Clamps pitch to MIN_PITCH_RADIANS through MAX_PITCH_RADIANS, inclusive.
 * @author Alex (<https://github.com/lextpf>)
 */
float ClampPitch(float pitchRadians);

/**
 * @fn float WrapYaw(float yawRadians)
 * @brief Wraps radians above -pi through +pi, inclusive at +pi.
 * @author Alex (<https://github.com/lextpf>)
 */
float WrapYaw(float yawRadians);

/**
 * @fn float DistanceForVisibleHeight(float visibleWorldHeight, float fovYRadians)
 * @brief Distance that frames the visible height at the focus.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Height and distance are world pixels; fovYRadians is radians.
 *
 * $$
 * d = (h / 2) / \tan(fov_y / 2)
 * $$
 */
float DistanceForVisibleHeight(float visibleWorldHeight, float fovYRadians);

/**
 * @fn glm::vec3 EyeDirection(float yawRadians, float pitchRadians)
 * @brief Unit vector from the focus toward the eye.
 * @author Alex (<https://github.com/lextpf>)
 */
glm::vec3 EyeDirection(float yawRadians, float pitchRadians);

/**
 * @fn glm::vec3 UpVector(float yawRadians, float pitchRadians)
 * @brief Analytical up vector, defined even at pitch pi/2.
 * @author Alex (<https://github.com/lextpf>)
 */
glm::vec3 UpVector(float yawRadians, float pitchRadians);

/**
 * @fn Basis MakeBasis(const RigParams& params)
 * @brief Resolve the full camera frame.
 * @author Alex (<https://github.com/lextpf>)
 */
Basis MakeBasis(const RigParams& params);

/**
 * @fn void DepthRange(const RigParams& params, float& outNear, float& outFar)
 * @brief Depth range in scene units.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Orthographic projection uses a symmetric slab around the orbit distance and can have
 * a negative near plane. perspective uses max(0.5, 0.05 * distance) through
 * distance + 2 * sceneRadius. Both derive distance from visibleWorldSize.y and fovYRadians.
 */
void DepthRange(const RigParams& params, float& outNear, float& outFar);

/**
 * @fn glm::mat4 BuildView(const RigParams& params)
 * @brief View matrix (scene space -> camera space).
 * @author Alex (<https://github.com/lextpf>)
 */
glm::mat4 BuildView(const RigParams& params);

/**
 * @fn glm::mat4 BuildProjection(const RigParams& params)
 * @brief Projection matrix, orthographic or perspective per params.kind.
 * @author Alex (<https://github.com/lextpf>)
 */
glm::mat4 BuildProjection(const RigParams& params);

/**
 * @fn glm::mat4 BuildViewProjection(const RigParams& params)
 * @brief Combined projection * view.
 * @author Alex (<https://github.com/lextpf>)
 */
glm::mat4 BuildViewProjection(const RigParams& params);

/**
 * @fn Ray ScreenToRay(glm::vec2 pixel, glm::vec2 viewportSize, const glm::mat4& invViewProj)
 * @brief Unprojects a framebuffer pixel to a normalized scene-space ray.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Pixel Y increases downward. Supply the inverse of the rig's projection * view matrix,
 * before backend clip correction. Each viewport dimension is clamped to at least one pixel.
 * The ray starts on the near plane; its direction points toward the far plane.
 *
 * A near-zero homogeneous divisor or near-to-far distance returns the default downward ray
 * at the origin. This can produce a valid-looking ground hit at world (0, 0).
 * Singular matrices are not detected reliably; callers must supply a valid inverse.
 */
Ray ScreenToRay(glm::vec2 pixel, glm::vec2 viewportSize, const glm::mat4& invViewProj);

/**
 * @fn std::optional<glm::vec2> IntersectGroundPlane(const Ray& ray, float planeHeight)
 * @brief Intersect a ray with the horizontal plane at planeHeight.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param ray Ray in scene units; the intersection must lie at or ahead of its origin.
 * @param planeHeight Height in scene units, independent of the terrain elevation map.
 * @return World-pixel hit position, or `nullopt` when the ray is parallel to the plane
 *         or the intersection lies behind the ray origin.
 */
std::optional<glm::vec2> IntersectGroundPlane(const Ray& ray, float planeHeight);

/**
 * @fn std::optional<glm::vec2> ScreenToGround(glm::vec2 pixel, glm::vec2 viewportSize, const \
 * glm::mat4& invViewProj, float planeHeight = 0.0f)
 * @brief Returns no position when the cursor ray misses the ground.
 * @author Alex (<https://github.com/lextpf>)
 */
std::optional<glm::vec2> ScreenToGround(glm::vec2 pixel,
                                        glm::vec2 viewportSize,
                                        const glm::mat4& invViewProj,
                                        float planeHeight = 0.0f);

/**
 * @fn std::optional<glm::vec2> WorldToScreen(glm::vec3 scenePoint, const glm::mat4& viewProj, \
 * glm::vec2 viewportSize)
 * @brief Project a scene point to framebuffer pixels.
 * @author Alex (<https://github.com/lextpf>)
 *
 * This does not test the viewport or near/far planes. Points outside the visible frame can return
 * coordinates outside the viewport.
 *
 * @return Framebuffer pixels with Y measured down, or `nullopt` for nonpositive or near-zero
 *         clip W. For perspective projection, this rejects points at or behind the eye.
 */
std::optional<glm::vec2> WorldToScreen(glm::vec3 scenePoint,
                                       const glm::mat4& viewProj,
                                       glm::vec2 viewportSize);

/**
 * @fn GroundBounds GroundFootprintAabb(const RigParams& params, float planeHeight = 0.0f)
 * @brief Bound the ground footprint of the four viewport corner rays.
 * @author Alex (<https://github.com/lextpf>)
 *
 * The bounds always include the focus point. A ray that misses the plane uses a finite sample
 * at four times the scene radius and clears `complete`; this bounds the estimate near the horizon.
 * The result is in world pixels and is not clamped to the map.
 */
GroundBounds GroundFootprintAabb(const RigParams& params, float planeHeight = 0.0f);

/**
 * @fn void ApplyPreset(RigParams& params, Preset preset)
 * @brief Applies preset angles, projection and FOV while retaining framing.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Classic and DS overwrite fovYRadians with DS_FOV_RADIANS. Free retains FOV and angles,
 * only wrapping yaw and clamping pitch.
 */
void ApplyPreset(RigParams& params, Preset preset);

/**
 * @fn glm::vec2 ClampFocusToMap(glm::vec2 focus, glm::vec2 mapPixelSize)
 * @brief Clamps the focus point to map bounds in world pixels.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Game::BuildCameraRig instead derives the focus from the clamped flat camera.
 */
glm::vec2 ClampFocusToMap(glm::vec2 focus, glm::vec2 mapPixelSize);

}  // namespace cameraRig

template <>
struct EnumTraits<cameraRig::ProjectionKind>
    : EnumTraitsBase<cameraRig::ProjectionKind, EnumTraits<cameraRig::ProjectionKind>>
{
    static constexpr std::size_t Count = cameraRig::PROJECTION_KIND_COUNT;
    static constexpr std::string_view Names[] = {"Orthographic", "Perspective"};
};

template <>
struct EnumTraits<cameraRig::Preset>
    : EnumTraitsBase<cameraRig::Preset, EnumTraits<cameraRig::Preset>>
{
    static constexpr std::size_t Count = cameraRig::PRESET_COUNT;
    static constexpr std::string_view Names[] = {"Classic", "DS", "Free"};
};

static_assert(std::size(EnumTraits<cameraRig::ProjectionKind>::Names) ==
                  cameraRig::PROJECTION_KIND_COUNT,
              "cameraRig::ProjectionKind names must stay in step with PROJECTION_KIND_COUNT");
static_assert(std::size(EnumTraits<cameraRig::Preset>::Names) == cameraRig::PRESET_COUNT,
              "cameraRig::Preset names must stay in step with PRESET_COUNT");
