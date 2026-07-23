#pragma once

#include "EnumTraits.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

/**
 * @brief Records sky elements for flat or 3D submission.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Positions are top-left offsets in viewport-relative world pixels.
 * Replay in emission order; sorting or regrouping changes painter order.
 * Sprites identify textures without requiring a graphics context.
 */
namespace skyDraw
{

/// Which sky texture an element samples.
enum class Sprite : std::uint8_t
{
    Ray = 0,        ///< Vertical gradient for sun and moon rays.
    Star,           ///< Small soft circle for star cores.
    StarGlow,       ///< Larger glow behind bright stars.
    ShootingStar,   ///< Elongated streak for meteors.
    Glow,           ///< Large soft glow: washes, halos, lightning.
    LightPool,      ///< Soft circle for a WorldLight pool.
    AuroraCurtain,  ///< Vertical streaked curtain for aurora bands.
    AuroraSmall,    ///< Soft dot for aurora wisps.
    AuroraBeam,     ///< Vertical oval for aurora beams.
    Solid           ///< Opaque white; the 3D stand-in for an untextured rect.
};

/// Effect identity for budgets; never an ordering key.
enum class Layer : std::uint8_t
{
    DawnWash = 0,     ///< Dawn gradient and dawn horizon glow.
    AtmosphericWash,  ///< Night horizon band and top shimmer band.
    Aurora,           ///< Curtains, ribbon halos, beams and wisps.
    Star,             ///< Background stars, star glows and star cores.
    Meteor,           ///< Shooting stars.
    Dew,              ///< Morning dew sparkles.
    Ray,              ///< Sun and moon ray fans.
    Flash,            ///< Lightning flash wash.
    Bolt              ///< Lightning bolt segments.
};

inline constexpr std::size_t SPRITE_COUNT = 10;

inline constexpr std::size_t LAYER_COUNT = 9;

/**
 * @struct Element
 * @brief Arguments shared by the flat and 3D submitters.
 * @author Alex (<https://github.com/lextpf>)
 */
struct Element
{
    Sprite sprite = Sprite::Glow;   ///< Texture to sample.
    Layer layer = Layer::DawnWash;  ///< Owning effect; budget and grouping only.
    glm::vec2 pos{0.0f};            ///< Top-left, viewport-relative world pixels.
    glm::vec2 size{0.0f};           ///< Width and height in world pixels.
    float rotation = 0.0f;          ///< Degrees, about the quad's own centre.
    glm::vec4 color{1.0f};          ///< RGB tint and alpha, pre-blend.
    bool additive = false;          ///< Additive rather than alpha compositing.

    /**
     * @brief Bypasses the atlas on the flat path.
     *
     * The 3D path ignores this flag to keep sky draws in one atlas batch.
     */
    bool standalone = false;
};

/**
 * @struct List
 * @brief Retains element storage between frames.
 * @author Alex (<https://github.com/lextpf>)
 */
struct List
{
    std::vector<Element> items;  ///< Emission order; never sort or regroup.

    /**
     * @fn void Clear() noexcept
     * @brief Drop every element, keeping the allocated capacity.
     * @author Alex (<https://github.com/lextpf>)
     */
    void Clear() noexcept { items.clear(); }

    /**
     * @fn void Add(Layer layer, Sprite sprite, glm::vec2 pos, glm::vec2 size, float rotation, \
     * glm::vec4 color, bool additive, bool standalone = false)
     * @brief Appends in painter order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param layer Effect identity used for budgets, without changing emission order.
     * @param sprite Texture identity resolved by the submitter.
     * @param pos Top-left in viewport-relative world pixels.
     * @param size World pixels; negative width mirrors artwork.
     * @param rotation Degrees about the centre.
     * @param color RGB tint and opacity applied by the submitter.
     * @param additive Select additive blending where the renderer supports it.
     * @param standalone Bypass the atlas in the flat submitter; ignored by the 3D submitter.
     */
    void Add(Layer layer,
             Sprite sprite,
             glm::vec2 pos,
             glm::vec2 size,
             float rotation,
             glm::vec4 color,
             bool additive,
             bool standalone = false)
    {
        Element e;
        e.sprite = sprite;
        e.layer = layer;
        e.pos = pos;
        e.size = size;
        e.rotation = rotation;
        e.color = color;
        e.additive = additive;
        e.standalone = standalone;
        items.push_back(e);
    }

    [[nodiscard]] std::size_t CountIn(Layer layer) const noexcept
    {
        std::size_t n = 0;
        for (const Element& e : items)
        {
            if (e.layer == layer)
            {
                ++n;
            }
        }
        return n;
    }
};

/**
 * @struct LightPool
 * @brief One lit WorldLight resolved for drawing, in world space.
 * @author Alex (<https://github.com/lextpf>)
 *
 * Produced by `worldLights::Build` for both paths. The flat path ignores
 * `surfaceHeight`; the 3D path lays the quad on the ground at that height.
 */
struct LightPool
{
    glm::vec2 centreWorld{0.0f};  ///< Light position in world pixels.
    float radius = 0.0f;          ///< Pool radius in world pixels.
    float surfaceHeight = 0.0f;   ///< Scene height of the ground under the light.
    glm::vec4 color{1.0f};        ///< Light colour with the resolved intensity as alpha.
};

/// The lit pools of one frame, in map order.
using LightPoolList = std::vector<LightPool>;

}  // namespace skyDraw

template <>
struct EnumTraits<skyDraw::Sprite> : EnumTraitsBase<skyDraw::Sprite, EnumTraits<skyDraw::Sprite>>
{
    static constexpr std::size_t Count = skyDraw::SPRITE_COUNT;
    static constexpr std::string_view Names[] = {"Ray",
                                                 "Star",
                                                 "StarGlow",
                                                 "ShootingStar",
                                                 "Glow",
                                                 "LightPool",
                                                 "AuroraCurtain",
                                                 "AuroraSmall",
                                                 "AuroraBeam",
                                                 "Solid"};
};

template <>
struct EnumTraits<skyDraw::Layer> : EnumTraitsBase<skyDraw::Layer, EnumTraits<skyDraw::Layer>>
{
    static constexpr std::size_t Count = skyDraw::LAYER_COUNT;
    static constexpr std::string_view Names[] = {
        "DawnWash", "AtmosphericWash", "Aurora", "Star", "Meteor", "Dew", "Ray", "Flash", "Bolt"};
};

static_assert(std::size(EnumTraits<skyDraw::Sprite>::Names) == skyDraw::SPRITE_COUNT,
              "skyDraw::Sprite names must stay in step with SPRITE_COUNT");
static_assert(std::size(EnumTraits<skyDraw::Layer>::Names) == skyDraw::LAYER_COUNT,
              "skyDraw::Layer names must stay in step with LAYER_COUNT");
