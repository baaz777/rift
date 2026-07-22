#pragma once

#include <entt/entt.hpp>

#include <string>
#include <vector>

class Tilemap;

/**
 * @class EditorCommand
 * @brief Undoable mutation against one tilemap and registry.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Use Execute when Apply must capture prior state. Push is only for already-applied
 * mutations with complete undo data. Every Apply and Revert must receive the same world.
 * Capture coordinates, IDs and values; never retain EditorContext references.
 *
 * ```mermaid
 * flowchart LR
 *     classDef ctor fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef apply fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef revert fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *
 *     A[ctor: eager delta<br/>or capture deferred to Apply]:::ctor
 *     A --> B1[UndoRedoStack.Execute]:::apply
 *     A --> B2[UndoRedoStack.Push<br/>tilemap already mutated]:::apply
 *     B1 --> C[Apply: writes newVal]:::apply
 *     C --> D{User presses Ctrl+Z?}
 *     B2 --> D
 *     D -->|yes| E[Revert: writes oldVal]:::revert
 *     E --> F{User presses Ctrl+Y?}
 *     F -->|yes| C
 * ```
 */
class EditorCommand
{
public:
    virtual ~EditorCommand() = default;

    /**
     * @fn void EditorCommand::Apply(Tilemap& tilemap, entt::registry& npcs)
     * @brief Runs on Execute and every Redo; Push skips this call.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void Apply(Tilemap& tilemap, entt::registry& npcs) = 0;

    /**
     * @fn void EditorCommand::Revert(Tilemap& tilemap, entt::registry& npcs)
     * @brief Restores the captured state on Undo.
     * @author Alex (<https://github.com/lextpf>)
     */
    virtual void Revert(Tilemap& tilemap, entt::registry& npcs) = 0;

    /**
     * @fn std::string EditorCommand::DebugLabel() const
     * @brief Short human-readable label for status toasts and HUD overlays.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] virtual std::string DebugLabel() const = 0;
};
