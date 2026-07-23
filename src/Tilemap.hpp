#pragma once

#include "CameraRig.hpp"
#include "CollisionMap.hpp"
#include "ColumnProxy.hpp"
#include "DefaultedVector.hpp"
#include "ElevationAxis.hpp"
#include "ElevationRole.hpp"
#include "IRenderer.hpp"
#include "NavigationMap.hpp"
#include "ParticleSystem.hpp"
#include "SupportSurface.hpp"
#include "Texture.hpp"
#include "TileMath.hpp"
#include "TileStance.hpp"
#include "WeatherDefinitions.hpp"

#include <entt/entt.hpp>

#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @struct Tile
 * @brief Unused tileset-coordinate record; layers store integer tile IDs.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
struct Tile
{
    int tileX;   ///< Column in tileset (0-based).
    int tileY;   ///< Row in tileset (0-based).
    int tileID;  ///< Unique identifier (tileY * tilesPerRow + tileX).
};

/**
 * @struct NoProjectionStructure
 * @brief Authored tile group with explicit world-space alignment anchors.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
struct NoProjectionStructure
{
    int id;                 ///< Unique structure ID (0+).
    std::string name;       ///< Optional name for editor display.
    glm::vec2 leftAnchor;   ///< Left anchor world position (click corner of tile).
    glm::vec2 rightAnchor;  ///< Right anchor world position (click corner of tile).

    NoProjectionStructure()
        : id(-1),
          leftAnchor(-1.0f, -1.0f),
          rightAnchor(-1.0f, -1.0f)
    {
    }
    NoProjectionStructure(int structId, glm::vec2 left, glm::vec2 right, const std::string& n = "")
        : id(structId),
          name(n),
          leftAnchor(left),
          rightAnchor(right)
    {
    }
};

/**
 * @struct TileLayer
 * @brief Row-major tile data, ordered by renderOrder.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
struct TileLayer
{
    std::string name;                        ///< Human-readable layer name.
    defaulted_vector<int, -1> tiles;         ///< Tile IDs in row-major order (-1 = empty).
    defaulted_vector<float, 0.0f> rotation;  ///< Rotation in degrees per tile.
    defaulted_vector<TileStance, TileStance::Flat>
        stance;  ///< Ground vs upright role (see TileStance).
    defaulted_vector<ElevationRole, ElevationRole::Ground>
        elevationRole;                    ///< Per-layer participation in the cell's elevation.
    defaulted_vector<bool, false> flipX;  ///< Mirror tile sprite around vertical axis.
    defaulted_vector<bool, false> flipY;  ///< Mirror tile sprite around horizontal axis.
    defaulted_vector<int, -1>
        structureId;  ///< Per-tile structure ID (-1 = auto flood-fill, 0+ = belongs to structure).
    /// Tiles that sort with entities by Y position. (Y-sort+1: player in front at same Y).
    defaulted_vector<bool, false> ySortPlus;
    defaulted_vector<bool, false>
        ySortMinus;  ///< When true, player renders behind tile at same Y (Y-sort-1: tile in front).
    defaulted_vector<int, -1> animationMap;  ///< Per-tile animation ID (-1 = not animated).
    int renderOrder;    ///< Lower = rendered first (background), higher = later (foreground).
    bool isBackground;  ///< True = before player/NPCs, false = after.

    TileLayer()
        : renderOrder(0),
          isBackground(true)
    {
    }
    TileLayer(const std::string& n, int order, bool bg)
        : name(n),
          renderOrder(order),
          isBackground(bg)
    {
    }

    /**
     * @fn void Resize(size_t size)
     * @brief Resize all per-tile fields to the given number of tiles.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param size Total number of tiles (mapWidth * mapHeight).
     */
    void Resize(size_t size)
    {
        resize_all(size,
                   tiles,
                   rotation,
                   stance,
                   elevationRole,
                   flipX,
                   flipY,
                   structureId,
                   ySortPlus,
                   ySortMinus,
                   animationMap);
    }

    /**
     * @fn void Clear()
     * @brief Reset all per-tile data to default values without changing size.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Clear()
    {
        reset_all(tiles,
                  rotation,
                  stance,
                  elevationRole,
                  flipX,
                  flipY,
                  structureId,
                  ySortPlus,
                  ySortMinus,
                  animationMap);
    }
};

/**
 * @struct AnimatedTile
 * @brief Definition of an animated tile sequence.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 */
struct AnimatedTile
{
    std::vector<int> frames;  ///< Tile IDs for each frame.
    float frameDuration;      ///< Seconds per frame.

    AnimatedTile()
        : frameDuration(0.2f)
    {
    }
    AnimatedTile(const std::vector<int>& f, float duration = 0.2f)
        : frames(f),
          frameDuration(duration)
    {
    }

    /**
     * @fn int GetFrameAtTime(float time) const
     * @brief Get the tile ID for a given elapsed time.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param time Elapsed animation time in seconds.
     * @return Tile ID for the current frame, or -1 if no frames exist.
     */
    int GetFrameAtTime(float time) const
    {
        if (frames.empty())
            return -1;
        if (frameDuration <= 0.0f)
            return frames[0];  // prevent division by zero
        int frameIndex = static_cast<int>(time / frameDuration) % static_cast<int>(frames.size());
        return frames[frameIndex];
    }
};

