#pragma once

#include "AssetRegistry.hpp"
#include "CameraController.hpp"
#include "CharacterDirection.hpp"
#include "Console.hpp"
#include "DialogueManager.hpp"
#include "DialogueStore.hpp"
#include "Editor.hpp"
#include "EntityStore.hpp"
#include "GameMode.hpp"
#include "GameStateManager.hpp"
#include "IRenderer.hpp"
#include "KeyToggle.hpp"
#include "MenuLogic.hpp"
#include "NpcIdle.hpp"
#include "ParticleSystem.hpp"
#include "RenderDrawable.hpp"
#include "RendererAPI.hpp"
#include "RendererFactory.hpp"
#include "SkyDrawList.hpp"
#include "SkyRenderer.hpp"
#include "TextureStore.hpp"
#include "Tilemap.hpp"
#include "TimeManager.hpp"
#include "ViewScaling.hpp"
#include "WeatherDirector.hpp"
#include "WorldServices.hpp"

#include <entt/entt.hpp>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <random>
#include <string>
#include <vector>

/**
 * @struct FPSCounter
 * @brief Frame rate measurement and display state.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 */
struct FPSCounter
{
    float updateTimer = 0.0f;   ///< accumulator for FPS update interval (seconds).
    float consoleTimer = 0.0f;  ///< Unused while console statistics are disabled.
    int frameCount = 0;
    float currentFps = 0.0f;
    float targetFps = 0.0f;  ///< target FPS limit; &lt;= 0 means unlimited.
    int drawCallAccumulator = 0;
    int currentDrawCalls = 0;
};

/**
 * @struct DialogueSnapState
 * @brief Player and NPC positions during pre-dialogue alignment.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 */
struct DialogueSnapState
{
    bool active = false;
    float timer = 0.0f;     ///< elapsed snap time in seconds.
    float duration = 0.4f;  ///< total snap animation duration in seconds.
    glm::vec2 playerStart{0.0f};
    glm::vec2 playerTarget{0.0f};
    glm::vec2 npcStart{0.0f};
    glm::vec2 npcTarget{0.0f};
    bool hasPlayerTile = true;
    int playerTileX = 0;
    int playerTileY = 0;
    int npcTileX = 0;
    int npcTileY = 0;
    Direction playerFacing = Direction::DOWN;
    Direction npcFacing = Direction::DOWN;
    bool prefersTree = false;
    std::string fallbackText;  ///< simple text if no tree available.
};

/**
 * @struct DialogueUiState
 * @brief Active conversation presentation; npcId remains stable across NPC despawn and respawn.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * DialogueManager owns branching-tree execution; this state holds simple text, pagination, fade,
 * typewriter and snap presentation. The stable speaker id lets the next update detect that an NPC
 * was removed.
 */
struct DialogueUiState
{
    bool inDialogue = false;
    std::uint64_t npcId = 0;  ///< instance id of the NPC being talked to (0 = none).
    std::string text;
    int page = 0;
    int totalPages = 1;
    float boxFadeTimer = 0.0f;  ///< dialogue-box fade-in timer (seconds).
    float charReveal = -1.0f;   ///< typewriter char count; &lt; 0 means fully revealed.
    DialogueSnapState snap;
};

