#pragma once

#include <random>

class TextureStore;
class DialogueStore;
class AssetRegistry;
class GameStateManager;

/**
 * @struct WorldServices
 * @brief Borrowed world services published in the registry context.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Core
 *
 * Game owns the services. Every pointer is optional; callers must handle absent services
 * and a missing bundle. Publishing the bundle does not extend any service lifetime.
 * Replace borrowed pointers when their owning services are replaced.
 */
struct WorldServices
{
    /// Null skips NPC texture and accent loading.
    TextureStore* textures = nullptr;
    /// Null leaves NPCs with their fallback dialogue line.
    DialogueStore* dialogue = nullptr;
    /// Null uses the type-named PNG under assets/non-player.
    AssetRegistry* assets = nullptr;
    /// Optional world-scoped RNG shared by NPC ai and editor recalculation.
    std::mt19937* npcRng = nullptr;
    /// Optional session flags; DialogueManager uses its separately injected pointer.
    GameStateManager* gameState = nullptr;
};
