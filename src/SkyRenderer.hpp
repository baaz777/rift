#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <vector>

#include "AmbienceConfig.hpp"
#include "IRenderer.hpp"
#include "SkyDrawList.hpp"
#include "Texture.hpp"
#include "TextureHandle.hpp"
#include "TextureStore.hpp"

class TimeManager;

namespace particleCards
{
struct Frame;
}

/**
 * @struct SkyAtlasOffsets
 * @brief Atlas offsets indexed by skyDraw::Sprite.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Region sizes come from the standalone textures at draw time.
 */
struct SkyAtlasOffsets
{
    /// Offsets in atlas pixels, indexed by skyDraw::Sprite.
    std::array<glm::vec2, skyDraw::SPRITE_COUNT> offsets{};

    glm::vec2& operator[](skyDraw::Sprite sprite)
    {
        return offsets[static_cast<std::size_t>(sprite)];
    }

    const glm::vec2& operator[](skyDraw::Sprite sprite) const
    {
        return offsets[static_cast<std::size_t>(sprite)];
    }
};

/**
 * @struct Star
 * @brief Star-field position and independent twinkle state.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Position is normalized within the wrapped field. background stars use one sine;
 * Foreground stars combine three sines and a sparkle term. glow requires brightness
 * above 0.25 and sparkle above 0.3.
 *
 * @verbatim
 *   visibility = TimeManager::GetStarVisibility() * 0.35
 *   b          = baseBrightness * twinkle * visibility
 *   background alpha      = b * 0.3
 *   foreground core alpha = b * 0.7
 *   foreground glow alpha = (b - 0.25) * 0.1
 * @endverbatim
 *
 * $$
 * twinkle = 0.6 + 0.4\sin(t \cdot speed \cdot 1.5 + phase)
 * $$
 *
 * $$
 * t_1 = \sin(t \cdot speed \cdot 1.2 + phase),\quad
 * t_2 = \sin(t \cdot speed \cdot 2.7 + 1.3\,phase),\quad
 * t_3 = \sin(t \cdot speed \cdot 0.5 + 2.1\,phase)
 * $$
 *
 * $$
 * twinkle = 0.4 + 0.35\,t_1 + 0.15\,t_3 + 0.25\max(0,\ t_1 t_2)
 * $$
 */
struct Star
{
    /**
     * @brief Normalized (0-1) position inside the star-field tile, not the screen.
     *
     * RenderStars scales it by the tile size (3 viewports wide x 2 tall) and wraps
     * the result near the camera, so one array covers the whole map.
     */
    glm::vec2 position;
    float baseBrightness;  ///< Base brightness (0-1), modulated by twinkle animation.
    float twinklePhase;    ///< Phase offset for twinkle sine wave (radians).
    float twinkleSpeed;    ///< Twinkle frequency multiplier (higher = faster flicker).
    /**
     * @brief Size input in 0 to 1.
     *
     * Background sprite = `1.0 + size * 1.2` px; the foreground core =
     * `(1.5 + size * 3.0) * (0.5 + brightness * 0.5)` px and its glow =
     * `6.0 + size * 8.0` px.
     */
    float size;
    glm::vec3 color;  ///< RGB color tint (typically near white with subtle hue).
};

/**
 * @struct LightRay
 * @brief Normalized shaping inputs for a sun or moon ray.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Moon rays use a 60-degree fan, angle * 8, width 50 + width * 70, and
 * length screenHeight * (0.35 + length * 0.45).
 *
 * @code
 *        Light Source
 *             *
 *            /|\
 *           / | \
 *          /  |  \    <- Individual rays at different angles
 *         /   |   \
 *        /    |    \
 * @endcode
 *
 * @verbatim
 *   rayAngleDeg = (xPosition - 0.5) * SUN_RAY_SPREAD + angle * 10   [degrees]
 *   rayWidth    = 50 + width * RAY_WIDTH                            [pixels]
 *   rayLength   = screenHeight * (0.5 + length * 0.4)               [pixels]
 *   originPx    = originOffset * (screenWidth * SUN_BAND_WIDTH * 0.5)
 * @endverbatim
 */
