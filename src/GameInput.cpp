#include "AnimationState.hpp"
#include "Appearance.hpp"
#include "CharacterConstants.hpp"
#include "CharacterKinematics.hpp"
#include "CollisionGeometry.hpp"
#include "Dialogue.hpp"
#include "DialogueStore.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "Game.hpp"
#include "Identity.hpp"
#include "KeyToggle.hpp"
#include "Logger.hpp"
#include "PlayerModes.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerSystem.hpp"
#include "TileMath.hpp"
#include "Transform.hpp"
#include "WorldServices.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Game";

// NPC interaction range in world pixels (2 tiles at 16px).
constexpr float INTERACTION_RANGE = 32.0f;
// below this world-pixel distance, allow DIRECTION_LENIENCY in the facing test.
constexpr float COLLISION_DISTANCE = 20.0f;
// pixels of directional slack allowed by that relaxed facing test.
constexpr float DIRECTION_LENIENCY = 8.0f;
}  // namespace

void Game::ProcessInput(float deltaTime)
{
    // check F12 before input capture so it also closes the console.
    if (m_KeyConsole.JustPressed(m_Window))
    {
        m_Console.Toggle();
    }
    if (m_Console.IsOpen())
    {
        // Clear motor state during input capture so frozen movement does not keep animating.
        if (m_GameMode == GameMode::Playing)
        {
            PlayerSystem::Stop(m_World, m_PlayerEntity);
        }
        PumpConsoleKeys();
        return;
    }

    // Title and pause consume all remaining input before editor and movement handling.
    if (m_GameMode == GameMode::Title)
    {
        ProcessTitleInput();
        return;
    }
    if (m_GameMode == GameMode::Paused)
    {
        ProcessPauseInput();
        return;
    }

    // always advance the esc latch; dialogue consumes its press before pause handling.
    {
        bool inAnyDialogue =
            m_DialogueUi.inDialogue || m_DialogueManager.IsActive() || m_DialogueUi.snap.active;
        bool escJustPressed = m_KeyEscape.JustPressed(m_Window);
        if (!inAnyDialogue && escJustPressed)
        {
            m_GameMode = GameMode::Paused;
            m_PauseMenu.enabled.assign(2, true);
            m_PauseMenu.selected = 0;
            // ignore stale hover and held clicks on the first pause frame.
            m_MenuLastMouseX = -1.0;
            m_MenuLastMouseY = -1.0;
            m_MenuMouseLeftPrev = true;
            return;
        }
    }

    // orbit only in 3D gameplay; flat rendering does not use these angles.
    if (m_World3DEnabled && !m_Editor.IsActive())
    {
        const bool orbiting = (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(m_Window, &cursorX, &cursorY);

        if (orbiting)
        {
            if (m_CameraDragActive)
            {
                const glm::vec2 dragPixels(static_cast<float>(cursorX - m_CameraDragCursor.x),
                                           static_cast<float>(cursorY - m_CameraDragCursor.y));

                cameraRig::OrbitAngles angles;
                angles.yawRadians = m_CameraYaw;
                angles.pitchRadians = m_CameraPitch;
                angles = cameraRig::ApplyOrbitDrag(angles,
                                                   dragPixels,
                                                   glm::vec2(static_cast<float>(m_ScreenWidth),
                                                             static_cast<float>(m_ScreenHeight)));

                // The drag helper already wraps yaw and clamps pitch; assign both without changing
                // preset semantics.
                m_CameraYaw = angles.yawRadians;
                m_CameraPitch = angles.pitchRadians;
                // dragging selects Free because fixed presets override angles.
                m_CameraPreset = cameraRig::Preset::Free;
            }
            m_CameraDragActive = true;
            m_CameraDragCursor = {cursorX, cursorY};
        }
        else
        {
            m_CameraDragActive = false;
        }
    }

    glm::vec2 moveDirection(0.0f);

    bool isRunning = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (isRunning && m_World.get<Appearance>(m_PlayerEntity).usingCopiedAppearance)
    {
        PlayerSystem::RestoreOriginalAppearance(m_World, m_PlayerEntity);
        PlayerSystem::UploadTextures(m_World, m_PlayerEntity, *m_Renderer);
    }

    m_World.get<PlayerModes>(m_PlayerEntity).isRunning = isRunning;

    // WASD follows world axes; positive Y points down.
    if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
    {
        moveDirection.y -= 1.0f;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
    {
        moveDirection.x -= 1.0f;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
    {
        moveDirection.y += 1.0f;
    }
    if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
    {
        moveDirection.x += 1.0f;
    }

    if (m_Editor.IsActive())
    {
        m_Editor.ProcessInput(deltaTime, MakeEditorContext());
    }

    // Reset view zoom and recenter on the player; editor mode also resets tile-picker navigation.
    if (m_KeyZ.JustPressed(m_Window))
    {
        if (!m_Editor.IsActive())
        {
            float worldWidth = static_cast<float>(m_TilesVisibleWidth * TILE_PIXEL_SIZE);
            float worldHeight = static_cast<float>(m_TilesVisibleHeight * TILE_PIXEL_SIZE);

            glm::vec2 playerAnchorTileCenter = PlayerMovementSystem::CurrentTileCenter(
                m_World.get<Transform>(m_PlayerEntity).position, 16.0f);
            glm::vec2 playerVisualCenter =
                glm::vec2(playerAnchorTileCenter.x, playerAnchorTileCenter.y - TILE_PIXEL_SIZE);

            float mapWidth = static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth());
            float mapHeight =
                static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight());

            m_Camera.ResetZoom(playerVisualCenter,
                               worldWidth,
                               worldHeight,
                               mapWidth,
                               mapHeight,
                               m_Editor.IsActive() && m_Camera.IsFreeMode());
        }
        else
        {
            m_Camera.GetState().zoom = 1.0f;
            Logger::Info(LOG_SUBSYSTEM, "Camera zoom reset to 1.0x");
        }

        if (m_Editor.IsActive())
        {
            m_Editor.ResetTilePickerState();
        }
    }

    // free camera stops following the player; movement input can still move the player while
    // panning.
    if (!m_DialogueUi.inDialogue && !m_DialogueManager.IsActive() && !m_DialogueUi.snap.active &&
        !m_Editor.IsActive())
    {
        if (m_KeySpaceFreeCamera.JustPressed(m_Window))
        {
            m_Camera.GetState().freeMode = !m_Camera.GetState().freeMode;
            Logger::InfoF(
                LOG_SUBSYSTEM, "Free Camera Mode: {}", m_Camera.GetState().freeMode ? "ON" : "OFF");
        }
    }

    // bicycle changes speed and sheets but retains the same collision hitbox.
    if (!m_Editor.IsActive() && m_KeyB.JustPressed(m_Window))
    {
        bool currentBicycling = m_World.get<PlayerModes>(m_PlayerEntity).isBicycling;
        bool newBicycling = !currentBicycling;

        if (newBicycling && m_World.get<Appearance>(m_PlayerEntity).usingCopiedAppearance)
        {
            PlayerSystem::RestoreOriginalAppearance(m_World, m_PlayerEntity);
            PlayerSystem::UploadTextures(m_World, m_PlayerEntity, *m_Renderer);
        }

        m_World.get<PlayerModes>(m_PlayerEntity).isBicycling = newBicycling;
        Logger::InfoF(LOG_SUBSYSTEM, "Bicycle: {}", newBicycling ? "ON" : "OFF");
    }

    // toggle the collision corner nearest the cursor within the hovered tile.
    if (m_Editor.IsDebugMode() && m_KeyX.JustPressed(m_Window))
    {
        double mouseX, mouseY;
        glfwGetCursorPos(m_Window, &mouseX, &mouseY);

        float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
        float baseWorldHeight =
            static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
        float worldWidth = baseWorldWidth / m_Camera.GetState().zoom;
        float worldHeight = baseWorldHeight / m_Camera.GetState().zoom;

        float worldX =
            (static_cast<float>(mouseX) / static_cast<float>(m_ScreenWidth)) * worldWidth +
            m_Camera.GetState().position.x;
        float worldY =
            (static_cast<float>(mouseY) / static_cast<float>(m_ScreenHeight)) * worldHeight +
            m_Camera.GetState().position.y;

        int tileWidth = m_Tilemap.GetTileWidth();
        int tileHeight = m_Tilemap.GetTileHeight();
        int tileX = static_cast<int>(worldX / tileWidth);
        int tileY = static_cast<int>(worldY / tileHeight);

        if (tileX >= 0 && tileY >= 0 && tileX < m_Tilemap.GetMapWidth() &&
            tileY < m_Tilemap.GetMapHeight())
        {
            if (m_Tilemap.GetTileCollision(tileX, tileY))
            {
                float localX = worldX - (tileX * tileWidth);
                float localY = worldY - (tileY * tileHeight);
                float halfTile = tileWidth * 0.5f;

                Tilemap::Corner corner;
                const char* cornerName;
                if (localX < halfTile && localY < halfTile)
                {
                    corner = Tilemap::CORNER_TL;
                    cornerName = "top-left";
                }
                else if (localX >= halfTile && localY < halfTile)
                {
                    corner = Tilemap::CORNER_TR;
                    cornerName = "top-right";
                }
                else if (localX < halfTile && localY >= halfTile)
                {
                    corner = Tilemap::CORNER_BL;
                    cornerName = "bottom-left";
                }
                else
                {
                    corner = Tilemap::CORNER_BR;
                    cornerName = "bottom-right";
                }

                bool currentlyBlocked = m_Tilemap.IsCornerCutBlocked(tileX, tileY, corner);
                m_Tilemap.SetCornerCutBlocked(tileX, tileY, corner, !currentlyBlocked);
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Corner cutting {} at ({}, {}): {}",
                              cornerName,
                              tileX,
                              tileY,
                              !currentlyBlocked ? "BLOCKED" : "ALLOWED");
            }
            else
            {
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Tile ({}, {}) has no collision - corner cutting N/A",
                              tileX,
                              tileY);
            }
        }
    }

    // F starts dialogue with an NPC when:
    //   1. Player is within INTERACTION_RANGE, AND
    //   2. NPC is in front of player, OR
    //   3. NPC hitbox is overlapping player hitbox.
    if (!m_Editor.IsActive() && !m_DialogueUi.inDialogue && !m_DialogueManager.IsActive() &&
        !m_DialogueUi.snap.active && m_KeyF.JustPressed(m_Window))
    {
        glm::vec2 playerPos = m_World.get<Transform>(m_PlayerEntity).position;
        const SupportSurface playerSurface = m_World.get<Elevation>(m_PlayerEntity).surface;
        Direction playerDir = m_World.get<Facing>(m_PlayerEntity).dir;

        int playerTileX = TileMath::TileIndex(playerPos.x, static_cast<float>(TILE_PIXEL_SIZE));
        int playerTileY =
            TileMath::StandingTileRow(playerPos.y, static_cast<float>(TILE_PIXEL_SIZE));

        int frontTileX = playerTileX;
        int frontTileY = playerTileY;

        switch (playerDir)
        {
            case Direction::DOWN:
                frontTileY += 1;
                break;
            case Direction::UP:
                frontTileY -= 1;
                break;
            case Direction::LEFT:
                frontTileX -= 1;
                break;
            case Direction::RIGHT:
                frontTileX += 1;
                break;
        }

        // Choose the first eligible NPC in stable instance-id order.
        for (const entt::entity npcE : EntityStore::Entities(m_World))
        {
            const Transform& npcTransform = m_World.get<Transform>(npcE);
            const Elevation& npcElevation = m_World.get<Elevation>(npcE);
            if (npcElevation.surface != playerSurface)
            {
                continue;
            }
            glm::vec2 npcPos = npcTransform.position;
            float distance = glm::length(npcPos - playerPos);

            if (distance <= INTERACTION_RANGE)
            {
                int npcTileX = TileMath::TileIndex(npcPos.x, static_cast<float>(TILE_PIXEL_SIZE));
                int npcTileY =
                    TileMath::StandingTileRow(npcPos.y, static_cast<float>(TILE_PIXEL_SIZE));

                bool isColliding =
                    CollisionGeometry::FeetBoxesOverlap(playerPos,
                                                        npcPos,
                                                        CharacterConstants::HALF_HITBOX_WIDTH,
                                                        CharacterConstants::HITBOX_HEIGHT,
                                                        CharacterConstants::COLLISION_EPS);

                bool isOnFrontTile = (npcTileX == frontTileX && npcTileY == frontTileY);

                int tileDistX = std::abs(playerTileX - npcTileX);
                int tileDistY = std::abs(playerTileY - npcTileY);
                bool isCardinalAdjacent =
                    (tileDistX == 1 && tileDistY == 0) || (tileDistX == 0 && tileDistY == 1);
                bool isSameTile = (tileDistX == 0 && tileDistY == 0);

                // cardinal adjacency starts conversation only on the player's facing side.
                bool isInCorrectDirection = false;
                if (isCardinalAdjacent)
                {
                    switch (playerDir)
                    {
                        case Direction::DOWN:
                            isInCorrectDirection =
                                (npcTileY > playerTileY && npcTileX == playerTileX);
                            break;
                        case Direction::UP:
                            isInCorrectDirection =
                                (npcTileY < playerTileY && npcTileX == playerTileX);
                            break;
                        case Direction::LEFT:
                            isInCorrectDirection =
                                (npcTileX < playerTileX && npcTileY == playerTileY);
                            break;
                        case Direction::RIGHT:
                            isInCorrectDirection =
                                (npcTileX > playerTileX && npcTileY == playerTileY);
                            break;
                    }
                }

                bool isVeryClose = (distance <= COLLISION_DISTANCE);
                glm::vec2 toNPC = npcPos - playerPos;
                bool isRoughlyInFront = false;
                if (isVeryClose)
                {
                    switch (playerDir)
                    {
                        case Direction::DOWN:
                            isRoughlyInFront = (toNPC.y > -DIRECTION_LENIENCY);
                            break;
                        case Direction::UP:
                            isRoughlyInFront = (toNPC.y < DIRECTION_LENIENCY);
                            break;
                        case Direction::LEFT:
                            isRoughlyInFront = (toNPC.x < DIRECTION_LENIENCY);
                            break;
                        case Direction::RIGHT:
                            isRoughlyInFront = (toNPC.x > -DIRECTION_LENIENCY);
                            break;
                    }
                }

                // allow contact or a front-facing nearby NPC; proximity alone behind the player
                // does not start dialogue.
                if (isColliding || isOnFrontTile || isInCorrectDirection ||
                    (isVeryClose && isRoughlyInFront))
                {
                    // Start dialogue only after alignment completes.
                    const Dialogue& npcDialogue = m_World.get<Dialogue>(npcE);
                    const WorldServices* npcSvc = m_World.ctx().find<WorldServices>();
                    m_DialogueUi.npcId = m_World.get<Identity>(npcE).instanceId;
                    m_DialogueUi.page = 0;
                    m_DialogueUi.snap.prefersTree = npcSvc != nullptr &&
                                                    npcSvc->dialogue != nullptr &&
                                                    npcSvc->dialogue->HasTree(npcDialogue.tree);
                    m_DialogueUi.snap.fallbackText = npcDialogue.text;

                    playerPos = m_World.get<Transform>(m_PlayerEntity).position;
                    npcPos = m_World.get<Transform>(npcE).position;

                    // feet anchor Y is at the bottom edge; subtract one tile for the standing row.
                    int snapTileY = static_cast<int>(
                        std::round((npcPos.y - TILE_PIXEL_SIZE) / TILE_PIXEL_SIZE));

                    m_DialogueUi.snap.npcTileX = npcTileX;
                    m_DialogueUi.snap.npcTileY = snapTileY;
                    glm::vec2 npcTargetPos(
                        static_cast<float>(m_DialogueUi.snap.npcTileX * TILE_PIXEL_SIZE +
                                           TILE_PIXEL_SIZE / 2),
                        static_cast<float>(m_DialogueUi.snap.npcTileY * TILE_PIXEL_SIZE +
                                           TILE_PIXEL_SIZE));

                    playerTileX =
                        TileMath::TileIndex(playerPos.x, static_cast<float>(TILE_PIXEL_SIZE));
                    playerTileY =
                        TileMath::StandingTileRow(playerPos.y, static_cast<float>(TILE_PIXEL_SIZE));

                    npcTileY = snapTileY;

                    int dx = playerTileX - npcTileX;
                    int dy = playerTileY - npcTileY;

                    // TODO: extract cardinal direction selection into the dialogue snap helper.
                    int finalDx = 0;
                    int finalDy = 0;
                    if (dx != 0 && dy != 0)
                    {
                        if (std::abs(dx) > std::abs(dy))
                        {
                            finalDx = (dx > 0) ? 1 : -1;
                            finalDy = 0;
                        }
                        else
                        {
                            finalDx = 0;
                            finalDy = (dy > 0) ? 1 : -1;
                        }
                    }
                    else if (dx != 0)
                    {
                        finalDx = (dx > 0) ? 1 : -1;
                        finalDy = 0;
                    }
                    else if (dy != 0)
                    {
                        finalDx = 0;
                        finalDy = (dy > 0) ? 1 : -1;
                    }
                    else
                    {
                        finalDx = 0;
                        finalDy = 1;
                    }

                    // Round to avoid snapping feet that are slightly off-center.
                    int currentPlayerTileX = static_cast<int>(
                        std::round((playerPos.x - TILE_PIXEL_SIZE / 2) / TILE_PIXEL_SIZE));
                    int currentPlayerTileY = static_cast<int>(
                        std::round((playerPos.y - TILE_PIXEL_SIZE) / TILE_PIXEL_SIZE));

                    glm::ivec2 snapTile = FindDialogueSnapTile(npcTileX,
                                                               npcTileY,
                                                               currentPlayerTileX,
                                                               currentPlayerTileY,
                                                               finalDx,
                                                               finalDy);
                    int playerTileXFinal = snapTile.x;
                    int playerTileYFinal = snapTile.y;

                    glm::vec2 playerTargetPos = playerPos;
                    bool hasPlayerTileTarget = (playerTileXFinal >= 0 && playerTileYFinal >= 0);
                    if (hasPlayerTileTarget)
                    {
                        playerTargetPos =
                            glm::vec2(static_cast<float>(playerTileXFinal * TILE_PIXEL_SIZE +
                                                         TILE_PIXEL_SIZE / 2),
                                      static_cast<float>(playerTileYFinal * TILE_PIXEL_SIZE +
                                                         TILE_PIXEL_SIZE));
                    }

                    glm::vec2 npcToPlayer = playerTargetPos - npcTargetPos;
                    Direction npcFacing = CardinalFromDelta(npcToPlayer.x, npcToPlayer.y);

                    glm::vec2 playerToNPC = npcTargetPos - playerTargetPos;
                    Direction playerFacing = CardinalFromDelta(playerToNPC.x, playerToNPC.y);

                    assert(!m_Editor.IsActive() && "Dialogue cannot start while editor is active");
                    PlayerSystem::Stop(m_World, m_PlayerEntity);
                    m_World.get<NpcIdle>(npcE).isStopped = true;
                    CharacterKinematics::ResetAnimation(m_World.get<AnimationState>(npcE));

                    m_DialogueUi.snap.active = true;
                    m_DialogueUi.snap.timer = 0.0f;
                    m_DialogueUi.snap.duration = 0.42f;
                    m_DialogueUi.snap.playerStart = playerPos;
                    m_DialogueUi.snap.playerTarget = playerTargetPos;
                    m_DialogueUi.snap.npcStart = npcPos;
                    m_DialogueUi.snap.npcTarget = npcTargetPos;
                    m_DialogueUi.snap.playerTileX =
                        hasPlayerTileTarget
                            ? playerTileXFinal
                            : static_cast<int>(std::round(
                                  (playerTargetPos.x - TILE_PIXEL_SIZE / 2) / TILE_PIXEL_SIZE));
                    m_DialogueUi.snap.playerTileY =
                        hasPlayerTileTarget
                            ? playerTileYFinal
                            : static_cast<int>(std::round((playerTargetPos.y - TILE_PIXEL_SIZE) /
                                                          TILE_PIXEL_SIZE));
                    m_DialogueUi.snap.hasPlayerTile = hasPlayerTileTarget;
                    m_DialogueUi.snap.playerFacing = playerFacing;
                    m_DialogueUi.snap.npcFacing = npcFacing;

                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Starting dialogue snap with NPC: {} target NPC tile ({}, "
                                  "{}), target player tile ({}, {})",
                                  m_World.get<Dialogue>(npcE).type,
                                  m_DialogueUi.snap.npcTileX,
                                  m_DialogueUi.snap.npcTileY,
                                  m_DialogueUi.snap.playerTileX,
                                  m_DialogueUi.snap.playerTileY);
                    break;
                }
            }
        }
    }

    ProcessDialogueInput();

    // movement stays world-relative; only the sprite row rotates with the camera.
    ProcessPlayerMovement(moveDirection, deltaTime);

    if (m_Editor.IsActive())
    {
        m_Editor.ProcessMouseInput(MakeEditorContext());
    }
}