/**
 * @class Game
 * @brief Composition root for the window, renderer, world and game subsystems.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Publishes borrowed `WorldServices` pointers through `m_World.ctx()`. Systems may use those
 * services while this object lives. Renderer switches replace the window and renderer separately.
 * The variable timestep is capped at 0.1 s to limit movement after stalls.
 * Editor and dialogue are mutually exclusive within Playing. Title updates only its cosmetic world.
 *
 * The camera uses ExpApproachAlpha for frame-rate-independent settling and clamps to map bounds.
 * World draws precede post-processing; UI draws after the composite.
 *
 * Initialize acquires the window and subsystems; Run drives them until close; Shutdown releases
 * them. Lifecycle and frame updates are in Game.cpp, input routing in GameInput.cpp, menu and 3D
 * frames in GameMenus.cpp, and dialogue presentation in GameDialogue.cpp.
 *
 * ### :material-state-machine: Game modes
 *
 * The console command `editor on` enables editing during gameplay. Dialogue input cannot start
 * a snap while the editor is active. Paused retains the world state under its menu.
 *
 * ```mermaid
 * stateDiagram-v2
 *     [*] --> Title
 *     Title --> Playing: Continue / New game
 *     Playing --> Paused: Esc (only when no dialogue is open)
 *     Paused --> Playing: Esc / Resume
 *     Paused --> Title: Quit to title
 *     state Playing {
 *         [*] --> Roaming
 *         Roaming --> Editing: Console command editor on
 *         Editing --> Roaming: Console command editor off
 *         Roaming --> Snapping: F near an NPC (editor off)
 *         Snapping --> Talking: Snap timer elapsed
 *         Talking --> Roaming: Conversation ends or Esc
 *     }
 * ```
 *
 * ### :material-layers-outline: Flat frame composition
 *
 * The flat path merges actors with depth tiles between background and foreground layers.
 * Particles, lights and sky complete the scene before post-processing and screen-space UI.
 *
 * ```mermaid
 * flowchart TD
 *     Begin[BeginFrame] --> Scene[BeginScene offscreen]
 *     Scene --> World[Background and no-projection layers]
 *     World --> Sort[Depth tiles and actors with support constraints]
 *     Sort --> Foreground[Foreground layers and particles]
 *     Foreground --> Lighting[World lights, sky overlay]
 *     Lighting --> PostFX[EndSceneApplyPostFX]
 *     PostFX --> UI[Editor, debug, dialogue, menu, console UI]
 *     UI --> End[EndFrame]
 * ```
 *
 * @code{.cpp}
 * Game g;
 * if (g.Initialize())
 * {
 *     g.Run();
 * }
 * g.Shutdown();  // Also releases resources after partial initialization.
 * @endcode
 */
class Game
{
public:
    /**
     * @fn Game::Game()
     * @brief Call Initialize separately to acquire resources.
     * @author Alex (<https://github.com/lextpf>)
     */
    Game();

    /**
     * @fn Game::~Game()
     * @brief Release owned resources through Shutdown.
     * @author Alex (<https://github.com/lextpf>)
     */
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    /**
     * @fn Game::Game(Game&&)
     * @brief Moving would invalidate GLFW callbacks and subsystem references.
     * @author Alex (<https://github.com/lextpf>)
     */
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    /**
     * @fn bool Game::Initialize()
     * @brief Create the player and title world with the configured renderer and assets.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Publish WorldServices and create the player before loading a world. Choose the renderer and
     * assets from rift.project.json, then initialize editor, particles, time, sky and dialogue.
     * NPC routes initialize on their first update.
     *
     * Continue and New Game select the gameplay world later. Startup leaves the day length at the
     * TimeManager default. On failure, call Shutdown or destroy this object to release resources
     * acquired before the failure. This method does not support repeated initialization.
     *
     * @return True when startup completes; false for a reported initialization failure.
     */
    bool Initialize();

    /**
     * @fn void Game::Run()
     * @brief Run frames until the window closes or an exception leaves the loop.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Poll GLFW before reading input so handlers use this frame's key state. Exceptions are
     * logged and consumed; returning does not distinguish an error from a normal close.
     * The optional FPS limiter sleeps, then yields to the deadline. A nonpositive limit skips it.
     * Initialize must have succeeded and the window must still be valid.
     *
     * ```mermaid
     * sequenceDiagram
     *     participant Loop as Game::Run
     *     participant GLFW as GLFW
     *     participant Input as ProcessInput
     *     participant Update as Update
     *     participant Render as Render
     *     Loop->>Loop: Sample frame start, compute deltaTime
     *     Loop->>GLFW: glfwPollEvents()
     *     Loop->>Loop: Clamp deltaTime to 0.1 s
     *     Loop->>Input: ProcessInput(deltaTime)
     *     Loop->>Update: Update(deltaTime)
     *     Loop->>Render: Render()
     *     Loop->>Loop: FPS limiter (sleep + spin)
     * ```
     */
    void Run();