/**
 * @class Tilemap
 * @brief Owns authored map data and derived collision, navigation, and rendering grids.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Layer count is data-driven; query GetLayerCount. isBackground selects each layer's
 * side of actors. Tile dimensions come from the project manifest.
 *
 * Entity queries use bottom-center feet anchors, shifting Y up by half a tile.
 * Use TileMath for row conventions; plain world-to-cell division differs by one row at edges.
 *
 * JSON stores non-default fields by row-major index. Boolean fields store set indices.
 * stance, elevationRole, and structureId are omitted when empty; animation IDs live in
 * layerAnimationMaps. Absent elevationRole means ground. LoadMapFromJSON converts
 * noProjection entries when stance is absent.
 *
 * @verbatim
 *              Layer 9 Overlay3     <- Top (front)
 *              Layer 8 Overlay2
 *              Layer 7 Overlay
 *              Layer 6 Foreground2
 *              Layer 5 Foreground
 *              ---- Player -------
 *              ---- NPCs ---------
 *              Layer 4 Objects3
 *              Layer 3 Objects2
 *              Layer 2 Objects
 *              Layer 1 Ground Detail
 *              Layer 0 Ground       <- Bottom (back)
 * @endverbatim
 *
 * $$
 * tileID = tileY \times tilesPerRow + tileX
 * $$
 *
 * $$
 * u_0 = \frac{tx \times tileWidth}{textureWidth}, \quad
 * v_0 = \frac{ty \times tileHeight}{textureHeight}
 * $$
 *
 * $$
 * u_1 = \frac{(tx + 1) \times tileWidth}{textureWidth}, \quad
 * v_1 = \frac{(ty + 1) \times tileHeight}{textureHeight}
 * $$
 *
 * @verbatim
 *   (0,0)-----> +X
 *     |
 *     |  Tile (x,y) at world position (x*tileWidth, y*tileHeight)
 *     v
 *    +Y
 * @endverbatim
 *
 * $$
 * tile_x = \lfloor \frac{world_x}{tileWidth} \rfloor, \quad
 * tile_y = \lfloor \frac{world_y}{tileHeight} \rfloor
 * $$
 *
 * $$
 * tile_y = \left\lfloor \frac{world_y - \frac{tileHeight}{2}}{tileHeight} \right\rfloor
 * $$
 *
 * $$
 * world_x = tile_x \times tileWidth, \quad
 * world_y = tile_y \times tileHeight
 * $$
 *
 * $$
 * index = y \times mapWidth + x
 * $$
 *
 * @verbatim
 *    +------------------+
 *    |    Tileset 1     |
 *    |    (256x256)     |
 *    +------------------+
 *    |    Tileset 2     |  <- Combined texture
 *    |    (256x128)     |
 *    +------------------+
 *    |    Tileset 3     |
 *    |    (256x64)      |
 *    +------------------+
 * @endverbatim
 *
 * @code{.json}
 * {
 *   "dynamicLayers": [
 *     {
 *       "name": "Ground",
 *       "renderOrder": 0,
 *       "isBackground": true,
 *       "tiles": {
 *         "42": 15,    // Tile at index 42 = tile ID 15
 *         "100": 23    // Tile at index 100 = tile ID 23
 *       },
 *       "rotation": { "42": 90.0 },
 *       "stance": { "42": 3 },
 *       "elevationRole": { "42": 1 },
 *       "flipX": [42],
 *       "flipY": [],
 *       "ySortPlus": [],
 *       "ySortMinus": [],
 *       "structureId": { "42": 0 }
 *     }
 *   ]
 * }
 * @endcode
 */
class Tilemap
{
public:
    /**
     * @fn Tilemap()
     * @brief Call LoadCombinedTilesets and SetTilemapSize before use.
     * @author Alex (<https://github.com/lextpf>)
     */
    Tilemap();

    ~Tilemap();

    Tilemap(Tilemap&&) noexcept = default;
    Tilemap& operator=(Tilemap&&) noexcept = default;
    Tilemap(const Tilemap&) = delete;
    Tilemap& operator=(const Tilemap&) = delete;

    /**
     * @fn bool LoadCombinedTilesets(const std::vector<std::string>& paths, int tileWidth = 16, \
     *     int tileHeight = 16)
     * @brief Load and combine multiple tileset images vertically.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param paths Vector of tileset paths (combined top-to-bottom).
     * @param tileWidth Tile width in pixels.
     * @param tileHeight Tile height in pixels.
     * @return `true` if all loaded and combined successfully.
     */
    bool LoadCombinedTilesets(const std::vector<std::string>& paths,
                              int tileWidth = 16,
                              int tileHeight = 16);

    /// One sheet to pack into the atlas.
    struct AtlasPackEntry
    {
        std::string key;         ///< Identifier used to look the offset back up.
        const Texture* texture;  ///< Borrowed for the call; null entries are skipped.
    };

    /**
     * @fn bool PackAdditionalSheets(const std::vector<AtlasPackEntry>& sheets)
     * @brief Rebuilds and uploads the atlas with appended sprite sheets.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Each call replaces the complete packed set. Omitted keys are removed; an empty set
     * restores the tileset-only atlas.
     *
     * @param sheets Borrowed for the call; null textures are skipped. sheets must match atlas
     * channels and fit its width.
     * @return False for incompatible dimensions, channels, or upload failure.
     */
    bool PackAdditionalSheets(const std::vector<AtlasPackEntry>& sheets);

    /**
     * @fn std::optional<glm::vec2> GetCharacterAtlasOffset(const std::string& key) const
     * @brief Resolves a packed sheet origin.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return GL-row offset in pixels from the atlas bottom, with X zero; nullopt for an unknown
     * key.
     */
    std::optional<glm::vec2> GetCharacterAtlasOffset(const std::string& key) const;

    /**
     * @fn void SetTilemapSize(int width, int height, bool generateMap = true)
     * @brief Set the tilemap dimensions.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Allocates storage for all layers, collision, and navigation.
     * Optionally generates a default map pattern.
     *
     * ### :material-information-outline: World size
     * world dimensions in pixels:
     * $$
     * worldWidth = width \times tileWidth
     * $$
     * $$
     * worldHeight = height \times tileHeight
     * $$
     *
     * @param width Map width in tiles.
     * @param height Map height in tiles.
     * @param generateMap If true, fills with a default pattern.
     */
    void SetTilemapSize(int width, int height, bool generateMap = true);

    /// Corner identifiers for corner cutting control
    enum Corner : uint8_t
    {
        CORNER_TL = 0,  ///< Top-left corner.
        CORNER_TR = 1,  ///< Top-right corner.
        CORNER_BL = 2,  ///< Bottom-left corner.
        CORNER_BR = 3   ///< Bottom-right corner.
    };

    void SetCornerCutBlocked(int x, int y, Corner corner, bool blocked);

    bool IsCornerCutBlocked(int x, int y, Corner corner) const;

    /**
     * @fn void SetTileCollision(int x, int y, bool hasCollision)
     * @brief Collision flags belong to map cells, independently of tile layers.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @verbatim
     *   per-cell grids (one bool per cell)      layer stack (GetLayerCount() layers)
     *
     *      Collision      Navigation              layer N-1 [tiles, rotation, flags, ...]
     *      +--+--+--+     +--+--+--+                  ...
     *      |  |##|  |     |##|##|  |              layer 1   [tiles, rotation, flags, ...]
     *      +--+--+--+     +--+--+--+              layer 0   [tiles, rotation, flags, ...]
     *      |##|##|  |     |  |##|##|
     *      +--+--+--+     +--+--+--+       one (x, y) indexes every grid and every layer
     * @endverbatim
     */

    void SetTileCollision(int x, int y, bool hasCollision);

    bool GetTileCollision(int x, int y) const;

    CollisionMap<std::vector>& GetCollisionMap() { return m_CollisionMap; }

    const CollisionMap<std::vector>& GetCollisionMap() const { return m_CollisionMap; }

    /**
     * @fn void SetNavigation(int x, int y, bool walkable)
     * @brief Navigation flags select NPC walkability independently of collision.
     * @author Alex (<https://github.com/lextpf>)
     */

    void SetNavigation(int x, int y, bool walkable);

    bool GetNavigation(int x, int y) const;

    NavigationMap<std::vector>& GetNavigationMap() { return m_NavigationMap; }

    const NavigationMap<std::vector>& GetNavigationMap() const { return m_NavigationMap; }

