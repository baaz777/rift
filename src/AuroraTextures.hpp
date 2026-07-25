#pragma once

#include <vector>

/**
 * @brief Procedural aurora alpha textures.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Dimensions are pixels. builders return white RGB with feathered alpha in tightly packed
 * RGBA8 buffers of width * height * 4 bytes.
 */
namespace AuroraTextures
{
/**
 * @fn std::vector<unsigned char> BuildCurtainPixels(int width, int height)
 * @brief Translucent curtain with a narrow core, broad halo and ray striations.
 * @author Alex (<https://github.com/lextpf>)
 */
std::vector<unsigned char> BuildCurtainPixels(int width, int height);

/**
 * @fn std::vector<unsigned char> BuildBeamPixels(int width, int height)
 * @brief Gaussian oval beam with feathered sides and ends.
 * @author Alex (<https://github.com/lextpf>)
 */
std::vector<unsigned char> BuildBeamPixels(int width, int height);
}  // namespace AuroraTextures
