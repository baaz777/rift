#pragma once

#include "EditorCommands.hpp"
#include "EditorStrokeAccumulators.hpp"
#include "IRenderer.hpp"
#include "ParticleSystem.hpp"
#include "Tilemap.hpp"
#include "UndoRedoStack.hpp"

#include <entt/entt.hpp>

#include <GLFW/glfw3.h>
#include <bitset>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct CameraState;

/**
 * @struct EditorContext
 * @brief Borrowed game state for one editor call.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Value fields are snapshots; references remain mutable through a const context.
 * Do not retain the context or its references beyond the frame.
 *
 * @code{.cpp}
 * // Inside Game:
 * EditorContext ctx = MakeEditorContext();
 * m_Editor.ProcessInput(deltaTime, ctx);
 * m_Editor.Render(ctx);
 * @endcode
 */
struct EditorContext
{
    GLFWwindow* window;
    int screenWidth;   ///< Window width in pixels.
    int screenHeight;  ///< Window height in pixels.
    int tilesVisibleWidth;
    int tilesVisibleHeight;
    CameraState& camera;  ///< Camera state (position, follow, zoom, free mode), mutable.
    Tilemap& tilemap;
    entt::entity playerEntity = entt::null;
    entt::registry& npcs;  ///< Live NPC + player store (the ECS registry / world).
    IRenderer& renderer;
    ParticleSystem& particles;
    std::string saveMapPath;
};

/**
 * @class Editor
 * @brief Level editor with undoable mutations.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Game supplies an EditorContext each frame. Editor activation opens the tile picker;
 * deactivation closes it. Modes claim buttons in priority order. Shift flood-fills B, G,
 * Y and O edits. In G mode, two Ctrl+clicks set anchors; Shift assigns the current structure.
 *
 * debug.overlays works independently of editor activation. Structure overlays require
 * EditMode::Structure; stance overlays include auto-detected anchors.
 *
 * @verbatim
 * LEFT    Ctrl-drag map selection (every mode but G)  -- consumes the button
 *         tile picker open                           -- consumes the button
 *         N -> J -> K (only with a selected animation) -> H -> G -> B -> Y -> O
 *         fall-through: stamp the multi-tile brush, else paint one tile
 *
 * RIGHT   tile picker open                           -- suppressed entirely
 *         K -> H -> G -> B -> Y/O -> J -> M
 *         else: toggle tile collision
 * @endverbatim
 *
 * @verbatim
 * Game::ProcessInput   -->  Editor::ProcessInput       (keyboard)
 *                      -->  Editor::ProcessMouseInput  (mouse)
 * Game::Update         -->  Editor::Update          (tile picker smoothing)
 * Game::Render         -->  Editor::Render          (overlays + tile picker)
 * Game::ScrollCallback -->  Editor::HandleScroll    (elevation / tile picker)
 * @endverbatim
 *
 * | Key | Mode         | Left click          | Right click         |
 * |-----|--------------|---------------------|---------------------|
 * | T   | picker       | select tile/region  | Consumed            |
 * | M   | navigation   | default action      | Paint walkability   |
 * | N   | NPC          | place/remove NPC    | Default action      |
 * | B   | stance       | paint stance        | Reset to Flat       |
 * | G   | structure    | anchor/flood/toggle | Clear assignment    |
 * | H   | elevation    | paint height + role | Clear height + role |
 * | J   | particles    | drag to create zone | Remove zone         |
 * | K   | animation    | apply animation     | Remove animation    |
 * | Y   | Y-sort-plus  | set flag            | Clear flag          |
 * | O   | Y-sort-minus | set flag            | Clear flag          |
 * | ----- | default      | paint tile          | Paint collision     |
 */
class Editor
{
public:
    Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    Editor(Editor&&) = default;
    Editor& operator=(Editor&&) = default;

    /**
     * @fn void Editor::Initialize(const std::vector<std::string>& npcTypes)
     * @brief Replace the NPC placement palette and select its first entry.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param npcTypes Sprite paths copied into the editor. An empty list disables placement.
     */
    void Initialize(const std::vector<std::string>& npcTypes);

    [[nodiscard]] bool IsActive() const { return m_Active; }