    /**
     * @fn int GetTileWidth() const
     * @brief Tile width in pixels.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetTileWidth() const { return m_TileWidth; }
    /**
     * @fn int GetTileHeight() const
     * @brief Tile height in pixels.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetTileHeight() const { return m_TileHeight; }
    /**
     * @fn int GetMapWidth() const
     * @brief Map width in tiles.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetMapWidth() const { return m_MapWidth; }
    /**
     * @fn int GetMapHeight() const
     * @brief Map height in tiles.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetMapHeight() const { return m_MapHeight; }

    /**
     * @fn size_t FlatIndex(int x, int y) const
     * @brief Row-major index with size_t multiplication.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Caller must supply coordinates within map bounds.
     */
    inline size_t FlatIndex(int x, int y) const
    {
        return static_cast<size_t>(y) * static_cast<size_t>(m_MapWidth) + static_cast<size_t>(x);
    }

    /**
     * @fn size_t MapCellCount() const
     * @brief Cell count with size_t multiplication to avoid intermediate int overflow.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline size_t MapCellCount() const
    {
        return static_cast<size_t>(m_MapWidth) * static_cast<size_t>(m_MapHeight);
    }
    inline const Texture& GetTilesetTexture() const
    {
        return m_TilesetTexture;
    }
    inline int GetTilesPerRow() const { return m_TilesPerRow; }
    /**
     * @fn int GetTilesetDataWidth() const
     * @brief Combined atlas width in pixels.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetTilesetDataWidth() const { return m_TilesetDataWidth; }
    /**
     * @fn int GetTilesetDataHeight() const
     * @brief Combined atlas height in pixels, including any sheets added by PackAdditionalSheets.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline int GetTilesetDataHeight() const { return m_TilesetDataHeight; }

    /**
     * @fn size_t GetLayerCount() const
     * @brief Return the current number of authored layers.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Loaded maps can replace the default stack. Use this count for layer iteration.
     */

    inline size_t GetLayerCount() const { return m_Layers.size(); }

    /**
     * @fn TileLayer& GetLayer(size_t index)
     * @brief Zero-based layer index.
     * @author Alex (<https://github.com/lextpf>)
     */
    TileLayer& GetLayer(size_t index);
    const TileLayer& GetLayer(size_t index) const;

    /**
     * @fn int GetLayerTile(int x, int y, size_t layer) const
     * @brief Zero-based layer index; tile ID -1 is empty.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetLayerTile(int x, int y, size_t layer) const;
    void SetLayerTile(int x, int y, size_t layer, int tileID);

    /**
     * @fn float GetLayerRotation(int x, int y, size_t layer) const
     * @brief Rotation in degrees.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetLayerRotation(int x, int y, size_t layer) const;
    void SetLayerRotation(int x, int y, size_t layer, float rotation);

    TileStance GetLayerStance(int x, int y, size_t layer) const;
    void SetLayerStance(int x, int y, size_t layer, TileStance stance);

    ElevationRole GetLayerElevationRole(int x, int y, size_t layer) const;
    void SetLayerElevationRole(int x, int y, size_t layer, ElevationRole role);

    bool GetLayerFlipX(int x, int y, size_t layer) const;
    void SetLayerFlipX(int x, int y, size_t layer, bool flipX);

    bool GetLayerFlipY(int x, int y, size_t layer) const;
    void SetLayerFlipY(int x, int y, size_t layer, bool flipY);

    bool GetLayerYSortPlus(int x, int y, size_t layer) const;
    void SetLayerYSortPlus(int x, int y, size_t layer, bool ySortPlus);

    /**
     * @fn bool GetLayerYSortMinus(int x, int y, size_t layer) const
     * @brief Tile wins near-depth entity comparisons when already in the depth queue.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool GetLayerYSortMinus(int x, int y, size_t layer) const;
    void SetLayerYSortMinus(int x, int y, size_t layer, bool ySortMinus);

    /**
     * @fn void RenderBackgroundLayers(IRenderer& renderer, glm::vec2 renderCam, glm::vec2 \
     *     renderSize, glm::vec2 cullCam, glm::vec2 cullSize)
     * @brief Submits background layers in render order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param renderer Backend that receives the draw commands.
     * @param renderCam Camera offset in world pixels.
     * @param renderSize Unused.
     * @param cullCam Top-left visible position in world pixels.
     * @param cullSize Visible rectangle dimensions in world pixels.
     */
    void RenderBackgroundLayers(IRenderer& renderer,
                                glm::vec2 renderCam,
                                glm::vec2 renderSize,
                                glm::vec2 cullCam,
                                glm::vec2 cullSize);

    /**
     * @fn void RenderForegroundLayers(IRenderer& renderer, glm::vec2 renderCam, glm::vec2 \
     *     renderSize, glm::vec2 cullCam, glm::vec2 cullSize)
     * @brief Submits foreground layers in render order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param renderer Backend that receives the draw commands.
     * @param renderCam Camera offset in world pixels.
     * @param renderSize Unused.
     * @param cullCam Top-left visible position in world pixels.
     * @param cullSize Visible rectangle dimensions in world pixels.
     */
    void RenderForegroundLayers(IRenderer& renderer,
                                glm::vec2 renderCam,
                                glm::vec2 renderSize,
                                glm::vec2 cullCam,
                                glm::vec2 cullSize);

    /**
     * @fn void RenderWorld3D(IRenderer& renderer, const cameraRig::RigParams& rig)
     * @brief Submits flat ground and upright artwork through DrawQuad3D.
     * @author Alex (<https://github.com/lextpf>)
     *
     * TileStance selects upright geometry; the depth buffer resolves opaque occlusion.
     */
    void RenderWorld3D(IRenderer& renderer, const cameraRig::RigParams& rig);

    /**
     * @fn int FindStructureBaseRow(const TileLayer& layer, int tileX, int tileY) const
     * @brief Finds the base row shared by a vertical structure run.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Scans south through non-empty structure cells on one layer.
     * A lone tile returns its own row. Props and walls end the run.
     */
    int FindStructureBaseRow(const TileLayer& layer, int tileX, int tileY) const;

    /**
     * @fn void FindStructureRunColumns( const TileLayer& layer, int tileX, int tileY, int& \
     *     outMinX, int& outMaxX) const
     * @brief Finds the shared pivot span of a horizontal structure run.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Shared axes keep adjacent tiles joined when the camera turns. Width also determines
     * facade yaw damping. Props and walls end the run; layers never merge.
     *
     * @param layer Layer whose structure cells form the run.
     * @param tileX Column of the queried structure cell.
     * @param tileY Row of the queried structure cell.
     * @param outMinX Westmost column, inclusive.
     * @param outMaxX Eastmost column, inclusive.
     */
    void FindStructureRunColumns(
        const TileLayer& layer, int tileX, int tileY, int& outMinX, int& outMaxX) const;

