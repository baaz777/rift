#ifdef _WIN32
#define NOMINMAX
#endif

#include "Game.hpp"

#include "AmbienceConfig.hpp"
#include "AnimationState.hpp"
#include "Billboard.hpp"
#include "CharacterConstants.hpp"
#include "Dialogue.hpp"
#include "DrawTracer.hpp"
#include "Elevation.hpp"
#include "Facing.hpp"
#include "Logger.hpp"
#include "NpcRender.hpp"
#include "NpcSprite.hpp"
#include "NpcTag.hpp"
#include "ParticleCards.hpp"
#include "ParticleSystem.hpp"
#include "PlayerModes.hpp"
#include "PlayerRender.hpp"
#include "PlayerSprite.hpp"
#include "PlayerSystem.hpp"
#include "PostFXParams.hpp"
#include "Transform.hpp"
#include "Version.hpp"
#include "ViewScaling.hpp"
#include "WorldLightPools.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <ranges>
#include <unordered_set>

#ifdef _WIN32
#include <Windows.h>
#undef DrawText
#endif

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Game";

// reserved player atlas keys must not collide with NPC type names.
constexpr const char* kPlayerWalkAtlasKey = "__player_walk__";
constexpr const char* kPlayerRunAtlasKey = "__player_run__";
constexpr const char* kPlayerBicycleAtlasKey = "__player_bicycle__";

constexpr const char* kSkyRayAtlasKey = "__sky_ray__";
constexpr const char* kSkyStarAtlasKey = "__sky_star__";
constexpr const char* kSkyStarGlowAtlasKey = "__sky_star_glow__";
constexpr const char* kSkyShootingStarAtlasKey = "__sky_shooting_star__";
constexpr const char* kSkyGlowAtlasKey = "__sky_glow__";
constexpr const char* kSkyLightPoolAtlasKey = "__sky_light_pool__";
constexpr const char* kSkyAuroraCurtainAtlasKey = "__sky_aurora_curtain__";
constexpr const char* kSkyAuroraSmallAtlasKey = "__sky_aurora_small__";
constexpr const char* kSkyAuroraBeamAtlasKey = "__sky_aurora_beam__";
constexpr const char* kSkySolidAtlasKey = "__sky_solid__";

constexpr glm::vec3 TITLE_TEXT_COLOR{1.0f, 1.0f, 1.0f};
constexpr glm::vec3 TITLE_DIM_COLOR{0.55f, 0.55f, 0.62f};
constexpr glm::vec3 TITLE_DISABLED_COLOR{0.30f, 0.30f, 0.34f};
constexpr glm::vec3 TITLE_HIGHLIGHT_COLOR{1.00f, 0.84f, 0.40f};
constexpr glm::vec4 TITLE_BG_COLOR{0.04f, 0.05f, 0.09f, 1.0f};
constexpr glm::vec4 PAUSE_DIM_COLOR{0.0f, 0.0f, 0.0f, 0.55f};
constexpr glm::vec4 MODAL_BACKDROP_COLOR{0.0f, 0.0f, 0.0f, 0.65f};
constexpr glm::vec4 MODAL_BOX_COLOR{0.10f, 0.11f, 0.16f, 0.95f};

// The headline atlas has a 96 px logical size in OpenGL; scale 1 keeps the title at that size.
constexpr float TITLE_LOGO_SCALE = 1.0f;
// outline offset = 2 * scale * outlineSize pixels.
constexpr float TITLE_LOGO_OUTLINE = 6.0f;
constexpr float MENU_ITEM_SCALE = 1.4f;
constexpr float MENU_LINE_HEIGHT = 48.0f;
// reference screen width for proportional menu scaling.
constexpr float MENU_REFERENCE_WIDTH = 1520.0f;
constexpr float MENU_REFERENCE_HEIGHT = 800.0f;
constexpr const char* FOOTER_WATERMARK_TEXT = "@lextpf";
constexpr float FOOTER_TEXT_SCALE = 0.7f;
constexpr float FOOTER_HORIZONTAL_MARGIN = 16.0f;
constexpr float FOOTER_BASELINE_OFFSET = 28.0f;
constexpr float PAUSE_HEADER_SCALE = 1.8f;
constexpr float MODAL_TEXT_SCALE = 1.0f;

// Keep outline weight fixed across menu selection so highlighting does not move glyph edges.
constexpr float MENU_ITEM_OUTLINE = 2.5f;

constexpr int TITLE_WORLD_MAP_WIDTH = 32;
constexpr int TITLE_WORLD_MAP_HEIGHT = 24;
constexpr float TITLE_WORLD_TIME_OF_DAY = 23.0f;
// tileset entry 1 must contain grass for the title world.
constexpr int TITLE_WORLD_GRASS_TILE_ID = 1;
// whole-map title zones need a higher cap to sustain long-lived particles.
constexpr size_t TITLE_PARTICLES_PER_ZONE = 240;
// Restore this cap after leaving the title world.
constexpr size_t GAMEPLAY_PARTICLES_PER_ZONE = 50;

enum TitleItem : int
{
    TITLE_NEW_GAME = 0,
    TITLE_CONTINUE = 1,
    TITLE_SETTINGS = 2,
    TITLE_QUIT = 3,
    TITLE_ITEM_COUNT = 4
};

constexpr const char* TITLE_LABELS[TITLE_ITEM_COUNT] = {
    "New Game",
    "Continue",
    "Settings",
    "Quit",
};

enum PauseItem : int
{
    PAUSE_RESUME = 0,
    PAUSE_QUIT_TO_TITLE = 1,
    PAUSE_ITEM_COUNT = 2
};

constexpr const char* PAUSE_LABELS[PAUSE_ITEM_COUNT] = {
    "Resume",
    "Quit to Title",
};

glm::mat4 MakeUIProjection(int screenWidth, int screenHeight)
{
    return glm::ortho(
        0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 0.0f, -1.0f, 1.0f);
}

glm::vec3 MenuItemColor(bool selected, bool enabled)
{
    if (!enabled)
    {
        return TITLE_DISABLED_COLOR;
    }
    return selected ? TITLE_HIGHLIGHT_COLOR : TITLE_DIM_COLOR;
}

// hit tests use DrawText Y as the glyph baseline.

int TitleMenuHitTest(
    IRenderer& renderer, int screenWidth, int screenHeight, double mouseX, double mouseY)
{
    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    // Keep layout math synchronized with RenderTitleContent.
    const float uiScale = viewScaling::MenuUiScale(
        screenWidth, screenHeight, MENU_REFERENCE_WIDTH, MENU_REFERENCE_HEIGHT);
    const float menuTopY = std::floor(screenH * 0.50f);
    const float lineHeight = MENU_LINE_HEIGHT * uiScale;
    const float ascent = renderer.GetTextAscent(MENU_ITEM_SCALE * uiScale);
    constexpr float HIT_PAD_X = 24.0f;

    for (int i = 0; i < TITLE_ITEM_COUNT; ++i)
    {
        // Use unselected width; horizontal padding absorbs the selection-prefix change.
        const std::string display = std::string("  ") + TITLE_LABELS[i];
        const float w = renderer.GetTextWidth(display, MENU_ITEM_SCALE * uiScale);
        const float x = std::floor((screenW - w) * 0.5f);
        const float baselineY = menuTopY + i * lineHeight;
        const float topY = baselineY - ascent;
        if (mouseX >= x - HIT_PAD_X && mouseX <= x + w + HIT_PAD_X && mouseY >= topY &&
            mouseY <= topY + lineHeight)
        {
            return i;
        }
    }
    return -1;
}

