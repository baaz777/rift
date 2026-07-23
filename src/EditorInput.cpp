#include "Appearance.hpp"
#include "CameraController.hpp"
#include "Dialogues.hpp"
#include "Editor.hpp"
#include "EditorBrushTransform.hpp"
#include "EditorCommands.hpp"
#include "EntityStore.hpp"
#include "Logger.hpp"
#include "Patrol.hpp"
#include "PlayerSystem.hpp"
#include "TileMath.hpp"
#include "Transform.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Editor";

template <typename ConditionFn, typename ActionFn>
int FloodFill(
    Tilemap& tilemap, int startX, int startY, ConditionFn shouldProcess, ActionFn applyAction)
{
    int mapWidth = tilemap.GetMapWidth();
    int mapHeight = tilemap.GetMapHeight();
    std::vector<bool> visited(static_cast<size_t>(mapWidth) * static_cast<size_t>(mapHeight),
                              false);
    std::vector<std::pair<int, int>> stack;
    stack.push_back({startX, startY});
    int count = 0;
    while (!stack.empty())
    {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        if (cx < 0 || cx >= mapWidth || cy < 0 || cy >= mapHeight)
            continue;
        size_t idx = static_cast<size_t>(cy * mapWidth + cx);
        if (visited[idx])
            continue;
        if (!shouldProcess(cx, cy))
            continue;
        visited[idx] = true;
        applyAction(cx, cy);
        count++;
        stack.push_back({cx - 1, cy});
        stack.push_back({cx + 1, cy});
        stack.push_back({cx, cy - 1});
        stack.push_back({cx, cy + 1});
    }
    return count;
}

}  // Anonymous namespace