    /**
     * @brief The scene heights of one cell's two edges along its slope axis.
     *
     * `minus` is the west or north edge, `plus` the east or south edge; they are
     * equal for a level cell.
     */
    struct SurfaceSlope
    {
        float minus = 0.0f;   ///< Height at the low-coordinate edge.
        float plus = 0.0f;    ///< Height at the high-coordinate edge.
        bool alongZ = false;  ///< True when the slope runs north-south.
    };

    /**
     * @fn SurfaceSlope ResolveSurfaceSlope(const TileLayer& layer, int tileX, int tileY) const
     * @brief Resolves layer-specific height and ramp endpoints.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Neighbour heights use the same layer's ElevationRole so non-participating artwork
     * cannot raise the ramp.
     */
    SurfaceSlope ResolveSurfaceSlope(const TileLayer& layer, int tileX, int tileY) const;

    /**
     * @fn void RenderBackgroundLayersNoProjection(IRenderer& renderer, glm::vec2 renderCam, \
     *     glm::vec2 renderSize, glm::vec2 cullCam, glm::vec2 cullSize)
     * @brief Submits upright background artwork.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param renderer Backend that receives the draw commands.
     * @param renderCam Camera offset in world pixels.
     * @param renderSize Unused.
     * @param cullCam Top-left visible position in world pixels.
     * @param cullSize Visible rectangle dimensions in world pixels.
     */
    void RenderBackgroundLayersNoProjection(IRenderer& renderer,
                                            glm::vec2 renderCam,
                                            glm::vec2 renderSize,
                                            glm::vec2 cullCam,
                                            glm::vec2 cullSize);

    /**
     * @fn void RenderForegroundLayersNoProjection(IRenderer& renderer, glm::vec2 renderCam, \
     *     glm::vec2 renderSize, glm::vec2 cullCam, glm::vec2 cullSize)
     * @brief Submits upright foreground artwork.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param renderer Backend that receives the draw commands.
     * @param renderCam Camera offset in world pixels.
     * @param renderSize Unused.
     * @param cullCam Top-left visible position in world pixels.
     * @param cullSize Visible rectangle dimensions in world pixels.
     */
    void RenderForegroundLayersNoProjection(IRenderer& renderer,
                                            glm::vec2 renderCam,
                                            glm::vec2 renderSize,
                                            glm::vec2 cullCam,
                                            glm::vec2 cullSize);

private:
    /**
     * @fn void RenderLayersNoProjection(IRenderer& renderer, glm::vec2 renderCam, glm::vec2 \
     *     renderSize, glm::vec2 cullCam, glm::vec2 cullSize, bool isBackground)
     * @brief Shared implementation for background/foreground no-projection rendering.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `renderSize` is accepted for signature symmetry only and is never read.
     */
    void RenderLayersNoProjection(IRenderer& renderer,
                                  glm::vec2 renderCam,
                                  glm::vec2 renderSize,
                                  glm::vec2 cullCam,
                                  glm::vec2 cullSize,
                                  bool isBackground);

public:
    /**
     * @fn std::vector<size_t> GetLayerRenderOrder() const
     * @brief Returns layer indices in ascending renderOrder.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::vector<size_t> GetLayerRenderOrder() const;

    /**
     * @fn bool IsStructureTile(int x, int y, int layer = 1) const
     * @brief Tests for TileStance::Structure.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer One-based; layer 1 maps to internal layer 0.
     */
    bool IsStructureTile(int x, int y, int layer = 1) const;

    /// Inclusive tile-space bounding box of a set of cells on one layer.
    struct StructureBounds
    {
        int minX{0}, maxX{0}, minY{0}, maxY{0};  ///< Inclusive tile columns and rows.
    };

    /**
     * @fn std::vector<StructureBounds> FindStructureGroups(size_t layer) const
     * @brief Finds edge-connected structure groups on one layer.
     * @author Alex (<https://github.com/lextpf>)
     *
     * stance alone determines membership, including empty tiles. structureId is ignored,
     * so adjacent authored structures can share a box. Other layers cannot bridge groups.
     *
     * @param layer Zero-based; an invalid layer returns no groups.
     * @return Inclusive tile bounds in row-major discovery order.
     * @warning Mutates shared flood-fill scratch; not reentrant or thread-safe.
     */
    std::vector<StructureBounds> FindStructureGroups(size_t layer) const;

    /**
     * @fn bool ProjectNoProjectionStructurePoint(const glm::vec2& worldPos, const glm::vec2& \
     *     cameraPos, glm::vec2& outScreenPos) const
     * @brief Maps effects onto the same stepped mesh as upright structure artwork.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param worldPos world pixels.
     * @param cameraPos world pixels.
     * @param outScreenPos Screen-space position.
     * @return False if no structure covers the point; caller may use plain placement.
     */
    bool ProjectNoProjectionStructurePoint(const glm::vec2& worldPos,
                                           const glm::vec2& cameraPos,
                                           glm::vec2& outScreenPos) const;

    /**
     * @struct StructureFacade
     * @brief Scene-space base of a structure for attached geometry.
     */
    struct StructureFacade
    {
        /// Scene point at the run centre on the base row's south edge, at the body's foot height.
        glm::vec3 foot{0.0f};
        float runCentreX = 0.0f;      ///< World x of the run's centre, in pixels.
        float baseSouthEdgeY = 0.0f;  ///< World y of the base row's south edge, in pixels.
        int widthTiles = 1;           ///< Run width in tiles; feeds tileRole::IsGridLocked.
    };

    /**
     * @fn std::optional<StructureFacade> FindStructureFacade(glm::vec2 world) const
     * @brief Finds the highest-renderOrder structure layer under a point.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Searches south up to SEARCH_DOWN_TILES rows; authored structure ID is optional.
     * @param world world pixels.
     * @return Nullopt if no structure covers the point.
     */
    std::optional<StructureFacade> FindStructureFacade(glm::vec2 world) const;

    /**
     * @fn int AddNoProjectionStructure(glm::vec2 leftAnchor, glm::vec2 rightAnchor, const \
     *     std::string& name = "")
     * @brief Add a new no-projection structure definition.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param leftAnchor Left anchor world position (corner of tile).
     * @param rightAnchor Right anchor world position (corner of tile).
     * @param name Optional name for editor display.
     * @return The structure ID.
     */
    int AddNoProjectionStructure(glm::vec2 leftAnchor,
                                 glm::vec2 rightAnchor,
                                 const std::string& name = "");

    /**
     * @fn const NoProjectionStructure* GetNoProjectionStructure(int id) const
     * @brief Returns nullptr for an invalid structure ID.
     * @author Alex (<https://github.com/lextpf>)
     */
    const NoProjectionStructure* GetNoProjectionStructure(int id) const;

