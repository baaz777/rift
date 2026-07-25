#pragma once

#include "IRenderer.hpp"
#include "MathUtils.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

/**
 * @struct CameraState
 * @brief Flat camera position and follow state.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Position is the viewport top-left in world pixels, with Y down.
 * Centering a point requires subtracting half the viewport size. console goto commands
 * assign the corner directly.
 *
 * @verbatim
 *   (0,0) map origin
 *     +--------------------------------------------+
 *     |                                            |
 *     | position -> +--------------+               |
 *     |             |              |               |
 *     |             |   viewport   | worldHeight   |
 *     |             |              |               |
 *     |             +--------------+               |
 *     |                worldWidth                  |
 *     +--------------------------------------------+
 *                                                (w,h) map size
 *
 *   clamp band: [0, mapPixels - worldSize] per axis, skipped in editor free mode.
 *   Zooming out grows worldSize, so the band shrinks - and collapses to {0} once
 *   the viewport is wider than the map.
 * @endverbatim
 */
struct CameraState
{
    /**
     * @brief Viewport top-left corner in world pixels.
     *
     * Game::Render overwrites this with a pixel-snapped value for the span of the draw, then
     * restores it.
     */
    glm::vec2 position{0.0f};
    /// Top-left corner the auto-follow is easing toward; same frame as position.
    glm::vec2 followTarget{0.0f};
    /**
     * @brief True while an auto-follow ease is in flight.
     *
     * Cleared on arrival, on arrow-key panning, in free mode, and by any explicit re-center.
     */
    bool hasFollowTarget = false;
    /**
     * @brief Zoom multiplier (1.0 = 100%).
     *
     * CameraController::HandleZoomScroll holds the wheel to [0.4, 4.0]; `camera.zoom` accepts the
     * wider [0.1, 10.0] and rejects anything outside.
     */
    float zoom = 1.0f;
    bool freeMode = false;  ///< Free camera mode (decoupled from the player).
};

/**
 * @struct CameraUpdateParams
 * @brief Camera inputs for one gameplay frame.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct CameraUpdateParams
{
    float deltaTime = 0.0f;  ///< Seconds since the previous frame.
    /**
     * @brief Player center minus half the viewport size, in world pixels.
     *
     * Uses the live position while moving and the tile center while idle.
     */
    glm::vec2 playerFollowTarget{0.0f};
    bool playerMoving = false;
    glm::vec2 playerVelocity{0.0f};  ///< Player velocity (px/s) for camera look-ahead.
    /**
     * @brief Arrow-key pan input, this frame's polled state.
     *
     * opposing keys cancel the motion but still read as manual input, which freezes the camera.
     */
    bool arrowUp = false;
    bool arrowDown = false;
    bool arrowLeft = false;
    bool arrowRight = false;
    bool shiftHeld = false;  ///< Shift multiplies the pan speed by 2.5x.
    /**
     * @brief Unzoomed width from the truncated visible tile count.
     *
     * ClampToMapBounds uses this extent. If the window is not a whole number of tiles wide,
     * the clamp can expose off-map pixels.
     */
    float baseWorldWidth = 0.0f;
    /**
     * @brief Unzoomed viewport height: tilesVisibleHeight * tileHeight.
     *
     * same truncation caveat as `baseWorldWidth`.
     */
    float baseWorldHeight = 0.0f;
    float mapPixelWidth = 0.0f;
    float mapPixelHeight = 0.0f;
    bool skipMapClamping = false;
    int tileWidth = 16;   ///< Tile width in px (for the free-mode grid snap).
    int tileHeight = 16;  ///< Tile height in px (for the free-mode grid snap).
};

/**
 * @class CameraController
 * @brief Flat-camera follow, pan and zoom.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Follow and free-mode grid snapping use frame-independent exponential easing.
 * The free-mode settle time is 0.5 s.
 *
 * Leaving free mode or manual pan does not reacquire an idle player. movement or
 * camera.follow on must set a target. The world3d path reads this state as an orbit focus
 * and extent; it bypasses GetOrthoProjection. Classic OpenGL must use the same pixel snap
 * as the flat sky path.
 *
 * @verbatim
 *   lead = normalize(v) * lookAheadDistance * min(1, |v| / LOOKAHEAD_REF_SPEED)
 * @endverbatim
 */
class CameraController
{
public:
    CameraController() = default;

    /**
     * @fn void Initialize(glm::vec2 playerVisualCenter, float viewWidth, float viewHeight)
     * @brief Centers immediately; map clamping waits until Update.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clears hasFollowTarget. All positions and extents are world pixels.
     */
    void Initialize(glm::vec2 playerVisualCenter, float viewWidth, float viewHeight);

