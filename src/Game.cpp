#ifdef _WIN32
#define NOMINMAX
#endif

#include "Game.hpp"

#include "AmbienceConfig.hpp"
#include "AnimationState.hpp"
#include "CharacterConstants.hpp"
#include "CharacterKinematics.hpp"
#include "CollisionGeometry.hpp"
#include "DrawTracer.hpp"
#include "Elevation.hpp"
#include "ElevationAxis.hpp"
#include "Facing.hpp"
#include "Identity.hpp"
#include "Logger.hpp"
#include "MathConstants.hpp"
#include "MathUtils.hpp"
#include "Motor.hpp"
#include "NpcAiSystem.hpp"
#include "NpcIdle.hpp"
#include "NpcRender.hpp"
#include "OpenGLRenderer.hpp"
#include "Patrol.hpp"
#include "PatrolRoute.hpp"
#include "PlayerModes.hpp"
#include "PlayerMovementSystem.hpp"
#include "PlayerSprite.hpp"
#include "PlayerSystem.hpp"
#include "PostFXParams.hpp"
#include "ProjectManifest.hpp"
#include "RendererFactory.hpp"
#include "Speed.hpp"
#include "Transform.hpp"
#include "Version.hpp"
#include "WorldLightPools.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#undef DrawText
#undef near
#undef far
#pragma comment(lib, "winmm.lib")
#endif

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Game";
constexpr float HORIZON_SCALE_BASE = 0.6f;
constexpr float HORIZON_SCALE_TILT_RANGE = 0.15f;
constexpr float DEBUG_TEXT_MARGIN = 12.0f;
constexpr float DEBUG_HUD_ALPHA = 0.6f;
constexpr float DEBUG_HUD_ALPHA_DIM = 0.5f;
constexpr float SEAM_FIX_OVERLAP = 0.1f;

std::string ToLowerCopy(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

RendererAPI RendererApiFromManifestName(const std::string& name)
{
    return ToLowerCopy(name) == "vulkan" ? RendererAPI::Vulkan : RendererAPI::OpenGL;
}

const char* RendererApiName(RendererAPI api)
{
    return api == RendererAPI::Vulkan ? "Vulkan" : "OpenGL";
}

void PrintManifestDiagnostics(const ManifestValidationResult& result)
{
    for (const ManifestDiagnostic& diagnostic : result.diagnostics)
    {
        std::string fieldSuffix =
            diagnostic.fieldPath.empty() ? std::string() : " [" + diagnostic.fieldPath + "]";
        if (diagnostic.severity == ManifestDiagnosticSeverity::Error)
        {
            Logger::ErrorF(
                LOG_SUBSYSTEM, "project manifest{}: {}", fieldSuffix, diagnostic.message);
        }
        else
        {
            Logger::WarnF(LOG_SUBSYSTEM, "project manifest{}: {}", fieldSuffix, diagnostic.message);
        }
    }
}
}  // namespace

Game::Game() = default;

Game::~Game()
{
    Shutdown();
}

