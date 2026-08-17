#pragma once

/**
 * @struct Patrol
 * @brief Authored patrol cursor used to regenerate the runtime PatrolRoute.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct Patrol
{
    /// Recomputed from feet X each frame; writing the cursor does not move the NPC.
    int tileX{0};
    /// Feet Y is nudged by STANDING_EPS so a boundary belongs to the tile above.
    int tileY{0};
    int targetTileX{0};
    int targetTileY{0};
};
