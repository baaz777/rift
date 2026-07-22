#pragma once

#include "EditorCommand.hpp"
#include "ElevationRole.hpp"
#include "NpcRecord.hpp"
#include "ParticleSystem.hpp"
#include "Tilemap.hpp"

#include <entt/entt.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Tile placement with captured before and after states.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Entries contain tile coordinates and zero-based layers. Tilemap ignores out-of-range writes.
 * Apply writes new values; Revert restores old values.
 */
class PlaceTilesCmd : public EditorCommand
{
public:
    /**
     * @brief One (tile, layer) cell's before/after state. Coordinates are tile indices; entries
     * for out-of-range cells are harmless because the Tilemap setters bounds-check.
     */
    struct Entry
    {
        int tileX;
        int tileY;
        std::size_t layer;  ///< Dynamic layer index.
        int oldTileId;
        float oldRotation;
        int newTileId;
        float newRotation;
        bool oldFlipX = false;
        bool newFlipX = false;
        bool oldFlipY = false;
        bool newFlipY = false;
    };

    explicit PlaceTilesCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Sets explicit collision values; the grid is per cell, independent of layers.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class CollisionToggleCmd : public EditorCommand
{
public:
    /// One cell's before/after collision state.
    struct Entry
    {
        int tileX;
        int tileY;
        bool oldCollision;
        bool newCollision;
    };

    explicit CollisionToggleCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Sets per-cell elevation in pixels, independent of layers.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class ElevationSetCmd : public EditorCommand
{
public:
    /// One cell's before/after elevation, in pixels (0 = ground level, positive = higher).
    struct Entry
    {
        int tileX;
        int tileY;
        int oldElevation;
        int newElevation;
    };

    explicit ElevationSetCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Spawns an NPC blueprint through EntityStore.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Revert finds the NPC by tile, captures its NpcRecord and removes it. Editor tile uniqueness
 * is required. The record preserves instanceId, but respawning creates a new entity handle.
 * Component state outside NpcRecord does not survive undo/redo. Missing NPCs make Revert a no-op.
 */
class PlaceNPCCmd : public EditorCommand
{
public:
    explicit PlaceNPCCmd(NpcRecord npc);

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

private:
    int m_TileX;
    int m_TileY;
    std::optional<NpcRecord> m_Held;
};

/**
 * @brief Captures and removes the NPC at a tile; Revert respawns it.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * A missing NPC leaves both Apply and Revert as no-ops.
 */
class RemoveNPCCmd : public EditorCommand
{
public:
    RemoveNPCCmd(int tileX, int tileY)
        : m_TileX(tileX),
          m_TileY(tileY)
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

private:
    int m_TileX;
    int m_TileY;
    std::optional<NpcRecord> m_Held;
};

/**
 * @brief Before and after values for a per-layer boolean flag.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
struct LayerFlagEntry
{
    int tileX;
    int tileY;
    std::size_t layer;  ///< Layer index (0-based); these flags are per-layer, not per-cell.
    bool oldFlag;
    bool newFlag;
};

/**
 * @brief Before and after stance values on a zero-based layer.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
struct LayerStanceEntry
{
    int tileX;
    int tileY;
    std::size_t layer;  ///< Layer index (0-based); stance is per-layer, not per-cell.
    TileStance oldStance;
    TileStance newStance;
};

/**
 * @brief Set per-layer tile stances for one or more (tile, layer) cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Used by B-mode single-tile click, B-mode Shift+flood-fill, and as part of
 * G-mode structure assignment (which also stamps a structureId).
 */
class SetTileStancesCmd : public EditorCommand
{
public:
    using Entry = LayerStanceEntry;

    explicit SetTileStancesCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Before and after elevation roles on a zero-based layer.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * H mode combines this with per-cell height changes in a CompositeCmd.
 */
struct LayerElevationRoleEntry
{
    int tileX;
    int tileY;
    std::size_t layer;  ///< Layer index (0-based).
    ElevationRole oldRole;
    ElevationRole newRole;
};

/**
 * @brief Set per-layer elevation roles for one or more (tile, layer) cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class SetElevationRolesCmd : public EditorCommand
{
public:
    using Entry = LayerElevationRoleEntry;

    explicit SetElevationRolesCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Set per-layer Y-sort-plus flags for one or more (tile, layer) cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class YSortPlusToggleCmd : public EditorCommand
{
public:
    using Entry = LayerFlagEntry;

    explicit YSortPlusToggleCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Set per-layer Y-sort-minus flags for one or more (tile, layer) cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class YSortMinusToggleCmd : public EditorCommand
{
public:
    using Entry = LayerFlagEntry;

    explicit YSortMinusToggleCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Changes animation assignments and restores overwritten tile IDs on undo.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * SetTileAnimation can replace tiles with the first animation frame. Revert restores the
 * animation ID, then oldTileId, including when the old animation ID is -1.
 */
class SetTileAnimationCmd : public EditorCommand
{
public:
    /// One (tile, layer) cell's before/after animation assignment.
    struct Entry
    {
        int tileX;
        int tileY;
        int layer;  ///< Layer index (0-based); animation ids are per-layer.
        int oldAnimId;
        int newAnimId;
        int oldTileId;
    };

    explicit SetTileAnimationCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Set per-tile structureId for one or more (tile, layer) cells.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Used by G-mode flood-fill assign and right-click clear. Often emitted with
 * a SetTileStancesCmd via CompositeCmd because structure assignment
 * normally also stamps TileStance::Structure onto the tiles.
 */
class SetTileStructureIdsCmd : public EditorCommand
{
public:
    /// One (tile, layer) cell's before/after structure assignment.
    struct Entry
    {
        int tileX;
        int tileY;
        /**
         * @brief One-based layer passed to GetTileStructureId and SetTileStructureId.
         *
         * Editor callers supply m_CurrentLayer + 1. Zero is rejected.
         */
        int layer;
        int oldStructId;
        int newStructId;
    };

    explicit SetTileStructureIdsCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
};

/**
 * @brief Adds a structure and removes it by captured ID on undo.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * LIFO undo must leave it last in the vector. Revert clears tile membership without
 * snapshotting it; redo does not restore that membership.
 */
class AddStructureCmd : public EditorCommand
{
public:
    AddStructureCmd(glm::vec2 leftAnchor, glm::vec2 rightAnchor, std::string name = {})
        : m_LeftAnchor(leftAnchor),
          m_RightAnchor(rightAnchor),
          m_Name(std::move(name))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] int StructureId() const { return m_StructureId; }

private:
    glm::vec2 m_LeftAnchor;
    glm::vec2 m_RightAnchor;
    std::string m_Name;
    int m_StructureId = -1;
};

/**
 * @brief Removes a structure and snapshots its tile references.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Revert reinserts at the original ID before restoring captured tile membership.
 * The capture loop supplies zero-based indices to a one-based accessor, so membership
 * in the highest layer is not captured.
 */
class RemoveStructureCmd : public EditorCommand
{
public:
    explicit RemoveStructureCmd(int id)
        : m_Id(id)
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

private:
    struct TileRef
    {
        int x;
        int y;
        int layer;
    };

    int m_Id;
    NoProjectionStructure m_Snapshot;
    std::vector<TileRef> m_TileRefs;
    bool m_Captured = false;
};

/**
 * @brief Appends a zone; undo removes the last zone.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Requires LIFO undo. Appending outside the stack can make Revert remove the wrong zone.
 */
class AddParticleZoneCmd : public EditorCommand
{
public:
    explicit AddParticleZoneCmd(ParticleZone zone)
        : m_Zone(std::move(zone))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

private:
    ParticleZone m_Zone;
};

/**
 * @brief Remove a particle zone at a specific index, capturing its data so
 * Revert can re-insert at the same index (preserves index-based tracking).
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class RemoveParticleZoneCmd : public EditorCommand
{
public:
    explicit RemoveParticleZoneCmd(std::size_t index)
        : m_Index(index)
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

private:
    std::size_t m_Index;
    ParticleZone m_Snapshot{};
    bool m_Captured = false;
};

/**
 * @brief Snapshot of one (tile, layer) cell for clipboard / paste operations.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Mirrors the per-layer per-tile fields of TileLayer. Every default here matches the
 * corresponding TileLayer default, so a default-constructed instance pastes as "empty".
 */
struct ClipboardCellLayer
{
    int tileId = -1;        ///< Tileset id (-1 = empty cell).
    float rotation = 0.0f;  ///< Rotation in degrees.
    TileStance stance = TileStance::Flat;
    ElevationRole elevationRole = ElevationRole::Ground;
    bool flipX = false;
    bool flipY = false;
    /**
     * @brief Structure ID, or -1 for automatic grouping.
     *
     * Clipboard slot i uses the one-based accessor with i, so it captures layer i - 1.
     * Slot 0 is always -1 and the top layer is omitted. Writing applies the same offset.
     */
    int structureId = -1;
    bool ySortPlus = false;
    bool ySortMinus = false;
    int animationMap = -1;  ///< Animated-tile id (-1 = not animated).
};

/**
 * @brief Clipboard tile with ten layers and per-cell flags.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Layers beyond LAYER_COUNT are neither copied nor overwritten. NPCs, particle zones and
 * structure definitions are excluded. Structure IDs have the offset documented on
 * ClipboardCellLayer::structureId.
 */
struct ClipboardCell
{
    static constexpr std::size_t LAYER_COUNT = 10;
    ClipboardCellLayer layers[LAYER_COUNT];  ///< Per-layer state, indexed by layer number.
    bool collision = false;                  ///< Per-cell blocking flag (not per-layer).
    bool navigation = false;                 ///< Per-cell NPC walkability flag (not per-layer).
    int elevation = 0;                       ///< Per-cell elevation in pixels (0 = ground level).
};

/**
 * @brief Rectangular region of tile snapshots used by Ctrl+C / Ctrl+V.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Cells outside the source map retain empty defaults. Paste skips destinations outside
 * the map, but writes those empty defaults when their destinations are inside the map.
 * Cells are stored in row-major order; width and height must match the cell vector.
 */
struct ClipboardRegion
{
    int width = 0;
    int height = 0;
    std::vector<ClipboardCell> cells;

    /**
     * @fn bool ClipboardRegion::Empty() const
     * @brief True when the region holds nothing pasteable.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool Empty() const { return width <= 0 || height <= 0 || cells.empty(); }
};

/**
 * @fn void ReflectClipboardRegion(ClipboardRegion& region, bool flipXAxis)
 * @brief Reflect a region in place around its geometric center.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Cell positions swap (columns for X-axis, rows for Y-axis), and per-tile
 * each layer's flip flag on the chosen axis is toggled while rotation is
 * negated (rot -> fmod(360 - rot, 360)). The transform is an involution,
 * so applying it twice reproduces the original.
 *
 * @param region    Region to mutate in place.
 * @param flipXAxis True for X-reflection (mirror around vertical axis,
 *                  toggles flipX), false for Y-reflection (toggles flipY).
 */
void ReflectClipboardRegion(ClipboardRegion& region, bool flipXAxis);

/**
 * @brief Paste a clipboard region at a destination tile, capturing the
 * pre-paste destination state for lossless undo.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 */
class PasteRegionCmd : public EditorCommand
{
public:
    PasteRegionCmd(int destX, int destY, ClipboardRegion source)
        : m_DestX(destX),
          m_DestY(destY),
          m_Source(std::move(source))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    /**
     * @fn ClipboardCell PasteRegionCmd::ReadCellFrom(const Tilemap& tm, int x, int y)
     * @brief Read a single tile (all 10 layers + per-tile fields) into a ClipboardCell. Public so
     * the editor's Ctrl+C path can snapshot a region using the same logic this cmd uses for its own
     * dest snapshot.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] static ClipboardCell ReadCellFrom(const Tilemap& tm, int x, int y);

    /**
     * @fn ClipboardRegion PasteRegionCmd::SnapshotRegion( const Tilemap& tm, int x, int y, int \
     *     width, int height)
     * @brief Snapshot a (width x height) region starting at (x, y) into a ClipboardRegion. Used by
     * Editor's Ctrl+C handler.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] static ClipboardRegion SnapshotRegion(
        const Tilemap& tm, int x, int y, int width, int height);

private:
    int m_DestX;
    int m_DestY;
    ClipboardRegion m_Source;
    ClipboardRegion m_DestSnapshot;
    bool m_Captured = false;

    static void WriteCellInto(Tilemap& tm, int destX, int destY, const ClipboardCell& cell);
};

/**
 * @brief Add an animated tile definition (K-mode Enter on collected frames).
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Apply: pushes the AnimatedTile onto Tilemap's vector, captures the assigned id.
 * Revert: pops the last animation (LIFO invariant - any per-tile reference to
 * this animation must have been removed by an earlier-stacked
 * SetTileAnimationCmd::Revert).
 */
class AddAnimatedTileCmd : public EditorCommand
{
public:
    explicit AddAnimatedTileCmd(AnimatedTile anim)
        : m_Anim(std::move(anim))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] int AnimId() const { return m_AnimId; }

private:
    AnimatedTile m_Anim;
    int m_AnimId = -1;
};

/**
 * @brief Groups commands into one undo entry.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Apply runs children forward; Revert runs them in reverse so dependent changes unwind
 * in reverse order. Children must be non-null. This is one history entry, not an atomic
 * transaction: a child exception leaves prior child effects applied.
 * Use UndoRedoStack::Push if the mutations have already been applied.
 */
class CompositeCmd : public EditorCommand
{
public:
    CompositeCmd(std::string label, std::vector<std::unique_ptr<EditorCommand>> children)
        : m_Label(std::move(label)),
          m_Children(std::move(children))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override { return m_Label; }

    [[nodiscard]] std::size_t ChildCount() const { return m_Children.size(); }

private:
    std::string m_Label;
    std::vector<std::unique_ptr<EditorCommand>> m_Children;
};

/**
 * @brief Sets walkability and captures displaced NPCs.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Apply removes NPCs on newly blocked tiles; Revert restores their blueprints. Both rebuild
 * patrol routes. Each Apply replaces the displacement snapshot.
 */
class NavigationStrokeCmd : public EditorCommand
{
public:
    /// One cell's before/after walkability. Navigation is a per-cell grid, so no layer.
    struct Entry
    {
        int tileX;
        int tileY;
        bool oldWalkable;
        bool newWalkable;
    };

    explicit NavigationStrokeCmd(std::vector<Entry> entries)
        : m_Entries(std::move(entries))
    {
    }

    void Apply(Tilemap& tilemap, entt::registry& npcs) override;
    void Revert(Tilemap& tilemap, entt::registry& npcs) override;
    [[nodiscard]] std::string DebugLabel() const override;

    [[nodiscard]] const std::vector<Entry>& Entries() const { return m_Entries; }

private:
    std::vector<Entry> m_Entries;
    /**
     * @brief Detached NPC blueprints for the NPCs this stroke displaced: filled by Apply,
     * re-spawned and cleared by Revert.
     */
    std::vector<NpcRecord> m_ErasedNPCs;
};
