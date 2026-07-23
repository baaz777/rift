#pragma once

#include "Billboard.hpp"
#include "EnumTraits.hpp"
#include "IRenderer.hpp"
#include "Texture.hpp"
#include "TextureHandle.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <random>
#include <vector>

class Tilemap;
class TextureStore;
struct ProjectManifest;
struct WeatherDefinition;
enum class WeatherParticleType;

namespace cameraRig
{
struct RigParams;
}

/**
 * @enum ParticleType
 * @brief Particle behavior identifiers persisted as integers in map JSON; append values without
 * reordering.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
enum class ParticleType
{
    Firefly = 0,         ///< Pulsing yellow-green glow, gentle drift.
    Rain = 1,            ///< Fast falling droplets, slight angle.
    Snow = 2,            ///< Slow falling flakes with side drift.
    Fog = 3,             ///< Large translucent patches, very slow.
    Sparkles = 4,        ///< Brief bright twinkles, stationary.
    Wisp = 5,            ///< Magical spiraling orbs, color variety.
    Lantern = 6,         ///< Warm glow, night-only visibility.
    Sunshine = 7,        ///< Sun rays (day=yellow) / moon beams (night=blue).
    DriftingLeaf = 8,    ///< Ambient: small green/yellow leaf drifting on wind.
    DustMote = 9,        ///< Ambient: tiny golden mote in sunbeams.
    Pollen = 10,         ///< Ambient: yellow pollen during golden hour.
    CherryBlossom = 11,  ///< Weather: drifting pink petals, gentle spiral.
    Ash = 12,            ///< Weather: gray-white particles, slow fall + flutter.
    Ember = 13,          ///< Weather: orange particles rising upward, additive flicker.
    Sand = 14,           ///< Weather: tan-gold particles, fast horizontal wind.

    // Map JSON stores these integer values; append new types without reordering.
    Smoke = 15,          ///< Rising, expanding puffs for chimneys/campfires (wind-bent).
    Steam = 16,          ///< Fast-rising short-lived white vapor (vents, hot springs).
    Aurora = 17,         ///< Soft aurora motes drifting on slow ribbons (night skies).
    Spark = 18,          ///< Energetic darting crackle, brief and bright.
    PixieDust = 19,      ///< Falling glittering trail dust with heavy twinkle.
    Arcane = 20,         ///< Violet glyph motes orbiting their spawn point.
    Enchant = 21,        ///< Rising enchantment glyphs with easing deceleration.
    Runes = 22,          ///< Slow-turning rune sigils with strong glow pulse.
    Hex = 23,            ///< Counter-orbiting witch-magic motes, eerie pulse.
    Curse = 24,          ///< Dark wobbling taint, slow rise, unsettling flicker.
    Void = 25,           ///< Dark matter spiraling inward toward the spawn point.
    Vortex = 26,         ///< Fast circular swirl tightening over lifetime.
    Soul = 27,           ///< Ghostly wisp rising in a slow S-curve wander.
    Fairy = 28,          ///< Darting hover-and-dash glow (quicker than fireflies).
    Butterfly = 29,      ///< Wandering flappy flight, daylight meadows.
    Bat = 30,            ///< Swooping erratic night flier.
    Bubble = 31,         ///< Buoyant wobbling bubble; converts to its pop strip on expiry.
    Coin = 32,           ///< Spinning coin glint (treasure rooms).
    Gem = 33,            ///< Floating gem with periodic sparkle glints.
    Confetti = 34,       ///< Celebration popper: rare bursts of tumbling scraps.
    Heart = 35,          ///< Affection emote floating up with a sway.
    Zap = 36,            ///< Electric arc strobe with positional jitter.
    Wind = 37,           ///< Fast horizontal gust streaks riding the wind.
    Zzz = 38,            ///< Sleep emote drifting up in an easing arc.
    Constellation = 39,  ///< Near-stationary star twinkle (night events).
    Planet = 40,         ///< Very slow drifting celestial body accent.
    Moon = 41,           ///< Stationary crescent accent with soft glow pulse.
    Ink = 42,            ///< Dark blot hovering in place, billowing softly.
    RainSplash = 43,     ///< One-shot 4-frame water splash burst at a rain impact point.
    SnowSplash = 44      ///< One-shot 4-frame snow-impact puff at a snow landing point.
};

template <>
struct EnumTraits<ParticleType> : EnumTraitsBase<ParticleType, EnumTraits<ParticleType>>
{
    static constexpr size_t Count = 45;
    static constexpr std::string_view Names[] = {
        "Firefly",  "Rain",         "Snow",      "Fog",    "Sparkles",      "Wisp",      "Lantern",
        "Sunshine", "DriftingLeaf", "DustMote",  "Pollen", "CherryBlossom", "Ash",       "Ember",
        "Sand",     "Smoke",        "Steam",     "Aurora", "Spark",         "PixieDust", "Arcane",
        "Enchant",  "Runes",        "Hex",       "Curse",  "Void",          "Vortex",    "Soul",
        "Fairy",    "Butterfly",    "Bat",       "Bubble", "Coin",          "Gem",       "Confetti",
        "Heart",    "Zap",          "Wind",      "Zzz",    "Constellation", "Planet",    "Moon",
        "Ink",      "RainSplash",   "SnowSplash"};

    static_assert(std::to_underlying(ParticleType::SnowSplash) == Count - 1,
                  "Update EnumTraits<ParticleType> when adding new ParticleType values");
    static_assert(std::size(Names) == Count,
                  "EnumTraits<ParticleType>::Names must have one entry per enumerator");
};

/**
 * @struct Particle
 * @brief Per-particle state survives removal of its spawning zone.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
struct Particle
{
    glm::vec2 position;  ///< World position (pixels).

    /// Pixels/s; velocity.x also stores CherryBlossom peak alpha or DriftingLeaf/Pollen wind sign.
    glm::vec2 velocity;

    glm::vec4 color;
    float size;      ///< Sprite size in pixels.
    float lifetime;  ///< Remaining life (seconds).
    float maxLifetime;
    float phase;
    float rotation;  ///< Sprite rotation (degrees).

    /// Camera-rebased ground Y for rain/snow splashes; 0 means unset.
    float bakedGroundY{0.0f};

    bool additive;
    bool noProjection;  ///< Render without perspective distortion.
    /**
     * @brief Spawn provenance used by culling and per-type behavior.
     *
     * | value | meaning                                                             |
     * |-------|---------------------------------------------------------------------|
     * | >= 0  | editor zone index; reindexed or culled by OnZoneRemoved.            |
     * | -1    | one-shot or ambient; exempt from orphan cleanup.                    |
     * | -2    | weather; counted by weather caps and culled outside the spawn rect. |
     *
     * Weather provenance also enables type-specific effects: wind-driven Snow, camera-rebased
     * Rain/Snow impacts, weather fog alpha and player avoidance for DriftingLeaf/Pollen.
     * anchorZone preserves a splash's source zone while zoneIndex stays -1.
     */
    int zoneIndex;
    ParticleType type;

    /// Sprite variant chosen at spawn from the atlas entries for this type.
    uint8_t variant{0};

    /**
     * @brief Borrowed zone for splash culling, surface height and lifetime; -1 means none.
     *
     * Reindexed on removal.
     */
    int anchorZone{-1};
};

