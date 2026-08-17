#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class GameStateManager
 * @brief Session-only dialogue and quest flags; map saves do not serialize them.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * New Game clears the store. FLAG_SET and FLAG_NOT_SET test key presence;
 * FLAG_EQUALS compares values. Quest APIs use accepted_ and completed_ prefixes.
 * The trace uses k = accepted_x, with no completed_x unless shown.
 *
 * ```mermaid
 * stateDiagram-v2
 *     classDef available fill:#164e54,stroke:#06b6d4,color:#e2e8f0
 *     classDef active fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef completed fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *
 *     state "Quest Available" as Available:::available
 *     state "Quest Active" as Active:::active
 *     state "Quest Completed" as Completed:::completed
 *
 *     [*] --> Available
 *     Available --> Active: Player accepts
 *     Active --> Completed: Objective done
 *     Completed --> [*]
 *
 *     note right of Available: FLAG_NOT_SET accepted_X_quest
 *     note right of Active: FLAG_SET accepted_X_quest
 *     note right of Completed: FLAG_SET completed_X_quest
 * ```
 *
 * ```mermaid
 * flowchart LR
 *     classDef core fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef good fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef bad fill:#4a2020,stroke:#ef4444,color:#e2e8f0
 *
 *     A[DialogueOption]:::core --> B{Has conditions?}
 *     B -->|No| C[Show option]:::good
 *     B -->|Yes| D{All conditions pass?}
 *     D -->|Yes| C
 *     D -->|No| E[Hide option]:::bad
 * ```
 *
 * @verbatim
 * accepted_<name>_quest  -> "Quest description here"
 * completed_<name>_quest -> "true"
 * @endverbatim
 *
 * @code{.cpp}
 * // Accept quest with description
 * stateManager.SetFlagValue("accepted_ufo_quest", "Find Anna's brother!");
 *
 * // Check if quest active
 * if (stateManager.HasFlag("accepted_ufo_quest") &&
 *     !stateManager.HasFlag("completed_ufo_quest")) {
 *     // Quest is in progress
 * }
 *
 * // Complete quest
 * stateManager.SetFlag("completed_ufo_quest", true);
 * @endcode
 *
 * | Write                      | HasFlag | GetFlag | Active? | Listed? | Description |
 * |----------------------------|---------|---------|---------|---------|-------------|
 * | SetFlag(k)                 | true    | true    | yes     | yes     | "" (!)      |
 * | SetFlag(k, false)          | false   | false   | no      | no      | ""          |
 * | SetFlagValue(k, "d")       | true    | false   | yes     | yes     | "d"         |
 * | SetFlagValue(k, "false")   | true    | false   | yes     | no  (!) | ""          |
 * | AcceptQuest("x", "d")      | true    | false   | yes     | yes     | "d"         |
 * | ...then CompleteQuest("x") | true    | false   | no      | no      | "d"         |
 *
 * Columns: HasFlag(k), GetFlag(k), IsQuestActive(x), GetActiveQuests membership,
 * and GetQuestDescription(x).
 */
class GameStateManager
{
public:
    GameStateManager() = default;

    /**
     * @fn void GameStateManager::SetFlag(const std::string& key, bool value = true)
     * @brief True stores the string true; false erases the key.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetFlag(const std::string& key, bool value = true)
    {
        if (value)
            m_Flags[key] = "true";
        else
            ClearFlag(key);
    }

    /**
     * @fn bool GameStateManager::GetFlag(const std::string& key) const
     * @brief True only when the stored string is true.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool GetFlag(const std::string& key) const
    {
        auto it = m_Flags.find(key);
        if (it == m_Flags.end())
        {
            return false;
        }
        return it->second == "true";
    }

    void ClearFlag(const std::string& key) { m_Flags.erase(key); }

    void SetFlagValue(const std::string& key, const std::string& value) { m_Flags[key] = value; }

    /**
     * @fn std::string GameStateManager::GetFlagValue(const std::string& key) const
     * @brief Return an empty string when the key is absent.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::string GetFlagValue(const std::string& key) const
    {
        auto it = m_Flags.find(key);
        return (it != m_Flags.end()) ? it->second : "";
    }

    /**
     * @fn bool GameStateManager::HasFlag(const std::string& key) const
     * @brief Test key presence regardless of the stored value.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool HasFlag(const std::string& key) const
    {
        return m_Flags.find(key) != m_Flags.end();
    }

    void Clear() { m_Flags.clear(); }

    /**
     * @fn void GameStateManager::AcceptQuest(const std::string& questName, const std::string& \
     *     description)
     * @brief Store description under accepted_<questName>.
     * @author Alex (<https://github.com/lextpf>)
     */
    void AcceptQuest(const std::string& questName, const std::string& description)
    {
        m_Flags["accepted_" + questName] = description;
    }

    /**
     * @fn void GameStateManager::CompleteQuest(const std::string& questName)
     * @brief Store true under completed_<questName>.
     * @author Alex (<https://github.com/lextpf>)
     */
    void CompleteQuest(const std::string& questName) { m_Flags["completed_" + questName] = "true"; }

    /**
     * @fn bool GameStateManager::IsQuestActive(const std::string& questName) const
     * @brief Active when accepted_ exists and completed_ is absent, regardless of either value.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsQuestActive(const std::string& questName) const
    {
        return HasFlag("accepted_" + questName) && !HasFlag("completed_" + questName);
    }

    /**
     * @fn bool GameStateManager::IsQuestCompleted(const std::string& questName) const
     * @brief Test presence of completed_<questName>, regardless of value.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] bool IsQuestCompleted(const std::string& questName) const
    {
        return HasFlag("completed_" + questName);
    }

    /**
     * @fn std::vector<std::string> GameStateManager::GetActiveQuests() const
     * @brief List quest names in unspecified order, with accepted_ removed.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Skip accepted values that are empty, false or 0. Completion requires the string true,
     * so this can disagree with the presence-based IsQuestActive.
     */
    [[nodiscard]] std::vector<std::string> GetActiveQuests() const;

    /**
     * @fn std::string GameStateManager::GetQuestDescription(const std::string& questName) const
     * @brief Return an empty string for an absent description or the stored strings true and false.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::string GetQuestDescription(const std::string& questName) const;

    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetAllFlags() const
    {
        return m_Flags;
    }

    GameStateManager(const GameStateManager&) = default;
    GameStateManager& operator=(const GameStateManager&) = default;
    GameStateManager(GameStateManager&&) = default;
    GameStateManager& operator=(GameStateManager&&) = default;

private:
    std::unordered_map<std::string, std::string> m_Flags;
};
