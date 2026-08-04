#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

/**
 * @brief Shared visible-world and UI scaling math.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
namespace viewScaling
{
/**
 * @fn glm::vec2 VisibleWorldSize(int screenWidth, int screenHeight, int pixelScale)
 * @brief Unzoomed world pixels for framebuffer dimensions.
 * @author Alex (<https://github.com/lextpf>)
 *
 * @param screenWidth Framebuffer width in pixels.
 * @param screenHeight Framebuffer height in pixels.
 * @param pixelScale Clamped to at least 1.
 */
inline glm::vec2 VisibleWorldSize(int screenWidth, int screenHeight, int pixelScale)
{
    const float scale = static_cast<float>(std::max(1, pixelScale));
    return {static_cast<float>(screenWidth) / scale, static_cast<float>(screenHeight) / scale};
}

/**
 * @fn glm::vec2 VisibleWorldSizeZoomed(int screenWidth, int screenHeight, int pixelScale, float \
 * zoom)
 * @brief Zoomed world pixels; zoom above 1 reduces the visible extent.
 * @author Alex (<https://github.com/lextpf>)
 *
 * pixelScale is clamped to 1 and zoom to 0.001.
 */
inline glm::vec2 VisibleWorldSizeZoomed(int screenWidth,
                                        int screenHeight,
                                        int pixelScale,
                                        float zoom)
{
    const float z = std::max(zoom, 0.001f);
    return VisibleWorldSize(screenWidth, screenHeight, pixelScale) / z;
}

/**
 * @fn float MenuUiScale(int screenWidth, int screenHeight, float referenceWidth, float \
 * referenceHeight)
 * @brief Scales UI by the smaller window-to-reference ratio.
 * @author Alex (<https://github.com/lextpf>)
 *
 * All dimensions are pixels. reference dimensions are clamped to at least 1.
 */
inline float MenuUiScale(int screenWidth,
                         int screenHeight,
                         float referenceWidth,
                         float referenceHeight)
{
    const float wRatio = static_cast<float>(screenWidth) / std::max(referenceWidth, 1.0f);
    const float hRatio = static_cast<float>(screenHeight) / std::max(referenceHeight, 1.0f);
    return std::min(wRatio, hRatio);
}

/**
 * @fn glm::ivec2 RequiredTitleWorldTiles(int screenWidth, int screenHeight, int pixelScale, int \
 * tileWidth, int tileHeight, float zoom, int marginTiles, int minTilesWide, int minTilesTall)
 * @brief Covers the viewport and margins without shrinking below the base map.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Screen and tile dimensions are pixels. pixelScale and tile dimensions clamp to 1;
 * Zoom clamps to 0.001. marginTiles applies on every side.
 *
 * @return Tile counts bounded below by minTilesWide and minTilesTall.
 */
inline glm::ivec2 RequiredTitleWorldTiles(int screenWidth,
                                          int screenHeight,
                                          int pixelScale,
                                          int tileWidth,
                                          int tileHeight,
                                          float zoom,
                                          int marginTiles,
                                          int minTilesWide,
                                          int minTilesTall)
{
    const glm::vec2 world = VisibleWorldSizeZoomed(screenWidth, screenHeight, pixelScale, zoom);
    const int tw = std::max(1, tileWidth);
    const int th = std::max(1, tileHeight);
    const int needW =
        static_cast<int>(std::ceil(world.x / static_cast<float>(tw))) + 2 * marginTiles;
    const int needH =
        static_cast<int>(std::ceil(world.y / static_cast<float>(th))) + 2 * marginTiles;
    return {std::max(minTilesWide, needW), std::max(minTilesTall, needH)};
}
}  // namespace viewScaling