/**
 * @struct ParticleZone
 * @brief Editor-owned emitter bounds in world pixels; ParticleSystem borrows the zone list.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 */
struct ParticleZone
{
    glm::vec2 position;  ///< Top-left corner (world pixels).
    glm::vec2 size;      ///< Width and height (world pixels).
    ParticleType type;
    bool enabled;
    bool noProjection;

    ParticleZone()
        : position(0.0f),
          size(32.0f),
          type(ParticleType::Firefly),
          enabled(true),
          noProjection(false)
    {
    }
    ParticleZone(glm::vec2 pos, glm::vec2 sz, ParticleType t)
        : position(pos),
          size(sz),
          type(t),
          enabled(true),
          noProjection(false)
    {
    }
};

/**
 * @class ParticleSystem
 * @brief Zone, weather and ambient particle simulation with a shared texture atlas.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup Effects
 *
 * Zone rates scale by area, clamped to 0.5x to 3x; only visible zones spawn.
 * Weather rates use each WeatherDefinition and zoom, with independent accumulators.
 * Transition endpoints share population caps: use the smaller nonzero endpoint cap.
 * Six accumulators cover four transition streams plus two overlay streams.
 *
 * noProjection particles follow ProjectNoProjectionStructurePoint when covered by a structure;
 * Otherwise they use regular projection. draw these particles in a separate batch.
 *
 * Manifest particle links resolve sprite variants. prefer the derived _strip.png sibling;
 * Width / height gives its frame count. non-divisible dimensions use one stretched frame.
 * Missing assets use a procedural circle; Lantern and Sunshine are procedural.
 * Animation loops use global time, while one-shots map frames onto lifetime.
 *
 * Zone rates come from ParticleBehavior::SpawnRate in particles per second before the area
 * multiplier. ambient DriftingLeaf, DustMote and Pollen use ambience spawn constants instead.
 * Weather uses WeatherDefinition::baseSpawnRate; a zone rate does not control storm density.
 *
 * Weather size is fixed from the spawning stream definition, while effects such as
 * fogAlphaMultiplier read the current blended definition. outgoing particles therefore keep their
 * original size through a transition. capped streams retain independent timers so one full
 * population cannot stall another.
 *
 * Variants are rolled at spawn. A type may reserve later variants for runtime states, such as
 * Bubble pop strips. weather culling removes particles far outside the spawn rectangle, which
 * prevents a fast camera move from retaining a distant population.
 * ```mermaid
 * flowchart LR
 *     classDef zone fill:#164e54,stroke:#06b6d4,color:#e2e8f0
 *     classDef system fill:#1e3a5f,stroke:#3b82f6,color:#e2e8f0
 *     classDef particle fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *
 *     Z1[Zone: Firefly]:::zone --> PS[ParticleSystem]:::system
 *     Z2[Zone: Rain]:::zone --> PS
 *     Z3[Zone: Lantern]:::zone --> PS
 *     W[Weather state]:::zone --> PS
 *     A[Ambient emitters]:::zone --> PS
 *     PS --> P1[Particle Pool]:::particle
 *     P1 --> R[Renderer]
 * ```
 *
 * @verbatim
 *   stream               accumulator                    rate weight  size from
 *   -------------------  -----------------------------  ------------ ---------
 *   base primary         m_WeatherSpawnTimer            1            base def
 *   base secondary       m_WeatherSpawnTimerSecondary   1            base def
 *     (both only while no transition is published)
 *
 *   transition out prim  m_WeatherSpawnTimerOut         1 - weight   outgoing
 *   transition out sec   m_WeatherSpawnTimerOutSecondary 1 - weight  outgoing
 *   transition in  prim  m_WeatherSpawnTimer            weight       incoming
 *   transition in  sec   m_WeatherSpawnTimerSecondary   weight       incoming
 *
 *   overlay primary      m_OverlaySpawnTimer            overlayFactor overlay
 *   overlay secondary    m_OverlaySpawnTimerSecondary   overlayFactor overlay
 *     (runs even with no base weather at all)
 * @endverbatim
 *
 * ```mermaid
 * stateDiagram-v2
 *     classDef spawn fill:#134e3a,stroke:#10b981,color:#e2e8f0
 *     classDef active fill:#4a3520,stroke:#f59e0b,color:#e2e8f0
 *     classDef dead fill:#4a2020,stroke:#ef4444,color:#e2e8f0
 *
 *     [*] --> Spawned: Zone visible / weather / ambient / one-shot
 *     Spawned --> Active: Initialize
 *     Active --> Active: Update position/alpha
 *     [*] --> Pending: Emitted by another particle's Update (splash/puff/halo)
 *     Pending --> Active: Merged after the update loop
 *     Active --> Dead: lifetime <= 0
 *     Active --> Dead: Zone deleted
 *     Active --> Dead: Drifted outside spawn rect (weather only)
 *     Dead --> [*]: Remove from pool
 *
 *     class Spawned spawn
 *     class Pending spawn
 *     class Active active
 *     class Dead dead
 * ```
 */
