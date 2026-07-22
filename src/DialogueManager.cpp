#include "DialogueManager.hpp"

#include "Dialogue.hpp"
#include "DialogueStore.hpp"
#include "GameStateManager.hpp"
#include "Logger.hpp"
#include "WorldServices.hpp"

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Dialogue";
}  // Namespace

namespace
{
bool EvaluateCondition(const GameStateManager& state, const DialogueCondition& condition)
{
    switch (condition.type)
    {
        case DialogueCondition::Type::FLAG_SET:
            return state.HasFlag(condition.key);

        case DialogueCondition::Type::FLAG_NOT_SET:
            return !state.HasFlag(condition.key);

        case DialogueCondition::Type::FLAG_EQUALS:
            return state.GetFlagValue(condition.key) == condition.value;
    }
    return false;
}

bool EvaluateConditions(const GameStateManager& state,
                        const std::vector<DialogueCondition>& conditions)
{
    for (const auto& condition : conditions)
    {
        if (!EvaluateCondition(state, condition))
        {
            return false;
        }
    }
    return true;
}
}  // Namespace

DialogueManager::DialogueManager()
    : m_StateManager(nullptr),
      m_Active(false),
      m_CurrentTree(nullptr),
      m_CurrentNode(nullptr),
      m_SelectedOption(0)
{
}

void DialogueManager::Initialize(GameStateManager* stateManager)
{
    m_StateManager = stateManager;
}

bool DialogueManager::StartDialogue(entt::entity npc, const entt::registry& world)
{
    if (m_Active)
    {
        Logger::Error(LOG_SUBSYSTEM, "Dialogue already active; refusing to start a new one");
        return false;
    }

    if (!world.valid(npc) || !m_StateManager)
    {
        return false;
    }

    const Dialogue* dialogue = world.try_get<Dialogue>(npc);
    const WorldServices* services = world.ctx().find<WorldServices>();
    if (dialogue == nullptr || services == nullptr || services->dialogue == nullptr ||
        !services->dialogue->HasTree(dialogue->tree))
    {
        return false;
    }

    const DialogueTree& tree = services->dialogue->Get(dialogue->tree);
    const DialogueNode* startNode = tree.GetStartNode();
    if (!startNode)
    {
        Logger::Error(LOG_SUBSYSTEM, "Start node not found in NPC tree");
        return false;
    }

    // Keep node and option pointers valid if the NPC is removed.
    m_ActiveTree = tree;
    startNode = m_ActiveTree.GetStartNode();
    if (!startNode)
    {
        Logger::Error(LOG_SUBSYSTEM, "Start node missing after tree copy");
        return false;
    }

    m_Active = true;
    m_CurrentTree = &m_ActiveTree;
    m_CurrentNode = startNode;
    m_SelectedOption = 0;

    RefreshVisibleOptions();

    Logger::InfoF(LOG_SUBSYSTEM, "Started dialogue with NPC '{}'", dialogue->type);
    return true;
}

void DialogueManager::EndDialogue()
{
    m_Active = false;
    m_CurrentTree = nullptr;
    m_CurrentNode = nullptr;
    m_ActiveTree = DialogueTree{};
    m_VisibleOptions.clear();
    m_SelectedOption = 0;

    Logger::Info(LOG_SUBSYSTEM, "Dialogue ended");
}

void DialogueManager::SelectOption(int optionIndex)
{
    if (!m_Active || !m_CurrentNode)
    {
        return;
    }
    if (optionIndex < 0 || optionIndex >= static_cast<int>(m_VisibleOptions.size()))
    {
        return;
    }

    const DialogueOption* option = m_VisibleOptions[optionIndex];

    ExecuteConsequences(option->consequences);

    TransitionToNode(option->nextNodeId);
}

void DialogueManager::TransitionToNode(const std::string& nodeId)
{
    if (nodeId.empty() || !m_CurrentTree)
    {
        EndDialogue();
        return;
    }

    const DialogueNode* nextNode = m_CurrentTree->GetNode(nodeId);
    if (!nextNode)
    {
        Logger::ErrorF(LOG_SUBSYSTEM, "Node not found: {}", nodeId);
        EndDialogue();
        return;
    }

    m_CurrentNode = nextNode;
    m_SelectedOption = 0;
    RefreshVisibleOptions();
}

void DialogueManager::SelectPrevious()
{
    if (m_VisibleOptions.empty())
    {
        return;
    }

    m_SelectedOption--;
    if (m_SelectedOption < 0)
    {
        m_SelectedOption = static_cast<int>(m_VisibleOptions.size()) - 1;
    }
}

void DialogueManager::SelectNext()
{
    if (m_VisibleOptions.empty())
    {
        return;
    }

    m_SelectedOption++;
    if (m_SelectedOption >= static_cast<int>(m_VisibleOptions.size()))
    {
        m_SelectedOption = 0;
    }
}

void DialogueManager::ConfirmSelection()
{
    if (m_VisibleOptions.empty())
    {
        // No options available: treat as end of dialogue. No consequences run here.
        // TODO: Support non-choice nodes (e.g., auto-advance) instead of always ending here.
        EndDialogue();
    }
    else
    {
        SelectOption(m_SelectedOption);
    }
}

void DialogueManager::ExecuteConsequences(const std::vector<DialogueConsequence>& consequences)
{
    if (!m_StateManager)
    {
        return;
    }

    for (const auto& cons : consequences)
    {
        switch (cons.type)
        {
            case DialogueConsequence::Type::SET_FLAG:
                // SET_FLAG ignores cons.value; the JSON flag:text form loses its payload.
                m_StateManager->SetFlag(cons.key, true);
                Logger::InfoF(LOG_SUBSYSTEM, "Set flag '{}' = true", cons.key);
                break;

            case DialogueConsequence::Type::CLEAR_FLAG:

                m_StateManager->ClearFlag(cons.key);
                Logger::InfoF(LOG_SUBSYSTEM, "Cleared flag '{}'", cons.key);
                break;

            case DialogueConsequence::Type::SET_FLAG_VALUE:

                m_StateManager->SetFlagValue(cons.key, cons.value);
                Logger::InfoF(LOG_SUBSYSTEM, "Set flag '{}' = '{}'", cons.key, cons.value);
                break;

            default:
                Logger::ErrorF(
                    LOG_SUBSYSTEM, "Unhandled consequence type {}", static_cast<int>(cons.type));
                break;
        }
        // TODO: Extend consequences to support scripted actions or item grants.
    }
}

void DialogueManager::RefreshVisibleOptions()
{
    m_VisibleOptions.clear();

    if (!m_CurrentNode || !m_StateManager)
    {
        return;
    }

    for (const auto& option : m_CurrentNode->options)
    {
        if (EvaluateConditions(*m_StateManager, option.conditions))
        {
            m_VisibleOptions.push_back(&option);
        }
    }

    if (m_SelectedOption >= static_cast<int>(m_VisibleOptions.size()))
    {
        m_SelectedOption = std::max(0, static_cast<int>(m_VisibleOptions.size()) - 1);
    }
}