    /**
     * @fn void Editor::ShowStatus(std::string message, glm::vec3 color, float durationSeconds = \
     *     3.0f)
     * @brief Push an on-screen status toast (e.g. From save/load).
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param message Text to display.
     * @param color Tint for the text (green for success, red for error).
     * @param durationSeconds How long the toast stays on screen.
     */
    void ShowStatus(std::string message, glm::vec3 color, float durationSeconds = 3.0f);

    /**
     * @fn void Editor::SetActive(bool active)
     * @brief Activation opens the tile picker; deactivation closes it.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetActive(bool active);

    /**
     * @fn void Editor::ProcessInput(float deltaTime, const EditorContext& ctx)
     * @brief Processes mode keys and tile-picker panning.
     * @author Alex (<https://github.com/lextpf>)
     *
     * deltaTime is in seconds; picker panning is 1000 px/s, multiplied by 2.5 with Shift.
     */
    void ProcessInput(float deltaTime, const EditorContext& ctx);

    /**
     * @fn void Editor::ProcessMouseInput(const EditorContext& ctx)
     * @brief Process mouse input for tile placement and drag operations.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ProcessMouseInput(const EditorContext& ctx);

    /**
     * @fn void Editor::HandleScroll(double yoffset, const EditorContext& ctx)
     * @brief Handle scroll wheel input for elevation and tile picker zoom.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Positive yoffset scrolls up.
     */
    void HandleScroll(double yoffset, const EditorContext& ctx);

    /**
     * @fn void Editor::Update(float deltaTime, const EditorContext& ctx)
     * @brief Update editor state (tile picker smooth scrolling).
     * @author Alex (<https://github.com/lextpf>)
     *
     * deltaTime is frame time in seconds.
     */
    void Update(float deltaTime, const EditorContext& ctx);

    /**
     * @fn void Editor::Render(const EditorContext& ctx)
     * @brief Draws overlays and tile-picker UI.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @post Leaves a UI projection bound while active; rebind before the next world draw.
     */
    void Render(const EditorContext& ctx);

    /**
     * @fn void Editor::RenderNoProjectionAnchors(const EditorContext& ctx)
     * @brief Draws anchors above UI when enabled.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Game calls this while the editor is inactive; active stance overlays draw the same anchors.
     *
     * @pre The world projection is bound.
     */
    void RenderNoProjectionAnchors(const EditorContext& ctx);

    [[nodiscard]] bool IsDebugMode() const { return m_DebugMode; }

    [[nodiscard]] bool IsShowDebugInfo() const { return m_ShowDebugInfo; }

    [[nodiscard]] bool IsShowNoProjectionAnchors() const { return m_ShowNoProjectionAnchors; }

    [[nodiscard]] bool IsShowTilePicker() const { return m_ShowTilePicker; }

    void ToggleDebugMode();

    void ToggleShowDebugInfo();

    /**
     * @fn void Editor::SetDebugMode(bool enabled)
     * @brief Sets debug overlays and no-projection anchor visibility together.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetDebugMode(bool enabled);

    void SetShowDebugInfo(bool enabled);

    /**
     * @fn void Editor::ResetTilePickerState()
     * @brief Reset tile picker zoom and pan to defaults.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Called from Game when Z key is pressed in editor mode.
     */
    void ResetTilePickerState();

    /**
     * @fn void Editor::ClearUndoHistory()
     * @brief Discard committed undo and redo commands.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Call when the tilemap or registry contents are replaced. Active drag accumulators and
     * the unsaved indicator are unchanged. The editor L-key load calls this after success;
     * map.load and Game::LoadGameWorld do not clear history.
     */
    void ClearUndoHistory();

private:
    void RenderEditorUI(const EditorContext& ctx);

    void RenderEditorHUD(const EditorContext& ctx);

    void RenderEditorTopBar(const EditorContext& ctx);

    void RenderCollisionOverlays(const EditorContext& ctx);

    void RenderNavigationOverlays(const EditorContext& ctx);

    void RenderElevationOverlays(const EditorContext& ctx);

    void RenderStanceOverlays(const EditorContext& ctx);
    /**
     * @fn void Editor::RenderNoProjectionAnchorsImpl(const EditorContext& ctx)
     * @brief Draws anchors above UI when enabled.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Game calls this while the editor is inactive; active stance overlays draw the same anchors.
     *
     * @pre The world projection is bound.
     */
    void RenderNoProjectionAnchorsImpl(const EditorContext& ctx);