class ParticleSystem
{
public:
    ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = default;
    ParticleSystem& operator=(ParticleSystem&&) noexcept = default;

    /**
     * @fn bool LoadTextures(TextureStore& store, const ProjectManifest& manifest)
     * @brief Build the atlas; missing assets use procedural sprites, so this always returns true.
     * @author Alex (<https://github.com/lextpf>)
     *
     * `store` is borrowed for this system's lifetime and owns the atlas. Its UploadAll
     * re-uploads the atlas after a renderer switch.
     *
     * Set zones and tilemap separately after loading; atlas construction does not bind emitters.
     */
    bool LoadTextures(TextureStore& store, const ProjectManifest& manifest);

    /**
     * @fn void SetZones(const std::vector<ParticleZone>* zones)
     * @brief Borrow the Tilemap zone list; null disables zone spawning.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetZones(const std::vector<ParticleZone>* zones) { m_Zones = zones; }

    /**
     * @fn void SetTileSize(int width, int height)
     * @brief Tile dimensions in pixels; stored but not consumed by projection.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTileSize(int width, int height)
    {
        m_TileWidth = width;
        m_TileHeight = height;
    }

    /**
     * @fn void SetTilemap(const Tilemap* tilemap)
     * @brief Borrow the tilemap for structure projection; null skips structure lookups.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTilemap(const Tilemap* tilemap) { m_Tilemap = tilemap; }

    void SetMaxParticlesPerZone(size_t count) { m_MaxParticlesPerZone = count; }

    /**
     * @fn void SetNightFactor(float factor)
     * @brief Lantern visibility: 0 is day, 1 is full night.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetNightFactor(float factor) { m_NightFactor = factor; }

    /**
     * @fn void SetSceneNightFactor(float factor)
     * @brief Splash darkness: 0 is day, 1 is night.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Use max(natural star visibility, weather star visibility) so precipitation does
     * not make nighttime splashes use daytime alpha.
     */
    void SetSceneNightFactor(float factor) { m_SceneNightFactor = factor; }

