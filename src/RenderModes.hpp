#pragma once

#include "EnumTraits.hpp"

#include <cstddef>
#include <iterator>
#include <string_view>

/**
 * @brief Controls blend, depth, and lighting for world-space passes.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Rendering
 *
 * | pass        | depth        | blend          | light   |
 * |-------------|--------------|----------------|---------|
 * | ground      | none         | Alpha          | Ambient |
 * | opaque      | TestAndWrite | Alpha          | Ambient |
 * | facade      | TestOnly     | Alpha/Additive | SelfLit |
 * | cards       | none         | Alpha/Additive | SelfLit |
 * | light pools | none         | Additive       | SelfLit |
 * | sky sheet   | none         | Additive       | SelfLit |
 *
 * Ground keeps authored layer order because layers are coplanar. opaque cutout
 * fragments write neither color nor depth below the alpha threshold.
 * Light pools ignore depth to cover actors and walls as in the flat path.
 * Particles keep non-additive draws before additive draws on their shared plane.
 *
 * SelfLit supplies neutral ambient per batch. changing global ambient cannot safely
 * replace it because SetAmbientColor flushes only the OpenGL 2D batch.
 */
namespace renderModes
{

/// How a draw's color combines with what is already in the target.
enum class BlendMode
{
    /// Standard src.a, 1 - src.a compositing.
    Alpha = 0,
    /// src.a, 1 accumulation for glows (fireflies, sparkles, light pools).
    Additive = 1
};

/// How a draw interacts with the depth buffer.
enum class DepthMode
{
    /**
     * @brief Ignore depth entirely: no test, no write.
     *
     * the flat ground sheet, whose layers are coplanar and must keep authored layer order.
     */
    None = 0,
    /**
     * @brief Test against existing depth but do not write.
     *
     * translucent world geometry, which must not occlude whatever is drawn after it.
     */
    TestOnly = 1,
    /// Test and write. opaque (alpha-cutout) world geometry.
    TestAndWrite = 2
};

/// Whether the scene's day/night ambient tints a draw.
enum class LightMode
{
    /// Multiply the scene ambient into the sampled texel. world geometry.
    Ambient = 0,
    /// Ignore the scene ambient. particles and other self-lit artwork.
    SelfLit = 1
};

inline constexpr std::size_t BLEND_MODE_COUNT = 2;

inline constexpr std::size_t DEPTH_MODE_COUNT = 3;

inline constexpr std::size_t LIGHT_MODE_COUNT = 2;

/**
 * @brief Alpha cutoff for opaque geometry.
 *
 * The 0.5 threshold preserves partially covered single-pixel edges.
 */
inline constexpr float OPAQUE_ALPHA_CUTOFF = 0.5f;

}  // namespace renderModes

template <>
struct EnumTraits<renderModes::BlendMode>
    : EnumTraitsBase<renderModes::BlendMode, EnumTraits<renderModes::BlendMode>>
{
    static constexpr std::size_t Count = renderModes::BLEND_MODE_COUNT;
    static constexpr std::string_view Names[] = {"Alpha", "Additive"};
};

template <>
struct EnumTraits<renderModes::DepthMode>
    : EnumTraitsBase<renderModes::DepthMode, EnumTraits<renderModes::DepthMode>>
{
    static constexpr std::size_t Count = renderModes::DEPTH_MODE_COUNT;
    static constexpr std::string_view Names[] = {"None", "TestOnly", "TestAndWrite"};
};

template <>
struct EnumTraits<renderModes::LightMode>
    : EnumTraitsBase<renderModes::LightMode, EnumTraits<renderModes::LightMode>>
{
    static constexpr std::size_t Count = renderModes::LIGHT_MODE_COUNT;
    static constexpr std::string_view Names[] = {"Ambient", "SelfLit"};
};

static_assert(std::size(EnumTraits<renderModes::BlendMode>::Names) == renderModes::BLEND_MODE_COUNT,
              "renderModes::BlendMode names must stay in step with BLEND_MODE_COUNT");
static_assert(std::size(EnumTraits<renderModes::DepthMode>::Names) == renderModes::DEPTH_MODE_COUNT,
              "renderModes::DepthMode names must stay in step with DEPTH_MODE_COUNT");
static_assert(std::size(EnumTraits<renderModes::LightMode>::Names) == renderModes::LIGHT_MODE_COUNT,
              "renderModes::LightMode names must stay in step with LIGHT_MODE_COUNT");