void Game::ProcessDialogueInput()
{
    if (m_DialogueManager.IsActive())
    {
        // Arrow keys or W/S change options; Enter/Space advances and Escape closes immediately.
        if (m_KeyDialogueUp.JustPressed(m_Window))
            m_DialogueManager.SelectPrevious();

        if (m_KeyDialogueDown.JustPressed(m_Window))
            m_DialogueManager.SelectNext();

        if (m_KeyDialogueEnterTree.JustPressed(m_Window))
            ConfirmOrAdvanceTreeDialogue();

        if (m_KeyDialogueSpaceTree.JustPressed(m_Window))
            ConfirmOrAdvanceTreeDialogue();

        if (m_KeyDialogueEscapeTree.JustPressed(m_Window))
            ForceCloseTreeDialogue();
    }

    if (m_DialogueUi.inDialogue)
    {
        if (m_KeyDialogueEnter.JustPressed(m_Window))
            CloseSimpleDialogue();

        if (m_KeyDialogueSpace.JustPressed(m_Window))
            CloseSimpleDialogue();

        if (m_KeyDialogueEscape.JustPressed(m_Window))
            CloseSimpleDialogue();
    }
}

void Game::ProcessPlayerMovement(glm::vec2 moveDirection, float deltaTime)
{
    if (!m_Editor.IsActive() && !m_DialogueUi.inDialogue && !m_DialogueManager.IsActive() &&
        !m_DialogueUi.snap.active && !m_Console.IsOpen())
    {
        const glm::vec2 beforeMove = m_World.get<Transform>(m_PlayerEntity).position;

        // reuse collision scratch storage across frames.
        BuildNpcCollisionBodies(m_World, m_NpcBodies);

        // Null world and NPC inputs select no-clip without changing the shared movement
        // implementation.
        const Tilemap* tilemap =
            m_World.get<PlayerModes>(m_PlayerEntity).noClip ? nullptr : &m_Tilemap;
        const std::vector<CharacterCollisionBody>* npcBodies =
            m_World.get<PlayerModes>(m_PlayerEntity).noClip ? nullptr : &m_NpcBodies;
        PlayerSystem::Move(m_World, m_PlayerEntity, moveDirection, deltaTime, tilemap, npcBodies);

        // no-clip has no accepted probe; derive support from the final position.
        if (!tilemap)
        {
            CharacterKinematics::DerivePlane(m_World.get<Elevation>(m_PlayerEntity),
                                             beforeMove,
                                             m_World.get<Transform>(m_PlayerEntity).position,
                                             m_Tilemap);
        }
    }
    else if (m_DialogueUi.inDialogue || m_DialogueUi.snap.active)
    {
        PlayerSystem::Stop(m_World, m_PlayerEntity);
    }
}