    /**
     * @fn void Update(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
     * @brief Advance existing particles and emit replacements; deltaTime is in seconds.
     * @author Alex (<https://github.com/lextpf>)
     *
     * cameraPos is the viewport top-left and viewSize is its extent, both in world pixels. age
     * particles, run per-type behavior, then merge deferred children and remove dead or orphaned
     * entries. rebuild counts before ambient, weather and visible-zone spawning.
     */
    void Update(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize);

    /**
     * @fn void Render(IRenderer& renderer, glm::vec2 cameraPos, bool noProjectionOnly = false, \
     * bool renderAll = true)
     * @brief Draw flat particles with cameraPos at the world-pixel viewport top-left.
     * @author Alex (<https://github.com/lextpf>)
     *
     * renderAll draws both classes and ignores noProjectionOnly. Otherwise true selects
     * noProjection particles and false selects regular particles. Game calls the two classes
     * separately so other layers can draw between them. missing textures fall back to colored
     * rectangles.
     */
    void Render(IRenderer& renderer,
                glm::vec2 cameraPos,
                bool noProjectionOnly = false,
                bool renderAll = true);

    /**
     * @fn void Render3D(IRenderer& renderer, const cameraRig::RigParams& rig)
     * @brief Draw scene-space cards without changing the renderer projection or view size.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Game must publish projection, view size and ambient state first. submit facade decals with
     * DepthMode::TestOnly before ordinary cards with DepthMode::None. within each pass, draw
     * non-additive particles first. ParticleCards defines the anchors and camera orientation.
     */
    void Render3D(IRenderer& renderer, const cameraRig::RigParams& rig);