    void RenderStructureOverlays(const EditorContext& ctx);
    /**
     * @fn void Editor::RenderLayerFlagOverlays(const EditorContext& ctx, bool editMode, bool \
     *     (Tilemap::*getter)(int, int, size_t) const, const glm::vec3& color)
     * @brief Draws flag coverage for the current layer or all layers.
     * @author Alex (<https://github.com/lextpf>)
     *
     * editMode draws set cells on m_CurrentLayer at alpha 0.5. Otherwise alpha is
     * 0.15 + 0.35 * the fraction of layers with the flag. Getter takes a zero-based layer.
     */
    void RenderLayerFlagOverlays(const EditorContext& ctx,
                                 bool editMode,
                                 bool (Tilemap::*getter)(int, int, size_t) const,
                                 const glm::vec3& color);

    void RenderYSortPlusOverlays(const EditorContext& ctx);

    void RenderYSortMinusOverlays(const EditorContext& ctx);

    void RenderParticleZoneOverlays(const EditorContext& ctx);

    void RenderNPCDebugInfo(const EditorContext& ctx);

    void RenderCornerCuttingOverlays(const EditorContext& ctx);

    void RenderLayerOverlay(const EditorContext& ctx, int layerIndex, const glm::vec4& color);

    void RenderPlacementPreview(const EditorContext& ctx);

    void RenderMapSelectionOverlay(const EditorContext& ctx);

    /**
     * @fn void Editor::ClearAllEditModes()
     * @brief Return to tile placement and discard unfinished tool state.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clears drag undo data without reverting cells already painted. Finish a drag before
     * switching tools if the mutation must remain undoable.
     */
    void ClearAllEditModes();

    /**
     * @fn void Editor::EnsureNoProjBoundsCache(const EditorContext& ctx)
     * @brief Lazily compute and cache the per-layer structure groups for the current frame.
     * @author Alex (<https://github.com/lextpf>)
     */
    void EnsureNoProjBoundsCache(const EditorContext& ctx);

    /**
     * @fn void Editor::CalculateRotatedSourceTile(int dx, int dy, int& sourceDx, int& sourceDy) \
     *     const
     * @brief Map rotated tile offset to source tile coordinates.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param dx Rotated offset X.
     * @param dy Rotated offset Y.
     * @param sourceDx Output source offset X.
     * @param sourceDy Output source offset Y.
     */
    void CalculateRotatedSourceTile(int dx, int dy, int& sourceDx, int& sourceDy) const;

    float GetCompensatedTileRotation() const;

    /**
     * @struct ScreenToTile
     * @brief Mouse position converted from screen pixels into
     * world/tile coordinates.
     */
    struct ScreenToTile
    {
        float worldX = 0.0f;  ///< World X in pixels (camera position + zoomed screen offset).
        float worldY = 0.0f;  ///< World Y in pixels; +Y is down, matching the tilemap.
        /**
         * @brief Bare floor(world / tileSize); not clamped to the map, so it can be negative or
         * past the last row/column. Callers bounds-check before touching the tilemap.
         */
        int tileX = -1;
        int tileY = -1;
    };

    /**
     * @fn ScreenToTile Editor::ScreenToTileCoords(const EditorContext& ctx, double mouseX, double \
     *     mouseY) const
     * @brief Converts flat-view mouse pixels to world and tile coordinates.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Valid only for the flat camera. world3d still receives editor input, so these edits
     * do not match the rendered tiles. Nonpositive screen or tile dimensions return
     * zero world coordinates and tile indices of -1. Valid results are not map-clamped.
     */
    [[nodiscard]] ScreenToTile ScreenToTileCoords(const EditorContext& ctx,
                                                  double mouseX,
                                                  double mouseY) const;

    /**
     * @fn void Editor::MarkDirty()
     * @brief Track a map-changing edit for the HUD unsaved indicator.
     * @author Alex (<https://github.com/lextpf>)
     */
    void MarkDirty() { m_HasUnsavedChanges = true; }
    /**
     * @fn void Editor::MarkClean()
     * @brief Clear the HUD unsaved indicator after successful save/load.
     * @author Alex (<https://github.com/lextpf>)
     */
    void MarkClean() { m_HasUnsavedChanges = false; }