struct LightRay
{
    /**
     * @brief Relative position within the ray fan.
     *
     * Ranges from 0.05 to 0.95. sets the angle to
     * `(xPosition - 0.5) * SUN_RAY_SPREAD` degrees off vertical.
     */
    float xPosition;
    float originOffset;  ///< Horizontal offset from sun center (-1 to 1, scaled by SUN_BAND_WIDTH).
    /**
     * @brief Dimensionless angle jitter, generated in -0.15, 0.15.
     *
     * Added to the fan angle as `angle * 10` degrees (sun) or `angle * 8` (moon),
     * i.e. roughly +/-1.5 degrees of wobble - despite the name it is not radians.
     */
    float angle;
    /**
     * @brief Length input, 0.45-0.90 (sun) / 0.30-0.70 (moon).
     *
     * scales screen height, not an absolute pixel count: sun rays span 68-86% of screen height.
     */
    float length;
    /**
     * @brief Width input, 0.7-1.2.
     *
     * A multiplier on top of a fixed 50 px base, so the drawn ray is 106-146 px wide for the sun.
     * not a pixel width itself.
     */
    float width;
    float brightness;  ///< Base brightness (0-1), modulated by time-of-day.
    float phase;       ///< Animation phase offset for pulsing effect.
};

/**
 * @struct ShootingStar
 * @brief Meteor with linear motion and a timed fade.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Alpha rises over 0.08 seconds and falls over the final 0.12 seconds.
 * Lifetime is 0.3-0.7 seconds, or 0.55-0.95 during a meteor shower.
 *
 * @code
 * Spawn (top of screen)
 *        \
 *         \  <- Trail rendered behind
 *          \
 *           * (current position)
 *            \
 *             (fades out)
 * @endcode
 *
 * @verbatim
 *   fadeIn  = min(1, (maxLifetime - lifetime) / 0.08)
 *   fadeOut = min(1, lifetime / 0.12)
 *   alpha   = brightness * fadeIn * fadeOut * starVisibility
 * @endverbatim
 */
struct ShootingStar
{
    /**
     * @brief World position inside the star-field tile (see SkyRenderer::RenderStars),
     *        in pixels - not a screen position.
     *
     * RenderShootingStars wraps it near the camera before drawing, so a streak may
     * cross a wrap seam mid-flight.
     */
    glm::vec2 position;
    glm::vec2 velocity;  ///< Movement vector (pixels per second).
    float lifetime;      ///< Remaining lifetime in seconds.
    float maxLifetime;   ///< Total lifetime for fade calculations.
    float brightness;    ///< Peak alpha reached on the plateau (see the curve above).
    float length;        ///< Trail length in pixels (stretched behind velocity).
};

/**
 * @struct DewSparkle
 * @brief Lower-viewport glint driven by the top half of a sine.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Clipping and squaring produces one short peak per period.
 *
 * $$
 * twinkle = \left(\frac{\max(0,\ \sin(t \cdot speed + phase) - 0.5)}{0.5}\right)^2
 * $$
 */
struct DewSparkle
{
    glm::vec2 position;  ///< Normalized position (0-1), biased to lower screen.
    float phase;         ///< Animation phase offset for twinkle timing.
    float brightness;    ///< Base brightness (0-1).
    float speed;         ///< Twinkle animation speed multiplier.
};

/**
 * @struct VisibleStar
 * @brief Caches foreground star geometry for separate glow and core passes.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Both passes are additive, so grouping them by texture preserves the result.
 */
