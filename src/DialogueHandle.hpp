#pragma once

#include <cstdint>

/// Dialogue-tree id type used as a DialogueStore key.
using DialogueId = std::uint32_t;

/**
 * @struct DialogueHandle
 * @brief Store-local dialogue tree handle.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Dialogue
 *
 * Zero means no tree. DialogueStore owns the graph.
 */
struct DialogueHandle
{
    DialogueId id = 0;  ///< Store key; 0 = invalid / no tree.
};

inline bool operator==(DialogueHandle a, DialogueHandle b) noexcept
{
    return a.id == b.id;
}
inline bool operator!=(DialogueHandle a, DialogueHandle b) noexcept
{
    return a.id != b.id;
}