int PauseMenuHitTest(
    IRenderer& renderer, int screenWidth, int screenHeight, double mouseX, double mouseY)
{
    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    // Keep layout math synchronized with RenderPauseOverlay.
    const float uiScale = viewScaling::MenuUiScale(
        screenWidth, screenHeight, MENU_REFERENCE_WIDTH, MENU_REFERENCE_HEIGHT);
    const float menuTopY = std::floor(screenH * 0.52f);
    const float lineHeight = MENU_LINE_HEIGHT * uiScale;
    const float ascent = renderer.GetTextAscent(MENU_ITEM_SCALE * uiScale);
    constexpr float HIT_PAD_X = 24.0f;

    for (int i = 0; i < PAUSE_ITEM_COUNT; ++i)
    {
        const std::string display = std::string("  ") + PAUSE_LABELS[i];
        const float w = renderer.GetTextWidth(display, MENU_ITEM_SCALE * uiScale);
        const float x = std::floor((screenW - w) * 0.5f);
        const float baselineY = menuTopY + i * lineHeight;
        const float topY = baselineY - ascent;
        if (mouseX >= x - HIT_PAD_X && mouseX <= x + w + HIT_PAD_X && mouseY >= topY &&
            mouseY <= topY + lineHeight)
        {
            return i;
        }
    }
    return -1;
}

// 0 = Cancel, 1 = New Game, -1 = neither.
int ConfirmPromptHitTest(
    IRenderer& renderer, int screenWidth, int screenHeight, double mouseX, double mouseY)
{
    const float screenW = static_cast<float>(screenWidth);
    const float screenH = static_cast<float>(screenHeight);
    const float boxW = std::floor(screenW * 0.55f);
    const float boxH = std::floor(screenH * 0.30f);
    const float boxX = std::floor((screenW - boxW) * 0.5f);
    const float boxY = std::floor((screenH - boxH) * 0.5f);

    constexpr float buttonScale = 1.1f;
    const std::string cancelDisplay = std::string("  ") + "Cancel";
    const std::string confirmDisplay = std::string("  ") + "New Game";
    const float cancelW = renderer.GetTextWidth(cancelDisplay, buttonScale);
    const float confirmW = renderer.GetTextWidth(confirmDisplay, buttonScale);
    const float ascent = renderer.GetTextAscent(buttonScale);
    const float buttonBaselineY = boxY + boxH * 0.72f;
    const float cancelX = std::floor(boxX + boxW * 0.30f - cancelW * 0.5f);
    const float confirmX = std::floor(boxX + boxW * 0.70f - confirmW * 0.5f);

    constexpr float HIT_PAD_X = 16.0f;
    constexpr float HIT_PAD_Y = 8.0f;
    const float topY = buttonBaselineY - ascent - HIT_PAD_Y;
    const float rowH = ascent + 2.0f * HIT_PAD_Y;
    if (mouseX >= cancelX - HIT_PAD_X && mouseX <= cancelX + cancelW + HIT_PAD_X &&
        mouseY >= topY && mouseY <= topY + rowH)
    {
        return 0;
    }
    if (mouseX >= confirmX - HIT_PAD_X && mouseX <= confirmX + confirmW + HIT_PAD_X &&
        mouseY >= topY && mouseY <= topY + rowH)
    {
        return 1;
    }
    return -1;
}

}  // namespace

bool Game::CheckSaveExists() const
{
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::exists(m_SaveMapPath, ec) && fs::is_regular_file(m_SaveMapPath, ec);
}

void Game::LoadGameWorld(bool loadSave)
{
    int loadedPlayerTileX = -1;
    int loadedPlayerTileY = -1;
    int loadedCharacterType = -1;
    bool mapLoaded = false;

    // New Game retains the authored map and resets only transient systems.
    if (loadSave || CheckSaveExists())
    {
        mapLoaded = m_Tilemap.LoadMapFromJSON(
            m_SaveMapPath, &m_World, &loadedPlayerTileX, &loadedPlayerTileY, &loadedCharacterType);
    }

    if (!mapLoaded)
    {
        Logger::InfoF(LOG_SUBSYSTEM,
                      "{}",
                      loadSave ? "No existing save found, generating default map"
                               : "New Game: authored map unavailable, generating default map");
        EntityStore::Clear(m_World);
        m_Tilemap.SetTilemapSize(m_DefaultMapWidth, m_DefaultMapHeight);
    }

    // Vulkan needs an explicit tileset upload; OpenGL uploads lazily.
    if (m_RendererAPI == RendererAPI::Vulkan && m_Renderer)
    {
        m_Renderer->UploadTexture(m_Tilemap.GetTilesetTexture());
    }

    // prefer saved character, then manifest order, then the default.
    CharacterType initialCharacter =
        (loadedCharacterType >= 0 &&
         loadedCharacterType < static_cast<int>(EnumTraits<CharacterType>::Count))
            ? static_cast<CharacterType>(loadedCharacterType)
            : (m_ConfiguredCharacters.empty() ? CharacterType::BW1_MALE
                                              : m_ConfiguredCharacters.front());
    if (!m_ConfiguredCharacters.empty() &&
        std::ranges::find(m_ConfiguredCharacters, initialCharacter) == m_ConfiguredCharacters.end())
    {
        initialCharacter = m_ConfiguredCharacters.front();
    }
    if (!PlayerSystem::SwitchCharacter(m_World, m_PlayerEntity, initialCharacter))
    {
        Logger::Error(LOG_SUBSYSTEM, "Failed to switch player character in LoadGameWorld");
    }

    int playerTileX = (loadedPlayerTileX >= 0) ? loadedPlayerTileX : 9;
    int playerTileY = (loadedPlayerTileY >= 0) ? loadedPlayerTileY : 5;
    PlayerSystem::SetTilePosition(m_World, m_PlayerEntity, playerTileX, playerTileY);

    // repack and rebind characters after every world load; loaded sheets may differ from the
    // Previous world.
    PackCharactersIntoAtlas();

    float camWorldWidth = static_cast<float>(m_TilesVisibleWidth * m_Tilemap.GetTileWidth());
    float camWorldHeight = static_cast<float>(m_TilesVisibleHeight * m_Tilemap.GetTileHeight());
    glm::vec2 playerPos = m_World.get<Transform>(m_PlayerEntity).position;
    glm::vec2 playerVisualCenter =
        glm::vec2(playerPos.x, playerPos.y - CharacterConstants::HITBOX_HEIGHT);
    m_Camera.Initialize(playerVisualCenter, camWorldWidth, camWorldHeight);

    m_Particles.SetZones(m_Tilemap.GetParticleZones());
    m_Particles.SetTilemap(&m_Tilemap);

    // Restore the gameplay cap after the title world's denser emitter budget.
    m_Particles.SetMaxParticlesPerZone(GAMEPLAY_PARTICLES_PER_ZONE);
}