struct VisibleStar
{
    glm::vec2 screenPos;  ///< Camera-relative screen position (top-left of glow/core).
    glm::vec3 color;      ///< Star tint.
    float size;           ///< Core sprite size in pixels.
    float brightness;     ///< Core alpha multiplier (pre-additive).
    float glowSize;       ///< Glow sprite size in pixels; <=0 means skip glow this frame.
    float glowAlpha;      ///< Glow alpha multiplier (pre-additive).
};

/**
 * @class SkyRenderer
 * @brief Builds and submits sky effects from continuous time and weather factors.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Stars, meteors, and aurora use world positions; washes, dew, and lightning use viewport
 * positions. continuous visibility and fade values avoid snapping at time-period boundaries.
 *
 * Emission order is dawn, atmosphere, aurora, stars, meteors, dew, rays, flash, then bolt.
 * Atmosphere requires star visibility >= 0.2. flashes and bolts start together and last
 * 0.08 and 0.18 seconds respectively.
 *
 * TextureStore owns generated textures and re-uploads them when the renderer changes.
 * A missing aurora mote asset uses an empty texture, rendered as a colored rectangle.
 *
 * @code
 * SkyRenderer sky;
 * sky.Initialize(textureStore);  // Generate textures, populate star/ray arrays
 *
 * // In game loop:
 * sky.Update(deltaTime, timeManager);
 * sky.Render(renderer, timeManager, cameraPos, screenWidth, screenHeight);
 * @endcode
 */
class SkyRenderer
{
public:
    /**
     * @fn SkyRenderer()
     * @brief Allocates no GPU resources; call Initialize before use.
     * @author Alex (<https://github.com/lextpf>)
     */
    SkyRenderer();
    ~SkyRenderer();

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;
    SkyRenderer(SkyRenderer&&) noexcept = default;
    SkyRenderer& operator=(SkyRenderer&&) noexcept = default;

    /**
     * @fn void Initialize(TextureStore& store, const std::string& auroraSpritePath = \
     * std::string())
     * @brief Creates textures and randomized sky state once.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Repeated calls retain the first initialization, including auroraSpritePath.
     *
     * @param store Owns the textures and must outlive this object.
     * @param auroraSpritePath Missing or empty uses a colored-rectangle fallback.
     */
    void Initialize(TextureStore& store, const std::string& auroraSpritePath = std::string());

    /**
     * @fn void SetAtlasBinding(const Texture* atlasTex, const SkyAtlasOffsets& offsets)
     * @brief Uses atlas regions to batch sky sprites.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Flat-path aurora halos and beams keep standalone bindings; the 3D path uses the atlas.
     *
     * @param atlasTex Borrowed texture; null restores standalone textures.
     * @param offsets Region origins in atlas pixels.
     */
    void SetAtlasBinding(const Texture* atlasTex, const SkyAtlasOffsets& offsets);

    /**
     * @fn const Texture& GetRayTexture() const
     * @brief Exposes textures for atlas packing.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @pre Initialize has completed; accessors dereference the stored TextureStore.
     */
    const Texture& GetRayTexture() const { return m_Store->Get(m_RayHandle); }
    const Texture& GetStarTexture() const { return m_Store->Get(m_StarHandle); }
    const Texture& GetStarGlowTexture() const { return m_Store->Get(m_StarGlowHandle); }
    const Texture& GetShootingStarTexture() const { return m_Store->Get(m_ShootingStarHandle); }
    const Texture& GetGlowTexture() const { return m_Store->Get(m_GlowHandle); }
    const Texture& GetAuroraCurtainTexture() const { return m_Store->Get(m_AuroraCurtainHandle); }
    const Texture& GetAuroraSmallTexture() const { return m_Store->Get(m_AuroraSmallHandle); }
    const Texture& GetAuroraBeamTexture() const { return m_Store->Get(m_AuroraBeamHandle); }
    const Texture& GetSolidTexture() const { return m_Store->Get(m_SolidHandle); }