    /**
     * @fn void Game::Shutdown()
     * @brief Release the renderer before the window; repeated calls and partial startup are
     * supported.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Shutdown();

    /**
     * @fn void Game::SetTargetFps(float fps)
     * @brief Disable the frame limiter for a nonpositive frame rate.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTargetFps(float fps) { m_Fps.targetFps = fps; }

    bool IsWorld3DEnabled() const { return m_World3DEnabled; }
    void SetWorld3DEnabled(bool enabled) { m_World3DEnabled = enabled; }
    cameraRig::Preset GetCameraPreset() const { return m_CameraPreset; }
    float GetCameraYaw() const { return m_CameraYaw; }
    float GetCameraPitch() const { return m_CameraPitch; }

    // inline so console tests link without Game translation units.

    /**
     * @fn void Game::SetCameraPreset(cameraRig::Preset preset)
     * @brief Adopt the preset yaw and pitch immediately.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetCameraPreset(cameraRig::Preset preset)
    {
        m_CameraPreset = preset;

        cameraRig::RigParams probe;
        probe.yawRadians = m_CameraYaw;
        probe.pitchRadians = m_CameraPitch;
        cameraRig::ApplyPreset(probe, preset);
        m_CameraYaw = probe.yawRadians;
        m_CameraPitch = probe.pitchRadians;
    }

    /**
     * @fn void Game::SetCameraYaw(float radians)
     * @brief Wrap yaw to greater than -pi and at most pi radians; select the Free preset.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetCameraYaw(float radians)
    {
        m_CameraYaw = cameraRig::WrapYaw(radians);
        m_CameraPreset = cameraRig::Preset::Free;
    }

    /**
     * @fn void Game::SetCameraPitch(float radians)
     * @brief Clamp pitch above the horizon and select the Free preset.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetCameraPitch(float radians)
    {
        m_CameraPitch = cameraRig::ClampPitch(radians);
        m_CameraPreset = cameraRig::Preset::Free;
    }

    /**
     * @fn bool Game::SwitchRenderer(RendererAPI api)
     * @brief Replace the renderer and window, then rebuild GPU resources.
     * @author Alex (<https://github.com/lextpf>)
     *
     * A replacement invalidates borrowed window and renderer pointers, including after rollback.
     * A request for the active or unavailable API returns before replacing resources.
     * If setup fails, retry the previous API. If that also fails, shut down the game.
     *
     * | Result | State after the call                                 |
     * |--------|------------------------------------------------------|
     * | True   | Requested API is active, or was already active.       |
     * | False  | API unavailable, rollback completed, or game stopped. |
     *
     * @param api Backend to activate.
     * @return Whether the requested API is active.
     * @pre The game has a live window when requesting a different available API.
     */
    bool SwitchRenderer(RendererAPI api);

    RendererAPI GetRendererAPI() const { return m_RendererAPI; }

    /**
     * @fn void Game::EndAnyDialogue()
     * @brief Close either dialogue path and clear pending alignment.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Release the speaker for patrol if its stable ID still resolves to a live NPC. Calls while
     * inactive are safe. An NPC removed during a conversation does not need special cleanup.
     */
    void EndAnyDialogue()
    {
        const bool treeActive = m_DialogueManager.IsActive();
        if (treeActive)
        {
            m_DialogueManager.EndDialogue();
            m_DialogueUi.page = 0;
        }
        if (m_DialogueUi.inDialogue)
        {
            m_DialogueUi.inDialogue = false;
            m_DialogueUi.text.clear();
        }
        if (const entt::entity dialogueNpc = FindNPCById(m_DialogueUi.npcId);
            dialogueNpc != entt::null)
        {
            m_World.get<NpcIdle>(dialogueNpc).isStopped = false;
        }
        m_DialogueUi.npcId = 0;
        m_DialogueUi.snap.active = false;
    }