void Game::PackCharactersIntoAtlas()
{
    // atlas copies preserve each source image row convention for standalone-equivalent uvs.
    std::vector<Tilemap::AtlasPackEntry> sheets;
    sheets.reserve(EntityStore::Count(m_World) + 3 + 8);

    // The first NPC in instance-id order owns each deduplicated atlas entry.
    std::unordered_set<std::string> seenTypes;
    for (const entt::entity entity : EntityStore::Entities(m_World))
    {
        const Dialogue& dialogue = m_World.get<Dialogue>(entity);
        const NpcSprite& sprite = m_World.get<NpcSprite>(entity);
        if (!dialogue.type.empty() && seenTypes.insert(dialogue.type).second)
        {
            sheets.push_back({dialogue.type, &m_TextureStore.Get(sprite.sheet)});
        }
    }

    const PlayerSprite& playerSprite = m_World.get<PlayerSprite>(m_PlayerEntity);
    sheets.push_back({kPlayerWalkAtlasKey, &PlayerSystem::GetSpriteSheet(m_World, playerSprite)});
    sheets.push_back(
        {kPlayerRunAtlasKey, &PlayerSystem::GetRunningSpriteSheet(m_World, playerSprite)});
    sheets.push_back(
        {kPlayerBicycleAtlasKey, &PlayerSystem::GetBicycleSpriteSheet(m_World, playerSprite)});

    sheets.push_back({kSkyRayAtlasKey, &m_SkyRenderer.GetRayTexture()});
    sheets.push_back({kSkyStarAtlasKey, &m_SkyRenderer.GetStarTexture()});
    sheets.push_back({kSkyStarGlowAtlasKey, &m_SkyRenderer.GetStarGlowTexture()});
    sheets.push_back({kSkyShootingStarAtlasKey, &m_SkyRenderer.GetShootingStarTexture()});
    sheets.push_back({kSkyGlowAtlasKey, &m_SkyRenderer.GetGlowTexture()});
    sheets.push_back({kSkyLightPoolAtlasKey, &m_SkyRenderer.GetLightPoolTexture()});
    sheets.push_back({kSkyAuroraCurtainAtlasKey, &m_SkyRenderer.GetAuroraCurtainTexture()});
    sheets.push_back({kSkyAuroraSmallAtlasKey, &m_SkyRenderer.GetAuroraSmallTexture()});
    // 3D sky quads share the atlas to remain in one batch.
    sheets.push_back({kSkyAuroraBeamAtlasKey, &m_SkyRenderer.GetAuroraBeamTexture()});
    sheets.push_back({kSkySolidAtlasKey, &m_SkyRenderer.GetSolidTexture()});

    if (!m_Tilemap.PackAdditionalSheets(sheets))
    {
        Logger::Error(
            LOG_SUBSYSTEM,
            "PackCharactersIntoAtlas: atlas pack failed; characters keep per-sheet textures");
        // Clear old atlas bindings before assigning new regions.
        m_World.view<NpcSprite, NpcTag>().each(
            [](NpcSprite& sprite)
            {
                sprite.atlas = nullptr;
                sprite.atlasOffset = glm::vec2(0.0f);
            });
        PlayerSystem::SetAtlasBinding(
            m_World, m_PlayerEntity, nullptr, glm::vec2(0.0f), glm::vec2(0.0f), glm::vec2(0.0f));
        m_SkyRenderer.SetAtlasBinding(nullptr, SkyAtlasOffsets{});
        return;
    }

    const Texture* atlasTex = &m_Tilemap.GetTilesetTexture();
    m_World.view<Dialogue, NpcSprite, NpcTag>().each(
        [&](const Dialogue& dialogue, NpcSprite& sprite)
        {
            const auto offset = m_Tilemap.GetCharacterAtlasOffset(dialogue.type);
            sprite.atlas = atlasTex;
            sprite.atlasOffset = offset.value_or(glm::vec2(0.0f));
        });

    glm::vec2 walkOff =
        m_Tilemap.GetCharacterAtlasOffset(kPlayerWalkAtlasKey).value_or(glm::vec2(0.0f));
    glm::vec2 runOff =
        m_Tilemap.GetCharacterAtlasOffset(kPlayerRunAtlasKey).value_or(glm::vec2(0.0f));
    glm::vec2 bikeOff =
        m_Tilemap.GetCharacterAtlasOffset(kPlayerBicycleAtlasKey).value_or(glm::vec2(0.0f));
    PlayerSystem::SetAtlasBinding(m_World, m_PlayerEntity, atlasTex, walkOff, runOff, bikeOff);

    auto skyOff = [this](const char* key)
    { return m_Tilemap.GetCharacterAtlasOffset(key).value_or(glm::vec2(0.0f)); };
    SkyAtlasOffsets skyOffsets;
    skyOffsets[skyDraw::Sprite::Ray] = skyOff(kSkyRayAtlasKey);
    skyOffsets[skyDraw::Sprite::Star] = skyOff(kSkyStarAtlasKey);
    skyOffsets[skyDraw::Sprite::StarGlow] = skyOff(kSkyStarGlowAtlasKey);
    skyOffsets[skyDraw::Sprite::ShootingStar] = skyOff(kSkyShootingStarAtlasKey);
    skyOffsets[skyDraw::Sprite::Glow] = skyOff(kSkyGlowAtlasKey);
    skyOffsets[skyDraw::Sprite::LightPool] = skyOff(kSkyLightPoolAtlasKey);
    skyOffsets[skyDraw::Sprite::AuroraCurtain] = skyOff(kSkyAuroraCurtainAtlasKey);
    skyOffsets[skyDraw::Sprite::AuroraSmall] = skyOff(kSkyAuroraSmallAtlasKey);
    skyOffsets[skyDraw::Sprite::AuroraBeam] = skyOff(kSkyAuroraBeamAtlasKey);
    skyOffsets[skyDraw::Sprite::Solid] = skyOff(kSkySolidAtlasKey);
    m_SkyRenderer.SetAtlasBinding(atlasTex, skyOffsets);
}