void Game::ScrollCallback(GLFWwindow* window, double, double yoffset)
{
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (!game)
    {
        return;
    }

    if (game->m_Console.IsOpen())
    {
        // scroll suggestions before console history when the cursor is over the dropdown.
        double mx = 0.0;
        double my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        if (!game->m_Console.TryScrollDropdown(mx, my, yoffset))
        {
            game->m_Console.OnScroll(yoffset);
        }
        return;
    }

    if (game->m_Editor.IsActive())
    {
        game->m_Editor.HandleScroll(yoffset, game->MakeEditorContext());

        if (game->m_Editor.IsShowTilePicker())
        {
            return;
        }
    }

    bool ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (ctrlPressed)
    {
        float baseWorldWidth =
            static_cast<float>(game->m_TilesVisibleWidth * game->m_Tilemap.GetTileWidth());
        float baseWorldHeight =
            static_cast<float>(game->m_TilesVisibleHeight * game->m_Tilemap.GetTileHeight());

        glm::vec2 playerPos = game->m_World.get<Transform>(game->m_PlayerEntity).position;
        glm::vec2 playerVisualCenter =
            playerPos - glm::vec2(0.0f, CharacterConstants::HITBOX_HEIGHT * 0.5f);

        float mapWidth =
            static_cast<float>(game->m_Tilemap.GetMapWidth() * game->m_Tilemap.GetTileWidth());
        float mapHeight =
            static_cast<float>(game->m_Tilemap.GetMapHeight() * game->m_Tilemap.GetTileHeight());
        bool editorFreeMode = game->m_Editor.IsActive() && game->m_Camera.IsFreeMode();

        game->m_Camera.HandleZoomScroll(yoffset,
                                        playerVisualCenter,
                                        baseWorldWidth,
                                        baseWorldHeight,
                                        mapWidth,
                                        mapHeight,
                                        editorFreeMode,
                                        editorFreeMode);
    }
}