    /**
     * @fn void Update(float deltaTime, const TimeManager& time)
     * @brief Advances animations, lightning, and cached weather factors.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Spawners use the viewport from the preceding Build call. before the first build,
     * meteor spawning is skipped and lightning uses a 1x1 viewport.
     *
     * @param deltaTime Seconds.
     * @param time Current clock and resolved weather channels.
     */
    void Update(float deltaTime, const TimeManager& time);

    /**
     * @fn void Render(IRenderer& renderer, const TimeManager& time, glm::vec2 cameraPos, int \
     * screenWidth, int screenHeight)
     * @brief Builds and submits the flat sky in emission order.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Stars and meteors wrap a 3-by-2 viewport field near the camera. sun and moon use
     * 3-viewport travel bands. washes ignore camera translation.
     *
     * @param renderer Initialized renderer that receives this frame's draws.
     * @param time Current clock and resolved weather channels.
     * @param cameraPos Viewport top-left in world pixels.
     * @param screenWidth Unzoomed visible width in world pixels.
     * @param screenHeight Unzoomed visible height in world pixels.
     */
    void Render(IRenderer& renderer,
                const TimeManager& time,
                glm::vec2 cameraPos,
                int screenWidth,
                int screenHeight);

    /**
     * @fn const skyDraw::List& Build(const TimeManager& time, glm::vec2 cameraPos, int \
     * screenWidth, int screenHeight)
     * @brief Rebuilds the frame list without renderer calls or RNG changes.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Also caches viewport dimensions for Update spawners. repeated calls with the same
     * state produce the same list.
     *
     * @param time Current clock and resolved weather channels.
     * @param cameraPos Viewport top-left in world pixels.
     * @param screenWidth Unzoomed visible width in world pixels.
     * @param screenHeight Unzoomed visible height in world pixels.
     * @return Internal list in emission order; empty before Initialize. overwritten by the next
     * Build.
     */
    const skyDraw::List& Build(const TimeManager& time,
                               glm::vec2 cameraPos,
                               int screenWidth,
                               int screenHeight);

    /**
     * @fn void SubmitFlat(IRenderer& renderer, const skyDraw::List& list) const
     * @brief Preserves emission order during flat submission.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SubmitFlat(IRenderer& renderer, const skyDraw::List& list) const;

    /**
     * @fn const skyDraw::List& GetLastDrawList() const
     * @brief The list the last Build produced.
     * @author Alex (<https://github.com/lextpf>)
     */
    const skyDraw::List& GetLastDrawList() const { return m_DrawList; }

    /**
     * @fn void Render3D(IRenderer& renderer, const TimeManager& time, const \
     * particleCards::Frame& frame, glm::vec2 visibleWorldSize)
     * @brief Builds from the frame camera, then submits on its sheet.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param renderer Initialized renderer that receives this frame's draws.
     * @param time Current clock and resolved weather channels.
     * @param frame Camera sheet axes, focus, and culling data for this frame.
     * @param visibleWorldSize Unzoomed world pixels, matching the flat path.
     */
    void Render3D(IRenderer& renderer,
                  const TimeManager& time,
                  const particleCards::Frame& frame,
                  glm::vec2 visibleWorldSize);

    /**
     * @fn std::size_t Submit3D(IRenderer& renderer, const skyDraw::List& list, const \
     * particleCards::Frame& frame)
     * @brief Submits visible elements as additive, self-lit quads without depth.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Reserved effects keep their budget; stars and aurora share the remainder by even thinning.
     *
     * @return Submitted quad count.
     */
    std::size_t Submit3D(IRenderer& renderer,
                         const skyDraw::List& list,
                         const particleCards::Frame& frame);

    /**
     * @fn std::size_t SubmitLightPools3D(IRenderer& renderer, const skyDraw::LightPoolList& \
     * pools, const particleCards::Frame& frame)
     * @brief Submits light pools at their lamp surface heights.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Frustum-culls pools and caps submission at skyCards::MAX_LIGHT_POOL_QUADS_3D.
     *
     * @return Submitted pool count.
     */
    std::size_t SubmitLightPools3D(IRenderer& renderer,
                                   const skyDraw::LightPoolList& pools,
                                   const particleCards::Frame& frame);