bool Game::Initialize()
{
    // Publish borrowed services for the registry lifetime.
    m_World.ctx().insert_or_assign(
        WorldServices{&m_TextureStore, &m_DialogueStore, &m_Assets, &m_NpcRng, &m_GameState});

    // create the player before loading a world; bind its sprite sheets after publishing the
    // services.
    m_PlayerEntity = EntityStore::SpawnPlayer(m_World);

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 1: Initializing GLFW...");

    if (!glfwInit())
    {
        Logger::Error(LOG_SUBSYSTEM, "Failed to initialize GLFW");
        return false;
    }
    m_GlfwInitialized = true;

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 2: Loading project manifest...");

    ManifestValidationResult manifestResult;
    ProjectManifest manifest = ProjectManifest::LoadDefaultOrFallback(manifestResult);
    PrintManifestDiagnostics(manifestResult);
    if (manifestResult.HasErrors())
    {
        Logger::Error(LOG_SUBSYSTEM, "Project manifest validation failed; aborting startup.");
        return false;
    }

    Logger::InfoF(
        LOG_SUBSYSTEM,
        "Project manifest: {}",
        manifest.loadedFromFile ? manifest.sourcePath.string() : std::string("built-in defaults"));

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 3: Selecting Renderer API...");

    m_RendererAPI = RendererApiFromManifestName(manifest.startupRenderer);
    m_FontCandidates = manifest.ResolvePathStrings(manifest.fonts);
    m_SaveMapPath = manifest.defaultMap.empty() ? "rift.save.json"
                                                : manifest.ResolvePathString(manifest.defaultMap);

    Logger::InfoF(LOG_SUBSYSTEM,
                  "Renderer API: {} (switch with the console command 'renderer.set "
                  "opengl|vulkan')",
                  RendererApiName(m_RendererAPI));
    Logger::Info(LOG_SUBSYSTEM, "Available renderers: OpenGL, Vulkan");

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 4: Setting window hints...");

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    else if (m_RendererAPI == RendererAPI::Vulkan)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 5: Creating GLFW window...");

    m_Window =
        glfwCreateWindow(m_ScreenWidth, m_ScreenHeight, "rift " RIFT_VERSION, nullptr, nullptr);
    if (!m_Window)
    {
        Logger::Error(LOG_SUBSYSTEM, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 6: Setting window callbacks...");

    glfwSetWindowUserPointer(m_Window, this);

    glfwSetScrollCallback(m_Window, ScrollCallback);
    glfwSetCharCallback(m_Window, CharCallback);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetWindowRefreshCallback(m_Window, WindowRefreshCallback);

    SetDebugDrawSleep(m_Window, false);

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 7: Creating Renderer...");

    m_Renderer = CreateRenderer(m_RendererAPI, m_Window);
    if (!m_Renderer)
    {
        Logger::Error(LOG_SUBSYSTEM, "Failed to create Renderer");
        glfwTerminate();
        return false;
    }
    m_Renderer->SetFontCandidates(m_FontCandidates);

    Logger::Info(LOG_SUBSYSTEM, "Initialize() step 8: Renderer created successfully");

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glfwMakeContextCurrent(m_Window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            Logger::Error(LOG_SUBSYSTEM, "Failed to initialize GLAD");
            m_Renderer->Shutdown();
            m_Renderer.reset();
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            glfwTerminate();
            return false;
        }
        Texture::AdvanceOpenGLContextGeneration();

        glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glfwSwapInterval(0);
    }

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        Logger::InfoF(
            LOG_SUBSYSTEM, "OpenGL: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        Logger::InfoF(LOG_SUBSYSTEM,
                      "GLSL: {}",
                      reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    }
    else
    {
        // TODO: report Vulkan device/driver info via renderer instead of calling glGetString().
    }

    Logger::Info(LOG_SUBSYSTEM, "About to call Renderer->Init()...");
    if (!m_Renderer->Init())
    {
        Logger::Error(LOG_SUBSYSTEM,
                      "Renderer->Init() failed; aborting startup rather than shipping a black "
                      "frame.");
        m_Renderer->Shutdown();
        m_Renderer.reset();
        return false;
    }
    Logger::Info(LOG_SUBSYSTEM, "Renderer->Init() completed successfully");

    // drivers may reset swap interval during initialization.
    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glfwSwapInterval(0);
    }

    m_Renderer->SetViewport(0, 0, m_ScreenWidth, m_ScreenHeight);

    float initWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float initWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    m_Renderer->SetViewSize({initWorldWidth, initWorldHeight});
    glm::mat4 projection = CameraController::GetOrthoProjection(initWorldWidth, initWorldHeight);
    m_Renderer->SetProjection(projection);

    std::vector<std::string> tilesetPaths = manifest.ResolvePathStrings(manifest.tilesets);
    bool loaded =
        m_Tilemap.LoadCombinedTilesets(tilesetPaths, manifest.tileWidth, manifest.tileHeight);
    if (!loaded)
    {
        Logger::Error(LOG_SUBSYSTEM,
                      "Failed to load combined tileset from project manifest. Tried:");
        for (const auto& path : tilesetPaths)
        {
            Logger::ErrorF(LOG_SUBSYSTEM, "    {}", path);
        }
        return false;
    }

    std::vector<std::string> npcSpritePaths = manifest.ResolvePathStrings(manifest.npcSprites);
    for (const std::string& npcSpritePath : npcSpritePaths)
    {
        m_Assets.SetNpcAsset(NpcType::FromSpritePath(npcSpritePath), npcSpritePath);
    }
    m_Editor.Initialize(npcSpritePaths);

    // cache manifest values for subsequent world loads.
    m_DefaultMapWidth = manifest.defaultMapWidth;
    m_DefaultMapHeight = manifest.defaultMapHeight;

    m_ConfiguredCharacters.clear();
    for (const auto& [characterName, character] : manifest.playerCharacters)
    {
        std::optional<CharacterType> characterType =
            EnumTraits<CharacterType>::FromString(characterName);
        if (!characterType.has_value())
        {
            continue;
        }

        m_ConfiguredCharacters.push_back(*characterType);
        for (const auto& [spriteType, path] : character.sprites)
        {
            m_Assets.SetCharacterAsset(
                *characterType, spriteType, manifest.ResolvePathString(path));
        }
    }

    m_LastFrameTime = static_cast<float>(glfwGetTime());

    // Load particle sprites now; the title or gameplay world binds its zone list later.
    m_Particles.LoadTextures(m_TextureStore, manifest);
    m_Particles.SetTileSize(m_Tilemap.GetTileWidth(), m_Tilemap.GetTileHeight());
    m_Particles.SetMaxParticlesPerZone(50);

    m_TimeManager.Initialize();
    const auto auroraSprite = manifest.particleSprites.find("aurora");
    m_SkyRenderer.Initialize(m_TextureStore,
                             auroraSprite != manifest.particleSprites.end()
                                 ? manifest.ResolvePathString(auroraSprite->second)
                                 : std::string());

    m_DialogueManager.Initialize(&m_GameState);

    // Load the title last so its world settings override subsystem defaults.
    LoadTitleScreenWorld();

    float camWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float camWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float mapWidth = static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth());
    float mapHeight = static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight());
    Logger::InfoF(LOG_SUBSYSTEM,
                  "Map size: {}x{} tiles = {}x{} pixels",
                  m_Tilemap.GetMapWidth(),
                  m_Tilemap.GetMapHeight(),
                  mapWidth,
                  mapHeight);
    Logger::InfoF(LOG_SUBSYSTEM,
                  "Camera view: {}x{} pixels ({} tiles wide, {} tiles tall)",
                  camWorldWidth,
                  camWorldHeight,
                  m_TilesVisibleWidth,
                  m_TilesVisibleHeight);
    Logger::InfoF(LOG_SUBSYSTEM,
                  "Camera position: ({}, {})",
                  m_Camera.GetState().position.x,
                  m_Camera.GetState().position.y);

    return true;
}