void Editor::ProcessInput(float deltaTime, const EditorContext& ctx)
{
    if (glfwGetKey(ctx.window, GLFW_KEY_T) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_T] && m_Active)
    {
        m_ShowTilePicker = !m_ShowTilePicker;
        m_KeyPressed[GLFW_KEY_T] = true;
        Logger::InfoF(LOG_SUBSYSTEM, "Tile picker: {}", m_ShowTilePicker ? "SHOWN" : "HIDDEN");

        if (m_ShowTilePicker)
        {
            // Sync smooth-scroll target so the view doesn't jump on open.
            m_TilePicker.targetOffsetX = m_TilePicker.offsetX;
            m_TilePicker.targetOffsetY = m_TilePicker.offsetY;
            std::vector<int> validTiles = ctx.tilemap.GetValidTileIDs();
            Logger::InfoF(LOG_SUBSYSTEM, "Total valid tiles available: {}", validTiles.size());
            Logger::InfoF(LOG_SUBSYSTEM, "Currently selected tile ID: {}", m_SelectedTileID);
        }
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_T) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_T] = false;
    }

    // Tile picker pan with arrow keys (Shift = 2.5x speed). Target-based
    // smooth scrolling.
    if (m_Active && m_ShowTilePicker)
    {
        float scrollSpeed = 1000.0f * deltaTime;

        if (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            scrollSpeed *= 2.5f;
        }

        if (glfwGetKey(ctx.window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            m_TilePicker.targetOffsetY += scrollSpeed;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            m_TilePicker.targetOffsetY -= scrollSpeed;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            m_TilePicker.targetOffsetX += scrollSpeed;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            m_TilePicker.targetOffsetX -= scrollSpeed;
        }

        int dataTilesPerRow = ctx.tilemap.GetTilesetDataWidth() / ctx.tilemap.GetTileWidth();
        int dataTilesPerCol = ctx.tilemap.GetTilesetDataHeight() / ctx.tilemap.GetTileHeight();

        float baseTileSizePixels =
            (static_cast<float>(ctx.screenWidth) / static_cast<float>(dataTilesPerRow)) * 1.5f;
        float tileSizePixels = baseTileSizePixels * m_TilePicker.zoom;

        float totalTilesWidth = tileSizePixels * dataTilesPerRow;
        float totalTilesHeight = tileSizePixels * dataTilesPerCol;

        // Clamp so the user can't scroll past the content edges.
        float minOffsetX = std::min(0.0f, ctx.screenWidth - totalTilesWidth);
        float maxOffsetX = 0.0f;
        float minOffsetY = std::min(0.0f, ctx.screenHeight - totalTilesHeight);
        float maxOffsetY = 0.0f;

        m_TilePicker.targetOffsetX =
            std::max(minOffsetX, std::min(maxOffsetX, m_TilePicker.targetOffsetX));
        m_TilePicker.targetOffsetY =
            std::max(minOffsetY, std::min(maxOffsetY, m_TilePicker.targetOffsetY));
    }

    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_M) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_M])
    {
        const bool enabling = m_EditMode != EditMode::Navigation;
        ClearAllEditModes();
        if (enabling)
            m_EditMode = EditMode::Navigation;
        Logger::InfoF(LOG_SUBSYSTEM, "Navigation edit mode: {}", enabling ? "ON" : "OFF");
        m_KeyPressed[GLFW_KEY_M] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_M) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_M] = false;
    }

    // N selects NPC placement; F handles the particle projection override.
    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_N) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_N])
    {
        const bool enabling = m_EditMode != EditMode::NPCPlacement;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::NPCPlacement;
            ClampNPCTypeIndex();
            if (!m_AvailableNPCTypes.empty())
            {
                Logger::InfoF(LOG_SUBSYSTEM,
                              "NPC placement mode: ON - Selected NPC: {}",
                              m_AvailableNPCTypes[m_SelectedNPCTypeIndex]);
                Logger::Info(LOG_SUBSYSTEM,
                             "Press , (comma) and . (period) to cycle through NPC types");
            }
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "NPC placement mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_N] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_N) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_N] = false;
    }

    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_H) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_H])
    {
        const bool enabling = m_EditMode != EditMode::Elevation;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::Elevation;
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Elevation edit mode: ON - Current elevation: {} pixels, role: {}",
                          m_CurrentElevation,
                          EnumTraits<ElevationRole>::ToString(m_CurrentElevationRole));
            Logger::Info(LOG_SUBSYSTEM, "Use scroll wheel to adjust elevation value");
            Logger::Info(LOG_SUBSYSTEM, "  , / . cycle the role a click paints");
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Elevation edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_H] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_H) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_H] = false;
    }

    // Exclude Ground; right-click clears the cell.
    if (m_Active && (m_EditMode == EditMode::Elevation))
    {
        constexpr int PAINTABLE = static_cast<int>(ELEVATION_ROLE_COUNT) - 1;
        const auto cycleRole = [this](int delta)
        {
            const int current = static_cast<int>(std::to_underlying(m_CurrentElevationRole)) - 1;
            const int next = (current + PAINTABLE + delta) % PAINTABLE;
            m_CurrentElevationRole = static_cast<ElevationRole>(next + 1);
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Paint elevation role: {}",
                          EnumTraits<ElevationRole>::ToString(m_CurrentElevationRole));
        };

        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            cycleRole(-1);
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_COMMA] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            cycleRole(1);
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_PERIOD] = false;
    }

    // B: stance mode. LMB paints the selected stance, RMB clears to Flat, both on
    // the current layer only. Comma and period cycle the painted stance.
    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_B) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_B])
    {
        const bool enabling = m_EditMode != EditMode::Stance;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::Stance;
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Stance edit mode: ON (Layer {}) - painting {}",
                          m_CurrentLayer,
                          EnumTraits<TileStance>::ToString(m_CurrentStance));
            Logger::Info(LOG_SUBSYSTEM,
                         "  Prop = turns to the camera, Wall = locked to the grid, "
                         "Structure = one tile of a taller body");
            Logger::Info(LOG_SUBSYSTEM,
                         "  , / . cycle stance | Shift+click floods | RMB clears to Flat");
            Logger::Info(LOG_SUBSYSTEM, "Use 1-9 and 0 keys to change layer");
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Stance edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_B] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_B] = false;
    }

    // Exclude Flat; right-click clears the cell.
    if (m_Active && (m_EditMode == EditMode::Stance))
    {
        constexpr int PAINTABLE = static_cast<int>(TILE_STANCE_COUNT) - 1;
        const auto cycleStance = [this](int delta)
        {
            const int current = static_cast<int>(std::to_underlying(m_CurrentStance)) - 1;
            const int next = (current + PAINTABLE + delta) % PAINTABLE;
            m_CurrentStance = static_cast<TileStance>(next + 1);
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Paint stance: {}",
                          EnumTraits<TileStance>::ToString(m_CurrentStance));
        };

        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            cycleStance(-1);
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_COMMA] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            cycleStance(1);
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_PERIOD] = false;
    }

    // Y: Y-sort-plus mode. LMB sets flag (tile sorts with entities by Y),
    // RMB clears. Skipped when Ctrl held - Ctrl+Y is redo.
    {
        const bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                              glfwGetKey(ctx.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        if (m_Active && !ctrlHeld && glfwGetKey(ctx.window, GLFW_KEY_Y) == GLFW_PRESS &&
            !m_KeyPressed[GLFW_KEY_Y])
        {
            const bool enabling = m_EditMode != EditMode::YSortPlus;
            ClearAllEditModes();
            if (enabling)
            {
                m_EditMode = EditMode::YSortPlus;
                Logger::InfoF(
                    LOG_SUBSYSTEM,
                    "Y-sort+1 edit mode: ON (Layer {}) - Click to mark tiles for Y-sorting "
                    "with entities",
                    m_CurrentLayer);
                Logger::Info(LOG_SUBSYSTEM, "Use 1-9 and 0 keys to change layer");
            }
            else
            {
                Logger::Info(LOG_SUBSYSTEM, "Y-sort-plus edit mode: OFF");
            }
            m_KeyPressed[GLFW_KEY_Y] = true;
        }
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_Y) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_Y] = false;
    }

    // O: Y-sort-minus mode. LMB sets flag (tile renders in front of player at
    // same Y), RMB clears. Only affects tiles already Y-sort-plus.
    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_O) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_O])
    {
        const bool enabling = m_EditMode != EditMode::YSortMinus;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::YSortMinus;
            Logger::InfoF(LOG_SUBSYSTEM, "Y-SORT-1 EDIT MODE: ON (Layer {})", m_CurrentLayer);
            Logger::Info(LOG_SUBSYSTEM, "Click the BOTTOM tile of a structure to mark it");
            Logger::Info(LOG_SUBSYSTEM, "(All tiles above will inherit the setting)");
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Y-sort-minus edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_O] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_O) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_O] = false;
    }

    // J: particle zone mode. LMB-drag creates a zone, RMB removes,
    // with comma and period cycling the particle type.
    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_J) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_J])
    {
        const bool enabling = m_EditMode != EditMode::ParticleZone;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::ParticleZone;
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Particle zone edit mode: ON - Type: {}",
                          EnumTraits<ParticleType>::ToString(m_CurrentParticleType));
            Logger::Info(LOG_SUBSYSTEM,
                         "Click and drag to place zones, use , and . to change type, F to "
                         "toggle noProjection override");
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Particle zone edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_J] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_J) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_J] = false;
    }

    if (m_Active && (m_EditMode == EditMode::ParticleZone))
    {
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            constexpr int N = static_cast<int>(EnumTraits<ParticleType>::Count);
            int type = (static_cast<int>(m_CurrentParticleType) + N - 1) % N;
            m_CurrentParticleType = static_cast<ParticleType>(type);
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Particle type: {}",
                          EnumTraits<ParticleType>::ToString(m_CurrentParticleType));
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_COMMA] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            constexpr int N = static_cast<int>(EnumTraits<ParticleType>::Count);
            int type = (static_cast<int>(m_CurrentParticleType) + 1) % N;
            m_CurrentParticleType = static_cast<ParticleType>(type);
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Particle type: {}",
                          EnumTraits<ParticleType>::ToString(m_CurrentParticleType));
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_PERIOD] = false;

        // Toggles manual noProjection override for new particle zones.
        // Auto-detection from tiles is always active; this is for forcing
        // noProjection on/off. Bound to F ("Flat projection") because N is
        // already claimed globally by the NPC-placement mode toggle.
        if (glfwGetKey(ctx.window, GLFW_KEY_F) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_F])
        {
            m_ParticleNoProjection = !m_ParticleNoProjection;
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Particle noProjection override: {}",
                          m_ParticleNoProjection ? "ON (forced)" : "OFF (auto-detect)");
            m_KeyPressed[GLFW_KEY_F] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_F) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_F] = false;
    }

    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_G) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_G])
    {
        const bool enabling = m_EditMode != EditMode::Structure;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::Structure;
            Logger::InfoF(LOG_SUBSYSTEM, "STRUCTURE EDIT MODE: ON (Layer {})", m_CurrentLayer + 1);
            Logger::Info(LOG_SUBSYSTEM, "Click = toggle no-projection");
            Logger::Info(LOG_SUBSYSTEM, "Shift+click = flood-fill no-projection");
            Logger::Info(LOG_SUBSYSTEM, "Ctrl+click = place anchors (left, then right)");
            Logger::Info(LOG_SUBSYSTEM, ", . = select existing structures");
            Logger::Info(LOG_SUBSYSTEM, "Delete = remove selected structure");
            Logger::InfoF(
                LOG_SUBSYSTEM, "Structures: {}", ctx.tilemap.GetNoProjectionStructureCount());
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Structure edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_G] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_G) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_G] = false;
    }

    if (m_Active && (m_EditMode == EditMode::Structure))
    {
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            size_t count = ctx.tilemap.GetNoProjectionStructureCount();
            if (count > 0)
            {
                if (m_CurrentStructureId < 0)
                    m_CurrentStructureId = static_cast<int>(count) - 1;
                else
                    m_CurrentStructureId = (m_CurrentStructureId - 1 + static_cast<int>(count)) %
                                           static_cast<int>(count);

                const NoProjectionStructure* s =
                    ctx.tilemap.GetNoProjectionStructure(m_CurrentStructureId);
                if (s)
                {
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Selected structure {}: \"{}\" anchors: ({},{}) - ({},{})",
                                  m_CurrentStructureId,
                                  s->name,
                                  s->leftAnchor.x,
                                  s->leftAnchor.y,
                                  s->rightAnchor.x,
                                  s->rightAnchor.y);
                }
            }
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_COMMA] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            size_t count = ctx.tilemap.GetNoProjectionStructureCount();
            if (count > 0)
            {
                m_CurrentStructureId = (m_CurrentStructureId + 1) % static_cast<int>(count);

                const NoProjectionStructure* s =
                    ctx.tilemap.GetNoProjectionStructure(m_CurrentStructureId);
                if (s)
                {
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Selected structure {}: \"{}\" anchors: ({},{}) - ({},{})",
                                  m_CurrentStructureId,
                                  s->name,
                                  s->leftAnchor.x,
                                  s->leftAnchor.y,
                                  s->rightAnchor.x,
                                  s->rightAnchor.y);
                }
            }
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_PERIOD] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS &&
            !m_KeyPressed[GLFW_KEY_ESCAPE] && m_PlacingAnchor != 0)
        {
            m_PlacingAnchor = 0;
            m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
            m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
            Logger::Info(LOG_SUBSYSTEM, "Anchor placement cancelled");
            m_KeyPressed[GLFW_KEY_ESCAPE] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_ESCAPE] = false;

        // Structure deletion bypasses undo, including cleared and renumbered tile references.
        if (glfwGetKey(ctx.window, GLFW_KEY_DELETE) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_DELETE])
        {
            if (m_CurrentStructureId >= 0)
            {
                Logger::InfoF(LOG_SUBSYSTEM, "Removed structure {}", m_CurrentStructureId);
                ctx.tilemap.RemoveNoProjectionStructure(m_CurrentStructureId);
                MarkDirty();
                m_CurrentStructureId = -1;
            }
            m_KeyPressed[GLFW_KEY_DELETE] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_DELETE) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_DELETE] = false;
    }

    // Toggles animated tile creation mode. When active:
    //   - Click tiles in the tile picker to add frames to animation
    //   - Press Enter to create the animation and apply to selected map tile
    //   - Press escape to cancel/clear frames
    //   - Use , and . To adjust frame duration
    if (m_Active && glfwGetKey(ctx.window, GLFW_KEY_K) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_K])
    {
        const bool enabling = m_EditMode != EditMode::Animation;
        ClearAllEditModes();
        if (enabling)
        {
            m_EditMode = EditMode::Animation;
            Logger::Info(LOG_SUBSYSTEM, "Animation edit mode: ON");
            Logger::Info(LOG_SUBSYSTEM,
                         "Click tiles in picker to add frames, Enter to create, Esc to cancel");
            Logger::Info(LOG_SUBSYSTEM,
                         "Left-click map to apply animation, Right-click to remove animation");
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Use , and . to adjust frame duration (current: {}s)",
                          m_AnimationFrameDuration);
        }
        else
        {
            Logger::Info(LOG_SUBSYSTEM, "Animation edit mode: OFF");
        }
        m_KeyPressed[GLFW_KEY_K] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_K) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_K] = false;
    }

    if (m_Active && (m_EditMode == EditMode::Animation))
    {
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            m_AnimationFrameDuration = std::max(0.05f, m_AnimationFrameDuration - 0.05f);
            Logger::InfoF(LOG_SUBSYSTEM, "Animation frame duration: {}s", m_AnimationFrameDuration);
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_COMMA] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            m_AnimationFrameDuration = std::min(2.0f, m_AnimationFrameDuration + 0.05f);
            Logger::InfoF(LOG_SUBSYSTEM, "Animation frame duration: {}s", m_AnimationFrameDuration);
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_PERIOD] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_ESCAPE])
        {
            m_AnimationFrames.clear();
            m_SelectedAnimationId = -1;
            Logger::Info(LOG_SUBSYSTEM, "Animation frames/selection cleared");
            m_KeyPressed[GLFW_KEY_ESCAPE] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_ESCAPE] = false;

        if (glfwGetKey(ctx.window, GLFW_KEY_ENTER) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_ENTER])
        {
            if (m_AnimationFrames.size() >= 2)
            {
                AnimatedTile anim(m_AnimationFrames, m_AnimationFrameDuration);
                auto cmd = std::make_unique<AddAnimatedTileCmd>(anim);
                AddAnimatedTileCmd* cmdPtr = cmd.get();
                ExecuteEditorCommand(std::move(cmd), ctx.tilemap, ctx.npcs);
                int animId = cmdPtr->AnimId();
                m_SelectedAnimationId = animId;
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Created animation #{} with {} frames at {}s per frame",
                              animId,
                              m_AnimationFrames.size(),
                              m_AnimationFrameDuration);
                Logger::Info(LOG_SUBSYSTEM,
                             "Click on map tiles to apply this animation (Esc to cancel)");
                m_AnimationFrames.clear();
                m_ShowTilePicker = false;
            }
            else
            {
                Logger::Info(LOG_SUBSYSTEM, "Need at least 2 frames to create animation");
            }
            m_KeyPressed[GLFW_KEY_ENTER] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_ENTER) == GLFW_RELEASE)
            m_KeyPressed[GLFW_KEY_ENTER] = false;
    }

    // Cycles through available NPC types when in NPC placement mode.
    // Comma (,) previous type, period (.) next type
    // wraps around at list boundaries.
    ClampNPCTypeIndex();
    if (m_Active && (m_EditMode == EditMode::NPCPlacement) && !m_AvailableNPCTypes.empty())
    {
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_COMMA])
        {
            if (m_SelectedNPCTypeIndex > 0)
            {
                m_SelectedNPCTypeIndex--;
            }
            else
            {
                m_SelectedNPCTypeIndex = m_AvailableNPCTypes.size() - 1;
            }
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Selected NPC type: {} ({}/{})",
                          m_AvailableNPCTypes[m_SelectedNPCTypeIndex],
                          m_SelectedNPCTypeIndex + 1,
                          m_AvailableNPCTypes.size());
            m_KeyPressed[GLFW_KEY_COMMA] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_COMMA) == GLFW_RELEASE)
        {
            m_KeyPressed[GLFW_KEY_COMMA] = false;
        }

        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_PERIOD])
        {
            m_SelectedNPCTypeIndex = (m_SelectedNPCTypeIndex + 1) % m_AvailableNPCTypes.size();
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Selected NPC type: {} ({}/{})",
                          m_AvailableNPCTypes[m_SelectedNPCTypeIndex],
                          m_SelectedNPCTypeIndex + 1,
                          m_AvailableNPCTypes.size());
            m_KeyPressed[GLFW_KEY_PERIOD] = true;
        }
        if (glfwGetKey(ctx.window, GLFW_KEY_PERIOD) == GLFW_RELEASE)
        {
            m_KeyPressed[GLFW_KEY_PERIOD] = false;
        }
    }

    if (glfwGetKey(ctx.window, GLFW_KEY_S) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_S] && m_Active)
    {
        glm::vec2 playerPos = ctx.npcs.get<Transform>(ctx.playerEntity).position;
        int playerTileX =
            TileMath::TileIndex(playerPos.x, static_cast<float>(ctx.tilemap.GetTileWidth()));
        int playerTileY =
            TileMath::StandingTileRow(playerPos.y, static_cast<float>(ctx.tilemap.GetTileHeight()));
        int characterType =
            static_cast<int>(ctx.npcs.get<Appearance>(ctx.playerEntity).characterType);

        const std::string savePath = ctx.saveMapPath.empty() ? "rift.save.json" : ctx.saveMapPath;
        if (ctx.tilemap.SaveMapToJSON(savePath, &ctx.npcs, playerTileX, playerTileY, characterType))
        {
            Logger::InfoF(LOG_SUBSYSTEM,
                          "Save successful! Player at tile ({}, {}), character type: {}",
                          playerTileX,
                          playerTileY,
                          characterType);
            MarkClean();
            ShowStatus("Saved map", glm::vec3(0.4f, 1.0f, 0.4f));
        }
        else
        {
            Logger::ErrorF(LOG_SUBSYSTEM, "Save FAILED to write {}", savePath);
            ShowStatus("SAVE FAILED - check console", glm::vec3(1.0f, 0.3f, 0.3f), 5.0f);
        }
        m_KeyPressed[GLFW_KEY_S] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_S) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_S] = false;
    }

    // Reloads the game state from the configured map JSON, replacing all current state.
    // Also restores player position, character type, and recenters camera.
    if (glfwGetKey(ctx.window, GLFW_KEY_L) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_L] && m_Active)
    {
        int loadedPlayerTileX = -1;
        int loadedPlayerTileY = -1;
        int loadedCharacterType = -1;
        const std::string savePath = ctx.saveMapPath.empty() ? "rift.save.json" : ctx.saveMapPath;
        if (ctx.tilemap.LoadMapFromJSON(
                savePath, &ctx.npcs, &loadedPlayerTileX, &loadedPlayerTileY, &loadedCharacterType))
        {
            Logger::Info(LOG_SUBSYSTEM, "Save loaded successfully!");
            MarkClean();
            ShowStatus("Loaded map", glm::vec3(0.4f, 1.0f, 0.4f));

            // Discard undo history - any captured commands reference the old
            // tilemap state and would corrupt the loaded map if reverted.
            ClearUndoHistory();
            m_MapSelection = MapRegionSelection{};
            m_Clipboard = ClipboardRegion{};

            if (loadedCharacterType >= 0)
            {
                PlayerSystem::SwitchCharacter(
                    ctx.npcs, ctx.playerEntity, static_cast<CharacterType>(loadedCharacterType));
                Logger::InfoF(
                    LOG_SUBSYSTEM, "Player character restored to type {}", loadedCharacterType);
            }

            if (loadedPlayerTileX >= 0 && loadedPlayerTileY >= 0)
            {
                PlayerSystem::SetTilePosition(
                    ctx.npcs, ctx.playerEntity, loadedPlayerTileX, loadedPlayerTileY);

                glm::vec2 playerPos = ctx.npcs.get<Transform>(ctx.playerEntity).position;
                float camWorldWidth =
                    static_cast<float>(ctx.tilesVisibleWidth * ctx.tilemap.GetTileWidth());
                float camWorldHeight =
                    static_cast<float>(ctx.tilesVisibleHeight * ctx.tilemap.GetTileHeight());
                glm::vec2 playerVisualCenter = glm::vec2(playerPos.x, playerPos.y - 16.0f);
                ctx.camera.position =
                    playerVisualCenter - glm::vec2(camWorldWidth / 2.0f, camWorldHeight / 2.0f);
                ctx.camera.followTarget = ctx.camera.position;
                ctx.camera.hasFollowTarget = false;
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Player position restored to tile ({}, {})",
                              loadedPlayerTileX,
                              loadedPlayerTileY);
            }
        }
        else
        {
            Logger::Error(LOG_SUBSYSTEM, "Failed to reload map!");
            ShowStatus("LOAD FAILED - check console", glm::vec3(1.0f, 0.3f, 0.3f), 5.0f);
        }
        m_KeyPressed[GLFW_KEY_L] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_L) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_L] = false;
    }

    // Delete-drag bypasses undo and clears animation assignments too.
    if (glfwGetKey(ctx.window, GLFW_KEY_DELETE) == GLFW_PRESS && m_Active && !m_ShowTilePicker)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(ctx.window, &mouseX, &mouseY);
        auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
        int tileX = st.tileX;
        int tileY = st.tileY;

        bool isNewTile = (tileX != m_LastDeletedTileX || tileY != m_LastDeletedTileY);

        if (isNewTile && tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
            tileY < ctx.tilemap.GetMapHeight())
        {
            ctx.tilemap.SetLayerTile(tileX, tileY, m_CurrentLayer, -1);
            ctx.tilemap.SetTileAnimation(tileX, tileY, static_cast<int>(m_CurrentLayer), -1);
            MarkDirty();
            m_LastDeletedTileX = tileX;
            m_LastDeletedTileY = tileY;
        }
        m_KeyPressed[GLFW_KEY_DELETE] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_DELETE) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_DELETE] = false;
        m_LastDeletedTileX = -1;
        m_LastDeletedTileY = -1;
    }

    // R updates both brush and hovered tile. Keep one handler: both share one debounce bit.
    if (glfwGetKey(ctx.window, GLFW_KEY_R) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_R] && m_Active &&
        !m_ShowTilePicker)
    {
        // Single-tile placement also reads brush rotation.
        m_MultiTile.rotation = (m_MultiTile.rotation + 90) % 360;
        Logger::InfoF(LOG_SUBSYSTEM, "Brush rotation: {} degrees", m_MultiTile.rotation);

        double mouseX, mouseY;
        glfwGetCursorPos(ctx.window, &mouseX, &mouseY);
        auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
        const bool inBounds = st.tileX >= 0 && st.tileX < ctx.tilemap.GetMapWidth() &&
                              st.tileY >= 0 && st.tileY < ctx.tilemap.GetMapHeight();
        const int tileId =
            inBounds ? ctx.tilemap.GetLayerTile(st.tileX, st.tileY, m_CurrentLayer) : -1;

        if (tileId >= 0)
        {
            PlaceTilesCmd::Entry entry{};
            entry.tileX = st.tileX;
            entry.tileY = st.tileY;
            entry.layer = m_CurrentLayer;
            entry.oldTileId = tileId;
            entry.newTileId = tileId;
            entry.oldRotation = ctx.tilemap.GetLayerRotation(st.tileX, st.tileY, m_CurrentLayer);
            entry.newRotation = std::fmod(entry.oldRotation + 90.0f, 360.0f);
            entry.oldFlipX = ctx.tilemap.GetLayerFlipX(st.tileX, st.tileY, m_CurrentLayer);
            entry.newFlipX = entry.oldFlipX;
            entry.oldFlipY = ctx.tilemap.GetLayerFlipY(st.tileX, st.tileY, m_CurrentLayer);
            entry.newFlipY = entry.oldFlipY;

            std::vector<PlaceTilesCmd::Entry> entries;
            entries.push_back(entry);
            ExecuteEditorCommand(
                std::make_unique<PlaceTilesCmd>(std::move(entries)), ctx.tilemap, ctx.npcs);

            Logger::InfoF(LOG_SUBSYSTEM,
                          "Rotated Layer {} tile at ({}, {}) to {} degrees",
                          m_CurrentLayer + 1,
                          st.tileX,
                          st.tileY,
                          entry.newRotation);
            ShowStatus("Rotated tile (90 deg)", glm::vec3(0.6f, 1.0f, 0.6f));
        }
        m_KeyPressed[GLFW_KEY_R] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_R) == GLFW_RELEASE)
    {
        m_KeyPressed[GLFW_KEY_R] = false;
    }

    // F / Shift+F mirror the brush flip flag (matching R's rotate contract) and
    // reflect the current selection on the same press. Gated outside ParticleZone
    // where F drives the per-zone noProjection toggle.
    if (m_Active && !m_ShowTilePicker && m_EditMode != EditMode::ParticleZone &&
        glfwGetKey(ctx.window, GLFW_KEY_F) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_F])
    {
        const bool shiftHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                               glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        const bool flipXAxis = !shiftHeld;
        const char axisName = flipXAxis ? 'X' : 'Y';

        if (flipXAxis)
            m_MultiTile.flipX = !m_MultiTile.flipX;
        else
            m_MultiTile.flipY = !m_MultiTile.flipY;
        Logger::InfoF(LOG_SUBSYSTEM,
                      "Brush flip: X={} Y={}",
                      m_MultiTile.flipX ? "on" : "off",
                      m_MultiTile.flipY ? "on" : "off");

        if (m_MapSelection.active)
        {
            const int w = m_MapSelection.Width();
            const int h = m_MapSelection.Height();
            ClipboardRegion region = PasteRegionCmd::SnapshotRegion(
                ctx.tilemap, m_MapSelection.MinX(), m_MapSelection.MinY(), w, h);
            ReflectClipboardRegion(region, flipXAxis);
            ExecuteEditorCommand(std::make_unique<PasteRegionCmd>(
                                     m_MapSelection.MinX(), m_MapSelection.MinY(), region),
                                 ctx.tilemap,
                                 ctx.npcs);
            ShowStatus(std::string("Reflected ") + std::to_string(w) + "x" + std::to_string(h) +
                           " (" + axisName + "-axis)",
                       glm::vec3(0.6f, 1.0f, 0.6f));
        }
        else
        {
            double mouseX, mouseY;
            glfwGetCursorPos(ctx.window, &mouseX, &mouseY);
            auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
            if (st.tileX >= 0 && st.tileX < ctx.tilemap.GetMapWidth() && st.tileY >= 0 &&
                st.tileY < ctx.tilemap.GetMapHeight())
            {
                PlaceTilesCmd::Entry entry{};
                entry.tileX = st.tileX;
                entry.tileY = st.tileY;
                entry.layer = m_CurrentLayer;
                entry.oldTileId = ctx.tilemap.GetLayerTile(st.tileX, st.tileY, m_CurrentLayer);
                entry.newTileId = entry.oldTileId;
                entry.oldRotation =
                    ctx.tilemap.GetLayerRotation(st.tileX, st.tileY, m_CurrentLayer);
                float newRot = std::fmod(360.0f - entry.oldRotation, 360.0f);
                if (newRot < 0.0f)
                    newRot += 360.0f;
                entry.newRotation = newRot;
                entry.oldFlipX = ctx.tilemap.GetLayerFlipX(st.tileX, st.tileY, m_CurrentLayer);
                entry.oldFlipY = ctx.tilemap.GetLayerFlipY(st.tileX, st.tileY, m_CurrentLayer);
                entry.newFlipX = flipXAxis ? !entry.oldFlipX : entry.oldFlipX;
                entry.newFlipY = flipXAxis ? entry.oldFlipY : !entry.oldFlipY;

                std::vector<PlaceTilesCmd::Entry> entries;
                entries.push_back(entry);
                ExecuteEditorCommand(
                    std::make_unique<PlaceTilesCmd>(std::move(entries)), ctx.tilemap, ctx.npcs);
                ShowStatus(std::string("Flipped tile (") + axisName + "-axis)",
                           glm::vec3(0.6f, 1.0f, 0.6f));
            }
            else
            {
                ShowStatus("No selection (Ctrl+drag a region or hover a tile)",
                           glm::vec3(0.7f, 0.7f, 0.7f),
                           1.5f);
            }
        }
        m_KeyPressed[GLFW_KEY_F] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_F) == GLFW_RELEASE)
        m_KeyPressed[GLFW_KEY_F] = false;

    // Selects which tile layer to edit.
    // Layer switching: keys 1-9,0 map to dynamic layers 0-9
    static constexpr struct
    {
        int key;
        int layerIndex;
        const char* name;
    } kLayerKeys[] = {
        {GLFW_KEY_1, 0, "Layer 1: Ground (background)"},
        {GLFW_KEY_2, 1, "Layer 2: Ground Detail (background)"},
        {GLFW_KEY_3, 2, "Layer 3: Objects (background)"},
        {GLFW_KEY_4, 3, "Layer 4: Objects2 (background)"},
        {GLFW_KEY_5, 4, "Layer 5: Objects3 (background)"},
        {GLFW_KEY_6, 5, "Layer 6: Foreground (foreground)"},
        {GLFW_KEY_7, 6, "Layer 7: Foreground2 (foreground)"},
        {GLFW_KEY_8, 7, "Layer 8: Overlay (foreground)"},
        {GLFW_KEY_9, 8, "Layer 9: Overlay2 (foreground)"},
        {GLFW_KEY_0, 9, "Layer 10: Overlay3 (foreground)"},
    };
    for (const auto& [key, layerIndex, name] : kLayerKeys)
    {
        if (glfwGetKey(ctx.window, key) == GLFW_PRESS && !m_KeyPressed[key] && m_Active)
        {
            m_CurrentLayer = layerIndex;
            Logger::InfoF(LOG_SUBSYSTEM, "Switched to {}", name);
            m_KeyPressed[key] = true;
        }
        if (glfwGetKey(ctx.window, key) == GLFW_RELEASE)
            m_KeyPressed[key] = false;
    }

    // Check Ctrl separately; KeyToggle fires for either key in a variadic list.
    // Game also handles Z, so undo resets world and picker zoom.
    const bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(ctx.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    if (m_Active && ctrlHeld && glfwGetKey(ctx.window, GLFW_KEY_Z) == GLFW_PRESS &&
        !m_KeyPressed[GLFW_KEY_Z])
    {
        const std::string label = m_UndoStack.UndoLabel();
        if (m_UndoStack.Undo(ctx.tilemap, ctx.npcs))
        {
            MarkDirty();
            ShowStatus("Undo: " + label, glm::vec3(0.7f, 0.85f, 1.0f));
        }
        else
        {
            ShowStatus("Nothing to undo", glm::vec3(0.7f, 0.7f, 0.7f), 1.5f);
        }
        m_KeyPressed[GLFW_KEY_Z] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_Z) == GLFW_RELEASE)
        m_KeyPressed[GLFW_KEY_Z] = false;

    if (m_Active && ctrlHeld && glfwGetKey(ctx.window, GLFW_KEY_Y) == GLFW_PRESS &&
        !m_KeyPressed[GLFW_KEY_Y])
    {
        const std::string label = m_UndoStack.RedoLabel();
        if (m_UndoStack.Redo(ctx.tilemap, ctx.npcs))
        {
            MarkDirty();
            ShowStatus("Redo: " + label, glm::vec3(0.7f, 1.0f, 0.85f));
        }
        else
        {
            ShowStatus("Nothing to redo", glm::vec3(0.7f, 0.7f, 0.7f), 1.5f);
        }
        m_KeyPressed[GLFW_KEY_Y] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_Y) == GLFW_RELEASE)
        m_KeyPressed[GLFW_KEY_Y] = false;

    if (m_Active && ctrlHeld && glfwGetKey(ctx.window, GLFW_KEY_C) == GLFW_PRESS &&
        !m_KeyPressed[GLFW_KEY_C])
    {
        if (m_MapSelection.active)
        {
            int w = m_MapSelection.Width();
            int h = m_MapSelection.Height();
            m_Clipboard = PasteRegionCmd::SnapshotRegion(
                ctx.tilemap, m_MapSelection.MinX(), m_MapSelection.MinY(), w, h);
            ShowStatus("Copied " + std::to_string(w) + "x" + std::to_string(h) + " region (" +
                           std::to_string(w * h) + " tiles)",
                       glm::vec3(0.6f, 1.0f, 0.6f));
        }
        else
        {
            ShowStatus(
                "No region selected (Ctrl+drag to select)", glm::vec3(0.7f, 0.7f, 0.7f), 1.5f);
        }
        m_KeyPressed[GLFW_KEY_C] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_C) == GLFW_RELEASE)
        m_KeyPressed[GLFW_KEY_C] = false;

    if (m_Active && ctrlHeld && glfwGetKey(ctx.window, GLFW_KEY_V) == GLFW_PRESS &&
        !m_KeyPressed[GLFW_KEY_V])
    {
        if (!m_Clipboard.Empty())
        {
            double mouseX, mouseY;
            glfwGetCursorPos(ctx.window, &mouseX, &mouseY);
            auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
            ExecuteEditorCommand(std::make_unique<PasteRegionCmd>(st.tileX, st.tileY, m_Clipboard),
                                 ctx.tilemap,
                                 ctx.npcs);
            ShowStatus("Pasted " + std::to_string(m_Clipboard.width) + "x" +
                           std::to_string(m_Clipboard.height) + " at (" + std::to_string(st.tileX) +
                           ", " + std::to_string(st.tileY) + ")",
                       glm::vec3(0.6f, 1.0f, 0.6f));
        }
        else
        {
            ShowStatus("Clipboard empty (Ctrl+C to copy first)", glm::vec3(0.7f, 0.7f, 0.7f), 1.5f);
        }
        m_KeyPressed[GLFW_KEY_V] = true;
    }
    if (glfwGetKey(ctx.window, GLFW_KEY_V) == GLFW_RELEASE)
        m_KeyPressed[GLFW_KEY_V] = false;

    if (m_Active && m_MapSelection.active &&
        glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !m_KeyPressed[GLFW_KEY_ESCAPE])
    {
        m_MapSelection = MapRegionSelection{};
        ShowStatus("Cleared selection", glm::vec3(0.7f, 0.7f, 0.7f), 1.0f);
        m_KeyPressed[GLFW_KEY_ESCAPE] = true;
    }
}