    const std::vector<Particle>& GetParticles() const { return m_Particles; }

    /**
     * @fn void SetRenderEnabled(bool enabled)
     * @brief Disable draws while simulation continues.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetRenderEnabled(bool enabled) { m_RenderEnabled = enabled; }
    bool IsRenderEnabled() const { return m_RenderEnabled; }

    /**
     * @fn size_t GetLastDrawnCount() const
     * @brief Particles drawn by the last Render or Render3D call; zero when rendering is disabled.
     * @author Alex (<https://github.com/lextpf>)
     */
    size_t GetLastDrawnCount() const { return m_LastDrawnCount; }

    void Clear() { m_Particles.clear(); }

    /**
     * @fn void SpawnOne(ParticleType type, glm::vec2 worldPos)
     * @brief Emit at worldPos in pixels; one emission may create several particles, all tagged
     * zoneIndex = -1.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Spawn initialization samples a 1x1 zone at worldPos, so the position may have sub-pixel
     * jitter. type behavior can emit a burst instead of exactly one particle.
     */
    void SpawnOne(ParticleType type, glm::vec2 worldPos);

    /**
     * @fn void OnZoneRemoved(int zoneIndex)
     * @brief Remove particles tied to this zone and decrement higher zoneIndex and anchorZone
     * values.
     * @author Alex (<https://github.com/lextpf>)
     */
    void OnZoneRemoved(int zoneIndex);

    /**
     * @fn void SetTimeOfDay(float timeOfDay)
     * @brief Hour from 0 to 24 for ambient spawn bias.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetTimeOfDay(float timeOfDay) { m_TimeOfDay = timeOfDay; }

    /**
     * @fn void SetWeatherState(const WeatherDefinition* def, float intensity)
     * @brief Borrow the weather definition through the next Update; null disables weather
     * spawning.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Intensity ranges from 0 to 1 and scales baseSpawnRate.
     */
    void SetWeatherState(const WeatherDefinition* def, float intensity);

    /**
     * @fn void SetWeatherTransition(const WeatherDefinition* outgoing, const WeatherDefinition* \
     * incoming, float weight)
     * @brief Blend four spawn streams when both endpoints and a base weather are present.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Otherwise use the normal base streams. endpoint definitions supply each stream's
     * size and caps; the base definition supplies live-read effects. borrowed definitions
     * must outlive the next Update. weight ranges from 0 to 1 for the incoming endpoint.
     *
     * Outgoing rates use 1 - weight and incoming rates use weight. A missing base definition
     * disables these four streams; an overlay can still spawn independently.
     */
    void SetWeatherTransition(const WeatherDefinition* outgoing,
                              const WeatherDefinition* incoming,
                              float weight);

    /**
     * @fn void SetWeatherOverlay(const WeatherDefinition* def, float factor)
     * @brief Add independent primary and secondary overlay streams.
     * @author Alex (<https://github.com/lextpf>)
     *
     * null disables them; factor ranges from 0 to 1.
     *
     * `def` is borrowed and must remain valid through the next Update, as with SetWeatherState.
     */
    void SetWeatherOverlay(const WeatherDefinition* def, float factor);

    /**
     * @fn void SetWind(glm::vec2 direction, float strength)
     * @brief Normalize direction on use; near-zero retains the previous direction.
     * @author Alex (<https://github.com/lextpf>)
     *
     * clamp strength to at least 0.
     */
    void SetWind(glm::vec2 direction, float strength);

