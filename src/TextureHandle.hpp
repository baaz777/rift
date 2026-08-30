#pragma once

#include <cstdint>

/**
 * @brief Store-local texture ID; assigned monotonically and never reused.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
using AssetId = std::uint32_t;

/**
 * @struct TextureHandle
 * @brief Non-owning texture reference valid only in its originating store.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * ID 0 means no texture. different stores can issue the same ID.
 */
struct TextureHandle
{
    AssetId id = 0;  ///< Store key; 0 = invalid / no texture.
};

inline bool operator==(TextureHandle a, TextureHandle b) noexcept
{
    return a.id == b.id;
}

inline bool operator!=(TextureHandle a, TextureHandle b) noexcept
{
    return a.id != b.id;
}