void Game::PaintTitleWorld(int tilesWide, int tilesTall)
{
    tilesWide = std::max(1, tilesWide);
    tilesTall = std::max(1, tilesTall);

    m_Tilemap.SetTilemapSize(tilesWide, tilesTall, false);

    for (int y = 0; y < tilesTall; ++y)
    {
        for (int x = 0; x < tilesWide; ++x)
        {
            m_Tilemap.SetLayerTile(x, y, 0, TITLE_WORLD_GRASS_TILE_ID);
        }
    }

    // One full-map zone per atmospheric type; Lantern requires a lit zone.
    if (auto* zones = m_Tilemap.GetParticleZonesMutable())
    {
        zones->clear();
        const glm::vec2 zonePos(0.0f, 0.0f);
        const glm::vec2 zoneSize(static_cast<float>(tilesWide * m_Tilemap.GetTileWidth()),
                                 static_cast<float>(tilesTall * m_Tilemap.GetTileHeight()));
        constexpr ParticleType TITLE_PARTICLE_TYPES[] = {
            ParticleType::Firefly,
            ParticleType::Rain,
            ParticleType::Snow,
            ParticleType::Fog,
            ParticleType::Sparkles,
            ParticleType::Wisp,
            ParticleType::Sunshine,
            ParticleType::DriftingLeaf,
            ParticleType::DustMote,
            ParticleType::Pollen,
            ParticleType::CherryBlossom,
        };
        for (ParticleType type : TITLE_PARTICLE_TYPES)
        {
            ParticleZone zone;
            zone.position = zonePos;
            zone.size = zoneSize;
            zone.type = type;
            zone.enabled = true;
            zone.noProjection = false;
            zones->push_back(zone);
        }
    }

    if (m_RendererAPI == RendererAPI::Vulkan && m_Renderer)
    {
        m_Renderer->UploadTexture(m_Tilemap.GetTilesetTexture());
    }

    // Refresh the borrowed zone list after tilemap rebuild.
    m_Particles.SetZones(m_Tilemap.GetParticleZones());
    m_Particles.SetTilemap(&m_Tilemap);
}

void Game::RefreshTitleWorldForViewport(bool forceRepaint)
{
    const glm::ivec2 titleTiles = viewScaling::RequiredTitleWorldTiles(m_ScreenWidth,
                                                                       m_ScreenHeight,
                                                                       PIXEL_SCALE,
                                                                       m_Tilemap.GetTileWidth(),
                                                                       m_Tilemap.GetTileHeight(),
                                                                       m_Camera.GetState().zoom,
                                                                       2,
                                                                       TITLE_WORLD_MAP_WIDTH,
                                                                       TITLE_WORLD_MAP_HEIGHT);

    if (forceRepaint || titleTiles.x != m_Tilemap.GetMapWidth() ||
        titleTiles.y != m_Tilemap.GetMapHeight())
    {
        PaintTitleWorld(titleTiles.x, titleTiles.y);
    }

    // Use the actual visible extent to center the resized title world.
    const glm::vec2 mapCenterPx(
        static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth()) * 0.5f,
        static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight()) * 0.5f);
    const glm::vec2 view = VisibleWorldSizeZoomed();
    m_Camera.Initialize(mapCenterPx, view.x, view.y);
}

void Game::LoadTitleScreenWorld()
{
    // Reset the title ambience latch for this menu session.
    m_TitleAmbientCleared = false;
    EntityStore::Clear(m_World);
    m_Editor.SetActive(false);
    m_DialogueManager.EndDialogue();
    m_DialogueUi.inDialogue = false;
    m_DialogueUi.text.clear();
    m_DialogueUi.npcId = 0;
    m_DialogueUi.page = 0;
    m_DialogueUi.charReveal = -1.0f;
    m_DialogueUi.boxFadeTimer = 0.0f;
    m_DialogueUi.snap.active = false;

    RefreshTitleWorldForViewport(true);

    // Keep the player entity at a valid tile for shared frame code; the title renderer skips it.
    PlayerSystem::SetTilePosition(m_World, m_PlayerEntity, 0, 0);

    // Title mode skips TimeManager::Update, so this authored nighttime hour remains fixed.
    m_TimeManager.Initialize();
    m_WeatherDirector.Reset(m_TimeManager);
    m_WeatherDirector.SetEnabled(false);
    m_TimeManager.SetTime(TITLE_WORLD_TIME_OF_DAY);

    // Aurora supplies sky curtains and wisps while zone emitters supply the other title particles.
    m_TimeManager.SetWeather(WeatherState::Aurora);

    // whole-map title zones use a higher cap; LoadGameWorld restores the gameplay value.
    m_Particles.SetMaxParticlesPerZone(TITLE_PARTICLES_PER_ZONE);

    // Set time and night factor before prewarming; zone spawn rules read them.
    m_Particles.SetTimeOfDay(m_TimeManager.GetTimeOfDay());
    m_Particles.SetNightFactor(m_TimeManager.GetStarVisibility());
    m_Particles.Clear();
    // Clear gameplay transitions and wind before prewarming title particles.
    m_Particles.SetWeatherTransition(nullptr, nullptr, 0.0f);
    m_Particles.SetWind(m_WeatherDirector.GetWindDirection(), m_WeatherDirector.GetWindStrength());
    const glm::vec2 prewarmCam = m_Camera.GetState().position;
    const glm::vec2 prewarmView = VisibleWorldSizeZoomed();
    constexpr float PREWARM_DURATION_S = 5.0f;
    constexpr float PREWARM_STEP_S = 1.0f / 60.0f;
    constexpr int PREWARM_STEPS = static_cast<int>(PREWARM_DURATION_S / PREWARM_STEP_S);
    for (int s = 0; s < PREWARM_STEPS; ++s)
    {
        m_Particles.Update(PREWARM_STEP_S, prewarmCam, prewarmView);
    }
}

void Game::ResetWorldToDefaults()
{
    LoadGameWorld(false);
    m_TimeManager.Initialize();
    m_WeatherDirector.Reset(m_TimeManager);
    m_WeatherDirector.SetEnabled(true);
    m_GameState.Clear();

    // Clear conversation state before the new world can update its NPCs.
    m_DialogueManager.EndDialogue();
    m_DialogueUi.inDialogue = false;
    m_DialogueUi.text.clear();
    m_DialogueUi.npcId = 0;
    m_DialogueUi.page = 0;
    m_DialogueUi.charReveal = -1.0f;
    m_DialogueUi.boxFadeTimer = 0.0f;
    m_DialogueUi.snap.active = false;
}

void Game::RebuildTitleMenu()
{
    const bool hasSave = CheckSaveExists();

    m_TitleMenu.enabled.assign(TITLE_ITEM_COUNT, true);
    m_TitleMenu.enabled[TITLE_CONTINUE] = hasSave;
    m_TitleMenu.enabled[TITLE_SETTINGS] = false;

    // prefer Continue when a save exists; otherwise select the first enabled item.
    m_TitleMenu.selected =
        hasSave ? static_cast<int>(TITLE_CONTINUE) : MenuLogic::FirstEnabledIndex(m_TitleMenu);

    m_ConfirmOverwriteShown = false;
    m_ConfirmPrompt.selected = MenuLogic::ConfirmChoice::Cancel;

    // ignore stale hover and held clicks on the first menu frame.
    m_MenuLastMouseX = -1.0;
    m_MenuLastMouseY = -1.0;
    m_MenuMouseLeftPrev = true;
}