    /**
     * @fn void Editor::ExecuteEditorCommand(std::unique_ptr<EditorCommand> cmd, Tilemap& tilemap, \
     *     entt::registry& npcs)
     * @brief Execute an undoable command and mark the map dirty.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ExecuteEditorCommand(std::unique_ptr<EditorCommand> cmd,
                              Tilemap& tilemap,
                              entt::registry& npcs);
    /**
     * @fn void Editor::PushEditorCommand(std::unique_ptr<EditorCommand> cmd)
     * @brief Push an already-applied undoable command and mark the map dirty.
     * @author Alex (<https://github.com/lextpf>)
     */
    void PushEditorCommand(std::unique_ptr<EditorCommand> cmd);

    /**
     * @fn std::vector<LayerFlagEntry> Editor::CollectYSortFlagToggle( const EditorContext& ctx, \
     *     int tileX, int tileY, bool (Tilemap::*getter)(int, int, size_t) const, void \
     *     (Tilemap::*setter)(int, int, size_t, bool), bool newValue, const std::string& flagName)
     * @brief Applies a single-cell or Shift flood edit and returns changed entries.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Captures old values before writing. Skips unchanged cells; push the result as an
     * already-applied undo command.
     */
    [[nodiscard]] std::vector<LayerFlagEntry> CollectYSortFlagToggle(
        const EditorContext& ctx,
        int tileX,
        int tileY,
        bool (Tilemap::*getter)(int, int, size_t) const,
        void (Tilemap::*setter)(int, int, size_t, bool),
        bool newValue,
        const std::string& flagName);

    /**
     * @struct TileZoneRect
     * @brief Tile-aligned rectangle in world pixels for zone editing.
     */
    struct TileZoneRect
    {
        float x, y, w, h;  ///< Position and size in world pixels.
    };

    /**
     * @fn TileZoneRect Editor::CalculateParticleZoneRect(float worldX, float worldY, int \
     *     tileWidth, int tileHeight) const
     * @brief Calculate particle zone rectangle from world position.
     * @author Alex (<https://github.com/lextpf>)
     */
    TileZoneRect CalculateParticleZoneRect(float worldX,
                                           float worldY,
                                           int tileWidth,
                                           int tileHeight) const;

    /**
     * @enum EditMode
     * @brief Mutually exclusive editor sub-modes, selected by hotkey.
     *
     * Only one sub-mode is active at a time. `None` means the default
     * tile-placement mode is active (left-click places, right-click toggles collision).
     */
    enum class EditMode
    {
        None,
        Navigation,    ///< Painting walkability flags (M key).
        Elevation,     ///< Painting elevation values (H key).
        NPCPlacement,  ///< Placing / removing NPCs (N key).
        Stance,        ///< Painting per-tile TileStance (B key).
        YSortPlus,     ///< Editing Y-sort-plus flags (Y key).
        YSortMinus,    ///< Editing Y-sort-minus flags (O key).
        ParticleZone,  ///< Defining particle emitter zones (J key).
        Structure,     ///< Assigning tiles to structures (G key).
        Animation,     ///< Applying animations to tiles (K key).
    };

    bool m_Active;  ///< Master toggle for the level editor (`editor` cmd).
    bool m_ShowTilePicker;
    EditMode m_EditMode;

    /**
     * @brief Stance selected by comma and period.
     *
     * Paintable values must remain contiguous from 1 through TILE_STANCE_COUNT - 1.
     * Flat is reserved for right-click clearing.
     */
    TileStance m_CurrentStance;

    ParticleType m_CurrentParticleType;  ///< Visual type for new zones (e.g. Firefly).
    bool m_ParticleNoProjection;
    bool m_PlacingParticleZone;
    glm::vec2 m_ParticleZoneStart;  ///< World position where the current drag began.

    /**
     * @brief State for the two-anchor structure workflow (G mode).
     *
     * Place the left anchor, then the right anchor, then flood-assign the tiles between
     * them to a structure ID.
     */
    int m_CurrentStructureId;
    int m_PlacingAnchor;          ///< Anchor step: 0 = idle, 1 = left, 2 = right.
    glm::vec2 m_TempLeftAnchor;   ///< World position of left anchor (-1 = unset).
    glm::vec2 m_TempRightAnchor;  ///< World position of right anchor (-1 = unset).
    bool m_AssigningTilesToStructure;

