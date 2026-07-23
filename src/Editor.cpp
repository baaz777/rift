#include "Editor.hpp"

#include "CameraController.hpp"
#include "Logger.hpp"
#include "MathUtils.hpp"
#include "NavigationRecalc.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Editor";
}  // Namespace

static constexpr glm::vec4 LAYER_COLORS[] = {
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.2f, 0.5f, 1.0f, 0.4f},
    {0.2f, 1.0f, 0.2f, 0.4f},
    {1.0f, 0.2f, 0.8f, 0.4f},
    {1.0f, 0.5f, 0.0f, 0.4f},
    {1.0f, 1.0f, 0.2f, 0.4f},
    {0.2f, 1.0f, 1.0f, 0.4f},
    {1.0f, 0.3f, 0.3f, 0.4f},
    {1.0f, 0.3f, 1.0f, 0.4f},
    {1.0f, 1.0f, 1.0f, 0.4f},
};

Editor::Editor()
    : m_Active(false),
      m_ShowTilePicker(false),
      m_EditMode(EditMode::None),
      m_CurrentStance(TileStance::Prop),

      m_CurrentParticleType(ParticleType::Firefly),
      m_ParticleNoProjection(false),
      m_PlacingParticleZone(false),
      m_ParticleZoneStart(0.0f, 0.0f),

      m_CurrentStructureId(-1),
      m_PlacingAnchor(0),
      m_TempLeftAnchor(-1.0f, -1.0f),
      m_TempRightAnchor(-1.0f, -1.0f),
      m_AssigningTilesToStructure(false),

      m_AnimationFrameDuration(0.2f),  // Seconds per frame
      m_SelectedAnimationId(-1),

      m_DebugMode(false),
      m_ShowDebugInfo(false),
      m_ShowNoProjectionAnchors(false),
      m_HasUnsavedChanges(false),

      m_SelectedTileID(0),
      m_CurrentLayer(0),
      m_CurrentElevation(4),
      m_CurrentElevationRole(ElevationRole::Raised),

      m_SelectedNPCTypeIndex(0),

      m_Mouse{},
      m_TilePicker{},
      m_MultiTile{},

      m_LastDeletedTileX(-1),
      m_LastDeletedTileY(-1)
{
}

void Editor::Initialize(const std::vector<std::string>& npcTypes)
{
    m_AvailableNPCTypes = npcTypes;
    m_SelectedNPCTypeIndex = 0;

    if (!m_AvailableNPCTypes.empty())
    {
        std::ostringstream line;
        line << "Available NPC types: ";
        for (size_t i = 0; i < m_AvailableNPCTypes.size(); ++i)
        {
            line << m_AvailableNPCTypes[i];
            if (i == m_SelectedNPCTypeIndex)
            {
                line << " (selected)";
            }
            if (i < m_AvailableNPCTypes.size() - 1)
            {
                line << ", ";
            }
        }
        Logger::Info(LOG_SUBSYSTEM, line.str());
    }
}

void Editor::SetActive(bool active)
{
    m_Active = active;
    if (active)
    {
        m_ShowTilePicker = true;
        m_TilePicker.targetOffsetX = m_TilePicker.offsetX;
        m_TilePicker.targetOffsetY = m_TilePicker.offsetY;
    }
    else
    {
        m_ShowTilePicker = false;
    }
}

void Editor::ToggleDebugMode()
{
    SetDebugMode(!m_DebugMode);
}

void Editor::ToggleShowDebugInfo()
{
    SetShowDebugInfo(!m_ShowDebugInfo);
}

void Editor::SetDebugMode(bool enabled)
{
    m_DebugMode = enabled;
    m_ShowNoProjectionAnchors = m_DebugMode;
    Logger::InfoF(LOG_SUBSYSTEM, "Debug mode: {}", m_DebugMode ? "ON" : "OFF");
}

void Editor::SetShowDebugInfo(bool enabled)
{
    m_ShowDebugInfo = enabled;
    Logger::InfoF(LOG_SUBSYSTEM, "Debug info display: {}", m_ShowDebugInfo ? "ON" : "OFF");
}

void Editor::ResetTilePickerState()
{
    m_TilePicker.zoom = 2.0f;
    m_TilePicker.offsetX = 0.0f;
    m_TilePicker.offsetY = 0.0f;
    m_TilePicker.targetOffsetX = 0.0f;
    m_TilePicker.targetOffsetY = 0.0f;
    Logger::Info(LOG_SUBSYSTEM, "Tile picker zoom and offset reset to defaults");
}

void Editor::Update(float deltaTime, const EditorContext& ctx)
{
    if (m_Active && m_ShowTilePicker)
    {
        float dt = rift::ExpApproachAlpha(deltaTime, 0.16f);

        m_TilePicker.offsetX =
            m_TilePicker.offsetX + (m_TilePicker.targetOffsetX - m_TilePicker.offsetX) * dt;
        m_TilePicker.offsetY =
            m_TilePicker.offsetY + (m_TilePicker.targetOffsetY - m_TilePicker.offsetY) * dt;

        if (std::abs(m_TilePicker.targetOffsetX - m_TilePicker.offsetX) < 0.1f)
        {
            m_TilePicker.offsetX = m_TilePicker.targetOffsetX;
        }
        if (std::abs(m_TilePicker.targetOffsetY - m_TilePicker.offsetY) < 0.1f)
        {
            m_TilePicker.offsetY = m_TilePicker.targetOffsetY;
        }
    }
    else
    {
        m_TilePicker.offsetX = m_TilePicker.targetOffsetX;
        m_TilePicker.offsetY = m_TilePicker.targetOffsetY;
    }

    if (m_StatusTimer > 0.0f)
    {
        m_StatusTimer = std::max(0.0f, m_StatusTimer - deltaTime);
        if (m_StatusTimer <= 0.0f)
            m_StatusMessage.clear();
    }
}