void Game::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (!game)
    {
        return;
    }
    game->m_Console.OnChar(codepoint);
}

void Game::PumpConsoleKeys()
{
    // Console key latches share function-local state across calls.
    static KeyToggle<GLFW_KEY_ENTER> kEnter;
    static KeyToggle<GLFW_KEY_BACKSPACE> kBackspace;
    static KeyToggle<GLFW_KEY_DELETE> kDelete;
    static KeyToggle<GLFW_KEY_TAB> kTab;
    static KeyToggle<GLFW_KEY_UP> kUp;
    static KeyToggle<GLFW_KEY_DOWN> kDown;
    static KeyToggle<GLFW_KEY_LEFT> kLeft;
    static KeyToggle<GLFW_KEY_RIGHT> kRight;
    static KeyToggle<GLFW_KEY_HOME> kHome;
    static KeyToggle<GLFW_KEY_END> kEnd;
    static KeyToggle<GLFW_KEY_ESCAPE> kEscape;

    if (kEnter.JustPressed(m_Window))
        m_Console.OnEnter();
    if (kBackspace.JustPressed(m_Window))
    {
        const bool ctrl = glfwGetKey(m_Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(m_Window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        if (ctrl)
            m_Console.OnBackspaceWord();
        else
            m_Console.OnBackspace();
    }
    if (kDelete.JustPressed(m_Window))
        m_Console.OnDelete();
    if (kTab.JustPressed(m_Window))
    {
        // Empty input toggles console size; nonempty input cycles completion.
        if (m_Console.Buffer().Input().empty())
            m_Console.ToggleFullscreen();
        else
            m_Console.OnTab();
    }
    if (kUp.JustPressed(m_Window))
        m_Console.OnUp();
    if (kDown.JustPressed(m_Window))
        m_Console.OnDown();
    if (kLeft.JustPressed(m_Window))
        m_Console.OnLeft();
    if (kRight.JustPressed(m_Window))
        m_Console.OnRight();
    if (kHome.JustPressed(m_Window))
        m_Console.OnHome();
    if (kEnd.JustPressed(m_Window))
        m_Console.OnEnd();
    if (kEscape.JustPressed(m_Window))
        m_Console.OnEscape();

    // hover selects a suggestion; click uses the same insertion path as Tab completion.
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);
    m_Console.OnMouseHover(mouseX, mouseY);
    const bool mouseDown = (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    if (mouseDown && !m_ConsoleMouseLeftPrev)
    {
        m_Console.OnMouseClick(mouseX, mouseY);
    }
    m_ConsoleMouseLeftPrev = mouseDown;
}

void Game::ReleaseDialogueNPC()
{
    if (const entt::entity dialogueNpc = FindNPCById(m_DialogueUi.npcId); dialogueNpc != entt::null)
    {
        m_World.get<NpcIdle>(dialogueNpc).isStopped = false;
    }
    m_DialogueUi.npcId = 0;
}

void Game::CloseSimpleDialogue()
{
    m_DialogueUi.inDialogue = false;
    ReleaseDialogueNPC();
    m_DialogueUi.text.clear();
}

void Game::ConfirmOrAdvanceTreeDialogue()
{
    if (m_DialogueUi.charReveal >= 0.0f)
    {
        m_DialogueUi.charReveal = -1.0f;
        return;
    }

    if (!IsDialogueOnLastPage())
    {
        m_DialogueUi.page++;
        m_DialogueUi.charReveal = 0.0f;
    }
    else
    {
        m_DialogueUi.page = 0;
        m_DialogueUi.charReveal = 0.0f;
        m_DialogueManager.ConfirmSelection();
        if (!m_DialogueManager.IsActive())
        {
            ReleaseDialogueNPC();
        }
    }
}

void Game::ForceCloseTreeDialogue()
{
    m_DialogueManager.EndDialogue();
    m_DialogueUi.page = 0;
    ReleaseDialogueNPC();
}

glm::ivec2 Game::FindDialogueSnapTile(int npcTileX,
                                      int npcTileY,
                                      int playerTileX,
                                      int playerTileY,
                                      int preferredDx,
                                      int preferredDy) const
{
    auto isValidSnapTile = [&](int tx, int ty)
    {
        if (tx < 0 || ty < 0 || tx >= m_Tilemap.GetMapWidth() || ty >= m_Tilemap.GetMapHeight())
        {
            return false;
        }
        if (tx == npcTileX && ty == npcTileY)
        {
            return false;
        }
        return !m_Tilemap.GetTileCollision(tx, ty);
    };

    if (playerTileX != npcTileX || playerTileY != npcTileY)
    {
        if (isValidSnapTile(playerTileX, playerTileY))
        {
            int tileDistX = std::abs(playerTileX - npcTileX);
            int tileDistY = std::abs(playerTileY - npcTileY);
            bool isCardinalAdjacent =
                (tileDistX == 1 && tileDistY == 0) || (tileDistX == 0 && tileDistY == 1);
            if (isCardinalAdjacent)
            {
                return glm::ivec2(playerTileX, playerTileY);
            }
        }
    }

    if (preferredDx == 0 && preferredDy == 0)
    {
        preferredDx = 0;
        preferredDy = 1;
    }

    struct CardinalDir
    {
        int dx, dy;
    };
    CardinalDir cardinals[] = {
        {preferredDx, preferredDy},
        {0, 1},
        {0, -1},
        {1, 0},
        {-1, 0},
    };

    for (const auto& dir : cardinals)
    {
        int testX = npcTileX + dir.dx;
        int testY = npcTileY + dir.dy;
        if (testX == npcTileX && testY == npcTileY)
        {
            continue;
        }
        if (isValidSnapTile(testX, testY))
        {
            return glm::ivec2(testX, testY);
        }
    }

    // If no adjacent tile is safe, allow the current player tile before reporting failure.
    if (isValidSnapTile(playerTileX, playerTileY))
    {
        return glm::ivec2(playerTileX, playerTileY);
    }

    return glm::ivec2(-1, -1);
}
