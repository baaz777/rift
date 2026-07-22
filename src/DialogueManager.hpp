#pragma once

#include "DialogueTypes.hpp"

#include <entt/entt.hpp>

#include <string>
#include <vector>

class GameStateManager;

/**
 * @class DialogueManager
 * @brief Dialogue state, option filtering and consequences.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * Copies the NPC tree from DialogueStore for the conversation. Removing or changing the
 * NPC does not update this copy. Call from the main thread.
 *
 * Visible options are cached on node entry. External flag changes do not refresh an
 * open node, and confirming a cached option does not evaluate its conditions again.
 *
 * ```mermaid
 * stateDiagram-v2
 *     classDef idle fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef active fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef done fill:#4a2020,stroke:#ef4444,color:#e2e8f0
 *
 *     state "Idle" as Idle:::idle
 *     state "Showing node" as Show:::active
 *     state "Awaiting input" as Wait:::active
 *     state "Executing consequences" as Exec:::active
 *     state "Ended" as Done:::done
 *
 *     [*] --> Idle
 *     Idle --> Show: StartDialogue(npc)
 *     Show --> Wait: RefreshVisibleOptions
 *     Wait --> Wait: SelectPrevious / SelectNext
 *     Wait --> Exec: ConfirmSelection (some option visible)
 *     Wait --> Done: ConfirmSelection (no visible option)
 *     Exec --> Show: nextNodeId resolves to a node
 *     Exec --> Done: nextNodeId empty
 *     Exec --> Done: nextNodeId unknown (logged as an error)
 *     Show --> Done: EndDialogue
 *     Wait --> Done: EndDialogue
 *     Done --> Idle
 * ```
 *
 * @code{.cpp}
 * DialogueManager d;
 * d.Initialize(&stateManager);
 *
 * // Start conversation with an NPC (entity handle + its registry)
 * if (d.StartDialogue(npc, world)) {
 *     // Dialogue is now active
 * }
 *
 * // Handle player input each frame
 * if (d.IsActive()) {
 *     if (upPressed) d.SelectPrevious();
 *     if (downPressed) d.SelectNext();
 *     if (confirmPressed) d.ConfirmSelection();
 * }
 *
 * // Render current dialogue state
 * if (d.IsActive()) {
 *     const auto* node = d.GetCurrentNode();
 *     RenderDialogue(node->speaker, node->text);
 *     for (const auto* opt : d.GetVisibleOptions()) {
 *         RenderOption(opt->text);
 *     }
 * }
 * @endcode
 */
class DialogueManager
{
public:
    DialogueManager();

    /**
     * @fn void DialogueManager::Initialize(GameStateManager* stateManager)
     * @brief Borrows the flag store; it must outlive this manager.
     * @author Alex (<https://github.com/lextpf>)
     *
     * A null store prevents StartDialogue and makes consequence execution a no-op.
     */
    void Initialize(GameStateManager* stateManager);

    /**
     * @fn bool DialogueManager::StartDialogue(entt::entity npc, const entt::registry& world)
     * @brief Copies the NPC tree and enters its start node.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Returns false if a conversation is active, the flag store is null, the NPC is invalid,
     * or its Dialogue handle does not resolve to a nonempty tree with a start node.
     * Active-dialogue and missing-start failures are logged. World is not retained.
     */
    bool StartDialogue(entt::entity npc, const entt::registry& world);

    /**
     * @fn void DialogueManager::EndDialogue()
     * @brief Clears conversation state; safe while inactive.
     * @author Alex (<https://github.com/lextpf>)
     */
    void EndDialogue();

    [[nodiscard]] bool IsActive() const { return m_Active; }

    /**
     * @fn const DialogueNode* DialogueManager::GetCurrentNode() const
     * @brief Returns nullptr while inactive.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] const DialogueNode* GetCurrentNode() const { return m_CurrentNode; }

    /**
     * @fn const std::vector<const DialogueOption*>& DialogueManager::GetVisibleOptions() const
     * @brief Read the options admitted when the current node was entered.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The vector is borrowed and is rebuilt on node changes. Its pointers refer to the
     * manager's active tree. EndDialogue or a new conversation invalidates those pointers.
     */
    [[nodiscard]] const std::vector<const DialogueOption*>& GetVisibleOptions() const
    {
        return m_VisibleOptions;
    }

    /**
     * @fn int DialogueManager::GetSelectedOptionIndex() const
     * @brief Zero-based index into the visible options.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] int GetSelectedOptionIndex() const { return m_SelectedOption; }

    /**
     * @fn void DialogueManager::SelectPrevious()
     * @brief Move selection up (previous option).
     * @author Alex (<https://github.com/lextpf>)
     *
     * Wraps around to last option if at the top.
     */
    void SelectPrevious();

    /**
     * @fn void DialogueManager::SelectNext()
     * @brief Move selection down (next option).
     * @author Alex (<https://github.com/lextpf>)
     *
     * Wraps around to first option if at the bottom.
     */
    void SelectNext();

    /**
     * @fn void DialogueManager::ConfirmSelection()
     * @brief Apply the selected option and enter its target node.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Consequences run in order before resolving the target. An empty or unknown target
     * ends the conversation; an unknown target logs an error. Applied flag changes remain.
     * With no visible options, the conversation ends without running consequences.
     */
    void ConfirmSelection();

private:
    /**
     * @fn void DialogueManager::SelectOption(int optionIndex)
     * @brief Select a dialogue option by index.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Triggers the selected option's consequences and transitions
     * to the next node. If the option's nextNodeId is empty,
     * the dialogue ends.
     *
     * @param optionIndex Index in the visible options list (0-based)
     */
    void SelectOption(int optionIndex);

    /**
     * @fn void DialogueManager::ExecuteConsequences(const std::vector<DialogueConsequence>& \
     *     consequences)
     * @brief Execute consequences for a selected option.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Processes each consequence in order, modifying game state flags.
     *
     * @param consequences List of consequences to execute
     */
    void ExecuteConsequences(const std::vector<DialogueConsequence>& consequences);

    /**
     * @fn void DialogueManager::RefreshVisibleOptions()
     * @brief Refresh the visible options list based on current conditions.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Evaluates each option's conditions against the current game state
     * and populates m_VisibleOptions with those that pass all checks.
     */
    void RefreshVisibleOptions();

    /**
     * @fn void DialogueManager::TransitionToNode(const std::string& nodeId)
     * @brief Enters a node and resets selection to zero.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Rebuilds visible options on success. An empty ID or missing tree ends the dialogue;
     * an unknown nonempty ID also ends it and logs an error.
     */
    void TransitionToNode(const std::string& nodeId);

    /**
     * @brief Non-owning flag store set by Initialize; must outlive this manager. Null
     * blocks `StartDialogue` and makes consequence execution a no-op.
     */
    GameStateManager* m_StateManager = nullptr;

    DialogueTree
        m_ActiveTree;  ///< Owned copy of active dialogue tree (avoids dangling NPC pointers).
    bool m_Active = false;
    const DialogueTree* m_CurrentTree = nullptr;
    const DialogueNode* m_CurrentNode = nullptr;

    std::vector<const DialogueOption*> m_VisibleOptions;
    int m_SelectedOption = 0;  ///< Index of highlighted option in m_VisibleOptions.
};