    /**
     * @fn std::size_t GetLastQuadCount3D() const
     * @brief Quads the last Submit3D submitted.
     * @author Alex (<https://github.com/lextpf>)
     */
    std::size_t GetLastQuadCount3D() const { return m_LastQuadCount3D; }

    const Texture& GetLightPoolTexture() const { return m_Store->Get(m_LightPoolHandle); }

    /**
     * @fn void DrawLightPool(IRenderer& renderer, glm::vec2 pos, glm::vec2 size, float rotation, \
     * glm::vec4 color, bool additive)
     * @brief Uses the bound atlas for light-pool sprites when available.
     * @author Alex (<https://github.com/lextpf>)
     */
    void DrawLightPool(IRenderer& renderer,
                       glm::vec2 pos,
                       glm::vec2 size,
                       float rotation,
                       glm::vec4 color,
                       bool additive);

private:
    void GenerateRayTexture();

    void GenerateStarTexture();

    void GenerateStarGlowTexture();

    void GenerateShootingStarTexture();

    void GenerateLightRays();

    void GenerateStars(int count);

    void GenerateBackgroundStars(int count);

    void GenerateDewSparkles();

    /**
     * @fn void UpdateShootingStars(float deltaTime, int screenWidth, int screenHeight)
     * @brief Advances meteors, removes expired ones, and may spawn replacements.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param deltaTime Seconds.
     * @param screenWidth Cached unzoomed visible width in world pixels.
     * @param screenHeight Cached unzoomed visible height in world pixels.
     */
    void UpdateShootingStars(float deltaTime, int screenWidth, int screenHeight);

    /**
     * @fn void SpawnShootingStar(int screenWidth, int screenHeight)
     * @brief Randomizes a meteor at the viewport top or side.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SpawnShootingStar(int screenWidth, int screenHeight);

    /**
     * @fn void BuildStars(skyDraw::List& out, const TimeManager& time, glm::vec2 cameraPos, int \
     * screenWidth, int screenHeight)
     * @brief Wraps the star field to the copy nearest the camera.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Normalized star positions scale to a field 3 viewports wide and 2 tall.
     * std::remainder selects the nearest copy without a discontinuity at the origin.
     * BuildShootingStars uses the same wrap.
     *
     * @verbatim
     *   +---------------------------------------------+  star-field tile
     *   |                                             |  = 3 viewports wide
     *   |         +-----------+                       |    2 viewports tall
     *   |  <----  |  camera   |  ---->                |
     *   |         |  viewport |     stars outside the |
     *   |         +-----------+     tile wrap back in |
     *   |            ^     v        along both axes   |
     *   +---------------------------------------------+
     *      wrap x mod 3*screenWidth, y mod 2*screenHeight, nearest camera
     * @endverbatim
     */
    void BuildStars(skyDraw::List& out,
                    const TimeManager& time,
                    glm::vec2 cameraPos,
                    int screenWidth,
                    int screenHeight);

    void BuildShootingStars(skyDraw::List& out,
                            const TimeManager& time,
                            glm::vec2 cameraPos,
                            int screenWidth,
                            int screenHeight);

    /**
     * @fn void BuildAurora(skyDraw::List& out, const TimeManager& time, glm::vec2 cameraPos, int \
     * screenWidth, int screenHeight)
     * @brief Scales all aurora layers by the cached transition or overlay fade.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Emits above fade 0.01 and subtracts full camera translation.
     */
    void BuildAurora(skyDraw::List& out,
                     const TimeManager& time,
                     glm::vec2 cameraPos,
                     int screenWidth,
                     int screenHeight);