void Editor::ProcessMouseInput(const EditorContext& ctx)
{
    double mouseX, mouseY;
    glfwGetCursorPos(ctx.window, &mouseX, &mouseY);

    int leftMouseButton = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT);
    int rightMouseButton = glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_RIGHT);
    bool leftMouseDown = (leftMouseButton == GLFW_PRESS);
    bool rightMouseDown = (rightMouseButton == GLFW_PRESS);

    // Selection takes priority outside Structure mode. Releasing Ctrl freezes its extent,
    // but continue consuming the button until mouse-up to prevent accidental painting.
    {
        bool ctrlHeld = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(ctx.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        bool inStructureMode = (m_EditMode == EditMode::Structure);
        if (m_Active && !m_ShowTilePicker && !inStructureMode)
        {
            if (m_MapSelection.isDragging)
            {
                if (leftMouseDown)
                {
                    if (ctrlHeld)
                    {
                        auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
                        m_MapSelection.endX = st.tileX;
                        m_MapSelection.endY = st.tileY;
                    }
                    m_Mouse.lastMouseX = mouseX;
                    m_Mouse.lastMouseY = mouseY;
                    return;
                }
                else
                {
                    m_MapSelection.isDragging = false;
                    int w = m_MapSelection.Width();
                    int h = m_MapSelection.Height();
                    ShowStatus("Selection: " + std::to_string(w) + "x" + std::to_string(h) +
                                   " at (" + std::to_string(m_MapSelection.MinX()) + ", " +
                                   std::to_string(m_MapSelection.MinY()) + ") - Ctrl+C to copy",
                               glm::vec3(0.7f, 0.85f, 1.0f));

                    m_Mouse.lastMouseX = mouseX;
                    m_Mouse.lastMouseY = mouseY;
                    return;
                }
            }
            else if (ctrlHeld && leftMouseDown)
            {
                auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
                m_MapSelection.active = true;
                m_MapSelection.isDragging = true;
                m_MapSelection.startX = st.tileX;
                m_MapSelection.startY = st.tileY;
                m_MapSelection.endX = st.tileX;
                m_MapSelection.endY = st.tileY;
                m_Mouse.lastMouseX = mouseX;
                m_Mouse.lastMouseY = mouseY;
                return;
            }
        }
    }

    // Right-click toggles collision or navigation flags depending on mode.
    // Supports drag-to-draw: first click sets target state, dragging applies it.
    if (rightMouseDown && !m_ShowTilePicker)
    {
        auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
        float worldX = st.worldX;
        float worldY = st.worldY;
        int tileX = st.tileX;
        int tileY = st.tileY;

        bool isNewNavigationTilePosition =
            (tileX != m_Mouse.lastNavigationTileX || tileY != m_Mouse.lastNavigationTileY);
        bool isNewCollisionTilePosition =
            (tileX != m_Mouse.lastCollisionTileX || tileY != m_Mouse.lastCollisionTileY);

        if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
            tileY < ctx.tilemap.GetMapHeight())
        {
            if ((m_EditMode == EditMode::Animation))
            {
                int currentAnim = ctx.tilemap.GetTileAnimation(tileX, tileY, m_CurrentLayer);
                if (currentAnim >= 0)
                {
                    int oldTileId = ctx.tilemap.GetLayerTile(tileX, tileY, m_CurrentLayer);
                    ExecuteEditorCommand(
                        std::make_unique<SetTileAnimationCmd>(
                            std::vector<SetTileAnimationCmd::Entry>{
                                {tileX, tileY, m_CurrentLayer, currentAnim, -1, oldTileId}}),
                        ctx.tilemap,
                        ctx.npcs);
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Removed animation from tile ({}, {}) on layer {}",
                                  tileX,
                                  tileY,
                                  m_CurrentLayer);
                }
                m_Mouse.rightMousePressed = true;
                return;
            }
            // Elevation edit mode, right-click clears elevation at tile.
            // Right-click is a single action rather than a drag, so it executes
            // directly rather than going through the stroke accumulator.
            else if ((m_EditMode == EditMode::Elevation))
            {
                const int oldElevation = ctx.tilemap.GetElevation(tileX, tileY);
                const ElevationRole oldRole =
                    ctx.tilemap.GetLayerElevationRole(tileX, tileY, m_CurrentLayer);

                std::vector<std::unique_ptr<EditorCommand>> children;
                if (oldElevation != 0)
                {
                    children.push_back(std::make_unique<ElevationSetCmd>(
                        std::vector<ElevationSetCmd::Entry>{{tileX, tileY, oldElevation, 0}}));
                }
                if (oldRole != ElevationRole::Ground)
                {
                    children.push_back(
                        std::make_unique<SetElevationRolesCmd>(std::vector<LayerElevationRoleEntry>{
                            {tileX,
                             tileY,
                             static_cast<std::size_t>(m_CurrentLayer),
                             oldRole,
                             ElevationRole::Ground}}));
                }
                if (children.size() == 1)
                {
                    ExecuteEditorCommand(std::move(children[0]), ctx.tilemap, ctx.npcs);
                }
                else if (!children.empty())
                {
                    ExecuteEditorCommand(
                        std::make_unique<CompositeCmd>("Clear elevation", std::move(children)),
                        ctx.tilemap,
                        ctx.npcs);
                }
                Logger::InfoF(
                    LOG_SUBSYSTEM, "Cleared elevation and role at ({}, {})", tileX, tileY);
                m_Mouse.rightMousePressed = true;
            }
            // Structure edit mode, right-click clears structure assignment from tiles
            // Shift+right-click, flood-fill to clear all connected tiles
            else if ((m_EditMode == EditMode::Structure))
            {
                bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                std::vector<SetTileStructureIdsCmd::Entry> structEntries;
                // +1 converts to Tilemap's 1-based structureId layer indexing (see
                // Tilemap::GetTileStructureId, where layer 0 always returns -1). It is not
                // the 1-based layer number that log text shows the user.
                int layer = m_CurrentLayer + 1;
                if (shiftHeld)
                {
                    int count = FloodFill(
                        ctx.tilemap,
                        tileX,
                        tileY,
                        [&](int cx, int cy)
                        { return ctx.tilemap.GetTileStructureId(cx, cy, layer) >= 0; },
                        [&](int cx, int cy)
                        {
                            int oldId = ctx.tilemap.GetTileStructureId(cx, cy, layer);
                            structEntries.push_back({cx, cy, layer, oldId, -1});
                            ctx.tilemap.SetTileStructureId(cx, cy, layer, -1);
                        });
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Cleared structure assignment from {} tiles (layer {})",
                                  count,
                                  layer);
                }
                else
                {
                    int oldId = ctx.tilemap.GetTileStructureId(tileX, tileY, layer);
                    if (oldId >= 0)
                    {
                        structEntries.push_back({tileX, tileY, layer, oldId, -1});
                        ctx.tilemap.SetTileStructureId(tileX, tileY, layer, -1);
                    }
                    Logger::InfoF(
                        LOG_SUBSYSTEM, "Cleared structure assignment at ({}, {})", tileX, tileY);
                }
                if (!structEntries.empty())
                    PushEditorCommand(
                        std::make_unique<SetTileStructureIdsCmd>(std::move(structEntries)));
                m_Mouse.rightMousePressed = true;
            }
            // Clear the current layer, or its connected non-Flat region with Shift.
            else if ((m_EditMode == EditMode::Stance))
            {
                bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                const size_t layer = m_CurrentLayer;
                std::vector<LayerStanceEntry> entries;
                if (shiftHeld)
                {
                    int count = FloodFill(
                        ctx.tilemap,
                        tileX,
                        tileY,
                        [&](int cx, int cy)
                        { return ctx.tilemap.GetLayerStance(cx, cy, layer) != TileStance::Flat; },
                        [&](int cx, int cy)
                        {
                            const TileStance oldStance = ctx.tilemap.GetLayerStance(cx, cy, layer);
                            if (oldStance != TileStance::Flat)
                            {
                                entries.push_back({cx, cy, layer, oldStance, TileStance::Flat});
                                ctx.tilemap.SetLayerStance(cx, cy, layer, TileStance::Flat);
                            }
                        });
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Cleared stance on {} connected tiles (layer {})",
                                  count,
                                  layer);
                }
                else
                {
                    const TileStance oldStance = ctx.tilemap.GetLayerStance(tileX, tileY, layer);
                    if (oldStance != TileStance::Flat)
                    {
                        entries.push_back({tileX, tileY, layer, oldStance, TileStance::Flat});
                        ctx.tilemap.SetLayerStance(tileX, tileY, layer, TileStance::Flat);
                    }
                    Logger::InfoF(
                        LOG_SUBSYSTEM, "Stance at ({}, {}) layer {} -> Flat", tileX, tileY, layer);
                }
                if (!entries.empty())
                    PushEditorCommand(std::make_unique<SetTileStancesCmd>(std::move(entries)));
                m_Mouse.rightMousePressed = true;
            }
            // Y-sort-plus / Y-sort-minus edit modes share the same clear logic.
            // Right-click clears flag on single tile; Shift+right-click flood-clears.
            else if (m_EditMode == EditMode::YSortPlus || m_EditMode == EditMode::YSortMinus)
            {
                bool isPlus = (m_EditMode == EditMode::YSortPlus);
                auto getter = isPlus ? &Tilemap::GetLayerYSortPlus : &Tilemap::GetLayerYSortMinus;
                auto setter = isPlus ? &Tilemap::SetLayerYSortPlus : &Tilemap::SetLayerYSortMinus;
                const char* label = isPlus ? "Y-sort-plus" : "Y-sort-minus";

                auto entries =
                    CollectYSortFlagToggle(ctx, tileX, tileY, getter, setter, false, label);
                if (!entries.empty())
                {
                    if (isPlus)
                        PushEditorCommand(std::make_unique<YSortPlusToggleCmd>(std::move(entries)));
                    else
                        PushEditorCommand(
                            std::make_unique<YSortMinusToggleCmd>(std::move(entries)));
                }
                m_Mouse.rightMousePressed = true;
            }

            else if ((m_EditMode == EditMode::ParticleZone))
            {
                auto* zones = ctx.tilemap.GetParticleZonesMutable();
                for (size_t i = 0; i < zones->size(); ++i)
                {
                    const ParticleZone& zone = (*zones)[i];
                    if (worldX >= zone.position.x && worldX < zone.position.x + zone.size.x &&
                        worldY >= zone.position.y && worldY < zone.position.y + zone.size.y)
                    {
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Removed {} zone at ({}, {})",
                                      EnumTraits<ParticleType>::ToString(zone.type),
                                      zone.position.x,
                                      zone.position.y);
                        ctx.particles.OnZoneRemoved(static_cast<int>(i));
                        ExecuteEditorCommand(
                            std::make_unique<RemoveParticleZoneCmd>(i), ctx.tilemap, ctx.npcs);
                        break;
                    }
                }
                m_Mouse.rightMousePressed = true;
            }
            else if ((m_EditMode == EditMode::Navigation))
            {
                // Defer NPC displacement and patrol rebuilding to mouse-up for one undo snapshot.
                if (!m_Mouse.rightMousePressed)
                {
                    bool walkable = ctx.tilemap.GetNavigation(tileX, tileY);
                    m_Mouse.navigationDragState = !walkable;
                    if (!m_NavigationStroke.IsActive())
                        m_NavigationStroke.Begin();
                    m_NavigationStroke.Touch(tileX, tileY, walkable, m_Mouse.navigationDragState);
                    ctx.tilemap.SetNavigation(tileX, tileY, m_Mouse.navigationDragState);
                    MarkDirty();
                    Logger::Info(LOG_SUBSYSTEM, "=== NAVIGATION DRAG START ===");
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Tile ({}, {}): {} -> {}",
                                  tileX,
                                  tileY,
                                  walkable ? "ON" : "OFF",
                                  m_Mouse.navigationDragState ? "ON" : "OFF");
                    m_Mouse.lastNavigationTileX = tileX;
                    m_Mouse.lastNavigationTileY = tileY;
                    m_Mouse.rightMousePressed = true;
                }
                else if (isNewNavigationTilePosition)
                {
                    bool currentWalkable = ctx.tilemap.GetNavigation(tileX, tileY);
                    if (currentWalkable != m_Mouse.navigationDragState)
                    {
                        m_NavigationStroke.Touch(
                            tileX, tileY, currentWalkable, m_Mouse.navigationDragState);
                        ctx.tilemap.SetNavigation(tileX, tileY, m_Mouse.navigationDragState);
                        MarkDirty();
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Navigation drag: Tile ({}, {}) -> {}",
                                      tileX,
                                      tileY,
                                      m_Mouse.navigationDragState ? "ON" : "OFF");
                    }
                    m_Mouse.lastNavigationTileX = tileX;
                    m_Mouse.lastNavigationTileY = tileY;
                }
            }
            else
            {
                // Collision editing mode, support drag-to-draw. Stroke
                // accumulator collapses the drag into one composite cmd.
                if (!m_Mouse.rightMousePressed)
                {
                    bool currentCollision = ctx.tilemap.GetTileCollision(tileX, tileY);
                    m_Mouse.collisionDragState = !currentCollision;
                    if (!m_CollisionStroke.IsActive())
                        m_CollisionStroke.Begin();
                    m_CollisionStroke.Touch(
                        tileX, tileY, currentCollision, m_Mouse.collisionDragState);
                    ctx.tilemap.SetTileCollision(tileX, tileY, m_Mouse.collisionDragState);
                    MarkDirty();
                    Logger::Info(LOG_SUBSYSTEM, "=== COLLISION DRAG START ===");
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Tile ({}, {}): {} -> {}",
                                  tileX,
                                  tileY,
                                  currentCollision ? "ON" : "OFF",
                                  m_Mouse.collisionDragState ? "ON" : "OFF");
                    m_Mouse.lastCollisionTileX = tileX;
                    m_Mouse.lastCollisionTileY = tileY;
                    m_Mouse.rightMousePressed = true;
                }
                else if (isNewCollisionTilePosition)
                {
                    bool currentCollision = ctx.tilemap.GetTileCollision(tileX, tileY);
                    if (currentCollision != m_Mouse.collisionDragState)
                    {
                        m_CollisionStroke.Touch(
                            tileX, tileY, currentCollision, m_Mouse.collisionDragState);
                        ctx.tilemap.SetTileCollision(tileX, tileY, m_Mouse.collisionDragState);
                        MarkDirty();
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Collision drag: Tile ({}, {}) -> {}",
                                      tileX,
                                      tileY,
                                      m_Mouse.collisionDragState ? "ON" : "OFF");
                    }
                    m_Mouse.lastCollisionTileX = tileX;
                    m_Mouse.lastCollisionTileY = tileY;
                }
            }
        }
        else
        {
            if (!m_Mouse.rightMousePressed)
            {
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Right-click outside map bounds (tileX={} tileY={} map size={}x{})",
                              tileX,
                              tileY,
                              ctx.tilemap.GetMapWidth(),
                              ctx.tilemap.GetMapHeight());
            }
        }
    }
    else if (!rightMouseDown)
    {
        m_Mouse.rightMousePressed = false;

        m_Mouse.lastNavigationTileX = -1;
        m_Mouse.lastNavigationTileY = -1;
        m_Mouse.lastCollisionTileX = -1;
        m_Mouse.lastCollisionTileY = -1;

        if (m_CollisionStroke.IsActive())
            m_CollisionStroke.Commit(m_UndoStack);

        if (m_NavigationStroke.IsActive())
            m_NavigationStroke.Commit(m_UndoStack, ctx.tilemap, ctx.npcs);
    }

    if (m_ShowTilePicker)
    {
        int dataTilesPerRow = ctx.tilemap.GetTilesetDataWidth() / ctx.tilemap.GetTileWidth();
        int dataTilesPerCol = ctx.tilemap.GetTilesetDataHeight() / ctx.tilemap.GetTileHeight();
        int totalTiles = dataTilesPerRow * dataTilesPerCol;
        int tilesPerRow = dataTilesPerRow;
        float baseTileSize =
            (static_cast<float>(ctx.screenWidth) / static_cast<float>(tilesPerRow)) * 1.5f;
        float tileSize = baseTileSize * m_TilePicker.zoom;

        if (leftMouseDown && !m_Mouse.mousePressed && !m_MultiTile.isSelecting)
        {
            if (mouseX >= 0 && mouseX < ctx.screenWidth && mouseY >= 0 && mouseY < ctx.screenHeight)
            {
                double adjustedMouseX = mouseX - m_TilePicker.offsetX;
                double adjustedMouseY = mouseY - m_TilePicker.offsetY;
                int pickerTileX = static_cast<int>(adjustedMouseX / tileSize);
                int pickerTileY = static_cast<int>(adjustedMouseY / tileSize);
                int localIndex = pickerTileY * tilesPerRow + pickerTileX;
                int clickedTileID = localIndex;

                if (clickedTileID >= 0 && clickedTileID < totalTiles)
                {
                    if ((m_EditMode == EditMode::Animation))
                    {
                        m_AnimationFrames.push_back(clickedTileID);
                        m_Mouse.mousePressed = true;
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Added animation frame: {} (total frames: {})",
                                      clickedTileID,
                                      m_AnimationFrames.size());
                    }
                    else
                    {
                        m_MultiTile.isSelecting = true;
                        m_MultiTile.selectionStartTileID = clickedTileID;
                        m_SelectedTileID = clickedTileID;
                        m_Mouse.mousePressed = true;  // Prevent other click handlers from firing
                        Logger::InfoF(
                            LOG_SUBSYSTEM,
                            "Started selection at tile ID: {} (mouse: {}, {}, adjusted: {}, "
                            "{}, offset: {}, {})",
                            clickedTileID,
                            mouseX,
                            mouseY,
                            adjustedMouseX,
                            adjustedMouseY,
                            m_TilePicker.offsetX,
                            m_TilePicker.offsetY);
                    }
                }
            }
        }

        if (leftMouseDown && m_MultiTile.isSelecting)
        {
            if (mouseX >= 0 && mouseX < ctx.screenWidth && mouseY >= 0 && mouseY < ctx.screenHeight)
            {
                double adjustedMouseX = mouseX - m_TilePicker.offsetX;
                double adjustedMouseY = mouseY - m_TilePicker.offsetY;
                int pickerTileX = static_cast<int>(adjustedMouseX / tileSize);
                int pickerTileY = static_cast<int>(adjustedMouseY / tileSize);
                int localIndex = pickerTileY * tilesPerRow + pickerTileX;
                int clickedTileID = localIndex;

                if (clickedTileID >= 0 && clickedTileID < totalTiles)
                {
                    m_SelectedTileID = clickedTileID;
                }
            }
        }

        if (!leftMouseDown && (m_EditMode == EditMode::Animation) && m_Mouse.mousePressed)
        {
            m_Mouse.mousePressed = false;
        }

        if (!leftMouseDown && m_MultiTile.isSelecting)
        {
            if (m_MultiTile.selectionStartTileID >= 0)
            {
                int startTileID = m_MultiTile.selectionStartTileID;
                int endTileID = m_SelectedTileID;

                int startX = startTileID % dataTilesPerRow;
                int startY = startTileID / dataTilesPerRow;
                int endX = endTileID % dataTilesPerRow;
                int endY = endTileID / dataTilesPerRow;

                int minX = std::min(startX, endX);
                int maxX = std::max(startX, endX);
                int minY = std::min(startY, endY);
                int maxY = std::max(startY, endY);

                m_MultiTile.selectedStartID = minY * dataTilesPerRow + minX;
                m_MultiTile.width = maxX - minX + 1;
                m_MultiTile.height = maxY - minY + 1;

                if (m_MultiTile.width > 1 || m_MultiTile.height > 1)
                {
                    // Multi-tile selection, enable placement mode,
                    // but do not change the world camera or zoom.
                    m_MultiTile.selectionMode = true;
                    m_MultiTile.isPlacing = true;
                    m_MultiTile.rotation = 0;
                    m_MultiTile.flipX = false;
                    m_MultiTile.flipY = false;
                    Logger::Info(LOG_SUBSYSTEM, "=== MULTI-TILE SELECTION ===");
                    Logger::InfoF(LOG_SUBSYSTEM, "Start tile ID: {}", m_MultiTile.selectedStartID);
                    Logger::InfoF(
                        LOG_SUBSYSTEM, "Size: {}x{}", m_MultiTile.width, m_MultiTile.height);
                }
                else
                {
                    m_MultiTile.selectionMode = false;
                    m_MultiTile.isPlacing = false;
                    m_MultiTile.rotation = 0;
                    m_MultiTile.flipX = false;
                    m_MultiTile.flipY = false;
                    Logger::Info(LOG_SUBSYSTEM, "=== SINGLE TILE SELECTION ===");
                    Logger::InfoF(LOG_SUBSYSTEM, "Tile ID: {}", m_MultiTile.selectedStartID);
                }

                m_ShowTilePicker = false;
            }
            m_MultiTile.isSelecting = false;
            m_MultiTile.selectionStartTileID = -1;
            m_Mouse.mousePressed = false;
        }

        // Early return to prevent tile placement when tile picker is shown
        if (m_ShowTilePicker)
        {
            m_Mouse.lastMouseX = mouseX;
            m_Mouse.lastMouseY = mouseY;
            return;
        }
    }

    if (leftMouseDown && !m_ShowTilePicker)
    {
        auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
        float worldX = st.worldX;
        float worldY = st.worldY;
        int tileX = st.tileX;
        int tileY = st.tileY;

        if (m_Active && (m_EditMode == EditMode::NPCPlacement))
        {
            if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                if (tileX == m_Mouse.lastNPCPlacementTileX &&
                    tileY == m_Mouse.lastNPCPlacementTileY)
                {
                    return;
                }
                m_Mouse.lastNPCPlacementTileX = tileX;
                m_Mouse.lastNPCPlacementTileY = tileY;

                const int tileSize = ctx.tilemap.GetTileWidth();

                bool removed = false;
                bool hasNPCHere = false;
                for (const entt::entity entity : EntityStore::Entities(ctx.npcs))
                {
                    const Patrol& patrol = ctx.npcs.get<Patrol>(entity);
                    if (patrol.tileX == tileX && patrol.tileY == tileY)
                    {
                        hasNPCHere = true;
                        break;
                    }
                }
                if (hasNPCHere)
                {
                    ExecuteEditorCommand(
                        std::make_unique<RemoveNPCCmd>(tileX, tileY), ctx.tilemap, ctx.npcs);
                    removed = true;
                    Logger::InfoF(LOG_SUBSYSTEM, "Removed NPC at tile ({}, {})", tileX, tileY);
                }

                if (!removed && ctx.tilemap.GetNavigation(tileX, tileY))
                {
                    ClampNPCTypeIndex();
                    if (!m_AvailableNPCTypes.empty())
                    {
                        std::string npcType = m_AvailableNPCTypes[m_SelectedNPCTypeIndex];

                        // Randomly assign a dialogue tree from the pool
                        // TODO: Load from rift.save.json only and create dialogues via editor
                        DialogueTree tree;
                        std::string npcName;
                        static std::mt19937 rng(std::random_device{}());

                        const int TOTAL_DIALOGUE_TYPES = kMysteryDialogueCount + 2;
                        std::uniform_int_distribution<int> dist(0, TOTAL_DIALOGUE_TYPES - 1);
                        int dialogueType = dist(rng);
                        if (dialogueType < kMysteryDialogueCount)
                        {
                            BuildMysteryDialogueTree(
                                tree, npcName, kMysteryDialogues[dialogueType]);
                        }
                        else if (dialogueType == kMysteryDialogueCount)
                        {
                            BuildEditorAwareDialogueTree(tree, npcName);
                        }
                        else
                        {
                            BuildAnnoyedNPCDialogueTree(tree, npcName);
                        }

                        NpcRecord record;
                        record.type = npcType;
                        record.name = npcName;
                        record.tileX = tileX;
                        record.tileY = tileY;
                        record.tileSize = tileSize;
                        record.tree = std::move(tree);
                        record.hasTree = true;

                        ExecuteEditorCommand(std::make_unique<PlaceNPCCmd>(std::move(record)),
                                             ctx.tilemap,
                                             ctx.npcs);
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Placed NPC {} at tile ({}, {}) with dialogue tree",
                                      npcType,
                                      tileX,
                                      tileY);
                    }
                    else
                    {
                        Logger::Error(LOG_SUBSYSTEM, "No NPC types available!");
                    }
                }
            }

            return;
        }

        if (m_Active && (m_EditMode == EditMode::ParticleZone))
        {
            if (!m_PlacingParticleZone)
            {
                m_PlacingParticleZone = true;

                m_ParticleZoneStart.x = static_cast<float>(tileX * ctx.tilemap.GetTileWidth());
                m_ParticleZoneStart.y = static_cast<float>(tileY * ctx.tilemap.GetTileHeight());
            }
            // Zone is created on mouse release, so just track mouse here
            return;
        }

        if (m_Active && (m_EditMode == EditMode::Animation) && m_SelectedAnimationId >= 0)
        {
            if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                int oldAnimId = ctx.tilemap.GetTileAnimation(tileX, tileY, m_CurrentLayer);
                int oldTileId = ctx.tilemap.GetLayerTile(tileX, tileY, m_CurrentLayer);
                if (oldAnimId != m_SelectedAnimationId)
                {
                    ExecuteEditorCommand(
                        std::make_unique<SetTileAnimationCmd>(
                            std::vector<SetTileAnimationCmd::Entry>{{tileX,
                                                                     tileY,
                                                                     m_CurrentLayer,
                                                                     oldAnimId,
                                                                     m_SelectedAnimationId,
                                                                     oldTileId}}),
                        ctx.tilemap,
                        ctx.npcs);
                }
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Applied animation #{} to tile ({}, {}) layer {}",
                              m_SelectedAnimationId,
                              tileX,
                              tileY,
                              m_CurrentLayer);
            }
            return;
        }

        // Elevation editing mode, paint elevation values via stroke
        // accumulator (left-click drag commits one composite cmd at mouse-up).
        if (m_Active && (m_EditMode == EditMode::Elevation))
        {
            if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                const int oldElevation = ctx.tilemap.GetElevation(tileX, tileY);
                const ElevationRole oldRole =
                    ctx.tilemap.GetLayerElevationRole(tileX, tileY, m_CurrentLayer);
                const bool heightChanged = oldElevation != m_CurrentElevation;
                const bool roleChanged = oldRole != m_CurrentElevationRole;

                // Height and role changes share one stroke, even when only one changes.
                if ((heightChanged || roleChanged) && !m_ElevationStroke.IsActive())
                    m_ElevationStroke.Begin();

                if (heightChanged)
                {
                    m_ElevationStroke.Touch(tileX, tileY, oldElevation, m_CurrentElevation);
                    ctx.tilemap.SetElevation(tileX, tileY, m_CurrentElevation);
                    MarkDirty();
                }
                if (roleChanged)
                {
                    m_ElevationStroke.TouchRole(tileX,
                                                tileY,
                                                static_cast<std::size_t>(m_CurrentLayer),
                                                oldRole,
                                                m_CurrentElevationRole);
                    ctx.tilemap.SetLayerElevationRole(
                        tileX, tileY, m_CurrentLayer, m_CurrentElevationRole);
                    MarkDirty();
                }
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Set elevation at ({}, {}) to {} pixels, role: {}",
                              tileX,
                              tileY,
                              m_CurrentElevation,
                              EnumTraits<ElevationRole>::ToString(m_CurrentElevationRole));
            }
            return;
        }

        // Structure editing mode - works like no-projection mode with anchor placement
        // Click = toggle no-projection, Shift+click = flood-fill, Ctrl+click = place anchors
        if (m_Active && (m_EditMode == EditMode::Structure))
        {
            if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
                bool ctrlHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                 glfwGetKey(ctx.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

                if (ctrlHeld && !m_Mouse.mousePressed)
                {
                    int tileWidth = ctx.tilemap.GetTileWidth();
                    int tileHeight = ctx.tilemap.GetTileHeight();
                    float tileCenterX = (tileX + 0.5f) * tileWidth;
                    float tileCenterY = (tileY + 0.5f) * tileHeight;

                    bool clickedRight = worldX >= tileCenterX;
                    bool clickedBottom = worldY >= tileCenterY;

                    float cornerX = static_cast<float>(clickedRight ? (tileX + 1) * tileWidth
                                                                    : tileX * tileWidth);
                    float cornerY = static_cast<float>(clickedBottom ? (tileY + 1) * tileHeight
                                                                     : tileY * tileHeight);

                    const char* cornerNames[4] = {
                        "top-left", "top-right", "bottom-left", "bottom-right"};
                    int cornerIdx = (clickedBottom ? 2 : 0) + (clickedRight ? 1 : 0);

                    if (m_PlacingAnchor == 0 || m_PlacingAnchor == 1)
                    {
                        m_TempLeftAnchor = glm::vec2(cornerX, cornerY);
                        m_PlacingAnchor = 2;
                        m_Mouse.mousePressed = true;
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Left anchor: {} of tile ({}, {})",
                                      cornerNames[cornerIdx],
                                      tileX,
                                      tileY);
                    }
                    else if (m_PlacingAnchor == 2)
                    {
                        m_TempRightAnchor = glm::vec2(cornerX, cornerY);
                        m_PlacingAnchor = 0;
                        m_Mouse.mousePressed = true;

                        auto cmd =
                            std::make_unique<AddStructureCmd>(m_TempLeftAnchor, m_TempRightAnchor);
                        AddStructureCmd* cmdPtr = cmd.get();
                        ExecuteEditorCommand(std::move(cmd), ctx.tilemap, ctx.npcs);
                        int id = cmdPtr->StructureId();
                        m_CurrentStructureId = id;
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Right anchor: {} of tile ({}, {})",
                                      cornerNames[cornerIdx],
                                      tileX,
                                      tileY);
                        Logger::InfoF(LOG_SUBSYSTEM, "Created structure {}", id);
                        m_TempLeftAnchor = glm::vec2(-1.0f, -1.0f);
                        m_TempRightAnchor = glm::vec2(-1.0f, -1.0f);
                    }
                }
                else if (shiftHeld && !m_Mouse.mousePressed)
                {
                    m_Mouse.mousePressed = true;
                    int layer = m_CurrentLayer;
                    int structId = m_CurrentStructureId;
                    std::vector<LayerStanceEntry> stanceEntries;
                    std::vector<SetTileStructureIdsCmd::Entry> structEntries;
                    int count = FloodFill(
                        ctx.tilemap,
                        tileX,
                        tileY,
                        [&](int cx, int cy)
                        {
                            return ctx.tilemap.GetLayerTile(cx, cy, layer) >= 0 ||
                                   ctx.tilemap.GetTileAnimation(cx, cy, layer) >= 0;
                        },
                        [&](int cx, int cy)
                        {
                            const TileStance oldStance = ctx.tilemap.GetLayerStance(cx, cy, layer);
                            if (oldStance != TileStance::Structure)
                            {
                                stanceEntries.push_back({cx,
                                                         cy,
                                                         static_cast<std::size_t>(layer),
                                                         oldStance,
                                                         TileStance::Structure});
                                ctx.tilemap.SetLayerStance(cx, cy, layer, TileStance::Structure);
                            }
                            if (structId >= 0)
                            {
                                int oldSid = ctx.tilemap.GetTileStructureId(cx, cy, layer + 1);
                                if (oldSid != structId)
                                {
                                    structEntries.push_back({cx, cy, layer + 1, oldSid, structId});
                                    ctx.tilemap.SetTileStructureId(cx, cy, layer + 1, structId);
                                }
                            }
                        });
                    if (structId >= 0)
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Set Structure stance on {} tiles, assigned to structure {}",
                                      count,
                                      structId);
                    else
                        Logger::InfoF(LOG_SUBSYSTEM,
                                      "Set Structure stance on {} tiles (no structure)",
                                      count);

                    std::vector<std::unique_ptr<EditorCommand>> children;
                    if (!stanceEntries.empty())
                        children.push_back(
                            std::make_unique<SetTileStancesCmd>(std::move(stanceEntries)));
                    if (!structEntries.empty())
                        children.push_back(
                            std::make_unique<SetTileStructureIdsCmd>(std::move(structEntries)));
                    if (children.size() == 1)
                        PushEditorCommand(std::move(children[0]));
                    else if (!children.empty())
                        PushEditorCommand(std::make_unique<CompositeCmd>(
                            "Structure flood-fill assign", std::move(children)));
                }
                else if (!ctrlHeld && !shiftHeld && !m_Mouse.mousePressed)
                {
                    m_Mouse.mousePressed = true;
                    // Promote any non-Structure stance; undo restores its prior value.
                    const TileStance oldStance =
                        ctx.tilemap.GetLayerStance(tileX, tileY, m_CurrentLayer);
                    const bool current = oldStance == TileStance::Structure;
                    const TileStance newStance = current ? TileStance::Flat : TileStance::Structure;
                    std::vector<LayerStanceEntry> stanceEntries;
                    std::vector<SetTileStructureIdsCmd::Entry> structEntries;
                    stanceEntries.push_back({tileX,
                                             tileY,
                                             static_cast<std::size_t>(m_CurrentLayer),
                                             oldStance,
                                             newStance});
                    ctx.tilemap.SetLayerStance(tileX, tileY, m_CurrentLayer, newStance);
                    if (m_CurrentStructureId >= 0 && !current)
                    {
                        int oldSid =
                            ctx.tilemap.GetTileStructureId(tileX, tileY, m_CurrentLayer + 1);
                        if (oldSid != m_CurrentStructureId)
                        {
                            structEntries.push_back(
                                {tileX, tileY, m_CurrentLayer + 1, oldSid, m_CurrentStructureId});
                            ctx.tilemap.SetTileStructureId(
                                tileX, tileY, m_CurrentLayer + 1, m_CurrentStructureId);
                        }
                    }
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Stance at ({}, {}) -> {}",
                                  tileX,
                                  tileY,
                                  EnumTraits<TileStance>::ToString(newStance));

                    std::vector<std::unique_ptr<EditorCommand>> children;
                    children.push_back(
                        std::make_unique<SetTileStancesCmd>(std::move(stanceEntries)));
                    if (!structEntries.empty())
                        children.push_back(
                            std::make_unique<SetTileStructureIdsCmd>(std::move(structEntries)));
                    if (children.size() == 1)
                        PushEditorCommand(std::move(children[0]));
                    else
                        PushEditorCommand(std::make_unique<CompositeCmd>(
                            "Structure single-tile assign", std::move(children)));
                }
            }
            return;
        }

        // Stance editing mode: paint the selected stance on the current layer.
        // Shift+click floods the connected shape, exactly as the other flag modes do.
        if (m_Active && (m_EditMode == EditMode::Stance))
        {
            if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                  glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

                std::vector<LayerStanceEntry> entries;
                std::size_t layer = static_cast<std::size_t>(m_CurrentLayer);
                const TileStance target = m_CurrentStance;

                if (shiftHeld)
                {
                    int count = FloodFill(
                        ctx.tilemap,
                        tileX,
                        tileY,
                        [&](int cx, int cy)
                        {
                            return ctx.tilemap.GetLayerTile(cx, cy, layer) >= 0 ||
                                   ctx.tilemap.GetTileAnimation(cx, cy, static_cast<int>(layer)) >=
                                       0;
                        },
                        [&](int cx, int cy)
                        {
                            const TileStance oldStance = ctx.tilemap.GetLayerStance(cx, cy, layer);
                            if (oldStance != target)
                            {
                                entries.push_back({cx, cy, layer, oldStance, target});
                                ctx.tilemap.SetLayerStance(cx, cy, layer, target);
                            }
                        });
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Set {} on {} connected tiles (layer {})",
                                  EnumTraits<TileStance>::ToString(target),
                                  count,
                                  layer + 1);
                }
                else
                {
                    const TileStance oldStance = ctx.tilemap.GetLayerStance(tileX, tileY, layer);
                    if (oldStance != target)
                    {
                        entries.push_back({tileX, tileY, layer, oldStance, target});
                        ctx.tilemap.SetLayerStance(tileX, tileY, layer, target);
                    }
                    Logger::InfoF(LOG_SUBSYSTEM,
                                  "Set {} at ({}, {}) on layer {}",
                                  EnumTraits<TileStance>::ToString(target),
                                  tileX,
                                  tileY,
                                  m_CurrentLayer + 1);
                }

                if (!entries.empty())
                    PushEditorCommand(std::make_unique<SetTileStancesCmd>(std::move(entries)));
            }
            return;
        }

        // Y-sort-plus editing mode, set Y-sort-plus flag for current layer
        // Shift+click, flood-fill to mark all connected tiles in the shape
        if (m_Active && (m_EditMode == EditMode::YSortPlus))
        {
            auto entries = CollectYSortFlagToggle(ctx,
                                                  tileX,
                                                  tileY,
                                                  &Tilemap::GetLayerYSortPlus,
                                                  &Tilemap::SetLayerYSortPlus,
                                                  true,
                                                  "Y-sort-plus");
            if (!entries.empty())
                PushEditorCommand(std::make_unique<YSortPlusToggleCmd>(std::move(entries)));
            return;
        }

        // Y-sort-minus editing mode, set Y-sort-minus flag for current layer
        // Shift+click, flood-fill to mark all connected tiles in the shape
        if (m_Active && (m_EditMode == EditMode::YSortMinus))
        {
            auto entries = CollectYSortFlagToggle(ctx,
                                                  tileX,
                                                  tileY,
                                                  &Tilemap::GetLayerYSortMinus,
                                                  &Tilemap::SetLayerYSortMinus,
                                                  true,
                                                  "Y-sort-minus");
            if (!entries.empty())
                PushEditorCommand(std::make_unique<YSortMinusToggleCmd>(std::move(entries)));
            // Warn if Y-sort-plus isn't set on this tile (only relevant for single-tile placement)
            bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                              glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
            if (!shiftHeld && tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                tileY < ctx.tilemap.GetMapHeight())
            {
                bool isYSortPlus = ctx.tilemap.GetLayerYSortPlus(tileX, tileY, m_CurrentLayer);
                if (!isYSortPlus)
                    Logger::Warn(LOG_SUBSYSTEM, "  tile must also be Y-sort-plus!");
            }
            return;
        }

        bool isNewTilePosition =
            (tileX != m_Mouse.lastPlacedTileX || tileY != m_Mouse.lastPlacedTileY);

        if (m_MultiTile.selectionMode)
        {
            if (!m_Mouse.mousePressed)
            {
                int dataTilesPerRow =
                    ctx.tilemap.GetTilesetDataWidth() / ctx.tilemap.GetTileWidth();

                int rotatedWidth = (m_MultiTile.rotation == 90 || m_MultiTile.rotation == 270)
                                       ? m_MultiTile.height
                                       : m_MultiTile.width;
                int rotatedHeight = (m_MultiTile.rotation == 90 || m_MultiTile.rotation == 270)
                                        ? m_MultiTile.width
                                        : m_MultiTile.height;
                float tileRotation = GetCompensatedTileRotation();

                std::vector<PlaceTilesCmd::Entry> entries;
                entries.reserve(static_cast<size_t>(rotatedWidth) *
                                static_cast<size_t>(rotatedHeight));
                for (int dy = 0; dy < rotatedHeight; ++dy)
                {
                    for (int dx = 0; dx < rotatedWidth; ++dx)
                    {
                        int sourceDx, sourceDy;
                        CalculateRotatedSourceTile(dx, dy, sourceDx, sourceDy);

                        int placeX = tileX + dx;
                        int placeY = tileY + dy;
                        int sourceTileID =
                            m_MultiTile.selectedStartID + sourceDy * dataTilesPerRow + sourceDx;

                        if (placeX >= 0 && placeX < ctx.tilemap.GetMapWidth() && placeY >= 0 &&
                            placeY < ctx.tilemap.GetMapHeight())
                        {
                            // Multi-tile stamp writes the brush's flip state onto each
                            // destination cell. The brush flip is uniform across the
                            // stamp; column/row remapping happens in CalculateRotatedSourceTile.
                            const bool oldFlipX =
                                ctx.tilemap.GetLayerFlipX(placeX, placeY, m_CurrentLayer);
                            const bool oldFlipY =
                                ctx.tilemap.GetLayerFlipY(placeX, placeY, m_CurrentLayer);
                            PlaceTilesCmd::Entry e{};
                            e.tileX = placeX;
                            e.tileY = placeY;
                            e.layer = static_cast<size_t>(m_CurrentLayer);
                            e.oldTileId = ctx.tilemap.GetLayerTile(placeX, placeY, m_CurrentLayer);
                            e.oldRotation =
                                ctx.tilemap.GetLayerRotation(placeX, placeY, m_CurrentLayer);
                            e.newTileId = sourceTileID;
                            e.newRotation = tileRotation;
                            e.oldFlipX = oldFlipX;
                            e.newFlipX = m_MultiTile.flipX;
                            e.oldFlipY = oldFlipY;
                            e.newFlipY = m_MultiTile.flipY;
                            entries.push_back(e);
                        }
                    }
                }
                if (!entries.empty())
                {
                    ExecuteEditorCommand(
                        std::make_unique<PlaceTilesCmd>(std::move(entries)), ctx.tilemap, ctx.npcs);
                }
                Logger::InfoF(LOG_SUBSYSTEM,
                              "Placed {}x{} tiles starting at ({}, {}) on layer {}",
                              m_MultiTile.width,
                              m_MultiTile.height,
                              tileX,
                              tileY,
                              m_CurrentLayer + 1);

                m_Mouse.lastPlacedTileX = tileX;
                m_Mouse.lastPlacedTileY = tileY;
                m_Mouse.mousePressed = true;
            }
        }
        else
        {
            if (isNewTilePosition || !m_Mouse.mousePressed)
            {
                if (tileX >= 0 && tileX < ctx.tilemap.GetMapWidth() && tileY >= 0 &&
                    tileY < ctx.tilemap.GetMapHeight())
                {
                    float tileRotation = GetCompensatedTileRotation();
                    if (!m_TileStroke.IsActive())
                        m_TileStroke.Begin();

                    int oldId = ctx.tilemap.GetLayerTile(tileX, tileY, m_CurrentLayer);
                    float oldRot = ctx.tilemap.GetLayerRotation(tileX, tileY, m_CurrentLayer);
                    bool oldFlipX = ctx.tilemap.GetLayerFlipX(tileX, tileY, m_CurrentLayer);
                    bool oldFlipY = ctx.tilemap.GetLayerFlipY(tileX, tileY, m_CurrentLayer);
                    m_TileStroke.Touch(tileX,
                                       tileY,
                                       static_cast<size_t>(m_CurrentLayer),
                                       oldId,
                                       oldRot,
                                       m_MultiTile.selectedStartID,
                                       tileRotation,
                                       oldFlipX,
                                       oldFlipY,
                                       m_MultiTile.flipX,
                                       m_MultiTile.flipY);
                    ctx.tilemap.SetLayerTile(
                        tileX, tileY, m_CurrentLayer, m_MultiTile.selectedStartID);
                    ctx.tilemap.SetLayerRotation(tileX, tileY, m_CurrentLayer, tileRotation);
                    ctx.tilemap.SetLayerFlipX(tileX, tileY, m_CurrentLayer, m_MultiTile.flipX);
                    ctx.tilemap.SetLayerFlipY(tileX, tileY, m_CurrentLayer, m_MultiTile.flipY);
                    MarkDirty();

                    m_Mouse.lastPlacedTileX = tileX;
                    m_Mouse.lastPlacedTileY = tileY;
                    m_Mouse.mousePressed = true;
                }
            }
        }
    }

    if (!leftMouseDown)
    {
        if (m_PlacingParticleZone && (m_EditMode == EditMode::ParticleZone))
        {
            auto st = ScreenToTileCoords(ctx, mouseX, mouseY);
            auto zr = CalculateParticleZoneRect(
                st.worldX, st.worldY, ctx.tilemap.GetTileWidth(), ctx.tilemap.GetTileHeight());

            ParticleZone zone;
            zone.position = glm::vec2(zr.x, zr.y);
            zone.size = glm::vec2(zr.w, zr.h);
            zone.type = m_CurrentParticleType;
            zone.enabled = true;

            int tw = ctx.tilemap.GetTileWidth();
            int th = ctx.tilemap.GetTileHeight();
            int minTileX = static_cast<int>(zr.x / tw);
            int minTileY = static_cast<int>(zr.y / th);
            int maxTileX = minTileX + static_cast<int>(zr.w / tw) - 1;
            int maxTileY = minTileY + static_cast<int>(zr.h / th) - 1;

            bool hasNoProjection = m_ParticleNoProjection;
            if (!hasNoProjection)
            {
                for (int ty = minTileY; ty <= maxTileY && !hasNoProjection; ty++)
                {
                    for (int tx = minTileX; tx <= maxTileX && !hasNoProjection; tx++)
                    {
                        for (size_t layer = 0; layer < ctx.tilemap.GetLayerCount(); layer++)
                        {
                            if (ctx.tilemap.GetLayerStance(tx, ty, layer) == TileStance::Structure)
                            {
                                hasNoProjection = true;
                                break;
                            }
                        }
                    }
                }
            }
            zone.noProjection = hasNoProjection;
            ExecuteEditorCommand(std::make_unique<AddParticleZoneCmd>(zone), ctx.tilemap, ctx.npcs);

            Logger::InfoF(LOG_SUBSYSTEM,
                          "Created {} zone at ({}, {}) size {}x{}{}",
                          EnumTraits<ParticleType>::ToString(m_CurrentParticleType),
                          zr.x,
                          zr.y,
                          zr.w,
                          zr.h,
                          hasNoProjection ? " [noProjection]" : "");

            m_PlacingParticleZone = false;
        }

        m_Mouse.mousePressed = false;
        m_Mouse.lastPlacedTileX = -1;
        m_Mouse.lastPlacedTileY = -1;
        m_Mouse.lastNPCPlacementTileX = -1;
        m_Mouse.lastNPCPlacementTileY = -1;

        if (m_TileStroke.IsActive())
            m_TileStroke.Commit(m_UndoStack);
        if (m_ElevationStroke.IsActive())
            m_ElevationStroke.Commit(m_UndoStack);
    }

    m_Mouse.lastMouseX = mouseX;
    m_Mouse.lastMouseY = mouseY;
}

