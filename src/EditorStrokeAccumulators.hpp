#pragma once

#include "EditorCommands.hpp"
#include "UndoRedoStack.hpp"

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Stroke key helper; keep it stateless across translation units.
 * @author Alex (<https://github.com/lextpf>)
 */
namespace
{

/**
 * @fn std::uint64_t MakeStrokeKey(int x, int y, std::size_t layer)
 * @brief Packs a zero-based layer and tile coordinates into a stroke key.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Coordinates wrap modulo 2^21. The upper 22 bits hold the layer. Callers must keep
 * coordinates within one such window to avoid key collisions.
 *
 * @verbatim
 *   bit 63                 42 41                 21 20                  0
 *        +-------------------+---------------------+---------------------+
 *        |  layer (22 bits)  |  y & 0x1FFFFF (21)  |  x & 0x1FFFFF (21)  |
 *        +-------------------+---------------------+---------------------+
 *          shifted << 42        shifted << 21          no shift
 * @endverbatim
 */
inline std::uint64_t MakeStrokeKey(int x, int y, std::size_t layer)
{
    return (static_cast<std::uint64_t>(layer) << 42) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y) & 0x1FFFFF) << 21) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x) & 0x1FFFFF));
}

}  // Namespace

/**
 * @struct TilePlaceStrokeAccum
 * @brief Coalesces a drag into one undo entry.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Shared by the four accumulators: Begin clears prior data; Touch is inactive outside a
 * stroke and retains the first old value and last new value per cell. Callers apply tile
 * writes during the drag. Commit ends and clears the stroke; empty strokes add nothing.
 * Drop keeps applied changes but discards their undo data.
 *
 * Commit uses Push, except NavigationStrokeAccum, which uses Execute for NPC displacement.
 *
 * @verbatim
 *   Begin()                 // mouse-down
 *   Touch(...)              // each per-tile mutation during drag
 *   Commit(stack)           // mouse-up: builds cmd, pushes
 *   Drop()                  // Mid-drag mode switch: discard without commit
 * @endverbatim
 */
struct TilePlaceStrokeAccum
{
    bool active = false;
    std::vector<PlaceTilesCmd::Entry> entries;
    /**
     * @brief MakeStrokeKey(x, y, layer) -> index into entries, so a re-touched tile updates its
     * existing entry instead of appending a second one. Valid only within one stroke.
     */
    std::unordered_map<std::uint64_t, std::size_t> indexOf;

    void Begin()
    {
        active = true;
        entries.clear();
        indexOf.clear();
    }

    void Touch(int x,
               int y,
               std::size_t layer,
               int oldId,
               float oldRot,
               int newId,
               float newRot,
               bool oldFlipX = false,
               bool oldFlipY = false,
               bool newFlipX = false,
               bool newFlipY = false)
    {
        if (!active)
            return;
        auto key = MakeStrokeKey(x, y, layer);
        auto it = indexOf.find(key);
        if (it == indexOf.end())
        {
            indexOf.emplace(key, entries.size());
            PlaceTilesCmd::Entry e{};
            e.tileX = x;
            e.tileY = y;
            e.layer = layer;
            e.oldTileId = oldId;
            e.oldRotation = oldRot;
            e.newTileId = newId;
            e.newRotation = newRot;
            e.oldFlipX = oldFlipX;
            e.newFlipX = newFlipX;
            e.oldFlipY = oldFlipY;
            e.newFlipY = newFlipY;
            entries.push_back(e);
        }
        else
        {
            entries[it->second].newTileId = newId;
            entries[it->second].newRotation = newRot;
            entries[it->second].newFlipX = newFlipX;
            entries[it->second].newFlipY = newFlipY;
        }
    }

    void Commit(UndoRedoStack& stack)
    {
        if (active && !entries.empty())
            stack.Push(std::make_unique<PlaceTilesCmd>(std::move(entries)));
        active = false;
        entries.clear();
        indexOf.clear();
    }

    void Drop()
    {
        active = false;
        entries.clear();
        indexOf.clear();
    }

    [[nodiscard]] bool IsActive() const { return active; }
};

/**
 * @struct CollisionStrokeAccum
 * @brief Collision stroke with the TilePlaceStrokeAccum lifecycle.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Keys are per cell and use layer zero.
 */
struct CollisionStrokeAccum
{
    bool active = false;
    std::vector<CollisionToggleCmd::Entry> entries;
    std::unordered_map<std::uint64_t, std::size_t> indexOf;

    void Begin()
    {
        active = true;
        entries.clear();
        indexOf.clear();
    }

    void Touch(int x, int y, bool oldHas, bool newHas)
    {
        if (!active)
            return;
        auto key = MakeStrokeKey(x, y, 0);
        auto it = indexOf.find(key);
        if (it == indexOf.end())
        {
            indexOf.emplace(key, entries.size());
            entries.push_back(CollisionToggleCmd::Entry{x, y, oldHas, newHas});
        }
        else
        {
            entries[it->second].newCollision = newHas;
        }
    }