    /**
     * @fn const std::string& Game::GetSaveMapPath() const
     * @brief Configured JSON path shared by map loading and editor saves.
     * @author Alex (<https://github.com/lextpf>)
     */
    const std::string& GetSaveMapPath() const { return m_SaveMapPath; }

    /**
     * @fn const FPSCounter& Game::GetFPSCounter() const
     * @brief Borrow live statistics; values update at the FPS sampling interval.
     * @author Alex (<https://github.com/lextpf>)
     */
    const FPSCounter& GetFPSCounter() const { return m_Fps; }

    /**
     * @fn GLFWwindow* Game::GetWindow() const
     * @brief Null before window creation and after shutdown.
     * @author Alex (<https://github.com/lextpf>)
     */
    GLFWwindow* GetWindow() const { return m_Window; }

    /**
     * @fn glm::vec2 Game::VisibleWorldSize() const
     * @brief World-pixel view extent matching the render projection.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec2 VisibleWorldSize() const
    {
        return viewScaling::VisibleWorldSize(m_ScreenWidth, m_ScreenHeight, PIXEL_SCALE);
    }
    /**
     * @fn glm::vec2 Game::VisibleWorldSizeZoomed() const
     * @brief World-pixel view extent after camera zoom.
     * @author Alex (<https://github.com/lextpf>)
     */
    glm::vec2 VisibleWorldSizeZoomed() const
    {
        return viewScaling::VisibleWorldSizeZoomed(
            m_ScreenWidth, m_ScreenHeight, PIXEL_SCALE, m_Camera.GetState().zoom);
    }

    bool IsInSimpleDialogue() const { return m_DialogueUi.inDialogue; }

    /**
     * @fn const std::string& Game::GetSimpleDialogueText() const
     * @brief Empty when simple dialogue is inactive.
     * @author Alex (<https://github.com/lextpf>)
     */
    const std::string& GetSimpleDialogueText() const { return m_DialogueUi.text; }

    /**
     * @fn std::uint64_t Game::GetDialogueNPCId() const
     * @brief Speaker instance id, or 0 when absent.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::uint64_t GetDialogueNPCId() const { return m_DialogueUi.npcId; }

    /**
     * @fn void Game::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
     * @brief Route scroll to console, editor, then Ctrl-camera zoom, in that priority order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * An open tile picker consumes editor scroll. Positive yoffset zooms in; gameplay
     * zoom spans 0.4x to 4x, with a 0.1x floor for editor free camera. bare gameplay scroll does
     * nothing.
     *
     * The console suggestion dropdown claims wheel input while hovered; otherwise the console
     * scrollback receives it. Editor scroll runs before Ctrl-zoom, and an open tile picker
     * consumes the event. xoffset is unused.
     */
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    /**
     * @fn void Game::CharCallback(GLFWwindow* window, unsigned int codepoint)
     * @brief Forward typed codepoints; the console consumes them only while open.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void CharCallback(GLFWwindow* window, unsigned int codepoint);

private:
    /// Console bindings mutate Game-owned state directly.
    friend class Console;

    /**
     * @fn void Game::ProcessInput(float deltaTime)
     * @brief Route console input before title, pause, editor and gameplay handlers; deltaTime is in
     * seconds.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessInput(float deltaTime);

    /**
     * @fn void Game::ProcessDialogueInput()
     * @brief Handle branching options and simple-dialogue dismissal before movement input.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessDialogueInput();

    /**
     * @fn void Game::ProcessPlayerMovement(glm::vec2 moveDirection, float deltaTime)
     * @brief Apply player movement and collision, then test nearby NPCs for dialogue; moveDirection
     * is raw input and deltaTime is in seconds.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessPlayerMovement(glm::vec2 moveDirection, float deltaTime);

    /**
     * @fn void Game::Update(float deltaTime)
     * @brief Advance simulation according to the active game mode.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Every mode updates FPS counters and deferred window sizing. non-paused modes advance sky,
     * particles, postfx and animated tiles. Playing also advances player, time and weather before
     * dialogue presentation, NPC AI, editor and camera follow. Apply NPC overlap stops after
     * positions settle.
     */
    void Update(float deltaTime);