    /**
     * @fn void BuildAtmosphericGlow(skyDraw::List& out, const TimeManager& time, int \
     * screenWidth, int screenHeight)
     * @brief Emits horizon and upper-edge washes when star visibility reaches 0.2.
     * @author Alex (<https://github.com/lextpf>)
     */
    void BuildAtmosphericGlow(skyDraw::List& out,
                              const TimeManager& time,
                              int screenWidth,
                              int screenHeight);

    /**
     * @fn void BuildSunRays(skyDraw::List& out, const TimeManager& time, glm::vec2 cameraPos, \
     * int screenWidth, int screenHeight)
     * @brief Emits sun rays strongest near the horizon.
     * @author Alex (<https://github.com/lextpf>)
     */
    void BuildSunRays(skyDraw::List& out,
                      const TimeManager& time,
                      glm::vec2 cameraPos,
                      int screenWidth,
                      int screenHeight);

    /**
     * @fn void BuildMoonRays(skyDraw::List& out, const TimeManager& time, glm::vec2 cameraPos, \
     * int screenWidth, int screenHeight)
     * @brief Scales moon rays by phase, with new-moon brightness floored at 0.3.
     * @author Alex (<https://github.com/lextpf>)
     */
    void BuildMoonRays(skyDraw::List& out,
                       const TimeManager& time,
                       glm::vec2 cameraPos,
                       int screenWidth,
                       int screenHeight);

    /**
     * @fn void GenerateLightningBolt(int screenWidth, int screenHeight)
     * @brief Rebuilds the main bolt and 0-3 branches at each flash.
     * @author Alex (<https://github.com/lextpf>)
     */
    void GenerateLightningBolt(int screenWidth, int screenHeight);

    /**
     * @fn void BuildLightningBolt(skyDraw::List& out, int screenWidth, int screenHeight)
     * @brief Emits the camera-locked bolt while its visibility timer is positive.
     * @author Alex (<https://github.com/lextpf>)
     */
    void BuildLightningBolt(skyDraw::List& out, int screenWidth, int screenHeight);

    void BuildDawnHorizonGlow(skyDraw::List& out,
                              const TimeManager& time,
                              int screenWidth,
                              int screenHeight);

    void BuildDawnGradient(skyDraw::List& out,
                           const TimeManager& time,
                           int screenWidth,
                           int screenHeight);

    void BuildDewSparkles(skyDraw::List& out,
                          const TimeManager& time,
                          int screenWidth,
                          int screenHeight);

    /**
     * @fn glm::vec2 GetLightSourcePosition(float arc, int screenWidth, int screenHeight, \
     * glm::vec2 cameraPos, float parallaxFactor) const
     * @brief Wraps a celestial body across camera-aligned bands.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Bands are 3 viewports wide. X moves right to left as arc increases.
     * Y stays 20 pixels above the viewport horizon and rises 40 pixels at the apex.
     *
     * @param arc 0 at horizon; 0.5 at zenith.
     * @param screenWidth Cached unzoomed visible width in world pixels.
     * @param screenHeight Unused.
     * @param cameraPos Viewport top-left in world pixels, before parallax scaling.
     * @param parallaxFactor Camera fraction subtracted: 0 screen-locked, 1 world-locked.
     * @return Camera-relative pixels.
     */
    glm::vec2 GetLightSourcePosition(float arc,
                                     int screenWidth,
                                     int screenHeight,
                                     glm::vec2 cameraPos,
                                     float parallaxFactor) const;