void Game::ProcessTitleInput()
{
    if (m_TitleMenu.enabled.size() != static_cast<size_t>(TITLE_ITEM_COUNT))
    {
        RebuildTitleMenu();
    }

    // poll every toggle so edge state cannot carry across modes.
    bool up = m_KeyMenuUp.JustPressed(m_Window);
    bool down = m_KeyMenuDown.JustPressed(m_Window);
    bool left = m_KeyMenuLeft.JustPressed(m_Window);
    bool right = m_KeyMenuRight.JustPressed(m_Window);
    bool confirm = m_KeyMenuConfirm.JustPressed(m_Window);
    bool esc = m_KeyEscape.JustPressed(m_Window);

    // Only mouse motion changes hover selection; stationary hover must not override keyboard input.
    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);
    const bool firstMouseFrame = (m_MenuLastMouseX < 0.0);
    const bool mouseMoved =
        !firstMouseFrame && ((mouseX != m_MenuLastMouseX) || (mouseY != m_MenuLastMouseY));
    m_MenuLastMouseX = mouseX;
    m_MenuLastMouseY = mouseY;
    const bool mouseDown = (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    const bool mouseClicked = mouseDown && !m_MenuMouseLeftPrev;
    m_MenuMouseLeftPrev = mouseDown;

    if (m_ConfirmOverwriteShown)
    {
        const int modalHit =
            ConfirmPromptHitTest(*m_Renderer, m_ScreenWidth, m_ScreenHeight, mouseX, mouseY);
        if (modalHit >= 0)
        {
            const auto target = (modalHit == 0) ? MenuLogic::ConfirmChoice::Cancel
                                                : MenuLogic::ConfirmChoice::Confirm;
            if (mouseMoved)
            {
                m_ConfirmPrompt.selected = target;
            }
            if (mouseClicked)
            {
                m_ConfirmPrompt.selected = target;
                confirm = true;
            }
        }

        if (esc)
        {
            m_ConfirmOverwriteShown = false;
            return;
        }
        if (left)
        {
            MenuLogic::ConfirmLeft(m_ConfirmPrompt);
        }
        if (right)
        {
            MenuLogic::ConfirmRight(m_ConfirmPrompt);
        }
        if (confirm)
        {
            const bool proceed = (m_ConfirmPrompt.selected == MenuLogic::ConfirmChoice::Confirm);
            m_ConfirmOverwriteShown = false;
            if (proceed)
            {
                Logger::Info(LOG_SUBSYSTEM, "New Game (overwrite confirmed)");
                ResetWorldToDefaults();
                m_GameMode = GameMode::Playing;
            }
        }
        return;
    }

    // Apply hover before confirmation so clicks act on the hovered item.
    const int titleHit =
        TitleMenuHitTest(*m_Renderer, m_ScreenWidth, m_ScreenHeight, mouseX, mouseY);
    if (titleHit >= 0 && m_TitleMenu.enabled[titleHit])
    {
        if (mouseMoved)
        {
            m_TitleMenu.selected = titleHit;
        }
        if (mouseClicked)
        {
            m_TitleMenu.selected = titleHit;
            confirm = true;
        }
    }

    if (up)
    {
        MenuLogic::NavigateUp(m_TitleMenu);
    }
    if (down)
    {
        MenuLogic::NavigateDown(m_TitleMenu);
    }
    if (esc)
    {
    }
    if (!confirm)
    {
        return;
    }

    switch (m_TitleMenu.selected)
    {
        case TITLE_NEW_GAME:
        {
            if (CheckSaveExists())
            {
                m_ConfirmOverwriteShown = true;
                m_ConfirmPrompt.selected = MenuLogic::ConfirmChoice::Cancel;
            }
            else
            {
                Logger::Info(LOG_SUBSYSTEM, "New Game (no existing save)");
                ResetWorldToDefaults();
                m_GameMode = GameMode::Playing;
            }
            break;
        }
        case TITLE_CONTINUE:
        {
            if (!CheckSaveExists())
            {
                break;
            }
            Logger::Info(LOG_SUBSYSTEM, "Continue: reloading save from disk");
            // continue must not inherit the title hour.
            m_TimeManager.Initialize();
            m_WeatherDirector.Reset(m_TimeManager);
            m_WeatherDirector.SetEnabled(true);
            LoadGameWorld(true);
            m_GameMode = GameMode::Playing;
            break;
        }
        case TITLE_SETTINGS:
        {
            break;
        }
        case TITLE_QUIT:
        {
            Logger::Info(LOG_SUBSYSTEM, "Quit from title");
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
            break;
        }
        default:
            break;
    }
}