    const std::vector<NoProjectionStructure>& GetNoProjectionStructures() const
    {
        return m_NoProjectionStructures;
    }

    /**
     * @fn void RemoveNoProjectionStructure(int id)
     * @brief Removes the structure and clears its tile references.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RemoveNoProjectionStructure(int id);

    /**
     * @fn void InsertNoProjectionStructureAt(size_t idx, const NoProjectionStructure& structure)
     * @brief Restores an indexed structure for editor undo.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Shifts later entries and restamps their IDs to match vector positions.
     * @param idx At most the current size.
     * @param structure ID is overwritten.
     */
    void InsertNoProjectionStructureAt(size_t idx, const NoProjectionStructure& structure);

    /**
     * @fn int GetTileStructureId(int x, int y, int layer) const
     * @brief Resolves an authored structure ID.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer One-based; zero returns -1.
     * @return -1 for auto detection, otherwise a structure ID.
     */
    int GetTileStructureId(int x, int y, int layer) const;

    /**
     * @fn void SetTileStructureId(int x, int y, int layer, int structId)
     * @brief Assigns authored structure membership.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer One-based; zero writes nothing.
     * @param structId -1 selects auto detection.
     */
    void SetTileStructureId(int x, int y, int layer, int structId);

    size_t GetNoProjectionStructureCount() const { return m_NoProjectionStructures.size(); }

    /**
     * @fn int GetElevation(int x, int y) const
     * @brief Elevation in pixels; zero is ground, positive values rise.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetElevation(int x, int y) const;

    /**
     * @fn void SetElevation(int x, int y, int elevation)
     * @brief Authored height in pixels.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetElevation(int x, int y, int elevation);

    /**
     * @fn int GetElevationRegionId(int x, int y) const
     * @brief Runtime ID of the connected elevated footprint.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Negative means ground. IDs may change after elevation edits.
     */
    int GetElevationRegionId(int x, int y) const;

    /**
     * @fn int GetElevationRegionIdAtWorldPos(float worldX, float worldY) const
     * @brief Elevation-region query using the bottom-center feet convention.
     * @author Alex (<https://github.com/lextpf>)
     */
    int GetElevationRegionIdAtWorldPos(float worldX, float worldY) const;

    /**
     * @fn float GetElevationAtWorldPos(float worldX, float worldY) const
     * @brief Cell elevation in pixels at a bottom-center feet anchor.
     * @author Alex (<https://github.com/lextpf>)
     */
    float GetElevationAtWorldPos(float worldX, float worldY) const;

    /**
     * @fn float SurfaceHeightAtWorldPos(glm::vec2 world) const
     * @brief Maximum participating layer height at the world point.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Uses plain cell division, without the feet-anchor shift. Returns zero off-map.
     * @param world world pixels.
     */
    float SurfaceHeightAtWorldPos(glm::vec2 world) const;

    /**
     * @fn void WorldToTileCoord(float worldX, float worldY, int& tileX, int& tileY) const
     * @brief Shifts world Y up by half a tile before resolving its row.
     * @author Alex (<https://github.com/lextpf>)
     */
    inline void WorldToTileCoord(float worldX, float worldY, int& tileX, int& tileY) const
    {
        tileX = TileMath::TileIndex(worldX, static_cast<float>(m_TileWidth));
        tileY = TileMath::AnchorTileRow(worldY, static_cast<float>(m_TileHeight));
    }

    /**
     * @fn ElevationAxis GetElevationAxisAt(int x, int y) const
     * @brief Infers the ramp axis from neighbouring elevations.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Zero height gives none. Compare absolute east-west and north-south gradients;
     * the larger selects the axis. Ties compare elevated spans up to 8 cells in each direction;
     * a remaining tie selects X. Out-of-bounds queries are safe.
     */
    ElevationAxis GetElevationAxisAt(int x, int y) const;

    /**
     * @struct DepthSortedTile
     * @brief Depth-queue payload for explicit Y-sort and inferred elevated artwork.
     * @author Alex (<https://github.com/lextpf>)
     *
     * anchorY is the depth key. Region identity and support height provide only local constraints.
     *
     * ```mermaid
     * flowchart TD
     *     Gate["IsDepthSortedTile: ySortPlus, or layer 2+ with elevation"]
     *     Build["GetVisibleDepthSortedTiles: anchorY = stack bottom edge"]
     *     Item["Game: Drawable.sortY = anchorY only"]
     *     Sort["SortDrawables: phase, then sortY, then per-region edges"]
     *     Gate --> Build
     *     Build -- "supportHeight + surfaceRegionId are metadata, not depth" --> Item
     *     Item --> Sort
     * ```
     */

    struct DepthSortedTile
    {
        int x = 0, y = 0;      ///< Tile coordinates.
        int layer = 0;         ///< Layer index (0-based).
        float anchorY = 0.0f;  ///< World Y position of the tile/stack bottom.
        /**
         * @brief Inherited surface elevation in pixels. Support metadata only; it is
         * never folded into the painter-depth key (`Drawable::supportHeight`).
         */
        float supportHeight = 0;
        SupportSurface supportSurface{
            SupportSurface::Ground};  ///< Logical surface the artwork belongs to.
        int surfaceRegionId = -1;     ///< Connected elevation footprint, or -1.
        bool authoredYSort = false;   ///< Copy of the cell's authored ySortPlus flag.
        bool isBackground = true;     ///< Original fixed-pass side of actors.
        bool isStructure = false;     ///< Render upright, without perspective distortion.
        bool ySortMinus = false;      ///< Tile wins near-depth entity comparisons.
    };

    /**
     * @fn bool IsDepthSortedTile(int x, int y, size_t layer) const
     * @brief Promotes ySortPlus tiles and elevated cells on layers 2 and above.
     * @author Alex (<https://github.com/lextpf>)
     *
     * ySortMinus only changes ties for tiles already promoted; it does not enter the queue.
     */
    bool IsDepthSortedTile(int x, int y, size_t layer) const;

    /**
     * @fn const std::vector<DepthSortedTile>& GetVisibleDepthSortedTiles(glm::vec2 cullCam, \
     *     glm::vec2 cullSize) const
     * @brief Collects depth tiles with an 8-tile culling margin for off-screen anchors.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return Internal cache, cleared by the next call. Copy it if the data must survive another
     * query.
     */
    const std::vector<DepthSortedTile>& GetVisibleDepthSortedTiles(glm::vec2 cullCam,
                                                                   glm::vec2 cullSize) const;

    /**
     * @fn void RenderSingleTile(IRenderer& r, int x, int y, int layer, glm::vec2 cameraPos)
     * @brief Draws one depth-queue tile.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param r Backend that receives the draw commands.
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer Zero-based.
     * @param cameraPos world pixels.
     */
    void RenderSingleTile(IRenderer& r, int x, int y, int layer, glm::vec2 cameraPos);