    /// Borrowed store set by Initialize; must outlive this object.
    TextureStore* m_Store = nullptr;
    TextureHandle m_RayHandle;            ///< Vertical gradient for light rays.
    TextureHandle m_StarHandle;           ///< Small soft circle for stars.
    TextureHandle m_StarGlowHandle;       ///< Larger glow behind bright stars.
    TextureHandle m_ShootingStarHandle;   ///< Elongated streak for meteors.
    TextureHandle m_GlowHandle;           ///< Large soft glow for atmosphere.
    TextureHandle m_LightPoolHandle;      ///< Soft circle for WorldLight pools.
    TextureHandle m_AuroraCurtainHandle;  ///< Vertical streaked curtain for aurora bands.
    TextureHandle m_AuroraBeamHandle;     ///< Vertical oval ray/beam for aurora beams.
    TextureHandle m_AuroraSmallHandle;    ///< Procedural soft dot for aurora wisps.
    /// Supplies untextured atmospheric rectangles to the 3D quad path.
    TextureHandle m_SolidHandle;

    /**
     * @brief Borrowed atlas; null selects standalone textures.
     *
     * Region sizes are read from the standalone textures at draw time.
     */
    const Texture* m_AtlasTexture{nullptr};
    SkyAtlasOffsets m_AtlasOffsets;  ///< Where each sprite sits inside that atlas.

    /**
     * @struct SpriteBinding
     * @brief Resolved sprite texture and optional atlas region.
     * @author Alex (<https://github.com/lextpf>)
     */
    struct SpriteBinding
    {
        const Texture* atlas = nullptr;       ///< Bound atlas, or null to bind the sprite's own.
        glm::vec2 atlasOffset{0.0f};          ///< Region top-left inside that atlas, pixels.
        const Texture* standalone = nullptr;  ///< The sprite's own texture; the region size.
    };

    /**
     * @fn SpriteBinding ResolveSprite(skyDraw::Sprite sprite, bool forceStandalone) const
     * @brief Resolves one sprite binding.
     * @author Alex (<https://github.com/lextpf>)
     *
     * @param sprite Logical sky texture identity to resolve.
     * @param forceStandalone Bypasses the atlas for flat-path aurora draws.
     * @return Standalone is non-null after Initialize.
     */
    SpriteBinding ResolveSprite(skyDraw::Sprite sprite, bool forceStandalone) const;

    /// This frame's elements, cleared and refilled by Build.
    skyDraw::List m_DrawList;

    void DrawLightPool3D(IRenderer& renderer, const glm::vec3 corners[4], glm::vec4 color) const;

    std::size_t m_LastQuadCount3D = 0;  ///< Quads the last Submit3D submitted.
    bool m_WarnedQuadBudget = false;    ///< Sky budget overrun already reported.
    bool m_WarnedPoolBudget = false;    ///< Light pool cap already reported.

    std::vector<Star> m_Stars;                       ///< Foreground stars (bright, prominent).
    std::vector<Star> m_BackgroundStars;             ///< Background stars (dim, distant).
    std::vector<VisibleStar> m_VisibleStarsScratch;  ///< Per-frame scratch for two-pass star emit.
    std::vector<LightRay> m_SunRays;                 ///< Sun god ray configurations.
    std::vector<LightRay> m_MoonRays;                ///< Moon ray configurations.
    std::vector<ShootingStar> m_ShootingStars;       ///< Active shooting stars.
    std::vector<DewSparkle> m_DewSparkles;           ///< Morning dew sparkle points.

    double m_Time;              ///< Accumulated time for twinkle animations (seconds).
    float m_ShootingStarTimer;  ///< Countdown to next shooting star spawn.
    float m_LastScreenWidth;    ///< Cached screen width for resize detection.
    float m_LastScreenHeight;   ///< Cached screen height for resize detection.

    float m_LightningTimer{0.0f};        ///< Countdown to next lightning flash (s).
    float m_LightningFlashTimer{0.0f};   ///< Remaining flash visibility (s).
    bool m_AuroraVisible{false};         ///< True when current weather wants aurora.
    float m_AuroraFade{1.0f};            ///< Master aurora alpha (transition fade).
    float m_CelestialFade{1.0f};         ///< Sun/moon ray alpha (transition fade).
    float m_MeteorRateMultiplier{1.0f};  ///< Shooting-star spawn rate multiplier.