void Editor::HandleScroll(double yoffset, const EditorContext& ctx)
{
    int ctrlState = glfwGetKey(ctx.window, GLFW_KEY_LEFT_CONTROL) |
                    glfwGetKey(ctx.window, GLFW_KEY_RIGHT_CONTROL);

    if ((m_EditMode == EditMode::Elevation) && ctrlState != GLFW_PRESS)
    {
        if (yoffset > 0)
        {
            m_CurrentElevation += 2;
            if (m_CurrentElevation > 32)
                m_CurrentElevation = 32;
        }
        else if (yoffset < 0)
        {
            m_CurrentElevation -= 2;
            if (m_CurrentElevation < -32)
                m_CurrentElevation = -32;
        }
        Logger::InfoF(LOG_SUBSYSTEM, "Elevation value: {} pixels", m_CurrentElevation);
        return;
    }

    if (m_ShowTilePicker)
    {
        int dataTilesPerRow = ctx.tilemap.GetTilesetDataWidth() / ctx.tilemap.GetTileWidth();
        int dataTilesPerCol = ctx.tilemap.GetTilesetDataHeight() / ctx.tilemap.GetTileHeight();
        float baseTileSizePixels =
            (static_cast<float>(ctx.screenWidth) / static_cast<float>(dataTilesPerRow)) * 1.5f;

        if (ctrlState == GLFW_PRESS)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(ctx.window, &mouseX, &mouseY);

            float oldTileSize = baseTileSizePixels * m_TilePicker.zoom;

            float adjustedMouseX = static_cast<float>(mouseX) - m_TilePicker.offsetX;
            float adjustedMouseY = static_cast<float>(mouseY) - m_TilePicker.offsetY;
            int pickerTileX = static_cast<int>(adjustedMouseX / oldTileSize);
            int pickerTileY = static_cast<int>(adjustedMouseY / oldTileSize);

            float zoomDelta = yoffset > 0 ? 1.1f : 0.9f;
            m_TilePicker.zoom *= zoomDelta;
            m_TilePicker.zoom = std::max(0.25f, std::min(8.0f, m_TilePicker.zoom));

            float newTileSize = baseTileSizePixels * m_TilePicker.zoom;

            float newTileCenterX = pickerTileX * newTileSize + newTileSize * 0.5f;
            float newTileCenterY = pickerTileY * newTileSize + newTileSize * 0.5f;
            float newOffsetX = static_cast<float>(mouseX) - newTileCenterX;
            float newOffsetY = static_cast<float>(mouseY) - newTileCenterY;

            // Clamp offsets so the sheet stays within viewable bounds
            float totalTilesWidth = newTileSize * dataTilesPerRow;
            float totalTilesHeight = newTileSize * dataTilesPerCol;
            float minOffsetX = ctx.screenWidth - totalTilesWidth;
            float maxOffsetX = 0.0f;
            float minOffsetY = ctx.screenHeight - totalTilesHeight;
            float maxOffsetY = 0.0f;

            newOffsetX = std::max(minOffsetX, std::min(maxOffsetX, newOffsetX));
            newOffsetY = std::max(minOffsetY, std::min(maxOffsetY, newOffsetY));

            m_TilePicker.offsetX = newOffsetX;
            m_TilePicker.offsetY = newOffsetY;
            m_TilePicker.targetOffsetX = newOffsetX;
            m_TilePicker.targetOffsetY = newOffsetY;

            Logger::InfoF(LOG_SUBSYSTEM,
                          "Tile picker zoom: {}x (offset: {}, {})",
                          m_TilePicker.zoom,
                          m_TilePicker.offsetX,
                          m_TilePicker.offsetY);
        }
        else
        {
            float panAmount = static_cast<float>(yoffset) * 200.0f;
            m_TilePicker.targetOffsetY += panAmount;

            float tileSizePixels = baseTileSizePixels * m_TilePicker.zoom;
            float totalTilesWidth = tileSizePixels * dataTilesPerRow;
            float totalTilesHeight = tileSizePixels * dataTilesPerCol;
            float minOffsetX = ctx.screenWidth - totalTilesWidth;
            float maxOffsetX = 0.0f;
            float minOffsetY = ctx.screenHeight - totalTilesHeight;
            float maxOffsetY = 0.0f;

            m_TilePicker.targetOffsetX =
                std::max(minOffsetX, std::min(maxOffsetX, m_TilePicker.targetOffsetX));
            m_TilePicker.targetOffsetY =
                std::max(minOffsetY, std::min(maxOffsetY, m_TilePicker.targetOffsetY));
        }
    }
}