    /**
     * @fn void SetPlayerPosition(glm::vec2 pos)
     * @brief Player feet in world pixels; anchors weather avoidance and the fog height gradient.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SetPlayerPosition(glm::vec2 pos) { m_PlayerPosition = pos; }

private:
    void SpawnParticleInZone(int zoneIndex, const ParticleZone& zone);

    /**
     * @fn void UpdateAmbientSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
     * @brief Ambient caps include zone and weather leaf/dust/pollen populations; deltaTime is in
     * seconds.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Daylight and golden-hour weights control the three ambient rates. counts include matching
     * types from other sources, so editor or weather populations can suppress ambient emission.
     */
    void UpdateAmbientSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize);

    /**
     * @fn void SpawnAmbientParticle(ParticleType type, glm::vec2 cameraPos, glm::vec2 viewSize)
     * @brief Spawn an ambient particle with zoneIndex = -1.
     * @author Alex (<https://github.com/lextpf>)
     */
    void SpawnAmbientParticle(ParticleType type, glm::vec2 cameraPos, glm::vec2 viewSize);

    /**
     * @fn void UpdateWeatherSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
     * @brief Spawn primary and secondary weather streams across the viewport with zoneIndex =
     * WEATHER_ZONE_INDEX.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Base and incoming streams share their two accumulators. outgoing and overlay streams each
     * have a separate primary/secondary pair. rebuild live counts before dispatch so all streams
     * observe the same population cap.
     */
    void UpdateWeatherSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize);

    /**
     * @fn float EffectiveRate(float baseSpawnRate, glm::vec2 viewSize) const
     * @brief Scale by intensity and view area relative to 320x180; clamp the area ratio to 0.25
     * through 4.
     * @author Alex (<https://github.com/lextpf>)
     */
    float EffectiveRate(float baseSpawnRate, glm::vec2 viewSize) const;

    /**
     * @fn void SpawnWeatherType(WeatherParticleType wpt, float effectiveRate, int \
     * maxWeatherParticles, float& spawnTimer, float deltaTime, glm::vec2 cameraPos, glm::vec2 \
     * viewSize, std::array<int, EnumTraits<ParticleType>::Count>& liveByType, const \
     * WeatherDefinition* streamDef)
     * @brief Each stream owns its accumulator; update liveByType as particles spawn and use
     * streamDef for size.
     * @author Alex (<https://github.com/lextpf>)
     *
     * A nonpositive cap disables population limiting. update the liveByType count after each spawn
     * because one initializer can append several particles. deltaTime is in seconds.
     */
    void SpawnWeatherType(WeatherParticleType wpt,
                          float effectiveRate,
                          int maxWeatherParticles,
                          float& spawnTimer,
                          float deltaTime,
                          glm::vec2 cameraPos,
                          glm::vec2 viewSize,
                          std::array<int, EnumTraits<ParticleType>::Count>& liveByType,
                          const WeatherDefinition* streamDef);

    /**
     * @fn void SpawnWeatherParticle(ParticleType type, glm::vec2 cameraPos, glm::vec2 viewSize, \
     * const WeatherDefinition* streamDef)
     * @brief Spawn rain, snow and ash above the view, sand upwind, and fog within it; streamDef
     * supplies size.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Create a temporary zone and reuse the per-type initializer, then apply stream size and
     * weather-specific motion. This keeps sprite and lifetime initialization shared with editor
     * emitters.
     */
    void SpawnWeatherParticle(ParticleType type,
                              glm::vec2 cameraPos,
                              glm::vec2 viewSize,
                              const WeatherDefinition* streamDef);

public:
    /// Weather provenance sentinel used by per-type behavior and population caps.
    static constexpr int WEATHER_ZONE_INDEX = -2;

    /**
     * @brief Reserve room for tiles in Vulkan's shared 10000-quad frame buffer, which silently
     * drops overflow.
     */
    static constexpr size_t MAX_PARTICLE_QUADS_3D = 4000;