void Editor::ShowStatus(std::string message, glm::vec3 color, float durationSeconds)
{
    m_StatusMessage = std::move(message);
    m_StatusColor = color;
    m_StatusTimer = durationSeconds;
}

void Editor::ClearUndoHistory()
{
    m_UndoStack.Clear();
}

Editor::ScreenToTile Editor::ScreenToTileCoords(const EditorContext& ctx,
                                                double mouseX,
                                                double mouseY) const
{
    if (ctx.screenWidth <= 0 || ctx.screenHeight <= 0 || ctx.tilemap.GetTileWidth() <= 0 ||
        ctx.tilemap.GetTileHeight() <= 0)
    {
        return {};
    }

    float zoom = std::max(ctx.camera.zoom, 0.01f);
    float worldW = static_cast<float>(ctx.tilesVisibleWidth * ctx.tilemap.GetTileWidth()) / zoom;
    float worldH = static_cast<float>(ctx.tilesVisibleHeight * ctx.tilemap.GetTileHeight()) / zoom;
    float worldX = (static_cast<float>(mouseX) / static_cast<float>(ctx.screenWidth)) * worldW +
                   ctx.camera.position.x;
    float worldY = (static_cast<float>(mouseY) / static_cast<float>(ctx.screenHeight)) * worldH +
                   ctx.camera.position.y;

    return {worldX,
            worldY,
            static_cast<int>(std::floor(worldX / ctx.tilemap.GetTileWidth())),
            static_cast<int>(std::floor(worldY / ctx.tilemap.GetTileHeight()))};
}

void Editor::ExecuteEditorCommand(std::unique_ptr<EditorCommand> cmd,
                                  Tilemap& tilemap,
                                  entt::registry& npcs)
{
    if (!cmd)
        return;

    m_UndoStack.Execute(std::move(cmd), tilemap, npcs);
    MarkDirty();
}

void Editor::PushEditorCommand(std::unique_ptr<EditorCommand> cmd)
{
    if (!cmd)
        return;

    m_UndoStack.Push(std::move(cmd));
    MarkDirty();
}

void Editor::ClearAllEditModes()
{
    // Clear transient state before changing modes to avoid stale previews.
    m_EditMode = EditMode::None;

    m_PlacingParticleZone = false;

    m_PlacingAnchor = 0;
    m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
    m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
    m_AssigningTilesToStructure = false;

    m_AnimationFrames.clear();
    m_SelectedAnimationId = -1;

    m_Mouse.mousePressed = false;
    m_Mouse.rightMousePressed = false;

    // Drop undo data for an unfinished drag; applied tile changes remain.
    m_TileStroke.Drop();
    m_CollisionStroke.Drop();
    m_ElevationStroke.Drop();
    m_NavigationStroke.Drop();
}

void Editor::Render(const EditorContext& ctx)
{
    m_NoProjBoundsCached = false;

    if (m_Active && m_ShowTilePicker)
    {
        RenderEditorUI(ctx);
    }

    if ((m_Active || m_DebugMode) && !m_ShowTilePicker)
    {
        RenderCollisionOverlays(ctx);
        RenderNavigationOverlays(ctx);
        RenderStanceOverlays(ctx);
        RenderElevationOverlays(ctx);
        RenderStructureOverlays(ctx);
        RenderYSortPlusOverlays(ctx);
        RenderYSortMinusOverlays(ctx);
    }

    if (m_Active && !m_ShowTilePicker)
    {
        if (!m_DebugMode && m_CurrentLayer >= 1 && m_CurrentLayer <= 9)
        {
            RenderLayerOverlay(ctx, m_CurrentLayer, LAYER_COLORS[m_CurrentLayer]);
        }

        RenderPlacementPreview(ctx);
        RenderMapSelectionOverlay(ctx);
    }

    if (m_DebugMode && !m_ShowTilePicker)
    {
        RenderCornerCuttingOverlays(ctx);
        RenderParticleZoneOverlays(ctx);
        RenderNPCDebugInfo(ctx);

        for (int i = 1; i <= 9; ++i)
        {
            RenderLayerOverlay(ctx, i, LAYER_COLORS[i]);
        }
    }

    if (m_Active)
    {
        RenderEditorTopBar(ctx);
        RenderEditorHUD(ctx);
    }

    // Draw the toast last: it binds a UI projection. Game restores the world projection.
    if (m_Active && m_StatusTimer > 0.0f && !m_StatusMessage.empty())
    {
        glm::mat4 uiProjection = glm::ortho(0.0f,
                                            static_cast<float>(ctx.screenWidth),
                                            static_cast<float>(ctx.screenHeight),
                                            0.0f,
                                            -1.0f,
                                            1.0f);
        ctx.renderer.SetProjection(uiProjection);
        const glm::vec2 pos(20.0f,
                            static_cast<float>(ctx.screenHeight) - EDITOR_HUD_HEIGHT - 24.0f);
        ctx.renderer.DrawText(m_StatusMessage, pos, 0.5f, m_StatusColor);
    }
}

void Editor::RenderNoProjectionAnchors(const EditorContext& ctx)
{
    RenderNoProjectionAnchorsImpl(ctx);
}