    /**
     * @fn void Game::Render()
     * @brief Skip reentrant draws during Render or Update.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Title and world3d each use a separate frame pipeline.
     *
     * The flat path combines tiles and actors in one Y-sorted list between background and
     * foreground layers. particles, world lights and sky follow; EndSceneApplyPostFX composites
     * the scene before editor, dialogue, HUD, pause and console UI.
     */
    void Render();

    /**
     * @fn EditorContext Game::MakeEditorContext()
     * @brief Borrow Game state for this frame only; pass the context directly to Editor and discard
     * it.
     * @author Alex (<https://github.com/lextpf>)
     */
    EditorContext MakeEditorContext();

    /**
     * @fn void Game::RenderNPCHeadText()
     * @brief Draw the simple-text fallback above the speaker when no dialogue tree is used.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderNPCHeadText();

    /**
     * @fn void Game::RenderDialogueText(glm::vec2 boxPos, glm::vec2 boxSize)
     * @brief Wrap to boxSize.x - 20 pixels and center each line; stop at the text area bottom.
     * @author Alex (<https://github.com/lextpf>)
     *
     * boxPos is the top-left in screen pixels; boxSize is the available extent.
     */
    void RenderDialogueText(glm::vec2 boxPos, glm::vec2 boxSize);

    /**
     * @fn void Game::RenderDialogueTreeBox()
     * @brief Draw the active tree page and response options; cache pagination for input handling.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderDialogueTreeBox();

    /**
     * @fn bool Game::IsDialogueOnLastPage()
     * @brief Use the page count cached by the last tree-dialogue render; valid only during that
     * conversation.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsDialogueOnLastPage();

    /**
     * @fn void Game::ReleaseDialogueNPC()
     * @brief Release the speaker and clear npcId; callers clear the remaining dialogue UI state.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ReleaseDialogueNPC();

    /**
     * @fn bool Game::HasDialogueNPC() const
     * @brief True only while the stored speaker instance id resolves to a live NPC.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool HasDialogueNPC() const;

    /**
     * @fn entt::entity Game::FindNPCById(std::uint64_t id)
     * @brief O(NPC count); return entt::null for id 0 or an absent NPC.
     * @author Alex (<https://github.com/lextpf>)
     */
    entt::entity FindNPCById(std::uint64_t id) { return EntityStore::FindById(m_World, id); }
    entt::entity FindNPCById(std::uint64_t id) const { return EntityStore::FindById(m_World, id); }

    /**
     * @fn void Game::CloseSimpleDialogue()
     * @brief Close simple dialogue and release the speaker for patrol.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CloseSimpleDialogue();

    /**
     * @fn void Game::ConfirmOrAdvanceTreeDialogue()
     * @brief Finish typewriter reveal first, then advance a page, then confirm the selected option.
     * @author Alex (<https://github.com/lextpf>)
     *
     * A reveal-completion press does nothing else. selecting an option resets pagination to page 0
     * and releases the speaker if the tree ends.
     */
    void ConfirmOrAdvanceTreeDialogue();

    /**
     * @fn void Game::ForceCloseTreeDialogue()
     * @brief Close the tree immediately on Escape, without advancing a page or confirming an
     * option.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ForceCloseTreeDialogue();

    /**
     * @fn bool Game::CheckSaveExists() const
     * @brief True only when the configured save path is a regular file.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool CheckSaveExists() const;

    /**
     * @fn void Game::LoadGameWorld(bool loadSave)
     * @brief Both menu paths load the authored map or generate terrain; New Game separately resets
     * transient state.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Restore NPCs, player placement and camera target after loading. boot uses
     * LoadTitleScreenWorld instead, so title content does not depend on the saved gameplay world.
     */
    void LoadGameWorld(bool loadSave);