private:
    std::vector<Particle> m_Particles;

    /// Defer mid-update spawns until iteration finishes to keep particle references valid.
    std::vector<Particle> m_PendingSpawns;

    const std::vector<ParticleZone>* m_Zones;  ///< Zone list (owned by Tilemap).
    const Tilemap* m_Tilemap;

    int m_TileWidth;
    int m_TileHeight;
    size_t m_MaxParticlesPerZone;
    float m_Time;
    float m_NightFactor;  ///< Day/night factor (0-1) for lanterns.
    float m_SceneNightFactor{0.0f};
    float m_TimeOfDay = 12.0f;  ///< Hour in [0, 24] for ambient spawn biasing.
    std::vector<float> m_ZoneSpawnTimers;
    std::vector<size_t> m_ZoneParticleCounts;

    /// Indexed by ParticleType; only DriftingLeaf, DustMote and Pollen use ambient timers.
    float m_AmbientSpawnTimers[EnumTraits<ParticleType>::Count] = {};

    const WeatherDefinition* m_CurrentWeatherDef{nullptr};
    float m_WeatherIntensity{1.0f};  ///< 0-1 density scalar.
    float m_WeatherSpawnTimer{0.0f};
    float m_WeatherSpawnTimerSecondary{0.0f};
    glm::vec2 m_WindDir{-1.0f, 0.0f};  ///< Prevailing wind direction (normalized on use).
    float m_WindStrength{0.5f};        ///< Gusted wind strength; 0.5 = calm engine default.

    const WeatherDefinition* m_OverlayWeatherDef{nullptr};
    float m_OverlayFactor{0.0f};  ///< Overlay spawn-rate scale (0-1).
    float m_OverlaySpawnTimer{0.0f};
    float m_OverlaySpawnTimerSecondary{0.0f};

    const WeatherDefinition* m_TransitionOut{nullptr};
    const WeatherDefinition* m_TransitionIn{nullptr};
    float m_TransitionWeight{0.0f};  ///< Incoming stream weight [0, 1].
    float m_WeatherSpawnTimerOut{0.0f};
    float m_WeatherSpawnTimerOutSecondary{0.0f};

    /// Camera velocity gates weather avoidance; player position anchors its hitbox region.
    glm::vec2 m_PrevCameraPos{0.0f};
    glm::vec2 m_CameraVelocity{0.0f};
    bool m_HasPrevCameraPos{false};
    glm::vec2 m_PlayerPosition{0.0f};

    std::mt19937 m_Rng;
    std::uniform_real_distribution<float> m_Dist01;  ///< Uniform [0, 1) distribution.

    /// Normalized atlas UV bounds.
    struct AtlasRegion
    {
        glm::vec2 uvMin;
        glm::vec2 uvMax;
    };

    /// Horizontal animation strip; frameCount = 1 is static (64x16 gives four frames).
    struct AtlasSlot
    {
        AtlasRegion region;
        int frameCount{1};
    };

public:
    static constexpr size_t MAX_PARTICLE_VARIANTS = 4;

