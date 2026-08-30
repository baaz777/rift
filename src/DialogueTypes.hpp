#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @struct DialogueCondition
 * @brief Flag condition required for option visibility.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * All conditions on an option must pass.
 *
 * @code{.cpp}
 * // Only show option if player has completed intro quest
 * DialogueCondition c(DialogueCondition::Type::FLAG_SET, "intro_complete");
 * option.conditions.push_back(c);
 * @endcode
 */
struct DialogueCondition
{
    /// Types of condition checks.
    enum class Type
    {
        FLAG_SET,
        FLAG_NOT_SET,
        FLAG_EQUALS
    };

    Type type = Type::FLAG_SET;
    std::string key;
    std::string value;  ///< Expected value (only used for FLAG_EQUALS).

    DialogueCondition() = default;

    DialogueCondition(Type t, const std::string& k, const std::string& v = "")
        : type(t),
          key(k),
          value(v)
    {
    }
};

/**
 * @struct DialogueConsequence
 * @brief Flag change applied when an option is selected.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * Consequences execute in declaration order.
 *
 * @code{.cpp}
 * // Mark quest as accepted when player chooses this option
 * DialogueConsequence c(DialogueConsequence::Type::SET_FLAG, "quest_accepted");
 * option.consequences.push_back(c);
 * @endcode
 */
struct DialogueConsequence
{
    /// Types of consequences.
    enum class Type
    {
        SET_FLAG,
        CLEAR_FLAG,
        SET_FLAG_VALUE,
    };

    Type type = Type::SET_FLAG;
    std::string key;
    std::string value;  ///< New value (for SET_FLAG_VALUE).

    DialogueConsequence() = default;

    DialogueConsequence(Type t, const std::string& k, const std::string& v = "")
        : type(t),
          key(k),
          value(v)
    {
    }
};

/**
 * @struct DialogueOption
 * @brief Response with visibility conditions and consequences.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * @code{.cpp}
 * DialogueOption o("Tell me about the quest", "quest_info");
 * o.conditions.push_back({DialogueCondition::Type::FLAG_NOT_SET, "knows_quest"});
 * o.consequences.push_back({DialogueConsequence::Type::SET_FLAG, "knows_quest"});
 * @endcode
 */
struct DialogueOption
{
    std::string text;
    std::string nextNodeId;  ///< ID of next node (empty ends dialogue).
    std::vector<DialogueCondition> conditions;
    std::vector<DialogueConsequence> consequences;

    DialogueOption() = default;

    DialogueOption(const std::string& t, const std::string& next = "")
        : text(t),
          nextNodeId(next)
    {
    }
};

/**
 * @struct DialogueNode
 * @brief Speaker text and response options.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * @code{.cpp}
 * DialogueNode n("greeting", "Stranger", "Hello there, traveler!");
 * n.options.push_back({"Who are you?", "introduce"});
 * n.options.push_back({"Goodbye", ""});  // Empty ends dialogue
 * @endcode
 */
struct DialogueNode
{
    std::string id;
    std::string speaker;
    std::string text;
    std::vector<DialogueOption> options;

    DialogueNode() = default;

    DialogueNode(const std::string& nodeId, const std::string& spk, const std::string& txt)
        : id(nodeId),
          speaker(spk),
          text(txt)
    {
    }

    /**
     * @fn bool DialogueNode::IsTerminal() const
     * @brief True when every option ends the dialogue, including an empty option list.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsTerminal() const
    {
        if (options.empty())
            return true;
        for (const auto& opt : options)
        {
            if (!opt.nextNodeId.empty())
                return false;
        }
        return true;
    }
};

/**
 * @struct DialogueTree
 * @brief Dialogue nodes keyed by ID and an entry node.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * DialogueStore owns NPC trees; DialogueManager copies the active tree. All JSON keys
 * below are optional. Conditions split only on the exact separator " & ". Negation works
 * only for bare keys; !flag=value is parsed as flag=value.
 *
 * Consequence parsing checks a colon before equals: a=b:c uses key a=b. The colon payload
 * is discarded by DialogueManager, which writes true. Use flag=text for journal text.
 *
 * @code{.cpp}
 * DialogueTree t("stranger_intro", "greeting");
 *
 * DialogueNode g("greeting", "Stranger", "Hello!");
 * g.options.push_back({"Hi!", "response"});
 * t.AddNode(g);
 *
 * DialogueNode r("response", "Stranger", "Nice to meet you.");
 * r.options.push_back({"Goodbye", ""}); // End dialogue
 * t.AddNode(r);
 * @endcode
 *
 * @code{.json}
 * {
 *   "dialogueTree": {
 *     "speaker": "Marcus",
 *     "start": "greeting",
 *     "nodes": {
 *       "greeting": {
 *         "text": "Hello, traveler!",
 *         "choices": [
 *           { "text": "Who are you?", "goto": "introduce" }
 *         ]
 *       },
 *       "introduce": { ... }
 *     }
 *   }
 * }
 * @endcode
 *
 * | Level  | Field   | Description                                               |
 * |--------|---------|-----------------------------------------------------------|
 * | tree   | id      | Tree identifier; defaults to the NPC's `type`             |
 * | tree   | start   | Starting node ID; defaults to "start"                     |
 * | tree   | speaker | Speaker inherited by nodes; defaults to the NPC's `name`  |
 * | tree   | nodes   | Object keyed by node ID; the key becomes DialogueNode::id |
 * | node   | speaker | Speaker for this node; overrides the tree-level speaker   |
 * | node   | text    | Dialogue text displayed to player                         |
 * | node   | choices | Array of player response options                          |
 * | choice | text    | Display text of the option                                |
 * | choice | goto    | Next node ID (empty or omitted ends dialogue)             |
 * | choice | when    | Condition string (see below)                              |
 * | choice | do      | Consequence array (see below)                             |
 *
 * | Syntax       | Description                     |
 * |--------------|---------------------------------|
 * | `flag`       | Show if flag key exists         |
 * | `!flag`      | Show if flag key does not exist |
 * | `flag=value` | Show if flag equals value       |
 * | `a & b`      | Multiple conditions (AND)       |
 *
 * | Syntax         | Description                                           |
 * |----------------|-------------------------------------------------------|
 * | `"flag"`       | Set flag to "true"                                    |
 * | `"-flag"`      | Clear/remove flag                                     |
 * | `"flag=value"` | Set flag to a specific value                          |
 * | `"flag:text"`  | Set flag to "true"; text after the colon is discarded |
 */
struct DialogueTree
{
    std::string id;
    std::string startNodeId;
    std::unordered_map<std::string, DialogueNode> nodes;

    DialogueTree() = default;

    DialogueTree(const std::string& treeId, const std::string& startNode)
        : id(treeId),
          startNodeId(startNode)
    {
    }

    DialogueTree(const DialogueTree&) = default;
    DialogueTree(DialogueTree&&) noexcept = default;
    DialogueTree& operator=(const DialogueTree&) = default;
    DialogueTree& operator=(DialogueTree&&) noexcept = default;

    [[nodiscard]] const DialogueNode* GetNode(const std::string& nodeId) const
    {
        auto it = nodes.find(nodeId);
        return (it != nodes.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] const DialogueNode* GetStartNode() const { return GetNode(startNodeId); }

    /**
     * @fn void DialogueTree::AddNode(const DialogueNode& node)
     * @brief Copies a node under its ID; replaces any existing node with that ID.
     * @author Alex (<https://github.com/lextpf>)
     *
     * An empty ID is a valid key.
     */
    void AddNode(const DialogueNode& node) { nodes[node.id] = node; }
};