void Editor::CalculateRotatedSourceTile(int dx, int dy, int& sourceDx, int& sourceDy) const
{
    BrushSourceCoord c = CalculateBrushSourceTile(dx,
                                                  dy,
                                                  BrushTransform{m_MultiTile.width,
                                                                 m_MultiTile.height,
                                                                 m_MultiTile.rotation,
                                                                 m_MultiTile.flipX,
                                                                 m_MultiTile.flipY});
    sourceDx = c.sourceDx;
    sourceDy = c.sourceDy;
}

float Editor::GetCompensatedTileRotation() const
{
    float tileRotation = static_cast<float>(m_MultiTile.rotation);
    if (m_MultiTile.rotation == 90 || m_MultiTile.rotation == 270)
        tileRotation = static_cast<float>((m_MultiTile.rotation + 180) % 360);
    return tileRotation;
}

std::vector<LayerFlagEntry> Editor::CollectYSortFlagToggle(
    const EditorContext& ctx,
    int tileX,
    int tileY,
    bool (Tilemap::*getter)(int, int, size_t) const,
    void (Tilemap::*setter)(int, int, size_t, bool),
    bool newValue,
    const std::string& flagName)
{
    std::vector<LayerFlagEntry> entries;
    if (tileX < 0 || tileX >= ctx.tilemap.GetMapWidth() || tileY < 0 ||
        tileY >= ctx.tilemap.GetMapHeight())
        return entries;

    bool shiftHeld = (glfwGetKey(ctx.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(ctx.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    std::size_t layer = static_cast<std::size_t>(m_CurrentLayer);

    if (shiftHeld)
    {
        int count = FloodFill(
            ctx.tilemap,
            tileX,
            tileY,
            [&](int cx, int cy)
            {
                return ctx.tilemap.GetLayerTile(cx, cy, layer) >= 0 ||
                       ctx.tilemap.GetTileAnimation(cx, cy, static_cast<int>(layer)) >= 0;
            },
            [&](int cx, int cy)
            {
                bool oldF = (ctx.tilemap.*getter)(cx, cy, layer);
                if (oldF != newValue)
                {
                    entries.push_back({cx, cy, layer, oldF, newValue});
                    (ctx.tilemap.*setter)(cx, cy, layer, newValue);
                }
            });
        Logger::InfoF(LOG_SUBSYSTEM,
                      "{}{} on {} connected tiles (layer {})",
                      newValue ? "Set " : "Cleared ",
                      flagName,
                      count,
                      layer + 1);
    }
    else
    {
        bool oldF = (ctx.tilemap.*getter)(tileX, tileY, layer);
        if (oldF != newValue)
        {
            entries.push_back({tileX, tileY, layer, oldF, newValue});
            (ctx.tilemap.*setter)(tileX, tileY, layer, newValue);
        }
        Logger::InfoF(LOG_SUBSYSTEM,
                      "{}{} at ({}, {}) layer {}",
                      newValue ? "Set " : "Cleared ",
                      flagName,
                      tileX,
                      tileY,
                      m_CurrentLayer + 1);
    }

    return entries;
}

Editor::TileZoneRect Editor::CalculateParticleZoneRect(float worldX,
                                                       float worldY,
                                                       int tileWidth,
                                                       int tileHeight) const
{
    int startTileX = static_cast<int>(m_ParticleZoneStart.x / tileWidth);
    int startTileY = static_cast<int>(m_ParticleZoneStart.y / tileHeight);
    int endTileX = static_cast<int>(std::floor(worldX / tileWidth));
    int endTileY = static_cast<int>(std::floor(worldY / tileHeight));

    int minTileX = std::min(startTileX, endTileX);
    int maxTileX = std::max(startTileX, endTileX);
    int minTileY = std::min(startTileY, endTileY);
    int maxTileY = std::max(startTileY, endTileY);

    return {static_cast<float>(minTileX * tileWidth),
            static_cast<float>(minTileY * tileHeight),
            static_cast<float>((maxTileX - minTileX + 1) * tileWidth),
            static_cast<float>((maxTileY - minTileY + 1) * tileHeight)};
}