private:
    TextureStore* m_Store = nullptr;  ///< Owns the adopted atlas (set in LoadTextures).
    TextureHandle m_AtlasHandle;

    /// Only the first m_VariantCounts(type) slots are initialized.
    AtlasSlot m_AtlasSlots[EnumTraits<ParticleType>::Count][MAX_PARTICLE_VARIANTS];

    /// At least 1 before texture loading, so headless spawn variant rolls remain valid.
    uint8_t m_VariantCounts[EnumTraits<ParticleType>::Count];

    bool m_TexturesLoaded;

    /**
     * @fn void AssignSpawnVariants(size_t firstIndex)
     * @brief Assign variants to every particle appended at or after firstIndex.
     * @author Alex (<https://github.com/lextpf>)
     */
    void AssignSpawnVariants(size_t firstIndex);

    /**
     * @fn void BuildAtlas(const ProjectManifest& manifest)
     * @brief Resolve each manifest particle link and pack all type variants into one atlas.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Compute normalized regions after packing; animation strips occupy one region and are sliced
     * when rendered.
     */
    void BuildAtlas(const ProjectManifest& manifest);

    /**
     * @brief Shared draw state cached in reusable vectors to avoid per-frame allocation.
     *
     * Retain classification buffers between frames to avoid repeated allocations. The flat path
     * fills screenPos; the 3D path supplies card placement separately.
     */
    struct ParticleRenderData
    {
        glm::vec2 screenPos;
        glm::vec2 size;
        glm::vec4 color;
        float rotation;
        float phase;
        float lifeT;  ///< Normalized age in [0, 1] for life-mapped strip playback.
        bool additive;
        ParticleType type;
        uint8_t variant;
    };

    /// Atlas frame and world-pixel size after per-type sprite rules.
    struct ParticleSprite
    {
        glm::vec2 renderSize{0.0f};  ///< Quad size in world pixels; x may be negative (Snow).
        glm::vec2 uvMin{0.0f};       ///< Atlas UV of the frame's top-left, quarter-texel inset.
        glm::vec2 uvMax{0.0f};       ///< Atlas UV of the frame's bottom-right.
    };

    /**
     * @fn ParticleRenderData MakeRenderData(const Particle& p) const
     * @brief Leave screenPos at its default; only the flat path supplies it.
     * @author Alex (<https://github.com/lextpf>)
     */
    ParticleRenderData MakeRenderData(const Particle& p) const;

    /**
     * @fn bool ResolveNoProjection(const Particle& p) const
     * @brief A live zone overrides the particle noProjection flag; zoneless particles keep the
     * stored flag.
     * @author Alex (<https://github.com/lextpf>)
     */
    bool ResolveNoProjection(const Particle& p) const;

    /**
     * @fn std::optional<ParticleSprite> ResolveSprite(const ParticleRenderData& data) const
     * @brief Requires loaded textures; return nullopt for an invalid particle type.
     * @author Alex (<https://github.com/lextpf>)
     *
     * Select the frame by global animation time or normalized lifetime, then apply type-specific
     * dimensions. The UV inset prevents adjacent atlas texels from bleeding into the sampled
     * frame.
     */
    std::optional<ParticleSprite> ResolveSprite(const ParticleRenderData& data) const;

    /// 3D card placement with sprite data shared by the flat path.
    struct Particle3DQuad
    {
        ParticleRenderData data;
        glm::vec3 centre{0.0f};
        billboard::Orientation axes;
    };

    std::vector<ParticleRenderData> m_NoProjectionBatch;
    std::vector<ParticleRenderData> m_RegularBatch;

    std::vector<Particle3DQuad> m_FacadeBatch3D;
    std::vector<Particle3DQuad> m_CardBatch3D;

    /// False skips both draw paths and reports zero drawn particles.
    bool m_RenderEnabled = true;
    size_t m_LastDrawnCount = 0;

    /**
     * @fn void GenerateLanternPixels(std::vector<unsigned char>& pixels, int& width, int& \
     * height)
     * @brief Write a 256x256 RGBA glow texture.
     * @author Alex (<https://github.com/lextpf>)
     */
    void GenerateLanternPixels(std::vector<unsigned char>& pixels, int& width, int& height);

    /**
     * @fn void GenerateSunshinePixels(std::vector<unsigned char>& pixels, int& width, int& \
     * height)
     * @brief Write a 48x192 RGBA ray texture.
     * @author Alex (<https://github.com/lextpf>)
     */
    void GenerateSunshinePixels(std::vector<unsigned char>& pixels, int& width, int& height);
};
