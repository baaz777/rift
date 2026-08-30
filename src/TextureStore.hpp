#pragma once

#include "Texture.hpp"
#include "TextureHandle.hpp"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

class IRenderer;

/**
 * @class TextureStore
 * @brief Owns textures for the lifetime of the store.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * Acquire deduplicates the exact path string; it does not normalize separators or resolve aliases.
 * Adopt always allocates a new entry, including for an empty texture. Handles belong to this store.
 * The same numeric ID in another store can refer to a different texture.
 *
 * References remain valid across insertions and renderer switches until this store is destroyed.
 * GPU handles are rebuilt; cached renderer pointers become invalid.
 *
 * There is no erase API. Reuse procedural handles instead of adopting textures on each map load.
 *
 * ```mermaid
 * sequenceDiagram
 *     participant G as Game
 *     participant T as Texture (static)
 *     participant S as TextureStore
 *     participant R as IRenderer
 *     G->>R: Shutdown(), then destroy the window
 *     G->>R: create window, then create the new backend
 *     G->>T: AdvanceOpenGLContextGeneration()
 *     Note over G,T: OpenGL target only - skipped when switching to Vulkan
 *     G->>R: UploadTexture(tileset)
 *     G->>S: UploadAll(renderer)
 *     S->>R: UploadTexture(tex), once per owned texture
 *     Note over S,R: GL recreates only on generation mismatch; Vulkan always recreates
 *     G->>G: PackCharactersIntoAtlas()
 *     Note over G: repacks from CPU pixels and overwrites the tileset upload
 * ```
 */
class TextureStore
{
public:
    /**
     * @fn TextureHandle Acquire(const std::string& path)
     * @brief Caches successful loads by path.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Tries the supplied path, then the same path prefixed with ../ for build-directory launches.
     * Only successful loads enter the cache. Failed loads are retried and logged on every call.
     *
     * @return A store-local handle, or an invalid handle if both loads fail.
     */
    TextureHandle Acquire(const std::string& path);

    /**
     * @fn TextureHandle Adopt(Texture&& texture)
     * @brief Take ownership of a texture without path deduplication.
     * @author Alex (<https://github.com/lextpf>)
     *
     * The source becomes empty. Reuse the returned handle across reloads; entries are never erased.
     */
    TextureHandle Adopt(Texture&& texture);

    /**
     * @fn bool IsValid(TextureHandle handle) const
     * @brief Test whether the handle names an entry in this store.
     * @author Alex (<https://github.com/lextpf>)
     *
     * This checks membership only. An adopted empty texture still has a valid handle.
     */
    [[nodiscard]] bool IsValid(TextureHandle handle) const;

    /**
     * @fn const Texture& Get(TextureHandle handle) const
     * @brief Resolve a handle to a borrowed texture.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Unknown IDs return a shared empty texture. A reference to an owned entry remains valid until
     * this store is destroyed. The returned reference does not transfer GPU resource ownership.
     */
    [[nodiscard]] const Texture& Get(TextureHandle handle) const;

    /**
     * @fn void UploadAll(IRenderer& renderer) const
     * @brief Synchronously uploads all textures to the renderer.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Call outside an active render pass. Vulkan waits on one fence per texture.
     * An upload exception propagates and leaves remaining entries unprocessed.
     * Iteration order is unspecified.
     */
    void UploadAll(IRenderer& renderer) const;

    /**
     * @fn glm::vec3 SampleAccent(TextureHandle handle, glm::vec3 fallback) const
     * @brief Accent color for a handle's texture (invalid/empty -> fallback).
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] glm::vec3 SampleAccent(TextureHandle handle, glm::vec3 fallback) const;

    /**
     * @fn std::size_t Count() const
     * @brief Count never decreases during the store lifetime.
     * @author Alex (<https://github.com/lextpf>)
     */
    [[nodiscard]] std::size_t Count() const { return m_Textures.size(); }

private:
    std::unordered_map<std::string, TextureHandle> m_ByPath;  ///< Dedup index for Acquire.
    std::unordered_map<AssetId, Texture> m_Textures;          ///< The owned textures.
    /// Next store-local ID; zero is reserved and IDs are never reused.
    AssetId m_NextId = 1;
};
