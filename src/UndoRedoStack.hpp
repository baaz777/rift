#pragma once

#include "EditorCommand.hpp"

#include <entt/entt.hpp>

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Tilemap;

/**
 * @brief Owns command history with oldest-first eviction.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * Execute applies before recording; Push records an already-applied command.
 * A non-null command clears redo history. Null commands leave both histories unchanged.
 * Eviction and Clear destroy snapshots without reverting world mutations.
 * Apply and Revert must use the world in which the command was recorded. Clear history
 * when replacing that world; the stack does not detect a different tilemap or registry.
 *
 * Command callbacks run synchronously. Exceptions propagate without rollback. Undo and
 * Redo remove the command from its source history before invoking the callback.
 */
class UndoRedoStack
{
public:
    /// Undo-history depth used when no capacity is passed to the constructor.
    static constexpr std::size_t DEFAULT_CAPACITY = 100;

    UndoRedoStack() = default;

    /**
     * @fn UndoRedoStack::UndoRedoStack(std::size_t capacity)
     * @brief Limit the number of retained undo entries.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param capacity Maximum entries. Zero disables history but Execute still applies changes.
     */
    explicit UndoRedoStack(std::size_t capacity)
        : m_Capacity(capacity)
    {
    }

    /**
     * @fn void UndoRedoStack::Execute(std::unique_ptr<EditorCommand> cmd, Tilemap& tilemap, \
     *     entt::registry& npcs)
     * @brief Apply a command and transfer it to undo history.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Apply runs before history changes, so it can capture prior state. A null command
     * has no effect. A successful non-null call discards redo history.
     */
    void Execute(std::unique_ptr<EditorCommand> cmd, Tilemap& tilemap, entt::registry& npcs)
    {
        if (!cmd)
            return;
        cmd->Apply(tilemap, npcs);
        Push(std::move(cmd));
    }

    /**
     * @fn void UndoRedoStack::Push(std::unique_ptr<EditorCommand> cmd)
     * @brief Record a command for a mutation that is already applied.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The command must hold complete undo data. Push never calls Apply, including for
     * commands that normally capture snapshots there. A null command has no effect.
     */
    void Push(std::unique_ptr<EditorCommand> cmd)
    {
        if (!cmd)
            return;
        m_Redo.clear();
        m_Undo.push_back(std::move(cmd));
        while (m_Undo.size() > m_Capacity)
            m_Undo.pop_front();
    }

    /**
     * @fn bool UndoRedoStack::Undo(Tilemap& tilemap, entt::registry& npcs)
     * @brief Revert the newest command and move it to redo history.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return True when a command was dispatched; false when undo history is empty.
     * A dispatched command can itself have no effect.
     */
    bool Undo(Tilemap& tilemap, entt::registry& npcs)
    {
        if (m_Undo.empty())
            return false;
        auto cmd = std::move(m_Undo.back());
        m_Undo.pop_back();
        cmd->Revert(tilemap, npcs);
        m_Redo.push_back(std::move(cmd));
        return true;
    }

    /**
     * @fn bool UndoRedoStack::Redo(Tilemap& tilemap, entt::registry& npcs)
     * @brief Reapply the newest reverted command and move it to undo history.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return True when a command was dispatched; false when redo history is empty.
     * A dispatched command can itself have no effect.
     */
    bool Redo(Tilemap& tilemap, entt::registry& npcs)
    {
        if (m_Redo.empty())
            return false;
        auto cmd = std::move(m_Redo.back());
        m_Redo.pop_back();
        cmd->Apply(tilemap, npcs);
        m_Undo.push_back(std::move(cmd));
        return true;
    }

    /**
     * @fn void UndoRedoStack::Clear()
     * @brief Drop all history, leaving both the undo and redo stacks empty.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Clear()
    {
        m_Undo.clear();
        m_Redo.clear();
    }

    [[nodiscard]] bool CanUndo() const { return !m_Undo.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_Redo.empty(); }

    [[nodiscard]] std::size_t UndoSize() const { return m_Undo.size(); }
    [[nodiscard]] std::size_t RedoSize() const { return m_Redo.size(); }
    [[nodiscard]] std::size_t Capacity() const { return m_Capacity; }

    /**
     * @fn std::string UndoRedoStack::UndoLabel() const
     * @brief Label of the next-to-undo command, or empty string if none.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::string UndoLabel() const
    {
        return m_Undo.empty() ? std::string{} : m_Undo.back()->DebugLabel();
    }

    /**
     * @fn std::string UndoRedoStack::RedoLabel() const
     * @brief Label of the next-to-redo command, or empty string if none.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::string RedoLabel() const
    {
        return m_Redo.empty() ? std::string{} : m_Redo.back()->DebugLabel();
    }

private:
    std::deque<std::unique_ptr<EditorCommand>> m_Undo;  ///< Undo history; newest action at back.
    std::deque<std::unique_ptr<EditorCommand>> m_Redo;  ///< Reverted commands; cleared on mutation.
    std::size_t m_Capacity = DEFAULT_CAPACITY;  ///< Max undo entries before oldest eviction.
};
