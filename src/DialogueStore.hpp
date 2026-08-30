#pragma once

#include "DialogueHandle.hpp"
#include "DialogueTypes.hpp"

#include <cstddef>
#include <unordered_map>

/**
 * @class DialogueStore
 * @brief Append-only owner of dialogue trees.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * Handles belong to the store that issued them. Despawning an NPC does not erase its tree;
 * loads and redo operations can accumulate unused trees. References remain valid across Add.
 * DialogueManager copies the active tree so a conversation survives NPC removal.
 *
 * ```mermaid
 * flowchart LR
 *     NPC["NPC entity"] --> Comp["Dialogue component"]
 *     Comp --> H["DialogueHandle .id"]
 *     H -- "map key" --> Map["DialogueStore::m_Trees"]
 *     Map --> Tree["DialogueTree (the one owner)"]
 *     Tree -- "copied at StartDialogue" --> Active["DialogueManager::m_ActiveTree"]
 * ```
 */
class DialogueStore
{
public:
    /**
     * @fn DialogueHandle DialogueStore::Add(DialogueTree tree)
     * @brief Take ownership of a tree (moved in); returns its handle.
     * @author Alex (<https://github.com/lextpf>)
     */
    DialogueHandle Add(DialogueTree tree);

    /**
     * @fn bool DialogueStore::IsValid(DialogueHandle handle) const
     * @brief True if handle refers to a stored tree.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsValid(DialogueHandle handle) const;

    /**
     * @fn bool DialogueStore::HasTree(DialogueHandle handle) const
     * @brief True if handle refers to a stored, non-empty tree (has nodes).
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool HasTree(DialogueHandle handle) const;

    /**
     * @fn const DialogueTree& DialogueStore::Get(DialogueHandle handle) const
     * @brief Read a stored tree or the shared empty fallback.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return A borrowed tree. Stored references survive Add calls until this store is
     * destroyed; the empty fallback has static lifetime.
     */
    [[nodiscard]] const DialogueTree& Get(DialogueHandle handle) const;

    [[nodiscard]] std::size_t Count() const { return m_Trees.size(); }

private:
    std::unordered_map<DialogueId, DialogueTree> m_Trees;  ///< The owned trees.
    DialogueId m_NextId = 1;                               ///< Next id to mint (0 = invalid).
};