    /**
     * @fn void Game::PackCharactersIntoAtlas()
     * @brief Pack character sheets into the tile atlas so the Y-sorted pass shares one texture
     * batch.
     * @author Alex (<https://github.com/lextpf>)
     */
    void PackCharactersIntoAtlas();

    /**
     * @fn void Game::PaintTitleWorld(int tilesWide, int tilesTall)
     * @brief Title only: rebuild grass, whole-map particle zones and the particle population.
     * @author Alex (<https://github.com/lextpf>)
     */
    void PaintTitleWorld(int tilesWide, int tilesTall);
    /**
     * @fn void Game::RefreshTitleWorldForViewport(bool forceRepaint)
     * @brief Cover the viewport without shrinking below base size; forceRepaint also rebuilds
     * unchanged sizes.
     * @author Alex (<https://github.com/lextpf>)
     *
     * recenter the camera after resizing. Use forceRepaint for initial load and false for resize
     * notifications so equal tile dimensions avoid rebuilding emitters.
     */
    void RefreshTitleWorldForViewport(bool forceRepaint);

    /**
     * @fn void Game::LoadTitleScreenWorld()
     * @brief Destroy npcs and park the player at (0, 0); freeze the cosmetic world at 23:00 with
     * Aurora.
     * @author Alex (<https://github.com/lextpf>)
     *
     * disable the weather director, raise the per-zone particle cap and pre-warm the pool. The
     * player entity stays valid for shared frame code and is skipped by title rendering.
     */
    void LoadTitleScreenWorld();

    /**
     * @fn void Game::ResetWorldToDefaults()
     * @brief Reset world, time and session flags without changing the on-disk save.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ResetWorldToDefaults();

    /**
     * @fn void Game::ProcessTitleInput()
     * @brief Navigate the title menu and handle the New Game overwrite prompt.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessTitleInput();

    /**
     * @fn void Game::ProcessPauseInput()
     * @brief Dispatch Resume and Quit to Title while gameplay remains paused.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessPauseInput();

    /**
     * @fn void Game::RebuildTitleMenu()
     * @brief Refresh enabled flags and select the first enabled item when entering Title.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RebuildTitleMenu();

    /**
     * @fn void Game::RenderTitleFrame()
     * @brief Own the complete title frame, including scene, postfx and UI.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderTitleFrame();

    /**
     * @fn void Game::RenderFrame3D()
     * @brief Own the complete gameplay frame for the world3d path.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderFrame3D();

    /**
     * @fn void Game::RenderWorldLights3D(const particleCards::Frame& frame)
     * @brief Draw light pools as ground quads in pass C0; frame is built once per 3D frame.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderWorldLights3D(const particleCards::Frame& frame);

    /**
     * @fn cameraRig::RigParams Game::BuildCameraRig() const
     * @brief Focus the orbit rig on the flat viewport center to preserve the view when switching
     * paths.
     * @author Alex (<https://github.com/lextpf>)
     */
    cameraRig::RigParams BuildCameraRig() const;

    /**
     * @fn void Game::RenderTitleContent()
     * @brief Draw title UI after EndSceneApplyPostFX.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderTitleContent();

    /**
     * @fn void Game::RenderVersionFooter()
     * @brief Leave screen-space UI projection active; callers must restore projection before world
     * draws.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Shared by title and gameplay HUD; place the author watermark at bottom-left and version at
     * bottom-right.
     */
    void RenderVersionFooter();

