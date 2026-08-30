#pragma once

#include <array>
#include <cstdint>
#include <vector>

/**
 * @brief RGBA channel order compatible with Texture::LoadFromData using four channels.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 */
using Pixel = std::array<uint8_t, 4>;

/**
 * @fn void GeneratePixels(std::vector<unsigned char>& pixels, int w, int h, PixelFn fn)
 * @brief Fill an RGBA buffer in row order with a caller-supplied pixel function.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * `w` and `h` must be positive texel dimensions; `fn` returns Pixel.
 * Upload with flipY = false to preserve the generated row order.
 *
 * @code{.cpp}
 * std::vector<unsigned char> pixels;
 * GeneratePixels(pixels, 64, 64,
 *     [](int x, int y, int w, int h) -> Pixel
 *     {
 *         float dx = x - w / 2.0f;
 *         float dy = y - h / 2.0f;
 *         float d  = std::sqrt(dx * dx + dy * dy) / (w / 2.0f);
 *         auto  a  = static_cast<uint8_t>(std::exp(-d * d * 3.0f) * 255);
 *         return {255, 255, 255, a};  // white with gaussian alpha
 *     });
 * @endcode
 */
template <typename PixelFn>
void GeneratePixels(std::vector<unsigned char>& pixels, int w, int h, PixelFn fn)
{
    pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            auto [r, g, b, a] = fn(x, y, w, h);
            int i = (y * w + x) * 4;
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = a;
        }
    }
}