void Game::Run()
{
#ifdef _WIN32
    // Request 1 ms sleep resolution for the FPS limiter; restore it when Run exits.
    struct TimerPeriodGuard
    {
        TimerPeriodGuard() { timeBeginPeriod(1); }
        ~TimerPeriodGuard() { timeEndPeriod(1); }
        TimerPeriodGuard(const TimerPeriodGuard&) = delete;
        TimerPeriodGuard& operator=(const TimerPeriodGuard&) = delete;
    } timerGuard;
#endif

    try
    {
        while (!glfwWindowShouldClose(m_Window))
        {
            // include event polling in the frame deadline.
            double frameStartTime = glfwGetTime();
            float deltaTime = static_cast<float>(frameStartTime) - m_LastFrameTime;
            m_LastFrameTime = static_cast<float>(frameStartTime);

            // GLFW updates cached input only while polling.
            glfwPollEvents();

            static constexpr float MAX_DELTA_TIME = 0.1f;
            deltaTime = std::min(deltaTime, MAX_DELTA_TIME);

            try
            {
                ProcessInput(deltaTime);
                Update(deltaTime);
                Render();
            }
            catch (const std::exception& e)
            {
                Logger::ErrorF(LOG_SUBSYSTEM, "Exception in game loop: {}", e.what());
                break;
            }
            catch (...)
            {
                Logger::Error(LOG_SUBSYSTEM, "Unknown exception in game loop");
                break;
            }

            // sleep for most of the interval, then yield near the deadline to reduce limiter
            // jitter.
            if (m_Fps.targetFps > 0.0f)
            {
                double targetFrameTime = 1.0 / static_cast<double>(m_Fps.targetFps);
                double elapsed = glfwGetTime() - frameStartTime;
                double remaining = targetFrameTime - elapsed;

                if (remaining > 0.0)
                {
                    using clock = std::chrono::steady_clock;
                    const auto sleepDuration = std::chrono::duration_cast<clock::duration>(
                        std::chrono::duration<double>(remaining));
                    const auto frameDeadline = clock::now() + sleepDuration;

                    // Leave 2 ms for spin-yield so sleep overshoot does not miss short frame
                    // deadlines.
                    constexpr auto spinThreshold = std::chrono::milliseconds(2);
                    while (true)
                    {
                        const auto now = clock::now();
                        if (now >= frameDeadline)
                            break;

                        const auto timeLeft = frameDeadline - now;
                        if (timeLeft > spinThreshold)
                        {
                            std::this_thread::sleep_for(timeLeft - spinThreshold);
                        }
                        else
                        {
                            std::this_thread::yield();
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        Logger::ErrorF(LOG_SUBSYSTEM, "Exception in Run(): {}", e.what());
    }
    catch (...)
    {
        Logger::Error(LOG_SUBSYSTEM, "Unknown exception in Run()");
    }
}

void Game::Update(float deltaTime)
{
    // synchronous resize callbacks must not render partially updated state.
    struct UpdateGuard
    {
        bool& flag;
        UpdateGuard(bool& f)
            : flag(f)
        {
            flag = true;
        }
        ~UpdateGuard() { flag = false; }
    } updateGuard(m_IsUpdating);

    // Clear title ambience once per title session when the console opens.
    if (m_GameMode == GameMode::Title && m_Console.IsOpen() && !m_TitleAmbientCleared)
    {
        m_TitleAmbientCleared = true;
        m_Particles.SetZones(nullptr);
        m_Particles.Clear();
        if (auto* zones = m_Tilemap.GetParticleZonesMutable())
        {
            zones->clear();
        }
        m_TimeManager.SetWeather(WeatherState::Clear);
        // retain full intensity so subsequent weather commands are visible.
        m_TimeManager.SetWeatherIntensity(1.0f);
    }

    m_Fps.frameCount++;
    m_Fps.updateTimer += deltaTime;
    if (m_Fps.updateTimer >= 1.0f)
    {
        m_Fps.currentFps = m_Fps.frameCount / m_Fps.updateTimer;
        m_Fps.currentDrawCalls =
            (m_Fps.frameCount > 0) ? m_Fps.drawCallAccumulator / m_Fps.frameCount : 0;
        m_Fps.frameCount = 0;
        m_Fps.updateTimer = 0.0f;
        m_Fps.drawCallAccumulator = 0;
    }

    m_Fps.consoleTimer += deltaTime;
    if (m_Fps.consoleTimer >= 1.0f)
    {
        const char* renderer = (m_RendererAPI == RendererAPI::OpenGL) ? "OpenGL" : "Vulkan";
        float frameTimeMs = (m_Fps.currentFps > 0) ? (1000.0f / m_Fps.currentFps) : 0.0f;

        m_Fps.consoleTimer = 0.0f;
    }

    if (m_PendingWindowSnap)
    {
        m_ResizeSnapTimer -= deltaTime;
        if (m_ResizeSnapTimer <= 0.0f)
        {
            SnapWindowToTileBoundaries();
        }
    }

    // Paused freezes simulation; Title runs only cosmetic updates.
    if (m_GameMode == GameMode::Paused)
    {
        return;
    }
    const bool isPlaying = (m_GameMode == GameMode::Playing);

    if (isPlaying)
    {
        PlayerSystem::Update(m_World, m_PlayerEntity, deltaTime);
        m_TimeManager.Update(deltaTime);
        m_WeatherDirector.Update(deltaTime, m_TimeManager);
    }
    else
    {
        // Keep the title hour fixed while manual weather overlays fade.
        m_TimeManager.UpdateWeatherEffects(deltaTime);
    }
    m_SkyRenderer.Update(deltaTime, m_TimeManager);

    // pixel extents match the render projection; truncated tile counts under-cover the view.
    const glm::vec2 particleCullCam = m_Camera.GetState().position;
    const glm::vec2 viewSize = VisibleWorldSizeZoomed();
    m_Particles.SetNightFactor(m_TimeManager.GetStarVisibility());
    // precipitation suppresses weather star visibility; include natural darkness for splash fading.
    m_Particles.SetSceneNightFactor(
        std::max(m_TimeManager.GetNaturalStarVisibility(), m_TimeManager.GetStarVisibility()));
    m_Particles.SetTimeOfDay(m_TimeManager.GetTimeOfDay());

    // Use the feet anchor for weather repulsion so it follows the collision body.
    m_Particles.SetPlayerPosition(m_World.get<Transform>(m_PlayerEntity).position);
    // endpoint definitions drive spawn streams; the blended definition supplies live-read effects.
    m_Particles.SetWind(m_WeatherDirector.GetWindDirection(), m_WeatherDirector.GetWindStrength());
    const WeatherDirector::SpawnStreams streams = m_WeatherDirector.GetSpawnStreams();
    m_Particles.SetWeatherTransition(streams.outgoing, streams.incoming, streams.weight);
    m_Particles.SetWeatherState(&m_TimeManager.GetEffectiveWeatherDefinition(),
                                m_TimeManager.GetWeatherIntensity());
    const float overlayBlend = m_TimeManager.GetOverlayBlend();
    m_Particles.SetWeatherOverlay(
        overlayBlend > 0.001f ? &GetWeatherDefinition(m_TimeManager.GetWeatherOverlay()) : nullptr,
        overlayBlend);
    m_Particles.Update(deltaTime, particleCullCam, viewSize);

    // Wrap postfx time to limit float precision loss.
    m_PostFXTime += deltaTime;
    if (m_PostFXTime > 86400.0f)
    {
        m_PostFXTime -= 86400.0f;
    }

    m_Tilemap.UpdateAnimations(deltaTime);

    if (!isPlaying)
    {
        return;
    }

    glm::vec2 playerPos = m_World.get<Transform>(m_PlayerEntity).position;

    if (m_DialogueUi.snap.active)
    {
        if (!HasDialogueNPC())
        {
            m_DialogueUi.snap.active = false;
        }
        else
        {
            const entt::entity npcE = FindNPCById(m_DialogueUi.npcId);
            m_DialogueUi.snap.timer += deltaTime;
            float duration = std::max(0.05f, m_DialogueUi.snap.duration);
            float t = std::clamp(m_DialogueUi.snap.timer / duration, 0.0f, 1.0f);
            float smoothT = t * t * (3.0f - 2.0f * t);

            glm::vec2 blendedPlayer =
                m_DialogueUi.snap.playerStart +
                (m_DialogueUi.snap.playerTarget - m_DialogueUi.snap.playerStart) * smoothT;
            glm::vec2 blendedNPC =
                m_DialogueUi.snap.npcStart +
                (m_DialogueUi.snap.npcTarget - m_DialogueUi.snap.npcStart) * smoothT;

            PlayerSystem::SetPositionRaw(m_World, m_PlayerEntity, blendedPlayer);
            m_World.get<Transform>(npcE).position = blendedNPC;
            m_World.get<NpcIdle>(npcE).isStopped = true;
            CharacterKinematics::ResetAnimation(m_World.get<AnimationState>(npcE));
            playerPos = blendedPlayer;

            if (t >= 1.0f)
            {
                if (m_DialogueUi.snap.hasPlayerTile)
                {
                    PlayerSystem::SetTilePosition(m_World,
                                                  m_PlayerEntity,
                                                  m_DialogueUi.snap.playerTileX,
                                                  m_DialogueUi.snap.playerTileY);
                }
                else
                {
                    PlayerSystem::SetPositionRaw(
                        m_World, m_PlayerEntity, m_DialogueUi.snap.playerTarget);
                }
                EntityStore::SetNpcTile(m_World.get<Transform>(npcE),
                                        m_World.get<Patrol>(npcE),
                                        m_World.get<PatrolRoute>(npcE),
                                        m_DialogueUi.snap.npcTileX,
                                        m_DialogueUi.snap.npcTileY,
                                        16,
                                        true);

                PlayerSystem::Stop(m_World, m_PlayerEntity);
                // Dialogue snapping bypasses movement probes; commit both participants support
                // here.
                CharacterKinematics::DerivePlane(m_World.get<Elevation>(m_PlayerEntity),
                                                 m_DialogueUi.snap.playerStart,
                                                 m_World.get<Transform>(m_PlayerEntity).position,
                                                 m_Tilemap);
                CharacterKinematics::DerivePlane(m_World.get<Elevation>(npcE),
                                                 m_DialogueUi.snap.npcStart,
                                                 m_World.get<Transform>(npcE).position,
                                                 m_Tilemap);
                m_World.get<Facing>(m_PlayerEntity).dir = m_DialogueUi.snap.playerFacing;
                m_World.get<Facing>(npcE).dir = m_DialogueUi.snap.npcFacing;
                m_World.get<NpcIdle>(npcE).isStopped = true;
                CharacterKinematics::ResetAnimation(m_World.get<AnimationState>(npcE));

                bool startedTree = false;
                if (m_DialogueUi.snap.prefersTree)
                {
                    startedTree = m_DialogueManager.StartDialogue(npcE, m_World);
                    if (startedTree)
                    {
                        m_DialogueUi.page = 0;
                        m_DialogueUi.boxFadeTimer = 0.0f;
                        m_DialogueUi.charReveal = 0.0f;
                    }
                }
                if (!startedTree)
                {
                    m_DialogueUi.inDialogue = true;
                    m_DialogueUi.text = m_DialogueUi.snap.fallbackText;
                }

                m_DialogueUi.snap.active = false;
                playerPos = m_World.get<Transform>(m_PlayerEntity).position;
            }
        }
    }

    if (m_DialogueManager.IsActive())
    {
        m_DialogueUi.boxFadeTimer += deltaTime;
        if (m_DialogueUi.charReveal >= 0.0f)
            m_DialogueUi.charReveal += 35.0f * deltaTime;
    }

    // Release a dialogue speaker whose stable id no longer resolves.
    if (m_DialogueUi.npcId != 0 && !HasDialogueNPC())
    {
        m_DialogueUi.npcId = 0;
    }

    // freeze the active speaker during both snap and dialogue; id 0 leaves every NPC eligible.
    const bool inAnyDialogue =
        m_DialogueUi.inDialogue || m_DialogueManager.IsActive() || m_DialogueUi.snap.active;
    const std::uint64_t frozenNpcId = inAnyDialogue ? m_DialogueUi.npcId : 0;
    NpcAiSystem::UpdateAll(
        m_World,
        m_Tilemap,
        CharacterCollisionBody{
            playerPos, CharacterKinematics::GetSupport(m_World.get<Elevation>(m_PlayerEntity))},
        m_NpcRng,
        frozenNpcId,
        deltaTime);

    m_Editor.Update(deltaTime, MakeEditorContext());

    float baseWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float baseWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    float worldWidth = baseWorldWidth / m_Camera.GetZoom();
    float worldHeight = baseWorldHeight / m_Camera.GetZoom();

    bool arrowUp = glfwGetKey(m_Window, GLFW_KEY_UP) == GLFW_PRESS;
    bool arrowDown = glfwGetKey(m_Window, GLFW_KEY_DOWN) == GLFW_PRESS;
    bool arrowLeft = glfwGetKey(m_Window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool arrowRight = glfwGetKey(m_Window, GLFW_KEY_RIGHT) == GLFW_PRESS;

    // do not pan the camera while UI owns the arrow keys.
    if (m_Editor.IsActive() && m_Editor.IsShowTilePicker())
    {
        arrowUp = arrowDown = arrowLeft = arrowRight = false;
    }
    if (m_DialogueManager.IsActive() || m_DialogueUi.inDialogue || m_DialogueUi.snap.active)
    {
        arrowUp = arrowDown = arrowLeft = arrowRight = false;
    }
    if (m_Console.IsOpen())
    {
        arrowUp = arrowDown = arrowLeft = arrowRight = false;
    }

    bool wasdPressed = (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS ||
                        glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS ||
                        glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS ||
                        glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS);

    // typing WASD must not change camera follow state.
    if (m_Console.IsOpen())
    {
        wasdPressed = false;
    }

    // follow feet while moving and the tile center while idle.
    glm::vec2 playerCamPos = m_World.get<Transform>(m_PlayerEntity).position;
    glm::vec2 playerVisualCenter =
        glm::vec2(playerCamPos.x, playerCamPos.y - CharacterConstants::HITBOX_HEIGHT);
    glm::vec2 smoothTarget = playerVisualCenter - glm::vec2(worldWidth / 2.0f, worldHeight / 2.0f);

    glm::vec2 playerBottomTileCenter = PlayerMovementSystem::CurrentTileCenter(
        m_World.get<Transform>(m_PlayerEntity).position, 16.0f);
    glm::vec2 tileVisualCenter = glm::vec2(
        playerBottomTileCenter.x, playerBottomTileCenter.y - CharacterConstants::HITBOX_HEIGHT);
    glm::vec2 gridTarget = tileVisualCenter - glm::vec2(worldWidth / 2.0f, worldHeight / 2.0f);

    glm::vec2 snappedTarget = wasdPressed ? smoothTarget : gridTarget;

    CameraUpdateParams camParams;
    camParams.deltaTime = deltaTime;
    camParams.playerFollowTarget = snappedTarget;
    camParams.playerMoving = wasdPressed;
    camParams.playerVelocity = m_World.get<Motor>(m_PlayerEntity).velocity;
    camParams.arrowUp = arrowUp;
    camParams.arrowDown = arrowDown;
    camParams.arrowLeft = arrowLeft;
    camParams.arrowRight = arrowRight;
    camParams.shiftHeld = (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                           glfwGetKey(m_Window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    camParams.baseWorldWidth = baseWorldWidth;
    camParams.baseWorldHeight = baseWorldHeight;
    camParams.mapPixelWidth =
        static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth());
    camParams.mapPixelHeight =
        static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight());
    camParams.skipMapClamping = m_Editor.IsActive() && m_Camera.IsFreeMode();
    camParams.tileWidth = m_Tilemap.GetTileWidth();
    camParams.tileHeight = m_Tilemap.GetTileHeight();
    m_Camera.Update(camParams);

    // assign overlap stops after player and NPC positions settle for this frame.
    NpcAiSystem::ApplyPlayerOverlapStop(
        m_World,
        CharacterCollisionBody{
            playerPos, CharacterKinematics::GetSupport(m_World.get<Elevation>(m_PlayerEntity))});
}

void Game::Render()
{
    // resize callbacks can render synchronously during Update; do not re-enter either frame stage.
    if (m_IsRendering || m_IsUpdating)
    {
        return;
    }
    struct RenderGuard
    {
        bool& flag;
        RenderGuard(bool& f)
            : flag(f)
        {
            flag = true;
        }
        ~RenderGuard() { flag = false; }
    } renderGuard(m_IsRendering);

    if (m_GameMode == GameMode::Title)
    {
        RenderTitleFrame();
        return;
    }

    if (m_World3DEnabled)
    {
        RenderFrame3D();
        return;
    }

    if (IsDebugDrawSleepEnabled())
    {
        ResetDebugDrawCallIndex();
        Logger::Debug(LOG_SUBSYSTEM, "===== FRAME START =====");
    }

    m_Renderer->BeginFrame();

    // render the world into the scene target; UI follows the composite so its text remains sharp.
    m_Renderer->BeginScene();

    DrawTracer::Mark("== gameplay frame ==", m_Renderer->GetDrawCallCount());

    glm::vec3 skyColor = m_TimeManager.GetSkyColor();
    m_Renderer->Clear(skyColor.r, skyColor.g, skyColor.b, 1.0f);

    // derive view dimensions from actual pixels; tile counts truncate partial tiles during
    // resizing.
    const glm::vec2 world = VisibleWorldSize();
    float worldWidth = world.x;
    float worldHeight = world.y;

    m_Renderer->SetAmbientColor(m_TimeManager.GetAmbientColor());

    float zoomedWidth = worldWidth / m_Camera.GetState().zoom;
    float zoomedHeight = worldHeight / m_Camera.GetState().zoom;
    m_Renderer->SetViewSize({zoomedWidth, zoomedHeight});
    glm::mat4 projection = CameraController::GetOrthoProjection(zoomedWidth, zoomedHeight);
    m_Renderer->SetProjection(projection);

    // Snap OpenGL rendering to pixels; retain the unsnapped camera for stable culling.
    const glm::vec2 originalCamera = m_Camera.GetState().position;
    glm::vec2 renderCam = originalCamera;
    glm::vec2 renderSize(zoomedWidth, zoomedHeight);
    glm::vec2 cullCam = originalCamera;
    glm::vec2 cullSize(zoomedWidth, zoomedHeight);
    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        const float pixelStepX = zoomedWidth / static_cast<float>(m_ScreenWidth);
        const float pixelStepY = zoomedHeight / static_cast<float>(m_ScreenHeight);
        auto snapToPixel = [](float value, float step)
        { return (step > 0.0f) ? std::round(value / step) * step : value; };
        renderCam.x = snapToPixel(originalCamera.x, pixelStepX);
        renderCam.y = snapToPixel(originalCamera.y, pixelStepY);
    }

    m_Camera.GetState().position = renderCam;

    DrawTracer::Mark("section: BackgroundLayers", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderBackgroundLayers(*m_Renderer, renderCam, renderSize, cullCam, cullSize);

    DrawTracer::Mark("section: BackgroundLayersNoProjection", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderBackgroundLayersNoProjection(
        *m_Renderer, renderCam, renderSize, cullCam, cullSize);

    const auto& depthSortedTiles = m_Tilemap.GetVisibleDepthSortedTiles(cullCam, cullSize);

    // Keep characters atomic and apply support constraints only inside their elevation footprint.
    m_RenderList.clear();
    size_t estimatedSize = depthSortedTiles.size() + EntityStore::Count(m_World) + 1;
    if (m_RenderList.capacity() < estimatedSize)
    {
        m_RenderList.reserve(estimatedSize);
    }

    int tileW = m_Tilemap.GetTileWidth();
    int tileH = m_Tilemap.GetTileHeight();
    for (const auto& tile : depthSortedTiles)
    {
        Drawable item;
        item.cls = DrawableClass::Tile;
        item.phase = TileDrawablePhase(tile);
        item.sortY = tile.anchorY;
        item.supportHeight = tile.supportHeight;
        item.surfaceRegionId = tile.surfaceRegionId;
        item.isYSortMinus = tile.ySortMinus;
        item.tieBias = TIE_TILE;
        item.tile = tile;
        m_RenderList.push_back(item);
    }

    // Keep each NPC atomic in the queue so no tile can sort between its two sprite halves.
    for (const entt::entity entity : EntityStore::Entities(m_World))
    {
        const Transform& transform = m_World.get<Transform>(entity);
        const Elevation& elevation = m_World.get<Elevation>(entity);
        const glm::vec2 npcPos = transform.position;

        AddNpcDrawable(m_RenderList,
                       m_World,
                       entity,
                       npcPos,
                       elevation.surface,
                       static_cast<float>(elevation.plane),
                       m_Tilemap.GetElevationRegionIdAtWorldPos(npcPos.x, npcPos.y),
                       TIE_NPC);
    }

    if (!m_Editor.IsActive() && m_GameMode != GameMode::Title)
    {
        glm::vec2 playerPos = m_World.get<Transform>(m_PlayerEntity).position;
        const Elevation& playerElevation = m_World.get<Elevation>(m_PlayerEntity);
        AddPlayerDrawable(m_RenderList,
                          m_World,
                          m_PlayerEntity,
                          playerPos,
                          playerElevation.surface,
                          static_cast<float>(playerElevation.plane),
                          m_Tilemap.GetElevationRegionIdAtWorldPos(playerPos.x, playerPos.y),
                          TIE_PLAYER);
    }

    // Preserve authored layer/Y order, then apply support relations only inside the relevant
    // elevation component.
    SortDrawables(m_RenderList);

    {
        char ysortLabel[64];
        std::snprintf(ysortLabel,
                      sizeof(ysortLabel),
                      "section: Y-sorted pass (%zu items)",
                      m_RenderList.size());
        DrawTracer::Mark(ysortLabel, m_Renderer->GetDrawCallCount());
    }
    for (const auto& item : m_RenderList)
    {
        if (item.cls == DrawableClass::Entity)
        {
            item.drawEntity(item, *m_Renderer, m_Camera.GetState().position);
        }
        else
        {
            m_Tilemap.RenderSingleTile(*m_Renderer,
                                       item.tile.x,
                                       item.tile.y,
                                       item.tile.layer,
                                       m_Camera.GetState().position);
        }
    }

    DrawTracer::Mark("section: ForegroundLayersNoProjection", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderForegroundLayersNoProjection(
        *m_Renderer, renderCam, renderSize, cullCam, cullSize);

    DrawTracer::Mark("section: Particles(noProjection)", m_Renderer->GetDrawCallCount());
    m_Particles.Render(*m_Renderer, m_Camera.GetState().position, true, false);

    DrawTracer::Mark("section: ForegroundLayers", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderForegroundLayers(*m_Renderer, renderCam, renderSize, cullCam, cullSize);

    DrawTracer::Mark("section: Particles(world)", m_Renderer->GetDrawCallCount());
    m_Particles.Render(*m_Renderer, m_Camera.GetState().position, false, false);

    // share light gating and alpha with the 3D path; placement remains path-specific.
    DrawTracer::Mark("section: WorldLights", m_Renderer->GetDrawCallCount());
    worldLights::Build(m_Tilemap.GetLights(),
                       m_TimeManager.GetTimeOfDay(),
                       m_TimeManager.GetStarVisibility(),
                       m_LightPoolScratch);
    for (const skyDraw::LightPool& pool : m_LightPoolScratch)
    {
        m_SkyRenderer.DrawLightPool(*m_Renderer,
                                    pool.centreWorld - renderCam - glm::vec2(pool.radius),
                                    glm::vec2(pool.radius * 2.0f),
                                    0.0f,
                                    pool.color,
                                    true);
    }

    // sky elements subtract cameraPos themselves and use the existing world projection.
    DrawTracer::Mark("section: Sky", m_Renderer->GetDrawCallCount());
    m_SkyRenderer.Render(*m_Renderer,
                         m_TimeManager,
                         m_Camera.GetState().position,
                         static_cast<int>(worldWidth),
                         static_cast<int>(worldHeight));

    {
        PostFXParams postFX;
        postFX.timeOfDay = m_TimeManager.GetTimeOfDay();
        postFX.nightFactor = m_TimeManager.GetStarVisibility();
        postFX.time = m_PostFXTime;
        postFX.postFXEnabled = m_PostFXEnabled;
        if (m_PostFXEnabled)
        {
            postFX.vignetteIntensity = ambience::VIGNETTE_INTENSITY;
            postFX.grainIntensity = ambience::GRAIN_INTENSITY;
            postFX.bloomIntensity = ambience::BLOOM_INTENSITY;
            postFX.gradingParams = ComputeGradingParams(postFX.timeOfDay, postFX.nightFactor);
        }
        else
        {
            postFX.vignetteIntensity = 0.0f;
            postFX.grainIntensity = 0.0f;
            postFX.bloomIntensity = 0.0f;
            postFX.saturation = 1.0f;
            // identity values back up the postFXEnabled gate if its uniform is absent.
        }

        DrawTracer::Mark("section: PostFX", m_Renderer->GetDrawCallCount());
        m_Renderer->EndSceneApplyPostFX(postFX);
    }

    DrawTracer::Mark("section: UI overlays", m_Renderer->GetDrawCallCount());

    if (m_Editor.IsActive() || m_Editor.IsDebugMode())
    {
        m_Editor.Render(MakeEditorContext());

        m_Renderer->SetProjection(projection);
    }

    m_Renderer->SetAmbientColor(glm::vec3(1.0f));

    if (m_DialogueUi.inDialogue)
    {
        RenderNPCHeadText();
    }

    if (m_DialogueManager.IsActive())
    {
        RenderDialogueTreeBox();
    }

    if (m_Editor.IsShowDebugInfo())
    {
        glm::mat4 uiProjection = glm::ortho(0.0f,
                                            static_cast<float>(m_ScreenWidth),
                                            static_cast<float>(m_ScreenHeight),
                                            0.0f,
                                            -1.0f,
                                            1.0f);
        m_Renderer->SetProjection(uiProjection);

        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %d", static_cast<int>(m_Fps.currentFps + 0.5f));

        glm::vec2 playerPos = m_World.get<Transform>(m_PlayerEntity).position;
        int playerTileX = static_cast<int>(std::floor(playerPos.x / m_Tilemap.GetTileWidth()));
        int playerTileY = static_cast<int>(std::floor(playerPos.y / m_Tilemap.GetTileHeight()));

        char posText[64];
        snprintf(posText, sizeof(posText), "Pos: (%.1f, %.1f)", playerPos.x, playerPos.y);

        char tileText[32];
        snprintf(tileText, sizeof(tileText), "Tile: (%d, %d)", playerTileX, playerTileY);

        float lineHeight = 28.0f;
        float currentLine = 0.0f;
        m_Renderer->DrawText(fpsText,
                             glm::vec2(DEBUG_TEXT_MARGIN, 32.0f + lineHeight * currentLine++),
                             1.0f,
                             glm::vec3(1.0f, 1.0f, 0.0f),
                             2.0f,
                             DEBUG_HUD_ALPHA);
        m_Renderer->DrawText(posText,
                             glm::vec2(DEBUG_TEXT_MARGIN, 32.0f + lineHeight * currentLine++),
                             1.0f,
                             glm::vec3(1.0f, 1.0f, 0.0f),
                             2.0f,
                             DEBUG_HUD_ALPHA);
        m_Renderer->DrawText(tileText,
                             glm::vec2(DEBUG_TEXT_MARGIN, 32.0f + lineHeight * currentLine++),
                             1.0f,
                             glm::vec3(1.0f, 1.0f, 0.0f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        char particlesText[64];
        snprintf(particlesText,
                 sizeof(particlesText),
                 "Particles: %zu live / %zu drawn",
                 m_Particles.GetParticles().size(),
                 m_Particles.GetLastDrawnCount());
        m_Renderer->DrawText(particlesText,
                             glm::vec2(DEBUG_TEXT_MARGIN, 32.0f + lineHeight * currentLine++),
                             1.0f,
                             glm::vec3(1.0f, 1.0f, 0.0f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        auto activeQuests = m_GameState.GetActiveQuests();
        if (!activeQuests.empty())
        {
            currentLine += 0.5f;
            glm::vec3 questGold(1.0f, 0.85f, 0.2f);
            glm::vec3 descColor(0.9f, 0.75f, 0.5f);

            for (const auto& quest : activeQuests)
            {
                // "wolf_quest" -> "wolf quest".
                std::string displayName = quest;
                for (size_t i = 0; i < displayName.size(); ++i)
                {
                    if (displayName[i] == '_')
                    {
                        displayName[i] = ' ';
                        if (i + 1 < displayName.size())
                        {
                            displayName[i + 1] =
                                static_cast<char>(std::toupper(displayName[i + 1]));
                        }
                    }
                }
                if (!displayName.empty())
                {
                    displayName[0] = static_cast<char>(std::toupper(displayName[0]));
                }

                float questTextX = 52.0f;
                glm::vec3 exclamYellow(1.0f, 1.0f, 0.0f);
                m_Renderer->DrawText(">!<",
                                     glm::vec2(DEBUG_TEXT_MARGIN, 32.0f + lineHeight * currentLine),
                                     1.0f,
                                     exclamYellow,
                                     2.0f,
                                     DEBUG_HUD_ALPHA);
                m_Renderer->DrawText(displayName,
                                     glm::vec2(questTextX, 32.0f + lineHeight * currentLine++),
                                     1.0f,
                                     questGold,
                                     2.0f,
                                     DEBUG_HUD_ALPHA);

                std::string description = m_GameState.GetQuestDescription(quest);
                if (!description.empty())
                {
                    if (description.size() > 20)
                    {
                        size_t cutPos = 20;
                        while (cutPos < description.size() && description[cutPos] != ' ')
                            ++cutPos;
                        description = description.substr(0, cutPos) + "...";
                    }
                    m_Renderer->DrawText(description,
                                         glm::vec2(questTextX, 32.0f + lineHeight * currentLine++),
                                         0.8f,
                                         descColor,
                                         2.0f,
                                         DEBUG_HUD_ALPHA_DIM);
                }
            }
        }

        const char* rendererName = (m_RendererAPI == RendererAPI::OpenGL) ? "OpenGL" : "Vulkan";
        float rightMargin = static_cast<float>(m_ScreenWidth) - DEBUG_TEXT_MARGIN;

        char rendererText[32];
        snprintf(rendererText, sizeof(rendererText), "%s", rendererName);
        float textWidth = m_Renderer->GetTextWidth(rendererText, 1.0f);
        m_Renderer->DrawText(rendererText,
                             glm::vec2(rightMargin - textWidth, 32.0f),
                             1.0f,
                             glm::vec3(1.0f, 0.3f, 0.3f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        char resText[32];
        snprintf(resText, sizeof(resText), "%dx%d", m_ScreenWidth, m_ScreenHeight);
        textWidth = m_Renderer->GetTextWidth(resText, 1.0f);
        m_Renderer->DrawText(resText,
                             glm::vec2(rightMargin - textWidth, 32.0f + lineHeight),
                             1.0f,
                             glm::vec3(1.0f, 0.3f, 0.3f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        char frameTimeText[32];
        float frameTimeMs = (m_Fps.currentFps > 0) ? (1000.0f / m_Fps.currentFps) : 0.0f;
        snprintf(frameTimeText, sizeof(frameTimeText), "%.2fms", frameTimeMs);
        textWidth = m_Renderer->GetTextWidth(frameTimeText, 1.0f);
        m_Renderer->DrawText(frameTimeText,
                             glm::vec2(rightMargin - textWidth, 32.0f + lineHeight * 2),
                             1.0f,
                             glm::vec3(1.0f, 0.3f, 0.3f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        char zoomText[32];
        snprintf(zoomText, sizeof(zoomText), "Zoom: %.1fx", m_Camera.GetState().zoom);
        textWidth = m_Renderer->GetTextWidth(zoomText, 1.0f);
        m_Renderer->DrawText(zoomText,
                             glm::vec2(rightMargin - textWidth, 32.0f + lineHeight * 3),
                             1.0f,
                             glm::vec3(1.0f, 0.3f, 0.3f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        char drawCallText[32];
        snprintf(drawCallText, sizeof(drawCallText), "Draws: %d", m_Fps.currentDrawCalls);
        textWidth = m_Renderer->GetTextWidth(drawCallText, 1.0f);
        m_Renderer->DrawText(drawCallText,
                             glm::vec2(rightMargin - textWidth, 32.0f + lineHeight * 4),
                             1.0f,
                             glm::vec3(1.0f, 0.3f, 0.3f),
                             2.0f,
                             DEBUG_HUD_ALPHA);

        // EndFrame can flush pending world batches.
        m_Renderer->SetProjection(projection);
    }

    // Restore world projection after the screen-space footer.
    if (m_GameMode == GameMode::Playing && !m_Editor.IsActive())
    {
        RenderVersionFooter();
        m_Renderer->SetProjection(projection);
    }

    // Editor overlays already show structure anchors.
    if (m_Editor.IsShowNoProjectionAnchors() && !m_Editor.IsActive())
    {
        m_Editor.RenderNoProjectionAnchors(MakeEditorContext());
    }

    // pause UI stays below the console.
    if (m_GameMode == GameMode::Paused)
    {
        RenderPauseOverlay();
    }

    if (m_GameMode == GameMode::Title)
    {
        RenderTitleContent();
        if (m_ConfirmOverwriteShown)
        {
            RenderConfirmOverwritePrompt();
        }
    }

    // Console must remain above every layer.
    m_Console.Render(*m_Renderer, m_ScreenWidth, m_ScreenHeight);

    m_Renderer->EndFrame();

    // Restore the unsnapped camera for simulation.
    m_Camera.GetState().position = originalCamera;

    m_Fps.drawCallAccumulator += m_Renderer->GetDrawCallCount();

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        if (IsDebugDrawSleepEnabled())
        {
            Logger::Debug(LOG_SUBSYSTEM, "===== FRAME END =====");
        }
        glfwSwapBuffers(m_Window);
    }
}

void Game::Shutdown()
{
    // Run owns the Windows timer-period guard; shutdown here only releases renderer and window
    // resources.
    if (m_Renderer)
    {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }

    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    if (m_GlfwInitialized)
    {
        glfwTerminate();
        m_GlfwInitialized = false;
    }
}

bool Game::SwitchRenderer(RendererAPI api)
{
    // The APIs require incompatible GLFW hints, so switching also replaces the window and its GPU
    // Context.
    if (api == m_RendererAPI)
    {
        Logger::InfoF(
            LOG_SUBSYSTEM, "Already using {}", api == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
        return true;
    }

    if (!IsRendererAvailable(api))
    {
        Logger::ErrorF(LOG_SUBSYSTEM,
                       "Renderer API not available: {}",
                       api == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
        return false;
    }

    Logger::InfoF(LOG_SUBSYSTEM,
                  "Switching renderer from {} to {}...",
                  m_RendererAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan",
                  api == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");

    RendererAPI oldAPI = m_RendererAPI;

    if (m_Renderer)
    {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }

    int windowX = 0, windowY = 0;
    glfwGetWindowPos(m_Window, &windowX, &windowY);

    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    // Each failed attempt clears its partial window and renderer so rollback can retry.
    auto setupRendererForAPI = [&](RendererAPI targetAPI) -> bool
    {
        m_RendererAPI = targetAPI;

        glfwDefaultWindowHints();
        if (m_RendererAPI == RendererAPI::OpenGL)
        {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }
        else if (m_RendererAPI == RendererAPI::Vulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        m_Window =
            glfwCreateWindow(m_ScreenWidth, m_ScreenHeight, "rift " RIFT_VERSION, nullptr, nullptr);
        if (!m_Window)
        {
            Logger::ErrorF(LOG_SUBSYSTEM,
                           "Failed to create GLFW window for {}",
                           targetAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
            return false;
        }
        glfwSetWindowPos(m_Window, windowX, windowY);

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetScrollCallback(m_Window, ScrollCallback);
        glfwSetCharCallback(m_Window, CharCallback);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
        glfwSetWindowRefreshCallback(m_Window, WindowRefreshCallback);

        m_Renderer = CreateRenderer(m_RendererAPI, m_Window);
        if (!m_Renderer)
        {
            Logger::ErrorF(LOG_SUBSYSTEM,
                           "Failed to create renderer for {}",
                           targetAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            return false;
        }
        m_Renderer->SetFontCandidates(m_FontCandidates);

        if (m_RendererAPI == RendererAPI::OpenGL)
        {
            glfwMakeContextCurrent(m_Window);
            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            {
                Logger::ErrorF(LOG_SUBSYSTEM,
                               "Failed to initialize GLAD for {}",
                               targetAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
                m_Renderer->Shutdown();
                m_Renderer.reset();
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
                return false;
            }
            Texture::AdvanceOpenGLContextGeneration();
            glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glfwSwapInterval(0);
        }

        if (!m_Renderer->Init())
        {
            Logger::Error(LOG_SUBSYSTEM, "Renderer->Init() failed during SwitchRenderer");
            m_Renderer->Shutdown();
            m_Renderer.reset();
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            return false;
        }

        // renderer initialization can reset swap interval.
        if (m_RendererAPI == RendererAPI::OpenGL)
        {
            glfwSwapInterval(0);
        }

        m_Renderer->SetViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
        const glm::vec2 world = VisibleWorldSizeZoomed();
        float worldWidth = world.x;
        float worldHeight = world.y;
        m_Renderer->SetViewSize({worldWidth, worldHeight});
        glm::mat4 projection = CameraController::GetOrthoProjection(worldWidth, worldHeight);
        m_Renderer->SetProjection(projection);

        // The tilemap owns its tileset separately; all other registered textures re-upload through
        // TextureStore.
        m_Renderer->UploadTexture(m_Tilemap.GetTilesetTexture());
        m_TextureStore.UploadAll(*m_Renderer);

        // Pack after standalone uploads so sheets remain usable if atlas packing fails.
        PackCharactersIntoAtlas();

        return true;
    };

    if (setupRendererForAPI(api))
    {
        Logger::InfoF(LOG_SUBSYSTEM,
                      "Renderer switch complete! Now using {}",
                      m_RendererAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
        // exclude renderer switch time from the next simulation delta.
        m_LastFrameTime = static_cast<float>(glfwGetTime());
        return true;
    }

    Logger::WarnF(LOG_SUBSYSTEM,
                  "New renderer failed, attempting rollback to {}...",
                  oldAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");

    if (setupRendererForAPI(oldAPI))
    {
        Logger::WarnF(LOG_SUBSYSTEM,
                      "Rollback successful, still using {}",
                      m_RendererAPI == RendererAPI::OpenGL ? "OpenGL" : "Vulkan");
        m_LastFrameTime = static_cast<float>(glfwGetTime());
        return false;
    }

    Logger::Fatal(LOG_SUBSYSTEM, "Rollback also failed, shutting down");
    Shutdown();
    return false;
}

void Game::OnFramebufferResized(int width, int height)
{
    // Apply dimensions immediately; defer snapping until 150 ms after the last resize event.

    if (!m_Window || width <= 0 || height <= 0)
        return;

    m_ScreenWidth = width;
    m_ScreenHeight = height;

    const int tileScreenSize = TILE_PIXEL_SIZE * PIXEL_SCALE;

    m_TilesVisibleWidth = std::max(1, m_ScreenWidth / tileScreenSize);
    m_TilesVisibleHeight = std::max(1, m_ScreenHeight / tileScreenSize);

    if (m_Renderer)
    {
        m_Renderer->SetViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
    }

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
    }

    // recenter and grow the cosmetic title world on viewport changes.
    if (m_GameMode == GameMode::Title && m_Tilemap.GetMapWidth() > 0 &&
        m_Tilemap.GetMapHeight() > 0)
    {
        // grow the cosmetic map and emitter bounds with the viewport; unchanged tile dimensions
        // Skip repainting.
        RefreshTitleWorldForViewport(false);
    }

    m_ResizeSnapTimer = 0.15f;
    m_PendingWindowSnap = true;
}

void Game::SnapWindowToTileBoundaries()
{
    if (!m_Window)
        return;

    const int tileScreenSize = TILE_PIXEL_SIZE * PIXEL_SCALE;

    int snappedWidth =
        std::max(5 * tileScreenSize, (m_ScreenWidth / tileScreenSize) * tileScreenSize);
    int snappedHeight =
        std::max(4 * tileScreenSize, (m_ScreenHeight / tileScreenSize) * tileScreenSize);

    if (snappedWidth != m_ScreenWidth || snappedHeight != m_ScreenHeight)
    {
        glfwSetWindowSize(m_Window, snappedWidth, snappedHeight);
        Logger::InfoF(LOG_SUBSYSTEM,
                      "Window snapped to {}x{} ({}x{} tiles)",
                      snappedWidth,
                      snappedHeight,
                      snappedWidth / tileScreenSize,
                      snappedHeight / tileScreenSize);
    }

    m_PendingWindowSnap = false;
}

void Game::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game)
    {
        game->OnFramebufferResized(width, height);
    }
}

void Game::WindowRefreshCallback(GLFWwindow* window)
{
    // Redraw during OS resize requests to avoid a blank client area.
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game)
    {
        game->Render();
    }
}

EditorContext Game::MakeEditorContext()
{
    return EditorContext{m_Window,
                         m_ScreenWidth,
                         m_ScreenHeight,
                         m_TilesVisibleWidth,
                         m_TilesVisibleHeight,
                         m_Camera.GetState(),
                         m_Tilemap,
                         m_PlayerEntity,
                         m_World,
                         *m_Renderer,
                         m_Particles,
                         m_SaveMapPath};
}