    std::vector<int> m_AnimationFrames;
    float m_AnimationFrameDuration;  ///< Seconds each frame is shown (default 0.2s).
    int m_SelectedAnimationId;       ///< Index of the animation being edited, or -1.

    bool m_DebugMode;      ///< Enables all debug overlays (`debug.overlays` cmd).
    bool m_ShowDebugInfo;  ///< Shows text debug info (FPS, tile coords, etc.).
    bool m_ShowNoProjectionAnchors;
    bool m_HasUnsavedChanges;

    /**
     * @brief Heights of the opaque bands drawn over the world view, in screen pixels.
     *
     * @warning Neither band is excluded from mouse picking. ProcessMouseInput converts the
     * raw cursor position straight to a tile, so a click on a top-bar chip or on the HUD
     * also edits the tile drawn behind it.
     */
    static constexpr float EDITOR_HUD_HEIGHT = 40.0f;
    static constexpr float EDITOR_TOPBAR_HEIGHT = 56.0f;

    /**
     * @brief On-screen transient message (save success/failure, load result).
     *
     * Drawn while m_StatusTimer &gt; 0, which Update() decrements by the frame delta.
     */
    std::string m_StatusMessage;       ///< Text to display; empty = hidden.
    float m_StatusTimer = 0.0f;        ///< Seconds remaining to display.
    glm::vec3 m_StatusColor{1, 1, 1};  ///< Tint (e.g. Green for success, red for error).

    /**
     * @brief Tile ID last clicked in the tile picker.
     *
     * While a picker drag is in progress it tracks the moving end corner of the selection
     * rectangle, with MultiTileState's selectionStartTileID as the anchor corner.
     */
    int m_SelectedTileID;
    /**
     * @brief Active tilemap layer index (0-based) for placement. Keys 1-9 and 0 select indices
     * 0-9, so a map loaded with more than 10 dynamicLayers has no hotkey for the rest.
     */
    int m_CurrentLayer;
    /**
     * @brief Painted height in pixels.
     *
     * Scroll steps by 2 within [-32, 32]; zero is ground.
     */
    int m_CurrentElevation;
    /**
     * @brief Elevation role selected by comma and period.
     *
     * Paintable values must remain contiguous from 1 through ELEVATION_ROLE_COUNT - 1.
     * Ground is reserved for right-click clearing.
     */
    ElevationRole m_CurrentElevationRole;

    std::vector<std::string> m_AvailableNPCTypes;
    size_t m_SelectedNPCTypeIndex;  ///< Index into m_AvailableNPCTypes.

    /**
     * @fn void Editor::ClampNPCTypeIndex()
     * @brief Clamp m_SelectedNPCTypeIndex to valid range after vector changes.
     * @author Alex (<https://github.com/lextpf>)
     */
    void ClampNPCTypeIndex()
    {
        if (!m_AvailableNPCTypes.empty())
            m_SelectedNPCTypeIndex =
                std::min(m_SelectedNPCTypeIndex, m_AvailableNPCTypes.size() - 1);
    }

    /**
     * @brief Drag deduplication and latched paint values.
     *
     * Last-tile pairs reset to -1 on release. The first right-drag cell chooses the value
     * written by the whole stroke.
     */
    struct MouseDragState
    {
        /**
         * @brief Cursor position in window pixels as of the last ProcessMouseInput call. Currently
         * write-only - nothing reads either field, so no behavior depends on them.
         */
        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        bool mousePressed = false;
        /**
         * @brief Right-button analog of mousePressed; also gates the "first cell of the
         * stroke" branch that decides `navigationDragState` / `collisionDragState`.
         */
        bool rightMousePressed = false;
        int lastPlacedTileX = -1;
        int lastPlacedTileY = -1;
        int lastNavigationTileX = -1;
        int lastNavigationTileY = -1;
        bool navigationDragState = false;
        int lastCollisionTileX = -1;
        int lastCollisionTileY = -1;
        bool collisionDragState = false;
        int lastNPCPlacementTileX = -1;
        int lastNPCPlacementTileY = -1;
    };
    MouseDragState m_Mouse;

