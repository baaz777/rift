#pragma once

#include "DialogueTypes.hpp"

#include <string>

/**
 * @struct MysteryDialogueData
 * @brief Quest dialogue template with flag-gated acceptance.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Editor
 *
 * BuildMysteryDialogueTree uses flagName to switch the start node to the revisit option.
 * Keep positional initializers in field order.
 *
 * @code{.cpp}
 * // In Dialogues.cpp:
 * const MysteryDialogueData kMysteryDialogues[] = {
 *     { "lost_locket", "Tessa", "lost_locket_quest",
 *       "details", "I lost my locket...", ...},
 *     // add more entries here; no other file needs to change
 * };
 * @endcode
 *
 * @verbatim
 *   start ---- askMoreText ------> [detailsNodeId]   (only while flag not set)
 *     |------- dismissText ------> (end)             (only while flag not set)
 *     '------- updatePromptText -> update            (only once flag is set)
 *
 *   [detailsNodeId]
 *     |------- questOfferText ---> accept   (only while flag not set;
 *     |                                      sets flag = questJournalText)
 *     '------- declineText ------> (end)    (always shown)
 *
 *   accept ---- acceptOptionText -> (end)
 *   update ---- updateOptionText -> (end)
 * @endverbatim
 */
struct MysteryDialogueData
{
    const char* treeId;
    const char* npcName;
    /// Acceptance flag; its value is questJournalText.
    const char* flagName;
    /// Details node ID and target of askMoreText.
    const char* detailsNodeId;
    const char* startText;
    const char* askMoreText;
    /**
     * @brief Start option that ends the conversation immediately ("I can't help").
     * Hidden once accepted.
     */
    const char* dismissText;
    /// Start option shown only after acceptance; goes to the update node.
    const char* updatePromptText;
    const char* detailsText;
    /**
     * @brief Acceptance option hidden after the quest is accepted.
     *
     * Enters the accept node and sets flagName to questJournalText.
     */
    const char* questOfferText;
    /**
     * @brief Journal text stored in the acceptance flag.
     *
     * GameStateManager::GetQuestDescription returns this text; options do not display it.
     */
    const char* questJournalText;
    /**
     * @brief Details option that ends the conversation without accepting. Unconditional,
     * so it stays selectable even after the quest was taken.
     */
    const char* declineText;
    const char* acceptText;
    const char* acceptOptionText;
    const char* updateText;
    const char* updateOptionText;
};

/// Pool of mystery dialogue templates used when placing NPCs in the editor.
extern const MysteryDialogueData kMysteryDialogues[];
/**
 * @brief Number of entries in kMysteryDialogues. The extern above declares an array of
 * unknown bound, so the count is recoverable only in Dialogues.cpp and is exported here.
 */
extern const int kMysteryDialogueCount;

/**
 * @fn void BuildMysteryDialogueTree(DialogueTree& tree, std::string& outNpcName, const \
 *     MysteryDialogueData& d)
 * @brief Adds the template nodes and writes the NPC name.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Existing unrelated nodes remain. Matching node IDs are replaced.
 */
void BuildMysteryDialogueTree(DialogueTree& tree,
                              std::string& outNpcName,
                              const MysteryDialogueData& d);

/**
 * @fn void BuildEditorAwareDialogueTree(DialogueTree& tree, std::string& outNpcName)
 * @brief Builds Wyatt dialogue; each visit restarts without stored flags.
 * @author Alex (<https://github.com/lextpf>)
 */
void BuildEditorAwareDialogueTree(DialogueTree& tree, std::string& outNpcName);

/**
 * @fn void BuildAnnoyedNPCDialogueTree(DialogueTree& tree, std::string& outNpcName)
 * @brief Builds Salma dialogue with five visit stages.
 * @author Alex (<https://github.com/lextpf>)
 *
 * talked_to_salma stores strings 1 through 5; subsequent visits repeat stage 5.
 */
void BuildAnnoyedNPCDialogueTree(DialogueTree& tree, std::string& outNpcName);