    /**
     * @fn void Game::RenderPauseOverlay()
     * @brief Draw before the console pass.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderPauseOverlay();

    /**
     * @fn void Game::RenderConfirmOverwritePrompt()
     * @brief Draw the New Game overwrite prompt above the title menu.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RenderConfirmOverwritePrompt();

    /**
     * @fn glm::ivec2 Game::FindDialogueSnapTile(int npcTileX, int npcTileY, int playerTileX, int \
     *     playerTileY, int preferredDx, int preferredDy) const
     * @brief Choose an in-bounds nonblocking tile other than the NPC tile.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Keep a valid adjacent player tile first. then try the preferred cardinal direction,
     * down, up, right, left, and finally the current player tile. Return (-1, -1) if none is safe.
     * preferred direction components are -1, 0 or 1; a zero direction becomes (0, 1).
     */
    glm::ivec2 FindDialogueSnapTile(int npcTileX,
                                    int npcTileY,
                                    int playerTileX,
                                    int playerTileY,
                                    int preferredDx,
                                    int preferredDy) const;

    GLFWwindow* m_Window = nullptr;
    int m_ScreenWidth = 1520;  ///< Window width in pixels (19 tiles * 80 px).
    int m_ScreenHeight = 800;  ///< Window height in pixels (10 tiles * 80 px).
    bool m_GlfwInitialized = false;

    /**
     * @brief Visible tile counts use integer division; window snapping uses TILE_PIXEL_SIZE *
     * PIXEL_SCALE pixels.
     */
    int m_TilesVisibleWidth = 19;
    int m_TilesVisibleHeight = 10;
    static constexpr int TILE_PIXEL_SIZE = 16;  ///< Tile size in world pixels.
    static constexpr int PIXEL_SCALE = 5;       ///< World-pixel to screen-pixel scale (5x).
    float m_ResizeSnapTimer = 0.0f;             ///< countdown (seconds) to the deferred snap.
    bool m_PendingWindowSnap = false;

    /**
     * @fn void Game::OnFramebufferResized(int width, int height)
     * @brief Apply framebuffer pixel dimensions immediately; defer tile snapping until resizing
     * settles.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnFramebufferResized(int width, int height);

    /**
     * @fn void Game::SnapWindowToTileBoundaries()
     * @brief Round the client area down to 80-pixel increments, with a 400x320 minimum.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SnapWindowToTileBoundaries();

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    /**
     * @fn void Game::WindowRefreshCallback(GLFWwindow* window)
     * @brief Redraw during resize drag.
     * @author Alex (<https://github.com/lextpf>)
     */
    static void WindowRefreshCallback(GLFWwindow* window);

    TextureStore m_TextureStore;
    DialogueStore m_DialogueStore;
    AssetRegistry m_Assets;
    /// Shared NPC RNG published through WorldServices::npcRng; tests can seed this stream.
    std::mt19937 m_NpcRng{std::random_device{}()};
    /// registry context holds WorldServices; entities hold gameplay components.
    entt::registry m_World;
    Tilemap m_Tilemap;
    entt::entity m_PlayerEntity = entt::null;
    ParticleSystem m_Particles;
    TimeManager m_TimeManager;
    WeatherDirector m_WeatherDirector;
    SkyRenderer m_SkyRenderer;
    std::unique_ptr<IRenderer> m_Renderer;
    RendererAPI m_RendererAPI = RendererAPI::OpenGL;
    std::vector<std::string> m_FontCandidates;
    std::string m_SaveMapPath = "rift.save.json";

    CameraController m_Camera;
    bool m_IsRendering = false;
    /// block refresh-callback rendering during Update, including synchronous WM_SIZE callbacks.
    bool m_IsUpdating = false;

    float m_LastFrameTime = 0.0f;  ///< Previous frame timestamp in seconds.

    /// Seconds for postfx animation; wrap periodically to limit float precision loss.
    float m_PostFXTime = 0.0f;

    /// False composites the raw scene without postfx effects.
    bool m_PostFXEnabled = true;

    FPSCounter m_Fps;

    Editor m_Editor;

    std::vector<CharacterCollisionBody> m_NpcBodies;

    /// reuse the Y-sort list across frames to avoid allocations.
    std::vector<Drawable> m_RenderList;

    /// reuse one light-pool buffer for the mutually exclusive flat and 3D paths.
    skyDraw::LightPoolList m_LightPoolScratch;

