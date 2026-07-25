#include "CameraController.hpp"

#include "Logger.hpp"

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Camera";
}  // namespace

void CameraController::Initialize(glm::vec2 playerVisualCenter, float viewWidth, float viewHeight)
{
    m_State.position = playerVisualCenter - glm::vec2(viewWidth / 2.0f, viewHeight / 2.0f);
    m_State.followTarget = m_State.position;
    m_State.hasFollowTarget = false;
}

void CameraController::Update(const CameraUpdateParams& params)
{
    float worldWidth = params.baseWorldWidth / m_State.zoom;
    float worldHeight = params.baseWorldHeight / m_State.zoom;
    bool arrowKeysPressed =
        params.arrowUp || params.arrowDown || params.arrowLeft || params.arrowRight;

    if (m_State.freeMode)
    {
        if (arrowKeysPressed)
        {
            float cameraSpeed = CAMERA_PAN_SPEED / m_State.zoom;

            if (params.shiftHeld)
            {
                cameraSpeed *= 2.5f;
            }

            glm::vec2 cameraMove(0.0f);

            if (params.arrowUp)
                cameraMove.y -= cameraSpeed * params.deltaTime;
            if (params.arrowDown)
                cameraMove.y += cameraSpeed * params.deltaTime;
            if (params.arrowLeft)
                cameraMove.x -= cameraSpeed * params.deltaTime;
            if (params.arrowRight)
                cameraMove.x += cameraSpeed * params.deltaTime;

            m_State.position += cameraMove;
        }
        else
        {
            // Ease onto whole tiles after manual panning.
            float tileW = static_cast<float>(params.tileWidth);
            float tileH = static_cast<float>(params.tileHeight);
            glm::vec2 snappedPos;
            snappedPos.x = std::round(m_State.position.x / tileW) * tileW;
            snappedPos.y = std::round(m_State.position.y / tileH) * tileH;

            // Free-mode grid correction settles in 0.5 s.
            float alpha = rift::ExpApproachAlpha(params.deltaTime, 0.5f);
            glm::vec2 newPos = m_State.position + (snappedPos - m_State.position) * alpha;

            if (glm::length(snappedPos - newPos) < CAMERA_SNAP_THRESHOLD)
            {
                m_State.position = snappedPos;
            }
            else
            {
                m_State.position = newPos;
            }
        }
        m_State.hasFollowTarget = false;
    }
    else if (arrowKeysPressed)
    {
        float cameraSpeed = CAMERA_PAN_SPEED / m_State.zoom;

        if (params.shiftHeld)
        {
            cameraSpeed *= 2.5f;
        }

        glm::vec2 cameraMove(0.0f);

        if (params.arrowUp)
        {
            cameraMove.y -= cameraSpeed * params.deltaTime;
        }
        if (params.arrowDown)
        {
            cameraMove.y += cameraSpeed * params.deltaTime;
        }
        if (params.arrowLeft)
        {
            cameraMove.x -= cameraSpeed * params.deltaTime;
        }
        if (params.arrowRight)
        {
            cameraMove.x += cameraSpeed * params.deltaTime;
        }

        m_State.position += cameraMove;

        m_State.hasFollowTarget = false;
    }
    else
    {
        // No manual camera input.
        // If player is moving with WASD, establish a follow target.
        if (params.playerMoving || m_State.hasFollowTarget)
        {
            // Scale look-ahead with speed.
            glm::vec2 lead(0.0f);
            float vlen = glm::length(params.playerVelocity);
            if (vlen > 1e-3f && m_LookAheadDistance > 0.0f)
            {
                float mag = std::min(1.0f, vlen / LOOKAHEAD_REF_SPEED);
                lead = (params.playerVelocity / vlen) * (m_LookAheadDistance * mag);
            }
            m_State.followTarget = params.playerFollowTarget + lead;
            m_State.hasFollowTarget = true;
        }

        if (m_State.hasFollowTarget)
        {
            float alpha = rift::ExpApproachAlpha(params.deltaTime, CAMERA_SETTLE_TIME);

            glm::vec2 newPos = m_State.position + (m_State.followTarget - m_State.position) * alpha;

            if (glm::length(m_State.followTarget - newPos) < CAMERA_SNAP_THRESHOLD)
            {
                m_State.position = m_State.followTarget;
                m_State.hasFollowTarget = false;
            }
            else
            {
                m_State.position = newPos;
            }
        }
    }

    // Clamp camera to map bounds (skip in editor free-camera mode)
    if (!params.skipMapClamping)
    {
        ClampToMapBounds(worldWidth, worldHeight, params.mapPixelWidth, params.mapPixelHeight);
    }
}

glm::mat4 CameraController::GetOrthoProjection(float width, float height)
{
    return glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
}

void CameraController::HandleZoomScroll(double yoffset,
                                        glm::vec2 playerVisualCenter,
                                        float baseWorldWidth,
                                        float baseWorldHeight,
                                        float mapPixelWidth,
                                        float mapPixelHeight,
                                        bool skipMapClamping,
                                        bool editorFreeMode)
{
    float oldZoom = m_State.zoom;

    float zoomDelta = yoffset > 0 ? 1.1f : 0.9f;
    m_State.zoom *= zoomDelta;

    float minZoom = editorFreeMode ? 0.1f : 0.4f;
    m_State.zoom = std::max(minZoom, std::min(4.0f, m_State.zoom));
    // Round each step to prevent multiplicative in/out zoom drift.
    m_State.zoom = std::round(m_State.zoom * 10.0f) / 10.0f;

    float newWorldWidth = baseWorldWidth / m_State.zoom;
    float newWorldHeight = baseWorldHeight / m_State.zoom;

    m_State.position = playerVisualCenter - glm::vec2(newWorldWidth * 0.5f, newWorldHeight * 0.5f);

    if (!skipMapClamping)
    {
        ClampToMapBounds(newWorldWidth, newWorldHeight, mapPixelWidth, mapPixelHeight);
    }

    // Also update the follow target so camera doesn't snap back
    m_State.followTarget = m_State.position;

    Logger::InfoF(LOG_SUBSYSTEM, "Camera zoom: {}x", m_State.zoom);
}

void CameraController::ResetZoom(glm::vec2 playerVisualCenter,
                                 float worldWidth,
                                 float worldHeight,
                                 float mapPixelWidth,
                                 float mapPixelHeight,
                                 bool skipMapClamping)
{
    m_State.zoom = 1.0f;

    m_State.position = playerVisualCenter - glm::vec2(worldWidth / 2.0f, worldHeight / 2.0f);

    if (!skipMapClamping)
    {
        ClampToMapBounds(worldWidth, worldHeight, mapPixelWidth, mapPixelHeight);
    }

    m_State.hasFollowTarget = false;
    Logger::Info(LOG_SUBSYSTEM, "Camera zoom reset to 1.0x");
}

void CameraController::ClampToMapBounds(float worldWidth, float worldHeight, float mapW, float mapH)
{
    m_State.position.x = std::max(0.0f, std::min(m_State.position.x, mapW - worldWidth));
    m_State.position.y = std::max(0.0f, std::min(m_State.position.y, mapH - worldHeight));
}