    void Commit(UndoRedoStack& stack)
    {
        if (active && !entries.empty())
            stack.Push(std::make_unique<CollisionToggleCmd>(std::move(entries)));
        active = false;
        entries.clear();
        indexOf.clear();
    }

    void Drop()
    {
        active = false;
        entries.clear();
        indexOf.clear();
    }

    [[nodiscard]] bool IsActive() const { return active; }
};

/**
 * @struct ElevationStrokeAccum
 * @brief Height and role stroke with the TilePlaceStrokeAccum lifecycle.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Height keys use layer zero; role keys include the layer in a separate index.
 * Commit groups both payloads into one undo entry.
 */
struct ElevationStrokeAccum
{
    bool active = false;
    std::vector<ElevationSetCmd::Entry> entries;
    std::unordered_map<std::uint64_t, std::size_t> indexOf;
    std::vector<LayerElevationRoleEntry> roleEntries;
    std::unordered_map<std::uint64_t, std::size_t> roleIndexOf;

    void Begin()
    {
        active = true;
        entries.clear();
        indexOf.clear();
        roleEntries.clear();
        roleIndexOf.clear();
    }

    void Touch(int x, int y, int oldElev, int newElev)
    {
        if (!active)
            return;
        auto key = MakeStrokeKey(x, y, 0);
        auto it = indexOf.find(key);
        if (it == indexOf.end())
        {
            indexOf.emplace(key, entries.size());
            entries.push_back(ElevationSetCmd::Entry{x, y, oldElev, newElev});
        }
        else
        {
            entries[it->second].newElevation = newElev;
        }
    }

    void TouchRole(int x, int y, std::size_t layer, ElevationRole oldRole, ElevationRole newRole)
    {
        if (!active)
            return;
        auto key = MakeStrokeKey(x, y, layer);
        auto it = roleIndexOf.find(key);
        if (it == roleIndexOf.end())
        {
            roleIndexOf.emplace(key, roleEntries.size());
            roleEntries.push_back(LayerElevationRoleEntry{x, y, layer, oldRole, newRole});
        }
        else
        {
            roleEntries[it->second].newRole = newRole;
        }
    }

    void Commit(UndoRedoStack& stack)
    {
        if (active)
        {
            const bool haveHeights = !entries.empty();
            const bool haveRoles = !roleEntries.empty();
            if (haveHeights && haveRoles)
            {
                std::vector<std::unique_ptr<EditorCommand>> children;
                children.push_back(std::make_unique<ElevationSetCmd>(std::move(entries)));
                children.push_back(std::make_unique<SetElevationRolesCmd>(std::move(roleEntries)));
                stack.Push(std::make_unique<CompositeCmd>("Paint elevation", std::move(children)));
            }
            else if (haveHeights)
            {
                stack.Push(std::make_unique<ElevationSetCmd>(std::move(entries)));
            }
            else if (haveRoles)
            {
                stack.Push(std::make_unique<SetElevationRolesCmd>(std::move(roleEntries)));
            }
        }
        Drop();
    }

    void Drop()
    {
        active = false;
        entries.clear();
        indexOf.clear();
        roleEntries.clear();
        roleIndexOf.clear();
    }

    [[nodiscard]] bool IsActive() const { return active; }
};

/**
 * @struct NavigationStrokeAccum
 * @brief Navigation stroke with deferred NPC displacement.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Begin, Touch and Drop follow TilePlaceStrokeAccum. Commit uses Execute to capture and
 * remove displaced NPCs and rebuild patrol routes. Flag writes already applied during
 * the drag are repeated. Drop leaves those writes without resolving NPC displacement.
 */
struct NavigationStrokeAccum
{
    bool active = false;
    std::vector<NavigationStrokeCmd::Entry> entries;
    std::unordered_map<std::uint64_t, std::size_t> indexOf;

    void Begin()
    {
        active = true;
        entries.clear();
        indexOf.clear();
    }

    void Touch(int x, int y, bool oldWalk, bool newWalk)
    {
        if (!active)
            return;
        auto key = MakeStrokeKey(x, y, 0);
        auto it = indexOf.find(key);
        if (it == indexOf.end())
        {
            indexOf.emplace(key, entries.size());
            entries.push_back(NavigationStrokeCmd::Entry{x, y, oldWalk, newWalk});
        }
        else
        {
            entries[it->second].newWalkable = newWalk;
        }
    }

    void Commit(UndoRedoStack& stack, Tilemap& tilemap, entt::registry& npcs)
    {
        if (active && !entries.empty())
            stack.Execute(std::make_unique<NavigationStrokeCmd>(std::move(entries)), tilemap, npcs);
        active = false;
        entries.clear();
        indexOf.clear();
    }

    void Drop()
    {
        active = false;
        entries.clear();
        indexOf.clear();
    }

    [[nodiscard]] bool IsActive() const { return active; }
};