    /**
     * @struct LightningBolt
     * @brief Procedurally generated bolt path, regenerated on each flash.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Coordinates are screen-space; the bolt is drawn camera-locked (no
     * parallax) for the brief duration of the strike.
     */
    struct LightningBolt
    {
        std::vector<glm::vec2> mainPath;               ///< Top-to-bottom jagged polyline.
        std::vector<std::vector<glm::vec2>> branches;  ///< 0-3 shorter sub-bolts off main.
    };
    LightningBolt m_LightningBolt;
    float m_LightningBoltTimer{
        0.0f};  ///< Remaining bolt visibility (s); slightly longer than the flash.

    static constexpr int RAY_TEXTURE_WIDTH = 64;        ///< Ray texture width (narrow).
    static constexpr int RAY_TEXTURE_HEIGHT = 512;      ///< Ray texture height (tall for length).
    static constexpr int STAR_TEXTURE_SIZE = 64;        ///< Star point texture size.
    static constexpr int STAR_GLOW_TEXTURE_SIZE = 128;  ///< Star glow texture size.
    static constexpr int GLOW_TEXTURE_SIZE = 256;       ///< Atmospheric glow texture size.

    /**
     * @brief Foreground population across a 3-by-2 viewport field.
     *
     * BuildStars uses visibility = GetStarVisibility() * 0.35. foreground and background
     * emission caps are visibility * 0.6 and visibility * 0.4 of their arrays.
     */
    static constexpr int STAR_COUNT = 1800;
    static constexpr int BACKGROUND_STAR_COUNT = 1200;  ///< Number of background stars.
    static constexpr int SUN_RAY_COUNT = 3;      ///< Sun ray count; spread over ~2/3 of the screen.
    static constexpr int MOON_RAY_COUNT = 3;     ///< Number of moon rays (very subtle).
    static constexpr int DEW_SPARKLE_COUNT = 4;  ///< Number of dew sparkles.
    /// Unused; ray length derives from viewport height.
    static constexpr float MAX_RAY_LENGTH = 1200.0f;
    /**
     * @brief Adds LightRay::width * RAY_WIDTH to a 50-pixel sun-ray base.
     *
     * Moon rays use their own 70-pixel multiplier.
     */
    static constexpr float RAY_WIDTH = 80.0f;
    static constexpr float SUN_RAY_SPREAD =
        120.0f;  ///< Total fan spread angle in degrees (~2/3 screen).
    static constexpr float SUN_BAND_WIDTH =
        0.35f;  ///< Width of sun origin band (fraction of screen width).

    /**
     * @brief Fraction of camera motion applied to celestial anchors.
     *
     * 0 is screen-locked; 1 is world-locked. Only the sun and moon constants are read.
     */
    static constexpr float SKY_PARALLAX_STARS_BG = 1.0f;  ///< Unused; see the warning above.
    static constexpr float SKY_PARALLAX_STARS_FG = 1.0f;  ///< Unused; see the warning above.
    static constexpr float SKY_PARALLAX_SUN = 1.0f;       ///< Sun + sun rays (world).
    static constexpr float SKY_PARALLAX_MOON = 1.0f;      ///< Moon + moon rays (world).
    static constexpr float SKY_PARALLAX_AURORA = 1.0f;    ///< Unused; see the warning above.

    /// Horizontal wrap period in viewports.
    static constexpr float STAR_FIELD_X_PERIODS = 3.0f;
    static constexpr float STAR_FIELD_Y_PERIODS = 2.0f;

    void GenerateLightPoolTexture();

    void GenerateSolidTexture();

    void GenerateAuroraCurtainTexture();

    void GenerateAuroraBeamTexture();

    bool m_Initialized;  ///< True after Initialize() completes successfully.
    std::mt19937 m_Rng;  ///< Shared RNG for all procedural generation.
};