    /// Select RenderFrame3D through the world3d console command.
    bool m_World3DEnabled = false;

    cameraRig::Preset m_CameraPreset = cameraRig::Preset::DS;
    /// orbit angles in radians.
    float m_CameraYaw = 0.0f;
    float m_CameraPitch = cameraRig::DS_PITCH_RADIANS;
    /// Start drag deltas on the second frame to avoid jumping from a stale cursor position.
    bool m_CameraDragActive = false;
    glm::dvec2 m_CameraDragCursor{0.0};

    DialogueUiState m_DialogueUi;
    DialogueManager m_DialogueManager;
    GameStateManager m_GameState;

    KeyToggle<GLFW_KEY_Z> m_KeyZ;
    KeyToggle<GLFW_KEY_SPACE> m_KeySpaceFreeCamera;
    KeyToggle<GLFW_KEY_B> m_KeyB;
    /// Debug corner-cut toggle; appearance copying uses console commands.
    KeyToggle<GLFW_KEY_X> m_KeyX;
    KeyToggle<GLFW_KEY_F> m_KeyF;

    KeyToggle<GLFW_KEY_F12> m_KeyConsole;

    KeyToggle<GLFW_KEY_UP, GLFW_KEY_W> m_KeyDialogueUp;
    KeyToggle<GLFW_KEY_DOWN, GLFW_KEY_S> m_KeyDialogueDown;
    KeyToggle<GLFW_KEY_ENTER> m_KeyDialogueEnterTree;
    KeyToggle<GLFW_KEY_SPACE> m_KeyDialogueSpaceTree;
    KeyToggle<GLFW_KEY_ESCAPE> m_KeyDialogueEscapeTree;
    KeyToggle<GLFW_KEY_ENTER> m_KeyDialogueEnter;
    KeyToggle<GLFW_KEY_SPACE> m_KeyDialogueSpace;
    KeyToggle<GLFW_KEY_ESCAPE> m_KeyDialogueEscape;

    // menu toggles keep edge state separate from dialogue toggles.
    KeyToggle<GLFW_KEY_UP, GLFW_KEY_W> m_KeyMenuUp;
    KeyToggle<GLFW_KEY_DOWN, GLFW_KEY_S> m_KeyMenuDown;
    KeyToggle<GLFW_KEY_LEFT, GLFW_KEY_A> m_KeyMenuLeft;
    KeyToggle<GLFW_KEY_RIGHT, GLFW_KEY_D> m_KeyMenuRight;
    KeyToggle<GLFW_KEY_ENTER, GLFW_KEY_SPACE> m_KeyMenuConfirm;
    KeyToggle<GLFW_KEY_ESCAPE> m_KeyEscape;

    GameMode m_GameMode = GameMode::Title;
    MenuLogic::ItemList m_TitleMenu;
    MenuLogic::ItemList m_PauseMenu;
    bool m_ConfirmOverwriteShown = false;
    MenuLogic::ConfirmPrompt m_ConfirmPrompt;
    /**
     * @brief Stationary mouse hover must not override keyboard selection; confirm on the
     * left-button press edge.
     */
    double m_MenuLastMouseX = -1.0;
    double m_MenuLastMouseY = -1.0;
    bool m_MenuMouseLeftPrev = false;
    /// Clear title ambience on first console open; reset only when loading the title world.
    bool m_TitleAmbientCleared = false;
    int m_DefaultMapWidth = 64;
    int m_DefaultMapHeight = 48;
    /// manifest character order cached for default character selection.
    std::vector<CharacterType> m_ConfiguredCharacters;

    /**
     * @fn void Game::PumpConsoleKeys()
     * @brief Forward key press edges while the console has focus.
     * @author Alex (<https://github.com/lextpf>)
     */
    void PumpConsoleKeys();

    Console m_Console{*this};
    /// press-edge latch for suggestion dropdown clicks.
    bool m_ConsoleMouseLeftPrev = false;
};