void Game::ProcessPauseInput()
{
    if (m_PauseMenu.enabled.size() != static_cast<size_t>(PAUSE_ITEM_COUNT))
    {
        m_PauseMenu.enabled.assign(PAUSE_ITEM_COUNT, true);
        m_PauseMenu.selected = 0;
    }

    bool up = m_KeyMenuUp.JustPressed(m_Window);
    bool down = m_KeyMenuDown.JustPressed(m_Window);
    bool confirm = m_KeyMenuConfirm.JustPressed(m_Window);
    bool esc = m_KeyEscape.JustPressed(m_Window);

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(m_Window, &mouseX, &mouseY);
    const bool firstMouseFrame = (m_MenuLastMouseX < 0.0);
    const bool mouseMoved =
        !firstMouseFrame && ((mouseX != m_MenuLastMouseX) || (mouseY != m_MenuLastMouseY));
    m_MenuLastMouseX = mouseX;
    m_MenuLastMouseY = mouseY;
    const bool mouseDown = (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    const bool mouseClicked = mouseDown && !m_MenuMouseLeftPrev;
    m_MenuMouseLeftPrev = mouseDown;

    if (esc)
    {
        m_GameMode = GameMode::Playing;
        return;
    }

    const int pauseHit =
        PauseMenuHitTest(*m_Renderer, m_ScreenWidth, m_ScreenHeight, mouseX, mouseY);
    if (pauseHit >= 0)
    {
        if (mouseMoved)
        {
            m_PauseMenu.selected = pauseHit;
        }
        if (mouseClicked)
        {
            m_PauseMenu.selected = pauseHit;
            confirm = true;
        }
    }

    if (up)
    {
        MenuLogic::NavigateUp(m_PauseMenu);
    }
    if (down)
    {
        MenuLogic::NavigateDown(m_PauseMenu);
    }

    if (!confirm)
    {
        return;
    }

    switch (m_PauseMenu.selected)
    {
        case PAUSE_RESUME:
        {
            m_GameMode = GameMode::Playing;
            break;
        }
        case PAUSE_QUIT_TO_TITLE:
        {
            Logger::Info(LOG_SUBSYSTEM, "Quit to Title (no save)");

            // Replace the abandoned session with the cosmetic world before showing the title menu.
            LoadTitleScreenWorld();
            m_GameMode = GameMode::Title;
            RebuildTitleMenu();
            break;
        }
        default:
            break;
    }
}

cameraRig::RigParams Game::BuildCameraRig() const
{
    const glm::vec2 world = VisibleWorldSize();
    const float zoom = std::max(m_Camera.GetState().zoom, 0.001f);
    const glm::vec2 visible = world / zoom;

    // match flat pixel snapping only for Classic; rotated world axes are not screen-pixel steps.
    glm::vec2 corner = m_Camera.GetState().position;
    if (m_RendererAPI == RendererAPI::OpenGL && m_CameraPreset == cameraRig::Preset::Classic)
    {
        const float pixelStepX = visible.x / static_cast<float>(m_ScreenWidth);
        const float pixelStepY = visible.y / static_cast<float>(m_ScreenHeight);
        auto snapToPixel = [](float value, float step)
        { return (step > 0.0f) ? std::round(value / step) * step : value; };
        corner.x = snapToPixel(corner.x, pixelStepX);
        corner.y = snapToPixel(corner.y, pixelStepY);
    }

    cameraRig::RigParams rig;
    rig.visibleWorldSize = visible;
    rig.target = corner + visible * 0.5f;
    rig.yawRadians = m_CameraYaw;
    rig.pitchRadians = m_CameraPitch;

    // span the map depth so tall edge billboards do not clip.
    const float mapW = static_cast<float>(m_Tilemap.GetMapWidth() * m_Tilemap.GetTileWidth());
    const float mapH = static_cast<float>(m_Tilemap.GetMapHeight() * m_Tilemap.GetTileHeight());
    rig.sceneRadius = std::max(256.0f, std::sqrt(mapW * mapW + mapH * mapH));

    // Classic and DS prescribe angles; Free retains the live orbit angles.
    cameraRig::ApplyPreset(rig, m_CameraPreset);
    return rig;
}

// Resolve support height here; the shared light builder has no tilemap.
void Game::RenderWorldLights3D(const particleCards::Frame& frame)
{
    worldLights::Build(m_Tilemap.GetLights(),
                       m_TimeManager.GetTimeOfDay(),
                       m_TimeManager.GetStarVisibility(),
                       m_LightPoolScratch);
    for (skyDraw::LightPool& pool : m_LightPoolScratch)
    {
        pool.surfaceHeight = m_Tilemap.SurfaceHeightAtWorldPos(pool.centreWorld);
    }
    m_SkyRenderer.SubmitLightPools3D(*m_Renderer, m_LightPoolScratch, frame);
}

void Game::RenderFrame3D()
{
    m_Renderer->BeginFrame();
    m_Renderer->BeginScene();

    DrawTracer::Mark("== gameplay frame (3D) ==", m_Renderer->GetDrawCallCount());

    const glm::vec3 skyColor = m_TimeManager.GetSkyColor();
    m_Renderer->Clear(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    m_Renderer->SetAmbientColor(m_TimeManager.GetAmbientColor());

    const cameraRig::RigParams rig = BuildCameraRig();
    m_Renderer->SetViewProjection(cameraRig::BuildViewProjection(rig));

    m_Renderer->SetViewSize(rig.visibleWorldSize);

    DrawTracer::Mark("section: World3D", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderWorld3D(*m_Renderer, rig);

    // characters follow full camera yaw to avoid edge-on sprites.
    const billboard::Orientation actorOrientation = billboard::Orient(
        rig.yawRadians, rig.pitchRadians, billboard::DefaultDamping(billboard::Role::Character));
    DrawTracer::Mark("section: NPCs3D", m_Renderer->GetDrawCallCount());
    for (const entt::entity entity : EntityStore::Entities(m_World))
    {
        NpcRender::Draw3D(m_World,
                          *m_Renderer,
                          actorOrientation,
                          m_World.get<Transform>(entity),
                          m_World.get<Elevation>(entity),
                          m_World.get<Facing>(entity),
                          m_World.get<AnimationState>(entity),
                          m_World.get<NpcSprite>(entity));
    }

    DrawTracer::Mark("section: Player3D", m_Renderer->GetDrawCallCount());
    if (m_World.valid(m_PlayerEntity))
    {
        PlayerRender::Draw3D(m_World,
                             *m_Renderer,
                             actorOrientation,
                             m_World.get<Transform>(m_PlayerEntity),
                             m_World.get<Elevation>(m_PlayerEntity),
                             m_World.get<Facing>(m_PlayerEntity),
                             m_World.get<AnimationState>(m_PlayerEntity),
                             m_World.get<PlayerModes>(m_PlayerEntity),
                             m_World.get<PlayerSprite>(m_PlayerEntity));
    }

    DrawTracer::Mark("section: Particles3D", m_Renderer->GetDrawCallCount());
    m_Particles.Render3D(*m_Renderer, rig);

    // reuse the camera frame for both light pools and sky; pools draw first as in the flat path.
    const particleCards::Frame skyFrame = particleCards::MakeFrame(rig);
    DrawTracer::Mark("section: WorldLights3D", m_Renderer->GetDrawCallCount());
    RenderWorldLights3D(skyFrame);

    // Use the unzoomed sky extent to match flat rendering.
    DrawTracer::Mark("section: Sky3D", m_Renderer->GetDrawCallCount());
    m_SkyRenderer.Render3D(*m_Renderer, m_TimeManager, skyFrame, VisibleWorldSize());

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
    DrawTracer::Mark("section: PostFX", m_Renderer->GetDrawCallCount());
    m_Renderer->EndSceneApplyPostFX(postFX);

    // Reset ambient after postfx; an earlier change can flush pending 3D geometry with white light.
    DrawTracer::Mark("section: UI overlays", m_Renderer->GetDrawCallCount());
    m_Renderer->SetAmbientColor(glm::vec3(1.0f));
    m_Renderer->SetProjection(MakeUIProjection(m_ScreenWidth, m_ScreenHeight));
    m_Console.Render(*m_Renderer, m_ScreenWidth, m_ScreenHeight);

    m_Renderer->EndFrame();

    // This complete frame path must swap OpenGL buffers itself.
    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glfwSwapBuffers(m_Window);
    }

    m_Fps.drawCallAccumulator += m_Renderer->GetDrawCallCount();
}

void Game::RenderTitleFrame()
{
    if (m_TitleMenu.enabled.size() != static_cast<size_t>(TITLE_ITEM_COUNT))
    {
        RebuildTitleMenu();
    }

    m_Renderer->BeginFrame();
    m_Renderer->BeginScene();

    DrawTracer::Mark("== title frame ==", m_Renderer->GetDrawCallCount());

    glm::vec3 skyColor = m_TimeManager.GetSkyColor();
    m_Renderer->Clear(skyColor.r, skyColor.g, skyColor.b, 1.0f);
    m_Renderer->SetAmbientColor(m_TimeManager.GetAmbientColor());

    // The title has no rendered actors, so it can omit gameplay Y-sort assembly.
    const float worldWidth = static_cast<float>(m_ScreenWidth) / static_cast<float>(PIXEL_SCALE);
    const float worldHeight = static_cast<float>(m_ScreenHeight) / static_cast<float>(PIXEL_SCALE);
    const float zoomedWidth = worldWidth / m_Camera.GetState().zoom;
    const float zoomedHeight = worldHeight / m_Camera.GetState().zoom;
    m_Renderer->SetViewSize({zoomedWidth, zoomedHeight});
    glm::mat4 projection = CameraController::GetOrthoProjection(zoomedWidth, zoomedHeight);
    m_Renderer->SetProjection(projection);

    const glm::vec2 renderCam = m_Camera.GetState().position;
    const glm::vec2 renderSize(zoomedWidth, zoomedHeight);
    DrawTracer::Mark("section: BackgroundLayers", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderBackgroundLayers(*m_Renderer, renderCam, renderSize, renderCam, renderSize);
    DrawTracer::Mark("section: ForegroundLayers", m_Renderer->GetDrawCallCount());
    m_Tilemap.RenderForegroundLayers(*m_Renderer, renderCam, renderSize, renderCam, renderSize);

    DrawTracer::Mark("section: Particles", m_Renderer->GetDrawCallCount());
    m_Particles.Render(*m_Renderer, renderCam, false, false);

    // Use renderCam for sky parallax so sky and cosmetic map respond to the same menu camera.
    DrawTracer::Mark("section: Sky", m_Renderer->GetDrawCallCount());
    m_SkyRenderer.Render(*m_Renderer,
                         m_TimeManager,
                         renderCam,
                         static_cast<int>(worldWidth),
                         static_cast<int>(worldHeight));

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
        // identity values back up postFXEnabled if its uniform is absent.
    }
    DrawTracer::Mark("section: PostFX", m_Renderer->GetDrawCallCount());
    m_Renderer->EndSceneApplyPostFX(postFX);

    DrawTracer::Mark("section: UI overlays", m_Renderer->GetDrawCallCount());

    RenderTitleContent();
    if (m_ConfirmOverwriteShown)
    {
        RenderConfirmOverwritePrompt();
    }

    // Title diagnostics show performance and renderer state; player and quest fields do not apply.
    if (m_Editor.IsShowDebugInfo())
    {
        constexpr float kMargin = 12.0f;
        constexpr float kLineHeight = 28.0f;
        constexpr float kHudAlpha = 0.6f;
        const glm::vec3 kFpsColor(1.0f, 1.0f, 0.0f);
        const glm::vec3 kRightColor(1.0f, 0.3f, 0.3f);

        glm::mat4 uiProjection = glm::ortho(0.0f,
                                            static_cast<float>(m_ScreenWidth),
                                            static_cast<float>(m_ScreenHeight),
                                            0.0f,
                                            -1.0f,
                                            1.0f);
        m_Renderer->SetProjection(uiProjection);

        char fpsText[32];
        std::snprintf(
            fpsText, sizeof(fpsText), "FPS: %d", static_cast<int>(m_Fps.currentFps + 0.5f));
        m_Renderer->DrawText(fpsText, glm::vec2(kMargin, 32.0f), 1.0f, kFpsColor, 2.0f, kHudAlpha);

        const char* rendererName = (m_RendererAPI == RendererAPI::OpenGL) ? "OpenGL" : "Vulkan";
        float rightMargin = static_cast<float>(m_ScreenWidth) - kMargin;

        char rendererText[32];
        std::snprintf(rendererText, sizeof(rendererText), "%s", rendererName);
        float textWidth = m_Renderer->GetTextWidth(rendererText, 1.0f);
        m_Renderer->DrawText(rendererText,
                             glm::vec2(rightMargin - textWidth, 32.0f),
                             1.0f,
                             kRightColor,
                             2.0f,
                             kHudAlpha);

        char resText[32];
        std::snprintf(resText, sizeof(resText), "%dx%d", m_ScreenWidth, m_ScreenHeight);
        textWidth = m_Renderer->GetTextWidth(resText, 1.0f);
        m_Renderer->DrawText(resText,
                             glm::vec2(rightMargin - textWidth, 32.0f + kLineHeight),
                             1.0f,
                             kRightColor,
                             2.0f,
                             kHudAlpha);

        char frameTimeText[32];
        float frameTimeMs = (m_Fps.currentFps > 0) ? (1000.0f / m_Fps.currentFps) : 0.0f;
        std::snprintf(frameTimeText, sizeof(frameTimeText), "%.2fms", frameTimeMs);
        textWidth = m_Renderer->GetTextWidth(frameTimeText, 1.0f);
        m_Renderer->DrawText(frameTimeText,
                             glm::vec2(rightMargin - textWidth, 32.0f + kLineHeight * 2),
                             1.0f,
                             kRightColor,
                             2.0f,
                             kHudAlpha);

        char zoomText[32];
        std::snprintf(zoomText, sizeof(zoomText), "Zoom: %.1fx", m_Camera.GetState().zoom);
        textWidth = m_Renderer->GetTextWidth(zoomText, 1.0f);
        m_Renderer->DrawText(zoomText,
                             glm::vec2(rightMargin - textWidth, 32.0f + kLineHeight * 3),
                             1.0f,
                             kRightColor,
                             2.0f,
                             kHudAlpha);

        char drawCallText[32];
        std::snprintf(drawCallText, sizeof(drawCallText), "Draws: %d", m_Fps.currentDrawCalls);
        textWidth = m_Renderer->GetTextWidth(drawCallText, 1.0f);
        m_Renderer->DrawText(drawCallText,
                             glm::vec2(rightMargin - textWidth, 32.0f + kLineHeight * 4),
                             1.0f,
                             kRightColor,
                             2.0f,
                             kHudAlpha);
    }

    m_Console.Render(*m_Renderer, m_ScreenWidth, m_ScreenHeight);

    m_Renderer->EndFrame();

    if (m_RendererAPI == RendererAPI::OpenGL)
    {
        glfwSwapBuffers(m_Window);
    }

    m_Fps.drawCallAccumulator += m_Renderer->GetDrawCallCount();
}

void Game::RenderTitleContent()
{
    glm::mat4 uiProjection = MakeUIProjection(m_ScreenWidth, m_ScreenHeight);
    m_Renderer->SetProjection(uiProjection);

    const float screenW = static_cast<float>(m_ScreenWidth);
    const float screenH = static_cast<float>(m_ScreenHeight);
    const float uiScale = viewScaling::MenuUiScale(
        m_ScreenWidth, m_ScreenHeight, MENU_REFERENCE_WIDTH, MENU_REFERENCE_HEIGHT);

    const std::string logoText = "RIFT";
    const float logoWidth = m_Renderer->GetTextWidthLarge(logoText, TITLE_LOGO_SCALE * uiScale);
    const float logoX = std::floor((screenW - logoWidth) * 0.5f);
    const float logoY = std::floor(screenH * 0.22f);
    m_Renderer->DrawTextLarge(logoText,
                              glm::vec2(logoX, logoY),
                              TITLE_LOGO_SCALE * uiScale,
                              TITLE_TEXT_COLOR,
                              TITLE_LOGO_OUTLINE,
                              1.0f);

    const float menuTopY = std::floor(screenH * 0.50f);
    const float lineHeight = MENU_LINE_HEIGHT * uiScale;
    for (int i = 0; i < TITLE_ITEM_COUNT; ++i)
    {
        const bool enabled = m_TitleMenu.enabled[i];
        const bool selected = (m_TitleMenu.selected == i);
        const std::string& label = TITLE_LABELS[i];
        const std::string display =
            selected ? std::string("> ") + label : std::string("  ") + label;

        const float w = m_Renderer->GetTextWidth(display, MENU_ITEM_SCALE * uiScale);
        const float x = std::floor((screenW - w) * 0.5f);
        const float y = menuTopY + i * lineHeight;
        m_Renderer->DrawText(display,
                             glm::vec2(x, y),
                             MENU_ITEM_SCALE * uiScale,
                             MenuItemColor(selected, enabled),
                             MENU_ITEM_OUTLINE,
                             1.0f);
    }

    RenderVersionFooter();
}

void Game::RenderVersionFooter()
{
    // Leave UI projection active; callers restore it before drawing world geometry.
    m_Renderer->SetProjection(MakeUIProjection(m_ScreenWidth, m_ScreenHeight));

    const float screenW = static_cast<float>(m_ScreenWidth);
    const float screenH = static_cast<float>(m_ScreenHeight);
    const float uiScale = viewScaling::MenuUiScale(
        m_ScreenWidth, m_ScreenHeight, MENU_REFERENCE_WIDTH, MENU_REFERENCE_HEIGHT);
    const float textScale = FOOTER_TEXT_SCALE * uiScale;
    const float footerY = screenH - FOOTER_BASELINE_OFFSET * uiScale;
    const float horizontalMargin = FOOTER_HORIZONTAL_MARGIN * uiScale;

    m_Renderer->DrawText(FOOTER_WATERMARK_TEXT,
                         glm::vec2(horizontalMargin, footerY),
                         textScale,
                         TITLE_DISABLED_COLOR,
                         1.0f,
                         0.85f);

    const std::string versionText = std::string("rift ") + RIFT_VERSION;
    const float versionWidth = m_Renderer->GetTextWidth(versionText, textScale);
    m_Renderer->DrawText(versionText,
                         glm::vec2(screenW - versionWidth - horizontalMargin, footerY),
                         textScale,
                         TITLE_DISABLED_COLOR,
                         1.0f,
                         0.85f);
}

void Game::RenderConfirmOverwritePrompt()
{
    glm::mat4 uiProjection = MakeUIProjection(m_ScreenWidth, m_ScreenHeight);
    m_Renderer->SetProjection(uiProjection);

    const float screenW = static_cast<float>(m_ScreenWidth);
    const float screenH = static_cast<float>(m_ScreenHeight);

    m_Renderer->DrawColoredRect(
        glm::vec2(0.0f, 0.0f), glm::vec2(screenW, screenH), MODAL_BACKDROP_COLOR);

    const float boxW = std::floor(screenW * 0.55f);
    const float boxH = std::floor(screenH * 0.30f);
    const float boxX = std::floor((screenW - boxW) * 0.5f);
    const float boxY = std::floor((screenH - boxH) * 0.5f);
    m_Renderer->DrawColoredRect(glm::vec2(boxX, boxY), glm::vec2(boxW, boxH), MODAL_BOX_COLOR);

    const std::string line1 = "Starting a new game will overwrite";
    const std::string line2 = "your existing save. Continue?";
    const float l1w = m_Renderer->GetTextWidth(line1, MODAL_TEXT_SCALE);
    const float l2w = m_Renderer->GetTextWidth(line2, MODAL_TEXT_SCALE);
    const float lineH = 32.0f;
    const float textY = boxY + boxH * 0.30f;
    m_Renderer->DrawText(line1,
                         glm::vec2(std::floor(boxX + (boxW - l1w) * 0.5f), textY),
                         MODAL_TEXT_SCALE,
                         TITLE_TEXT_COLOR,
                         1.0f,
                         1.0f);
    m_Renderer->DrawText(line2,
                         glm::vec2(std::floor(boxX + (boxW - l2w) * 0.5f), textY + lineH),
                         MODAL_TEXT_SCALE,
                         TITLE_TEXT_COLOR,
                         1.0f,
                         1.0f);

    const std::string cancelLabel = "Cancel";
    const std::string confirmLabel = "New Game";
    const bool confirmSelected = (m_ConfirmPrompt.selected == MenuLogic::ConfirmChoice::Confirm);

    const std::string cancelDisplay =
        !confirmSelected ? std::string("> ") + cancelLabel : std::string("  ") + cancelLabel;
    const std::string confirmDisplay =
        confirmSelected ? std::string("> ") + confirmLabel : std::string("  ") + confirmLabel;

    const float buttonScale = 1.1f;
    const float cancelW = m_Renderer->GetTextWidth(cancelDisplay, buttonScale);
    const float confirmW = m_Renderer->GetTextWidth(confirmDisplay, buttonScale);
    const float buttonY = boxY + boxH * 0.72f;
    const float cancelX = std::floor(boxX + boxW * 0.30f - cancelW * 0.5f);
    const float confirmX = std::floor(boxX + boxW * 0.70f - confirmW * 0.5f);

    m_Renderer->DrawText(cancelDisplay,
                         glm::vec2(cancelX, buttonY),
                         buttonScale,
                         confirmSelected ? TITLE_DIM_COLOR : TITLE_HIGHLIGHT_COLOR,
                         MENU_ITEM_OUTLINE,
                         1.0f);
    m_Renderer->DrawText(confirmDisplay,
                         glm::vec2(confirmX, buttonY),
                         buttonScale,
                         confirmSelected ? TITLE_HIGHLIGHT_COLOR : TITLE_DIM_COLOR,
                         MENU_ITEM_OUTLINE,
                         1.0f);
}

void Game::RenderPauseOverlay()
{
    if (m_PauseMenu.enabled.size() != static_cast<size_t>(PAUSE_ITEM_COUNT))
    {
        m_PauseMenu.enabled.assign(PAUSE_ITEM_COUNT, true);
        m_PauseMenu.selected = 0;
    }

    glm::mat4 uiProjection = MakeUIProjection(m_ScreenWidth, m_ScreenHeight);
    m_Renderer->SetProjection(uiProjection);

    const float screenW = static_cast<float>(m_ScreenWidth);
    const float screenH = static_cast<float>(m_ScreenHeight);
    const float uiScale = viewScaling::MenuUiScale(
        m_ScreenWidth, m_ScreenHeight, MENU_REFERENCE_WIDTH, MENU_REFERENCE_HEIGHT);

    m_Renderer->DrawColoredRect(
        glm::vec2(0.0f, 0.0f), glm::vec2(screenW, screenH), PAUSE_DIM_COLOR);

    const std::string headerText = "-- PAUSED --";
    const float headerW = m_Renderer->GetTextWidth(headerText, PAUSE_HEADER_SCALE * uiScale);
    const float headerX = std::floor((screenW - headerW) * 0.5f);
    const float headerY = std::floor(screenH * 0.32f);
    m_Renderer->DrawText(headerText,
                         glm::vec2(headerX, headerY),
                         PAUSE_HEADER_SCALE * uiScale,
                         TITLE_TEXT_COLOR,
                         2.5f,
                         1.0f);

    const float menuTopY = std::floor(screenH * 0.52f);
    const float lineHeight = MENU_LINE_HEIGHT * uiScale;
    for (int i = 0; i < PAUSE_ITEM_COUNT; ++i)
    {
        const bool selected = (m_PauseMenu.selected == i);
        const std::string& label = PAUSE_LABELS[i];
        const std::string display =
            selected ? std::string("> ") + label : std::string("  ") + label;

        const float w = m_Renderer->GetTextWidth(display, MENU_ITEM_SCALE * uiScale);
        const float x = std::floor((screenW - w) * 0.5f);
        const float y = menuTopY + i * lineHeight;
        m_Renderer->DrawText(display,
                             glm::vec2(x, y),
                             MENU_ITEM_SCALE * uiScale,
                             MenuItemColor(selected, true),
                             MENU_ITEM_OUTLINE,
                             1.0f);
    }
}