    /**
     * @fn bool SaveMapToJSON(const std::string& filename, const entt::registry* npcs = nullptr, \
     *     int playerTileX = -1, int playerTileY = -1, int characterType = -1) const
     * @brief Saves authored map data and optional actor state as sparse JSON.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Per-cell keys use row-major indices.
     * @param filename JSON file path; relative paths use the process working directory.
     * @param npcs Null skips NPC data.
     * @param playerTileX -1 skips player state.
     * @param playerTileY -1 skips player state.
     * @param characterType -1 skips player state.
     * @return False if writing fails.
     *
     * @code{.json}
     * {
     *   "width": 64,
     *   "height": 64,
     *   "tileWidth": 16,
     *   "tileHeight": 16,
     *   "collision": [42, 43],
     *   "navigation": [100, 101],
     *   "elevation": { "512": 8 },
     *   "dynamicLayers": [
     *     {
     *       "name": "Ground",
     *       "renderOrder": 0,
     *       "isBackground": true,
     *       "tiles": { "42": 15 },
     *       "rotation": { "42": 90.0 },
     *       "stance": { "42": 3 },
     *       "elevationRole": { "42": 1 },
     *       "flipX": [42],
     *       "flipY": [],
     *       "ySortPlus": [],
     *       "ySortMinus": [],
     *       "structureId": { "42": 0 }
     *     }
     *   ],
     *   "noProjectionStructures": [
     *     { "id": 0, "name": "Cabin", "leftAnchor": [160, 192], "rightAnchor": [208, 192] }
     *   ],
     *   "particleZones": [
     *     { "x": 10, "y": 20, "width": 64, "height": 32, "type": 0,
     *       "enabled": true, "noProjection": false }
     *   ],
     *   "worldLights": [
     *     { "x": 120, "y": 88, "r": 1.0, "g": 0.85, "b": 0.55, "radius": 64,
     *       "schedule": "NightOnly" }
     *   ],
     *   "animatedTiles": [{ "frames": [1, 2, 3], "frameDuration": 0.2 }],
     *   "layerAnimationMaps": [{ "42": 0 }],
     *   "cornerCutBlocked": { "42": 3 },
     *   "npcs": [
     *     { "type": "BW2_NPC1", "tileX": 10, "tileY": 5, "name": "Ari",
     *       "dialogueTree": { "...": "..." } }
     *   ],
     *   "player": { "tileX": 5, "tileY": 5, "characterType": 0 }
     * }
     * @endcode
     *
     * ```mermaid
     * flowchart LR
     *     MapJSON["Map JSON"] --> Layers["dynamicLayers[]"]
     *     MapJSON --> Grids["collision / navigation / elevation / cornerCutBlocked"]
     *     MapJSON --> Structures["noProjectionStructures[]"]
     *     MapJSON --> Effects["particleZones[] / worldLights[] / animatedTiles[]"]
     *     MapJSON --> Actors["npcs[] / player"]
     *     Layers --> PerTile["Tile artwork and per-cell layer fields"]
     *     Structures --> PerTile
     * ```
     */
    bool SaveMapToJSON(const std::string& filename,
                       const entt::registry* npcs = nullptr,
                       int playerTileX = -1,
                       int playerTileY = -1,
                       int characterType = -1) const;

    /**
     * @fn bool LoadMapFromJSON(const std::string& filename, entt::registry* npcs = nullptr, int* \
     *     playerTileX = nullptr, int* playerTileY = nullptr, int* characterType = nullptr)
     * @brief Replaces map state from JSON with optional NPC and player data.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Accepts ySorted, navmesh, and flat animationMap keys. Invalid per-cell grid or dynamicLayers
     * entries are skipped with capped warnings. Malformed structure, effect, animation, or
     * corner-mask
     * entries fail the load and reset the map to empty.
     *
     * Without stance, noProjection maps to structure. Object/foreground Y-sort cells become wall
     * when connected to another upright cell, otherwise prop. Other cells remain Flat.
     * Saving writes stance and omits noProjection.
     *
     * @param filename JSON file path; relative paths use the process working directory.
     * @param npcs Null skips NPC loading. An NPCs array replaces the registry's NPC roster;
     * an absent array leaves it unchanged.
     * @param playerTileX Optional output in tiles.
     * @param playerTileY Optional output in tiles.
     * @param characterType Optional output.
     * @return False on file, parse, or unhandled entry failure.
     */
    bool LoadMapFromJSON(const std::string& filename,
                         entt::registry* npcs = nullptr,
                         int* playerTileX = nullptr,
                         int* playerTileY = nullptr,
                         int* characterType = nullptr);

    /**
     * @fn std::vector<int> GetValidTileIDs() const
     * @brief Lists tile IDs with at least one nonzero-alpha pixel.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::vector<int> GetValidTileIDs() const;

    /**
     * @fn bool IsTileTransparent(int tileID) const
     * @brief True when every pixel has zero alpha.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool IsTileTransparent(int tileID) const;

    const std::vector<ParticleZone>* GetParticleZones() const { return &m_ParticleZones; }

    std::vector<ParticleZone>* GetParticleZonesMutable() { return &m_ParticleZones; }

    void AddParticleZone(const ParticleZone& zone) { m_ParticleZones.push_back(zone); }

    void RemoveParticleZone(size_t index)
    {
        if (index < m_ParticleZones.size())
        {
            m_ParticleZones.erase(m_ParticleZones.begin() + index);
        }
    }

    /**
     * @fn void InsertParticleZoneAt(size_t index, const ParticleZone& zone)
     * @brief Restores a zone's index for editor undo and ParticleSystem tracking.
     * @author Alex (<https://github.com/lextpf>)
     */
    void InsertParticleZoneAt(size_t index, const ParticleZone& zone)
    {
        if (index <= m_ParticleZones.size())
            m_ParticleZones.insert(m_ParticleZones.begin() + index, zone);
    }

    const std::vector<WorldLight>& GetLights() const { return m_Lights; }

    std::vector<WorldLight>& GetLightsMutable() { return m_Lights; }

    void AddLight(const WorldLight& light) { m_Lights.push_back(light); }