    /**
     * @brief Tile-picker pan and zoom.
     *
     * Pan writes targets; Update eases offsets and snaps within 0.1 px. Zoom and reset write
     * both immediately. Opening the picker cancels stale easing.
     */
    struct TilePickerCamera
    {
        float zoom = 2.0f;     ///< Picker magnification, clamped to [0.25, 8] by Ctrl+scroll.
        float offsetX = 0.0f;  ///< Rendered pan X in screen pixels (eased toward targetOffsetX).
        float offsetY = 0.0f;  ///< Rendered pan Y in screen pixels (eased toward targetOffsetY).
        float targetOffsetX = 0.0f;  ///< Requested pan X; clamped so the sheet stays on screen.
        float targetOffsetY = 0.0f;  ///< Requested pan Y; clamped so the sheet stays on screen.
    };
    TilePickerCamera m_TilePicker;

    /**
     * @brief Rectangular brush selected from the picker.
     *
     * selectionMode requires a region larger than 1x1. Width and height are unrotated source
     * dimensions; placement applies rotation and flips.
     */
    struct MultiTileState
    {
        /**
         * @brief True once a region larger than 1x1 is armed: left-click stamps the whole brush
         * instead of a single tile. Reset to false when a 1x1 region is picked.
         */
        bool selectionMode = false;
        int selectedStartID = 0;
        int width = 1;
        int height = 1;
        bool isSelecting = false;
        int selectionStartTileID = -1;  ///< Anchor corner of that drag (-1 = not dragging).
        /// Unused: no editor behavior depends on this value.
        float placementCameraZoom = 1.0f;
        /// Write-only mirror of selectionMode; nothing reads it, so it drives no behavior.
        bool isPlacing = false;
        /**
         * @brief Brush rotation in degrees counter-clockwise: 0, 90, 180, or 270, cycled by R.
         * At 90/270 the stamped footprint is height x width (the dimensions swap).
         */
        int rotation = 0;
        bool flipX = false;  ///< Brush mirror around vertical axis (toggled by F).
        bool flipY = false;  ///< Brush mirror around horizontal axis (toggled by Shift+F).
    };
    MultiTileState m_MultiTile;

    /**
     * @brief Per-key pressed tracking for edge-triggered input.
     *
     * Held per instance rather than in function-local statics, so two Editor
     * instances (or a reload) cannot share debounce state.
     */
    std::bitset<GLFW_KEY_LAST + 1> m_KeyPressed;
    int m_LastDeletedTileX;
    int m_LastDeletedTileY;

    /**
     * @brief Per-layer structure bounds shared by overlay passes.
     *
     * One inclusive bounding box per 4-connected Structure group; groups never span layers.
     * Render invalidates the cache each frame.
     */
    std::vector<Tilemap::StructureBounds> m_CachedNoProjBounds;
    bool m_NoProjBoundsCached = false;  ///< Reset at the start of each Render() call.

    /**
     * @brief Undo history and drag accumulators.
     *
     * Mouse-up commits one command per stroke. Changing mode drops the active accumulator.
     * Load paths must call ClearUndoHistory before replacing the map.
     */
    UndoRedoStack m_UndoStack;
    TilePlaceStrokeAccum m_TileStroke;
    CollisionStrokeAccum m_CollisionStroke;
    ElevationStrokeAccum m_ElevationStroke;
    NavigationStrokeAccum m_NavigationStroke;

    /**
     * @brief On-map rectangular selection and the copied-region clipboard.
     *
     * MapRegionSelection tracks the current rectangular tile-region the user has selected
     * on the map (Ctrl+left-drag to define). m_Clipboard holds the most recently copied
     * region; PasteRegionCmd writes from there.
     */

    /// Inclusive tile rectangle; either corner order is accepted.
    struct MapRegionSelection
    {
        bool active = false;  ///< A committed selection exists (Ctrl+C / overlay use it).
        bool isDragging = false;
        int startX = 0;  ///< Anchor corner column (where the drag began).
        int startY = 0;
        int endX = 0;  ///< Moving corner column (frozen if Ctrl is let go mid-drag).
        int endY = 0;

        [[nodiscard]] int MinX() const { return std::min(startX, endX); }
        [[nodiscard]] int MinY() const { return std::min(startY, endY); }
        [[nodiscard]] int Width() const { return std::abs(endX - startX) + 1; }
        [[nodiscard]] int Height() const { return std::abs(endY - startY) + 1; }
    };
    MapRegionSelection m_MapSelection;
    ClipboardRegion m_Clipboard;  ///< Last Ctrl+C snapshot; empty until the first copy.
};
