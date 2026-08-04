#pragma once

#include <cstdint>

/**
 * @struct Identity
 * @brief Runtime NPC id preserved through snapshot, undo and respawn; never serialized. 0 means
 * unassigned.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Entities
 */
struct Identity
{
    std::uint64_t instanceId{0};
};