    bool RemoveLight(size_t index)
    {
        if (index >= m_Lights.size())
            return false;
        m_Lights.erase(m_Lights.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    void ClearLights() { m_Lights.clear(); }

    /**
     * @fn void PopLastAnimatedTile()
     * @brief Removes the last animation during LIFO undo.
     * @author Alex (<https://github.com/lextpf>)
     *
     * All per-tile references must have been reverted first.
     */
    void PopLastAnimatedTile()
    {
        if (!m_AnimatedTiles.empty())
            m_AnimatedTiles.pop_back();
    }

    /**
     * @fn int AddAnimatedTile(const AnimatedTile& anim)
     * @brief Returns the appended animation's index.
     * @author Alex (<https://github.com/lextpf>)
     */
    int AddAnimatedTile(const AnimatedTile& anim)
    {
        m_AnimatedTiles.push_back(anim);
        return static_cast<int>(m_AnimatedTiles.size() - 1);
    }

    /**
     * @fn const AnimatedTile* GetAnimatedTile(int id) const
     * @brief Returns nullptr for an invalid animation ID.
     * @author Alex (<https://github.com/lextpf>)
     */
    const AnimatedTile* GetAnimatedTile(int id) const
    {
        if (id < 0 || id >= static_cast<int>(m_AnimatedTiles.size()))
            return nullptr;
        return &m_AnimatedTiles[id];
    }

    /**
     * @fn void SetTileAnimation(int x, int y, int layer, int animId)
     * @brief Assigns animation and paints its first frame when available.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Clearing with -1 retains the painted tile. Undo must restore both IDs.
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer Zero-based.
     * @param animId -1 clears the animation.
     */
    void SetTileAnimation(int x, int y, int layer, int animId)
    {
        if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
            return;
        if (layer < 0 || layer >= static_cast<int>(m_Layers.size()))
            return;
        size_t idx = FlatIndex(x, y);
        if (idx >= m_Layers[layer].animationMap.size())
            return;

        m_Layers[layer].animationMap[idx] = animId;

        // Place the first frame on the layer so there's a tile to render
        // (the animation check happens after tile existence check)
        if (animId >= 0 && animId < static_cast<int>(m_AnimatedTiles.size()) &&
            !m_AnimatedTiles[animId].frames.empty())
        {
            m_Layers[layer].tiles[idx] = m_AnimatedTiles[animId].frames[0];
        }
    }

    /**
     * @fn int GetTileAnimation(int x, int y, int layer) const
     * @brief Returns -1 for an unanimated tile or invalid coordinates.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param x Tile column, zero-based.
     * @param y Tile row, zero-based.
     * @param layer Zero-based.
     */
    int GetTileAnimation(int x, int y, int layer) const
    {
        if (x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
            return -1;
        if (layer < 0 || layer >= static_cast<int>(m_Layers.size()))
            return -1;
        size_t idx = FlatIndex(x, y);
        if (idx >= m_Layers[layer].animationMap.size())
            return -1;
        return m_Layers[layer].animationMap[idx];
    }

    /**
     * @fn void UpdateAnimations(float deltaTime)
     * @brief Advances elapsed animation time in seconds.
     * @author Alex (<https://github.com/lextpf>)
     */
    void UpdateAnimations(float deltaTime) { m_AnimationTime += deltaTime; }

    /**
     * @fn auto GetLayerField(int x, int y, size_t layer) const
     * @brief Get a per-tile field value with bounds checking.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Returns the field's default on out of bounds.
     */
    template <auto Field>
    auto GetLayerField(int x, int y, size_t layer) const
    {
        using Vec = std::decay_t<decltype(std::declval<TileLayer>().*Field)>;
        if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
            return static_cast<typename Vec::value_type>(Vec::default_value);
        return static_cast<typename Vec::value_type>((m_Layers[layer].*Field)[FlatIndex(x, y)]);
    }

    /**
     * @fn void SetLayerField( int x, int y, size_t layer, typename \
     *     std::decay_t<decltype(std::declval<TileLayer>().*Field)>::value_type value)
     * @brief Set a per-tile field value with bounds checking.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Silently ignores out of bounds.
     */
    template <auto Field>
    void SetLayerField(
        int x,
        int y,
        size_t layer,
        typename std::decay_t<decltype(std::declval<TileLayer>().*Field)>::value_type value)
    {
        if (layer >= m_Layers.size() || x < 0 || x >= m_MapWidth || y < 0 || y >= m_MapHeight)
            return;
        (m_Layers[layer].*Field)[FlatIndex(x, y)] = value;
    }

    /**
     * @fn void ComputeTileRange(int mapW, int mapH, int tileW, int tileH, const glm::vec2& \
     *     cullCam, const glm::vec2& cullSize, int& x0, int& y0, int& x1, int& y1)
     * @brief Compute the visible tile range from a camera rectangle, clamped to map bounds.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param mapW Map width in tiles.
     * @param mapH Map height in tiles.
     * @param tileW Tile width in pixels.
     * @param tileH Tile height in pixels.
     * @param cullCam Top-left corner of the visible area in world pixels.
     * @param cullSize Size of the visible area in world pixels.
     * @param x0 First visible tile column (inclusive).
     * @param y0 First visible tile row (inclusive).
     * @param x1 Last visible tile column (inclusive).
     * @param y1 Last visible tile row (inclusive).
     */
    static inline void ComputeTileRange(int mapW,
                                        int mapH,
                                        int tileW,
                                        int tileH,
                                        const glm::vec2& cullCam,
                                        const glm::vec2& cullSize,
                                        int& x0,
                                        int& y0,
                                        int& x1,
                                        int& y1)
    {
        float minX = cullCam.x;
        float minY = cullCam.y;
        float maxX = cullCam.x + cullSize.x;
        float maxY = cullCam.y + cullSize.y;

        x0 = (int)std::floor(minX / tileW);
        y0 = (int)std::floor(minY / tileH);
        x1 = (int)std::floor(maxX / tileW);
        y1 = (int)std::floor(maxY / tileH);

        // Clamp to map
        x0 = std::max(0, std::min(x0, mapW - 1));
        y0 = std::max(0, std::min(y0, mapH - 1));
        x1 = std::max(0, std::min(x1, mapW - 1));
        y1 = std::max(0, std::min(y1, mapH - 1));
    }

    /**
     * @fn ColumnProxy<defaulted_vector<int, -1>, int, -1> operator[](int x)
     * @brief Indexes internal layer 0 by column then row.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @code{.cpp}
     * tilemap[10][20] = 15;  // Set tile at (10,20) to ID 15
     * @endcode
     */
    ColumnProxy<defaulted_vector<int, -1>, int, -1> operator[](int x)
    {
        return ColumnProxy<defaulted_vector<int, -1>, int, -1>(
            &m_Layers[0].tiles, &m_MapWidth, &m_MapHeight, x);
    }

    /**
     * @fn ConstColumnProxy<defaulted_vector<int, -1>, int, -1> operator[](int x) const
     * @brief Indexes internal layer 0 by column then row.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @code{.cpp}
     * int tileID = tilemap[10][20];
     * @endcode
     */
    ConstColumnProxy<defaulted_vector<int, -1>, int, -1> operator[](int x) const
    {
        return ConstColumnProxy<defaulted_vector<int, -1>, int, -1>(
            &m_Layers[0].tiles, &m_MapWidth, &m_MapHeight, x);
    }

private:
    Texture m_TilesetTexture;               ///< Combined tileset texture.
    int m_TileWidth{16}, m_TileHeight{16};  ///< Tile dimensions in pixels.
    /**
     * @brief Combined atlas dimensions in pixels, not tile counts. The height grows
     * when `PackAdditionalSheets` appends character sheets.
     */
    int m_TilesetWidth{0}, m_TilesetHeight{0};
    int m_TilesPerRow{0};  ///< Tiles per row in tileset.

    using TilesetDataPtr = std::unique_ptr<unsigned char[], void (*)(unsigned char*)>;
    TilesetDataPtr m_TilesetData{nullptr, +[](unsigned char* p) { delete[] p; }};
    int m_TilesetDataWidth{0}, m_TilesetDataHeight{0};  ///< Raw image dimensions.
    int m_TilesetChannels{0};  ///< Number of color channels (3=RGB, 4=RGBA).
    /// Tileset-only pixel height used to replace packed sprite sheets.
    int m_TilesetOnlyHeight{0};
    std::vector<uint8_t> m_TileTransparencyCache;  ///< Cached transparency results per tile ID.
    bool m_TransparencyCacheBuilt{false};          ///< Whether the cache has been built.

    /**
     * @brief Pixel offsets within the atlas for character sprite sheets packed via
     * `PackAdditionalSheets`. Keyed by caller-supplied identifier.
     */
    std::unordered_map<std::string, glm::vec2> m_CharacterAtlasOffsets;

    int m_MapWidth{125}, m_MapHeight{125};  ///< Map dimensions in tiles.

    /// Layer count follows loaded map data; query GetLayerCount.
    std::vector<TileLayer> m_Layers;

    CollisionMap<std::vector> m_CollisionMap;    ///< Collision flags.
    NavigationMap<std::vector> m_NavigationMap;  ///< NPC walkability flags.
    std::vector<uint8_t>
        m_CornerCutBlocked;  ///< Per-tile corner cut disable mask (4 bits per tile).

    std::vector<int> m_Elevation;  ///< Per-tile elevation in pixels (0 = ground).
    mutable std::vector<int>
        m_ElevationRegionIds;  ///< Cached connected-component id per elevation cell.
    mutable bool m_ElevationRegionIdsDirty{true};

    /**
     * @fn void RebuildElevationRegionIds() const
     * @brief Rebuild connected non-zero-elevation component IDs after map edits.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RebuildElevationRegionIds() const;

    std::vector<ParticleZone> m_ParticleZones;  ///< Placeable particle emitter zones.

    /// Map-authored lights use scheduled intensity and additive pools.
    std::vector<WorldLight> m_Lights;  ///< Persistent point lights (lamps, windows).

    std::vector<AnimatedTile> m_AnimatedTiles;  ///< Animation definitions.
    /// Unused storage; live animation IDs are in TileLayer::animationMap.
    std::vector<int> m_TileAnimationMap;
    float m_AnimationTime{0.0f};  ///< Global animation timer.

    std::vector<NoProjectionStructure>
        m_NoProjectionStructures;  ///< Manually defined structures with anchors.

    /**
     * @fn void RebuildStructureBoundsCache() const
     * @brief Rebuild the full structure bounds cache from tile data (called lazily).
     * @author Alex (<https://github.com/lextpf>)
     */
    void RebuildStructureBoundsCache() const;

    /**
     * @fn void RebuildSingleStructureBounds(size_t layerIdx, int structId, int64_t key) const
     * @brief Rebuild bounds for a single (layer, structId) pair by scanning one layer.
     * @author Alex (<https://github.com/lextpf>)
     */
    void RebuildSingleStructureBounds(size_t layerIdx, int structId, int64_t key) const;

    /**
     * @fn void InvalidateStructureBoundsCache()
     * @brief Invalidate the entire cache so it is rebuilt on next access.
     * @author Alex (<https://github.com/lextpf>)
     */
    void InvalidateStructureBoundsCache();

    /**
     * @fn void InvalidateStructureBoundsForTile( size_t layerIdx, int x, int y, int oldStructId, \
     *     int newStructId)
     * @brief Incrementally update cache when a single tile's structure ID changes.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Expands new-structure bounds o(1); marks old structure dirty for lazy re-scan.
     */
    void InvalidateStructureBoundsForTile(
        size_t layerIdx, int x, int y, int oldStructId, int newStructId);

    /**
     * @fn const StructureBounds* GetCachedStructureBounds(size_t layerIdx, int structId) const
     * @brief Look up cached bounds for a (layer, structId) pair.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @return Pointer to bounds, or nullptr if not found.
     */
    const StructureBounds* GetCachedStructureBounds(size_t layerIdx, int structId) const;

    mutable std::unordered_map<int64_t, StructureBounds>
        m_StructureBoundsCache;  ///< (layerIdx<<32 | structId) -> bounds.
    mutable std::unordered_set<int64_t> m_DirtyStructureKeys;  ///< Per-structure dirty keys.
    mutable bool m_StructureBoundsCacheDirty = true;  ///< Whether the full cache needs a rebuild.

    /// Column span and base row of the structure body a structure cell belongs to.
    struct StructureBody
    {
        int minX = 0;     ///< Westmost column of the body.
        int maxX = 0;     ///< Eastmost column of the body.
        int baseRow = 0;  ///< Row the whole body stands on.
    };

    /**
     * @fn StructureBody ResolveStructureBody(const TileLayer& layer, size_t layerIdx, int tileX, \
     *     int tileY) const
     * @brief Uses authored structure bounds, or the cell's run for auto detection.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Bodies never cross layers. Authored bounds preserve l-shaped and hollow groups.
     */
    StructureBody ResolveStructureBody(const TileLayer& layer,
                                       size_t layerIdx,
                                       int tileX,
                                       int tileY) const;

    /**
     * @fn float StructureFootHeight(const TileLayer& layer, int runMinX, int runMaxX, int \
     *     baseRow) const
     * @brief Samples the base row at the centre column to keep the body rigid.
     * @author Alex (<https://github.com/lextpf>)
     */
    float StructureFootHeight(const TileLayer& layer, int runMinX, int runMaxX, int baseRow) const;

    mutable std::vector<DepthSortedTile>
        m_DepthSortedTilesCache;                 ///< Cached depth tiles (reused each frame).
    mutable std::vector<bool> m_ProcessedCache;  ///< Cached processed flags (reused each frame).
    mutable std::vector<bool>
        m_RenderedStructuresCache;                   ///< Cached structure flags, reused each frame.
    mutable std::vector<bool> m_FloodFillProcessed;  ///< Reusable buffer for flood-fill searches.

    /**
     * @fn void GenerateDefaultMap()
     * @brief Fills layer 0 with random non-transparent tile IDs.
     * @author Alex (<https://github.com/lextpf>)
     */
    void GenerateDefaultMap();

    /**
     * @fn void BuildTransparencyCache()
     * @brief Caches per-tile alpha scans during tileset loading.
     * @author Alex (<https://github.com/lextpf>)
     */
    void BuildTransparencyCache();
};