    /**
     * @fn void Update(const CameraUpdateParams& params)
     * @brief Updates movement, then clamps unless skipMapClamping is set.
     * @author Alex (<https://github.com/lextpf>)
     *
     * | priority | mode        | action                                       |
     * |----------|-------------|----------------------------------------------|
     * | 1        | free        | pan, or ease the top-left onto the tile grid |
     * | 2        | manual pan  | pan and clear hasFollowTarget                |
     * | 3        | auto-follow | ease toward the player plus look-ahead       |
     */
    void Update(const CameraUpdateParams& params);

    /**
     * @fn static glm::mat4 GetOrthoProjection(float width, float height)
     * @brief Projection with a top-left origin and downward Y.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Callers subtract camera position and supply zoom-adjusted world-pixel dimensions.
     * This function applies no camera transform.
     */
    static glm::mat4 GetOrthoProjection(float width, float height);

    /**
     * @fn void HandleZoomScroll(double yoffset, glm::vec2 playerVisualCenter, float \
     * baseWorldWidth, float baseWorldHeight, float mapPixelWidth, float mapPixelHeight, bool \
     * skipMapClamping, bool editorFreeMode)
     * @brief Zooms by scroll direction and re-centers on the supplied player point.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Positive yoffset multiplies zoom by 1.1; negative uses 0.9. clamp to
     * [editorFreeMode ? 0.1 : 0.4, 4.0], then round to 0.1. repeated wheel steps stop at 0.5.
     * hasFollowTarget is unchanged. All positions and extents are world pixels.
     *
     * Game::ScrollCallback supplies a center 8 px below auto-follow, so that offset persists
     * until the player moves. skipMapClamping permits positions beyond map bounds.
     */
    void HandleZoomScroll(double yoffset,
                          glm::vec2 playerVisualCenter,
                          float baseWorldWidth,
                          float baseWorldHeight,
                          float mapPixelWidth,
                          float mapPixelHeight,
                          bool skipMapClamping,
                          bool editorFreeMode);

    /**
     * @fn void ResetZoom(glm::vec2 playerVisualCenter, float worldWidth, float worldHeight, \
     * float mapPixelWidth, float mapPixelHeight, bool skipMapClamping)
     * @brief Resets zoom to 1 and centers immediately.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clears hasFollowTarget. All positions and extents are world pixels;
     * skipMapClamping permits positions beyond map bounds.
     */
    void ResetZoom(glm::vec2 playerVisualCenter,
                   float worldWidth,
                   float worldHeight,
                   float mapPixelWidth,
                   float mapPixelHeight,
                   bool skipMapClamping);

    CameraState& GetState() { return m_State; }
    const CameraState& GetState() const { return m_State; }
    const glm::vec2& GetPosition() const { return m_State.position; }
    float GetZoom() const { return m_State.zoom; }
    bool IsFreeMode() const { return m_State.freeMode; }
    /**
     * @fn void SetLookAheadDistance(float d)
     * @brief Set the camera look-ahead distance in world pixels; 0 disables it.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetLookAheadDistance(float d) { m_LookAheadDistance = d; }

    float GetLookAheadDistance() const { return m_LookAheadDistance; }

private:
    CameraState m_State;
    float m_LookAheadDistance = 12.0f;  ///< Camera lead in the direction of travel (px).

    static constexpr float CAMERA_PAN_SPEED = 600.0f;
    /// Auto-follow settle time: after this long, 1% of the distance is left.
    static constexpr float CAMERA_SETTLE_TIME = 0.6f;
    /// World-pixel distance below which an ease snaps to its target, killing jitter.
    static constexpr float CAMERA_SNAP_THRESHOLD = 0.1f;
    /**
     * @brief Speed in px/s at which look-ahead reaches its full distance.
     *
     * Keep equal to PLAYER_BASE_SPEED * RUN_SPEED_MULTIPLIER; no check enforces this.
     */
    static constexpr float LOOKAHEAD_REF_SPEED = 87.5f;

    /**
     * @fn void ClampToMapBounds(float worldWidth, float worldHeight, float mapW, float mapH)
     * @brief Clamps to the valid camera interval.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clamp position into [0, mapSize - worldSize] per axis; a negative upper
     * bound (viewport wider than the map) collapses the band to 0.
     */
    void ClampToMapBounds(float worldWidth, float worldHeight, float mapW, float mapH);
};
