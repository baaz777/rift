// Each ParticleType needs ParticleBehavior, EnumTraits, kParticleVisuals and editor-color entries.
// Dispatch tables instantiate every behavior; a missing specialization fails at link time.
// Behaviors receive per-frame or per-spawn contexts instead of accessing ParticleSystem.
// Defer secondary spawns to pendingSpawns so pool iteration remains valid.
//
//     static constexpr float SpawnRate;                    // zone spawns/sec
//     static void Update(Particle&, const ParticleUpdateContext&);
//     static void Spawn(int zoneIndex, const ParticleZone&, ParticleSpawnContext&);

#include "ParticleSystem.hpp"

#include "AmbienceConfig.hpp"
#include "Logger.hpp"
#include "MathConstants.hpp"
#include "ParticleCards.hpp"
#include "ProceduralTexture.hpp"
#include "ProjectManifest.hpp"
#include "TextureStore.hpp"
#include "Tilemap.hpp"
#include "WeatherBlend.hpp"
#include "WeatherDefinitions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <tuple>
#include <utility>

namespace
{
constexpr const char* LOG_SUBSYSTEM = "Particle";

enum class ParticleAnimMode : uint8_t
{
    Loop,       // Cycle frames on global time, offset + rate-jittered per particle.
    LifeMapped  // Play the strip exactly once across the particle's lifetime.
};

// Ground uses per-particle scene depth; Card keeps falling/rising zone motion screen-vertical.
//
// Ground anchors keep stationary types at their own world point. rising or falling types use one
// zone card so their motion remains screen-vertical.
enum class ParticleAnchor : uint8_t
{
    Ground,
    Card
};

// Manifest sprite names; empty variant lists generate procedural sprites.
//
// Logical sprite names resolve through manifest particle links. variants share one animation and
// anchor policy per type; empty lists use procedural sprites.
struct ParticleVisuals
{
    const char* variants[ParticleSystem::MAX_PARTICLE_VARIANTS];
    float animFps;  // Loop playback rate; ignored for LifeMapped strips.
    ParticleAnimMode animMode;

    ParticleAnchor anchor{ParticleAnchor::Ground};
    // 0 rolls all variants; N reserves later entries for runtime states such as bubble pops.
    uint8_t spawnVariantCount{0};
};

// Order must match the ParticleType enum.
constexpr ParticleVisuals kParticleVisuals[] = {
    // Firefly
    {{"firefly", nullptr, nullptr, nullptr}, 7.0f, ParticleAnimMode::Loop},
    // Rain
    {{"rain", "rain2", "rain3", "rain4"}, 0.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Snow
    {{"snow", "snow2", "snow3", nullptr}, 6.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Fog
    {{"fog", "fog2", nullptr, nullptr}, 3.0f, ParticleAnimMode::Loop},
    // Sparkles
    {{"glitter", "glitter2", "glitter3", nullptr}, 0.0f, ParticleAnimMode::LifeMapped},
    // Wisp
    {{"wisp", "wisp2", "wisp3", nullptr}, 7.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Lantern
    {{nullptr, nullptr, nullptr, nullptr}, 0.0f, ParticleAnimMode::Loop},
    // Sunshine
    {{nullptr, nullptr, nullptr, nullptr}, 0.0f, ParticleAnimMode::Loop},
    // DriftingLeaf
    {{"leaf", "leaf2", "leaf3", nullptr}, 6.0f, ParticleAnimMode::Loop},
    // DustMote
    {{"dust", "dust2", "dust3", "mote"}, 5.0f, ParticleAnimMode::Loop},
    // Pollen
    {{"pollen", "pollen2", "pollen3", nullptr}, 6.0f, ParticleAnimMode::Loop},
    // CherryBlossom
    {{"cherryblossom", nullptr, nullptr, nullptr},
     0.0f,
     ParticleAnimMode::Loop,
     ParticleAnchor::Card},
    // Ash
    {{"ash", nullptr, nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Ember
    {{"ember", "ember2", nullptr, nullptr}, 10.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Sand
    {{"sand", nullptr, nullptr, nullptr}, 10.0f, ParticleAnimMode::Loop},
    // Smoke
    {{"smoke", "smoke2", "smoke3", nullptr}, 5.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Steam
    {{"steam", nullptr, nullptr, nullptr}, 8.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Aurora
    {{"aurora", "aurora2", "aurora3", nullptr}, 4.0f, ParticleAnimMode::Loop},
    // Spark
    {{"spark", "spark2", nullptr, nullptr}, 0.0f, ParticleAnimMode::LifeMapped},
    // PixieDust
    {{"pixiedust", "pixiedust2", "pixiedust3", nullptr},
     10.0f,
     ParticleAnimMode::Loop,
     ParticleAnchor::Card},
    // Arcane
    {{"arcane", "arcane2", nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Enchant
    {{"enchant", nullptr, nullptr, nullptr}, 7.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Runes
    {{"runes", nullptr, nullptr, nullptr}, 4.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Hex
    {{"hex", nullptr, nullptr, nullptr}, 5.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Curse
    {{"curse", nullptr, nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Void
    {{"void", nullptr, nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop},
    // Vortex
    {{"vortex", nullptr, nullptr, nullptr}, 10.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Soul
    {{"soul", nullptr, nullptr, nullptr}, 5.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Fairy
    {{"fairy", nullptr, nullptr, nullptr}, 9.0f, ParticleAnimMode::Loop},
    // Butterfly
    {{"butterfly", nullptr, nullptr, nullptr}, 8.0f, ParticleAnimMode::Loop},
    // Bat
    {{"bat", nullptr, nullptr, nullptr}, 10.0f, ParticleAnimMode::Loop},
    // Bubble
    {{"bubble", "bubblepop", nullptr, nullptr},
     4.0f,
     ParticleAnimMode::Loop,
     ParticleAnchor::Card,
     1},
    // Coin
    {{"coin", nullptr, nullptr, nullptr}, 8.0f, ParticleAnimMode::Loop},
    // Gem
    {{"gem", nullptr, nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop},
    // Confetti
    {{"confetti", "confetti2", nullptr, nullptr},
     9.0f,
     ParticleAnimMode::Loop,
     ParticleAnchor::Card},
    // Heart
    {{"heart", nullptr, nullptr, nullptr}, 6.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Zap
    {{"zap", nullptr, nullptr, nullptr}, 14.0f, ParticleAnimMode::Loop},
    // Wind
    {{"wind", nullptr, nullptr, nullptr}, 12.0f, ParticleAnimMode::Loop},
    // Zzz
    {{"zzz", nullptr, nullptr, nullptr}, 3.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // Constellation
    {{"constellation", "constellation2", "constellation3", nullptr}, 4.0f, ParticleAnimMode::Loop},
    // Planet
    {{"planet", nullptr, nullptr, nullptr}, 3.0f, ParticleAnimMode::Loop},
    // Moon
    {{"moon", nullptr, nullptr, nullptr}, 0.0f, ParticleAnimMode::Loop},
    // Ink
    {{"ink", nullptr, nullptr, nullptr}, 0.0f, ParticleAnimMode::Loop, ParticleAnchor::Card},
    // RainSplash
    {{"rainsplash", "rainsplash2", nullptr, nullptr}, 0.0f, ParticleAnimMode::LifeMapped},
    // SnowSplash
    {{"snowsplash", "snowsplash2", nullptr, nullptr}, 0.0f, ParticleAnimMode::LifeMapped},
};

static_assert(std::size(kParticleVisuals) == EnumTraits<ParticleType>::Count,
              "kParticleVisuals must have one row per ParticleType");

// Atlas width in texels, shared by packing and UV inset calculations.
constexpr int kParticleAtlasWidth = 512;
}  // namespace

struct ParticleUpdateContext
{
    float time;
    float deltaTime;
    float nightFactor;
    // Splash darkness includes natural night; weather star visibility is zero during precipitation.
    float sceneNightFactor;
    const std::vector<ParticleZone>* zones;
    bool hasZones;
    // Fog alpha scale; 1 leaves alpha unchanged.
    float fogAlphaMultiplier;
    // Smoothed camera velocity in world px/s; gates weather particle avoidance.
    glm::vec2 cameraVelocity;
    // Camera position in world pixels.
    glm::vec2 cameraPos;
    // World-pixel view extent; clamp oversized zone impacts into this region.
    glm::vec2 viewSize;
    // Player feet in world pixels; anchors the 16x32 avoidance box and fog height gradient.
    glm::vec2 playerPos;
    // Deferred spawn sink; direct pool insertion could invalidate iteration.
    std::vector<Particle>* pendingSpawns;
    // Shared RNG for spawn and update behaviors.
    std::mt19937* rng;
    std::uniform_real_distribution<float>* dist;
    // Normalized wind direction; strength is nonnegative, with 0.5 as calm default.
    glm::vec2 windDir;
    float windStrength;
    // World-pixel camera displacement for rebasing weather impact bands.
    glm::vec2 cameraDelta;
};

struct ParticleSpawnContext
{
    std::mt19937& rng;
    std::uniform_real_distribution<float>& dist;
    std::vector<Particle>& particles;
    glm::vec2 windDir;   // Prevailing wind direction (normalized).
    float windStrength;  // Gusted wind strength (>= 0; 0.5 = calm default).
};

// Two sine octaves per axis; output is approximately -1.5 to 1.5 before caller speed scaling.
inline glm::vec2 FlowNoise(glm::vec2 pos, float time, float phase)
{
    const float x = pos.x * 0.020f;
    const float y = pos.y * 0.020f;
    return {std::sin(time * 0.55f + y * 1.7f + phase) +
                0.5f * std::sin(time * 1.31f + y * 3.1f + phase * 2.3f),
            std::cos(time * 0.47f + x * 1.9f + phase * 1.7f) +
                0.5f * std::cos(time * 1.13f + x * 2.7f + phase * 3.1f)};
}

// Move overlapping weather particles around the 16x32 player box without changing velocity.
inline void ApplyPlayerHitboxRepulsion(Particle& p,
                                       const ParticleUpdateContext& ctx,
                                       float* outProximity = nullptr)
{
    if (outProximity)
        *outProximity = 0.0f;

    // Only a moving camera creates the player wake; a stationary player lets particles pass.
    const float camSpeed = glm::length(ctx.cameraVelocity);
    if (camSpeed <= 5.0f)
    {
        return;
    }

    const float boxMinX = ctx.playerPos.x - 8.0f;
    const float boxMaxX = ctx.playerPos.x + 8.0f;
    const float boxMinY = ctx.playerPos.y - 32.0f;
    const float boxMaxY = ctx.playerPos.y;
    const float nearestX = std::clamp(p.position.x, boxMinX, boxMaxX);
    const float nearestY = std::clamp(p.position.y, boxMinY, boxMaxY);
    const glm::vec2 fromBox = p.position - glm::vec2(nearestX, nearestY);
    const float dist = glm::length(fromBox);

    // Inside the box, push opposite player motion instead of choosing an arbitrary nearest face.
    const glm::vec2 motionDir = ctx.cameraVelocity / camSpeed;
    glm::vec2 outward;
    if (dist > 0.01f)
    {
        outward = fromBox / dist;
    }
    else
    {
        outward = -motionDir;
    }

    // Position-only effects preserve the velocity.x wind-sign convention.
    const float motionFactor = std::clamp(camSpeed / 100.0f, 0.3f, 1.5f);

    // Hard shell: cap outward displacement to the remaining gap.
    constexpr float kHardShellRadius = 6.0f;
    if (dist < kHardShellRadius)
    {
        const float depth = kHardShellRadius - dist;
        constexpr float kHardShellPush = 50.0f;
        const float pushAmount = std::min(kHardShellPush * ctx.deltaTime, depth);
        p.position += outward * pushAmount;
    }

    // Choose the tangent that points behind player motion.
    constexpr float kSwirlOuterRadius = 20.0f;
    constexpr float kSwirlPeak = 40.0f;
    float proximity = 0.0f;
    if (dist < kSwirlOuterRadius)
    {
        proximity = std::clamp(
            (kSwirlOuterRadius - dist) / (kSwirlOuterRadius - kHardShellRadius), 0.0f, 1.0f);
        const glm::vec2 tangentA(outward.y, -outward.x);
        const glm::vec2 tangentB(-outward.y, outward.x);
        const glm::vec2 backward = -motionDir;
        const glm::vec2 tangent = (glm::dot(tangentA, backward) > 0.0f) ? tangentA : tangentB;
        // Phase variance prevents leaves from moving in lockstep.
        const float tangentVariance = 1.0f + 0.35f * std::sin(p.phase * 1.3f);  // [0.65, 1.35]
        p.position +=
            tangent * (kSwirlPeak * tangentVariance * proximity * motionFactor * ctx.deltaTime);
    }
    if (outProximity)
        *outProximity = proximity;

    // Elliptical wake drags nearby trailing particles along player motion.
    constexpr float kWakeOffset = 15.0f;
    constexpr float kWakeHalfLen = 15.0f;
    constexpr float kWakeHalfWidth = 8.0f;
    constexpr float kWakePeak = 30.0f;
    const glm::vec2 wakeCenter = ctx.playerPos - motionDir * kWakeOffset;
    const glm::vec2 perpDir(-motionDir.y, motionDir.x);
    const glm::vec2 wakeOffset = p.position - wakeCenter;
    const float alongN = glm::dot(wakeOffset, motionDir) / kWakeHalfLen;
    const float perpN = glm::dot(wakeOffset, perpDir) / kWakeHalfWidth;
    const float wakeR = std::sqrt(alongN * alongN + perpN * perpN);
    if (wakeR < 1.0f)
    {
        const float wakeFactor = 1.0f - wakeR;
        // Phase varies lateral scatter and drag to prevent a single trailing line.
        const float lateralSign = std::sin(p.phase * 2.0f);                 // [-1, 1]
        const float dragVariance = 1.0f + 0.3f * std::cos(p.phase * 1.7f);  // [0.7, 1.3]
        constexpr float kLateralScatter = 12.0f;
        p.position +=
            motionDir * (kWakePeak * dragVariance * wakeFactor * motionFactor * ctx.deltaTime);
        p.position +=
            perpDir * (lateralSign * kLateralScatter * wakeFactor * motionFactor * ctx.deltaTime);
    }
}

// Match spawn and update alpha; deferred splashes render once before their first update.
inline float ImpactSplashAlpha(float fade, float sceneNight)
{
    // Keep splash alpha below 0.3 by day and 0.15 by night so impacts do not glare over the scene.
    return 0.3f * fade * glm::mix(1.0f, 0.5f, std::clamp(sceneNight, 0.0f, 1.0f));
}

// Sample impacts sparsely so splash sprites do not cover the ground.
constexpr float kRainSplashImpactChance = 0.30f;

// Defer one life-mapped rain splash; variants are assigned when pendingSpawns merges.
inline void SpawnRainSplash(const Particle& parent, float impactY, const ParticleUpdateContext& ctx)
{
    if (!ctx.pendingSpawns || !ctx.rng || !ctx.dist)
        return;
    auto& rng = *ctx.rng;
    auto& dist = *ctx.dist;
    if (dist(rng) > kRainSplashImpactChance)
    {
        return;
    }

    Particle s;
    s.zoneIndex = -1;
    // Borrow parent zone culling and height; weather and ambient splashes remain on the sheet.
    s.anchorZone = (parent.zoneIndex >= 0) ? parent.zoneIndex : parent.anchorZone;
    s.type = ParticleType::RainSplash;
    s.noProjection = parent.noProjection;

    // Keep the impact stationary; the strip supplies spreading motion. jitter X to separate nearby
    // hits.
    s.position = glm::vec2(parent.position.x + (dist(rng) - 0.5f) * 6.0f, impactY);
    s.velocity = glm::vec2(0.0f);

    s.color = glm::vec4(0.82f, 0.88f, 1.0f, ImpactSplashAlpha(1.0f, ctx.sceneNightFactor));
    s.phase = 0.0f;
    s.size = 12.0f + dist(rng) * 4.0f;
    s.lifetime = 0.30f + dist(rng) * 0.10f;
    s.maxLifetime = s.lifetime;
    s.rotation = 0.0f;
    s.bakedGroundY = 0.0f;
    s.additive = false;
    ctx.pendingSpawns->push_back(s);
}

// Sample snow impacts sparsely to avoid covering the ground with puff sprites.
constexpr float kSnowSplashImpactChance = 0.45f;

// Defer one life-mapped snow puff; variants are assigned when pendingSpawns merges.
inline void SpawnSnowPuff(const Particle& parent, float impactY, const ParticleUpdateContext& ctx)
{
    if (!ctx.pendingSpawns || !ctx.rng || !ctx.dist)
        return;
    auto& rng = *ctx.rng;
    auto& dist = *ctx.dist;
    if (dist(rng) > kSnowSplashImpactChance)
    {
        return;
    }

    Particle s;
    s.zoneIndex = -1;
    // Borrow parent zone culling and height; weather and ambient puffs remain on the sheet.
    s.anchorZone = (parent.zoneIndex >= 0) ? parent.zoneIndex : parent.anchorZone;
    s.type = ParticleType::SnowSplash;
    s.noProjection = parent.noProjection;

    // Keep the snow impact stationary; the strip supplies the spread.
    s.position = glm::vec2(parent.position.x + (dist(rng) - 0.5f) * 6.0f, impactY);
    s.velocity = glm::vec2(0.0f);

    s.color = glm::vec4(1.0f, 1.0f, 1.0f, ImpactSplashAlpha(1.0f, ctx.sceneNightFactor));
    s.phase = 0.0f;
    s.size = 10.0f + dist(rng) * 4.0f;
    s.lifetime = 0.40f + dist(rng) * 0.15f;
    s.maxLifetime = s.lifetime;
    s.rotation = 0.0f;
    s.bakedGroundY = 0.0f;
    s.additive = false;
    ctx.pendingSpawns->push_back(s);
}

template <ParticleType PT>
struct ParticleBehavior
{
    static constexpr float SpawnRate = 5.0f;
    static void Update(Particle& p, const ParticleUpdateContext& ctx);
    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx);
};

template <>
struct ParticleBehavior<ParticleType::Firefly>
{
    static constexpr float SpawnRate = 8.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float driftX = std::sin(ctx.time * 2.0f + p.phase) * 10.0f;
        float driftY = std::cos(ctx.time * 1.5f + p.phase * 1.3f) * 8.0f;
        p.position.x += driftX * ctx.deltaTime;
        p.position.y += driftY * ctx.deltaTime;

        float rotationSpeed = 20.0f + (p.phase / 6.28f) * 40.0f;  // 20-60 degrees per second
        if (std::fmod(p.phase, 2.0f) < 1.0f)
            rotationSpeed = -rotationSpeed;
        p.rotation += rotationSpeed * ctx.deltaTime;

        float pulse = 0.5f + 0.5f * std::sin(ctx.time * 4.0f + p.phase);
        float lifeFade = std::min(1.0f, p.lifetime / (p.maxLifetime * 0.3f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.5f);
        p.color.a = pulse * lifeFade * fadeIn * 0.7f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Firefly;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 5.0f;
        p.velocity.y = (ctx.dist(ctx.rng) - 0.5f) * 5.0f;

        float colorChoice = ctx.dist(ctx.rng);
        if (colorChoice < 0.30f)
        {
            p.color = glm::vec4(
                1.0f, 0.9f + ctx.dist(ctx.rng) * 0.1f, 0.3f + ctx.dist(ctx.rng) * 0.2f, 0.0f);
        }
        else if (colorChoice < 0.45f)
        {
            p.color = glm::vec4(
                0.4f + ctx.dist(ctx.rng) * 0.2f, 1.0f, 0.5f + ctx.dist(ctx.rng) * 0.2f, 0.0f);
        }
        else if (colorChoice < 0.60f)
        {
            p.color = glm::vec4(0.4f, 0.8f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.0f);
        }
        else if (colorChoice < 0.75f)
        {
            p.color = glm::vec4(1.0f, 0.4f + ctx.dist(ctx.rng) * 0.15f, 0.8f, 0.0f);
        }
        else if (colorChoice < 0.90f)
        {
            p.color = glm::vec4(1.0f, 0.4f + ctx.dist(ctx.rng) * 0.15f, 0.2f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.8f + ctx.dist(ctx.rng) * 0.15f, 0.4f, 1.0f, 0.0f);
        }

        p.size = 3.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 5.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Rain>
{
    static constexpr float SpawnRate = 25.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.15f);
        // Phase stores target alpha for rain.
        p.color.a = fadeIn * p.phase;

        // Zone rain lands at the bottom edge; oversized zones clamp impacts to the visible area.
        if (ctx.hasZones && p.zoneIndex >= 0 && p.zoneIndex < static_cast<int>(ctx.zones->size()))
        {
            const auto& zone = (*ctx.zones)[p.zoneIndex];

            float heightVariation =
                std::fmod(std::abs(p.position.x * 7.3f + p.phase * 100.0f), 60.0f);
            float groundY = zone.position.y + zone.size.y + 20.0f + heightVariation;
            // Only oversized zones spread impacts across the viewport; normal zones retain their
            // bottom edge.
            if (zone.size.y > ctx.viewSize.y)
            {
                float spreadT = heightVariation / 60.0f;
                float viewSplashY = ctx.cameraPos.y + ctx.viewSize.y * (0.35f + spreadT * 0.60f);
                groundY = std::min(groundY, viewSplashY);
            }
            if (p.position.y > groundY)
            {
                SpawnRainSplash(p, groundY, ctx);
                p.lifetime = 0.0f;
            }
        }

        // Rebase the weather impact band so a moving camera cannot outrun it.
        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX && p.bakedGroundY > 0.0f)
        {
            p.bakedGroundY += ctx.cameraDelta.y;
            if (p.position.y > p.bakedGroundY)
            {
                SpawnRainSplash(p, p.bakedGroundY, ctx);
                p.lifetime = 0.0f;
            }
        }
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Rain;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * 10.0f;

        p.velocity.x = 0.0f;
        p.velocity.y = 150.0f + ctx.dist(ctx.rng) * 100.0f;

        float targetAlpha = 0.6f + ctx.dist(ctx.rng) * 0.15f;
        p.color = glm::vec4(0.8f, 0.85f, 1.0f, 0.0f);
        p.phase = targetAlpha;

        p.size = 10.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 2.0f;
        p.maxLifetime = p.lifetime;
        p.rotation = -35.0f - ctx.dist(ctx.rng) * 30.0f;
        p.additive = true;

        // Weather spawning bakes a per-particle ground band and lifetime later. editor rain keeps
        // the zone edge as its impact height.
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Snow>
{
    static constexpr float SpawnRate = 25.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float drift = std::sin(ctx.time * 1.5f + p.phase) * 20.0f;
        p.position.x += drift * ctx.deltaTime;

        // Weather snow adds 4 hz jitter and a 0.7 hz surge; editor snow keeps smooth drift.
        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            float gust = std::sin(ctx.time * 4.0f + p.phase * 2.3f) * 12.0f +
                         std::sin(ctx.time * 0.7f + p.phase * 0.4f) * 8.0f;
            p.position.x += gust * ctx.deltaTime;
        }

        float rotationSpeed = 30.0f + (p.phase / 6.28f) * 60.0f;  // 30-90 degrees per second
        if (std::fmod(p.phase, 2.0f) < 1.0f)
            rotationSpeed = -rotationSpeed;
        p.rotation += rotationSpeed * ctx.deltaTime;

        // Zone snow impacts its bottom edge, clamped into view for oversized zones.
        if (ctx.hasZones && p.zoneIndex >= 0 && p.zoneIndex < static_cast<int>(ctx.zones->size()))
        {
            const auto& zone = (*ctx.zones)[p.zoneIndex];
            float heightVariation =
                std::fmod(std::abs(p.position.x * 5.7f + p.phase * 80.0f), 60.0f);
            float groundY = zone.position.y + zone.size.y + 20.0f + heightVariation;

            // A whole-map title zone spreads impacts across the view; normal zones keep their own
            // bottom edge.
            if (zone.size.y > ctx.viewSize.y)
            {
                float spreadT = heightVariation / 60.0f;
                float viewSplashY = ctx.cameraPos.y + ctx.viewSize.y * (0.35f + spreadT * 0.60f);
                groundY = std::min(groundY, viewSplashY);
            }
            if (p.position.y > groundY)
            {
                SpawnSnowPuff(p, groundY, ctx);
                p.lifetime = 0.0f;
            }
        }

        // Rebase the weather impact band with camera motion.
        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX && p.bakedGroundY > 0.0f)
        {
            p.bakedGroundY += ctx.cameraDelta.y;
            if (p.position.y > p.bakedGroundY)
            {
                SpawnSnowPuff(p, p.bakedGroundY, ctx);
                p.lifetime = 0.0f;
            }
        }
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Snow;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * 10.0f;

        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 12.0f;
        p.velocity.y = 12.0f + ctx.dist(ctx.rng) * 10.0f;

        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.6f + ctx.dist(ctx.rng) * 0.15f);

        // Keep the base flakes small; SpawnWeatherParticle applies any weather size scale
        // afterwards.
        p.size = 3.0f + ctx.dist(ctx.rng) * 2.5f;
        p.lifetime = 15.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Fog>
{
    static constexpr float SpawnRate = 2.5f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float driftX = std::sin(ctx.time * 0.15f + p.phase) * 2.5f;
        float driftY = std::cos(ctx.time * 0.1f + p.phase * 0.5f) * 1.0f;

        float swirl = std::sin(ctx.time * 0.4f + p.phase * 2.0f) * 1.5f;
        p.position.x += (driftX + swirl) * ctx.deltaTime;
        p.position.y += driftY * ctx.deltaTime;

        float pulse = 0.9f + 0.1f * std::sin(ctx.time * 0.25f + p.phase);

        float lifeFade = std::min(1.0f, p.lifetime / (p.maxLifetime * 0.4f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 4.0f);

        // Reduce alpha at night and keep the daytime boost small to avoid opaque overlapping fog.
        float dayBoost = 1.0f + (1.0f - ctx.nightFactor) * 0.15f;
        float nightReduce = 1.0f - ctx.nightFactor * 0.6f;

        // Cap weather fog softening at 0.65 to prevent transition brightening; other sources use
        // 0.5.
        constexpr float kMaxFogSoftening = 0.65f;
        const float fogMul = (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
                                 ? std::min(ctx.fogAlphaMultiplier, kMaxFogSoftening)
                                 : 0.5f;

        // Anchor the fog height gradient to player feet; retain a 0.3 floor above ground.
        const float groundRefY = ctx.playerPos.y + 40.0f;
        constexpr float kFadeRange = 200.0f;
        const float verticalFactor =
            std::clamp(1.0f - (groundRefY - p.position.y) / kFadeRange, 0.3f, 1.0f);

        // Weather fog reaches its population cap; per-puff alpha then controls visible density.
        constexpr float kBaseAlpha = 0.40f;
        p.color.a = pulse * lifeFade * fadeIn * kBaseAlpha * fogMul * dayBoost * nightReduce *
                    verticalFactor;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Fog;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 3.0f;
        p.velocity.y = (ctx.dist(ctx.rng) - 0.5f) * 1.5f;

        float grey = 0.88f + ctx.dist(ctx.rng) * 0.12f;
        p.color = glm::vec4(grey, grey, grey, 0.0f);

        p.size = 48.0f + ctx.dist(ctx.rng) * 48.0f;

        // Limit lifetime as well as spawn rate: long-lived puffs accumulate alpha even when
        // emission is sparse.
        p.lifetime = 12.0f + ctx.dist(ctx.rng) * 6.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Sparkles>
{
    static constexpr float SpawnRate = 28.0f;

    static void Update(Particle& p, const ParticleUpdateContext&)
    {
        // Life-mapped glitter uses a fast attack and quadratic decay.
        float lifeRatio = 1.0f - (p.lifetime / p.maxLifetime);
        float attack = std::min(1.0f, lifeRatio / 0.12f);
        float decay = 1.0f - lifeRatio;
        p.color.a = attack * decay * decay * 0.85f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Sparkles;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

        p.velocity.x = 0.0f;
        p.velocity.y = 0.0f;

        float hueChoice = ctx.dist(ctx.rng);
        if (hueChoice < 0.25f)
        {
            p.color = glm::vec4(
                1.0f, 0.9f + ctx.dist(ctx.rng) * 0.1f, 0.6f + ctx.dist(ctx.rng) * 0.2f, 1.0f);
        }
        else if (hueChoice < 0.45f)
        {
            p.color = glm::vec4(0.65f + ctx.dist(ctx.rng) * 0.1f, 0.85f, 1.0f, 1.0f);
        }
        else if (hueChoice < 0.60f)
        {
            p.color = glm::vec4(1.0f, 0.65f + ctx.dist(ctx.rng) * 0.1f, 0.9f, 1.0f);
        }
        else if (hueChoice < 0.75f)
        {
            p.color = glm::vec4(0.7f, 1.0f, 0.8f + ctx.dist(ctx.rng) * 0.1f, 1.0f);
        }
        else if (hueChoice < 0.90f)
        {
            p.color = glm::vec4(0.75f + ctx.dist(ctx.rng) * 0.1f, 0.7f, 1.0f, 1.0f);
        }
        else
        {
            p.color = glm::vec4(1.0f, 0.75f + ctx.dist(ctx.rng) * 0.1f, 0.55f, 1.0f);
        }

        // Start at zero alpha to match the attack envelope on the first rendered frame.
        p.color.a = 0.0f;

        p.size = 2.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 0.5f + ctx.dist(ctx.rng) * 0.5f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Wisp>
{
    static constexpr float SpawnRate = 7.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float spiralX = std::sin(ctx.time * 1.5f + p.phase) * 20.0f;
        float spiralY = std::cos(ctx.time * 1.2f + p.phase * 0.7f) * 15.0f;
        float wobble = std::sin(ctx.time * 3.0f + p.phase * 2.0f) * 8.0f;
        p.position.x += (spiralX + wobble) * ctx.deltaTime;
        p.position.y += spiralY * ctx.deltaTime;

        float rotSpeed = 45.0f + (p.phase / 6.28f) * 30.0f;  // 45-75 deg/sec
        if (std::fmod(p.phase, 2.0f) < 1.0f)
            rotSpeed = -rotSpeed;
        p.rotation += rotSpeed * ctx.deltaTime;

        float twinkle = 0.5f + 0.5f * std::sin(ctx.time * 4.0f + p.phase * 3.0f);
        float shimmer = 0.8f + 0.2f * std::sin(ctx.time * 7.0f + p.phase);
        float lifeFade = std::min(1.0f, p.lifetime / (p.maxLifetime * 0.25f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 1.0f);
        p.color.a = twinkle * shimmer * lifeFade * fadeIn * 0.7f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Wisp;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 8.0f;
        p.velocity.y = (ctx.dist(ctx.rng) - 0.5f) * 6.0f - 5.0f;

        float colorChoice = ctx.dist(ctx.rng);
        if (colorChoice < 0.16f)
        {
            p.color = glm::vec4(
                0.5f + ctx.dist(ctx.rng) * 0.2f, 0.75f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.0f);
        }
        else if (colorChoice < 0.32f)
        {
            p.color = glm::vec4(
                0.75f + ctx.dist(ctx.rng) * 0.15f, 0.5f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.0f);
        }
        else if (colorChoice < 0.46f)
        {
            p.color = glm::vec4(
                0.85f + ctx.dist(ctx.rng) * 0.15f, 0.85f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.0f);
        }
        else if (colorChoice < 0.60f)
        {
            p.color = glm::vec4(
                1.0f, 0.5f + ctx.dist(ctx.rng) * 0.2f, 0.85f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else if (colorChoice < 0.72f)
        {
            p.color = glm::vec4(
                0.5f + ctx.dist(ctx.rng) * 0.2f, 1.0f, 0.6f + ctx.dist(ctx.rng) * 0.2f, 0.0f);
        }
        else if (colorChoice < 0.82f)
        {
            p.color = glm::vec4(
                1.0f, 0.7f + ctx.dist(ctx.rng) * 0.15f, 0.4f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else if (colorChoice < 0.92f)
        {
            p.color = glm::vec4(
                1.0f, 0.95f + ctx.dist(ctx.rng) * 0.05f, 0.5f + ctx.dist(ctx.rng) * 0.2f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(
                1.0f, 0.4f + ctx.dist(ctx.rng) * 0.2f, 0.4f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }

        p.size = 3.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = ctx.dist(ctx.rng) * 360.0f;
        p.additive = true;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Lantern>
{
    static constexpr float SpawnRate = 0.5f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        if (ctx.nightFactor < 0.05f)
        {
            p.color.a = 0.0f;
            return;
        }
        float pulse = 0.9f + 0.1f * std::sin(ctx.time * 1.5f + p.phase);
        float flicker = 0.97f + 0.03f * std::sin(ctx.time * 6.0f + p.phase * 2.0f);

        float nightAlpha = ctx.nightFactor * 0.35f;
        p.color.a = pulse * flicker * nightAlpha;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Lantern;
        p.noProjection = zone.noProjection;

        p.position.x = zone.position.x + zone.size.x * 0.5f;
        p.position.y = zone.position.y + zone.size.y * 0.5f;

        p.velocity.x = 0.0f;
        p.velocity.y = 0.0f;

        p.color = glm::vec4(1.0f, 0.85f, 0.6f, 0.5f);

        p.size = std::min(zone.size.x, zone.size.y) * 4.5f;
        p.lifetime = 10.0f + ctx.dist(ctx.rng) * 5.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;

        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Sunshine>
{
    static constexpr float SpawnRate = 1.3f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float shimmer = 0.95f + 0.05f * std::sin(ctx.time * 1.2f + p.phase);
        float flicker = 0.97f + 0.03f * std::sin(ctx.time * 3.0f + p.phase * 1.5f);

        float lifeFade = std::min(1.0f, p.lifetime / (p.maxLifetime * 0.4f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 2.0f);

        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            // Derive the rainbow tier from spawn phase so hues stay fixed throughout each beam
            // lifetime.
            static constexpr glm::vec3 kRainbow[7] = {
                {1.00f, 0.20f, 0.20f},
                {1.00f, 0.55f, 0.15f},
                {1.00f, 0.95f, 0.30f},
                {0.30f, 0.95f, 0.40f},
                {0.30f, 0.85f, 1.00f},
                {0.30f, 0.45f, 1.00f},
                {0.75f, 0.30f, 1.00f},
            };
            const int hue = std::min(6, static_cast<int>(p.phase * (7.0f / 6.2832f)));
            p.color.r = kRainbow[hue].r;
            p.color.g = kRainbow[hue].g;
            p.color.b = kRainbow[hue].b;
        }
        else
        {
            // Editor rays interpolate from daytime gold to nighttime blue.
            float nightBlend = ctx.nightFactor;
            p.color.r = 1.0f * (1.0f - nightBlend) + 0.5f * nightBlend;
            p.color.g = 0.9f * (1.0f - nightBlend) + 0.7f * nightBlend;
            p.color.b = 0.5f * (1.0f - nightBlend) + 1.0f * nightBlend;
        }

        float baseAlpha = 0.16f + (1.0f - ctx.nightFactor) * 0.06f;
        p.color.a = shimmer * flicker * lifeFade * fadeIn * baseAlpha;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        auto pointInRay = [](glm::vec2 point, const Particle& ray) -> bool
        {
            float halfWidth = ray.size * 0.5f;
            float halfHeight = ray.size * 2.0f;

            glm::vec2 local = point - ray.position;

            float radians = glm::radians(-ray.rotation);
            float cosR = std::cos(radians);
            float sinR = std::sin(radians);
            glm::vec2 rotated(local.x * cosR - local.y * sinR, local.x * sinR + local.y * cosR);

            return std::abs(rotated.x) <= halfWidth && std::abs(rotated.y) <= halfHeight;
        };

        auto countRaysAtPoint = [&](glm::vec2 point) -> int
        {
            int count = 0;
            for (const auto& existing : ctx.particles)
            {
                if (existing.type == ParticleType::Sunshine && pointInRay(point, existing))
                    count++;
            }
            return count;
        };

        // Reject a candidate when its coverage would create a third overlapping ray.
        auto wouldOvercrowd = [&](glm::vec2 pos, float rotation, float size) -> bool
        {
            float halfWidth = size * 0.5f;
            float halfHeight = size * 2.0f;
            float radians = glm::radians(rotation);
            float cosR = std::cos(radians);
            float sinR = std::sin(radians);

            const int numSamples = 7;
            for (int i = 0; i < numSamples; i++)
            {
                float t = (i / (float)(numSamples - 1)) - 0.5f;
                float localY = t * halfHeight * 2.0f;

                for (float xOffset : {0.0f, -halfWidth * 0.7f, halfWidth * 0.7f})
                {
                    glm::vec2 sampleWorld(pos.x + xOffset * cosR - localY * sinR,
                                          pos.y + xOffset * sinR + localY * cosR);

                    if (countRaysAtPoint(sampleWorld) >= 2)
                        return true;
                }
            }
            return false;
        };

        for (int attempt = 0; attempt < 3; attempt++)
        {
            Particle p;
            p.zoneIndex = zoneIndex;
            p.type = ParticleType::Sunshine;
            p.noProjection = zone.noProjection;
            p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
            p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

            p.velocity.x = 0.0f;
            p.velocity.y = 0.0f;

            p.color = glm::vec4(1.0f, 0.9f, 0.5f, 0.0f);

            p.size = 40.0f + ctx.dist(ctx.rng) * 24.0f;

            p.lifetime = 5.0f + ctx.dist(ctx.rng) * 4.0f;
            p.maxLifetime = p.lifetime;
            p.phase = ctx.dist(ctx.rng) * 6.28f;

            float baseAngle = (ctx.dist(ctx.rng) < 0.5f) ? -18.0f : 18.0f;
            p.rotation = baseAngle + (ctx.dist(ctx.rng) - 0.5f) * 20.0f;

            p.additive = true;

            if (!wouldOvercrowd(p.position, p.rotation, p.size))
            {
                ctx.particles.push_back(p);
                return;
            }
        }
    }
};

template <>
struct ParticleBehavior<ParticleType::DriftingLeaf>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // velocity.x > 0 reverses wind X for left-edge spawns; ambient uses the global direction.
        glm::vec2 wind = glm::normalize(ctx.windDir);
        if (p.velocity.x > 0.0f)
        {
            wind.x = -wind.x;
        }
        p.position += wind * (18.0f * 2.0f * ctx.windStrength) * ctx.deltaTime;
        p.position.y += std::sin(ctx.time * 1.4f + p.phase) * 6.0f * ctx.deltaTime;

        // Only weather leaves interact with the player avoidance field.
        float proximity = 0.0f;
        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            ApplyPlayerHitboxRepulsion(p, ctx, &proximity);
        }

        // Increase tumble in the wake band so disturbed leaves separate from the background drift.
        p.rotation += 25.0f * (1.0f + 2.0f * proximity) * ctx.deltaTime;

        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.8f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.5f);
        p.color.a = fadeIn * lifeFade * ambience::AMBIENT_PARTICLE_ALPHA_CAP;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::DriftingLeaf;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = ctx.dist(ctx.rng) * 360.0f;
        float leafChoice = ctx.dist(ctx.rng);
        if (leafChoice < 0.10f)
        {
            p.color = glm::vec4(0.35f + ctx.dist(ctx.rng) * 0.20f,
                                0.65f + ctx.dist(ctx.rng) * 0.25f,
                                0.20f + ctx.dist(ctx.rng) * 0.20f,
                                0.0f);
        }
        else if (leafChoice < 0.28f)
        {
            p.color = glm::vec4(0.50f + ctx.dist(ctx.rng) * 0.20f,
                                0.25f + ctx.dist(ctx.rng) * 0.20f,
                                0.10f + ctx.dist(ctx.rng) * 0.15f,
                                0.0f);
        }
        else if (leafChoice < 0.48f)
        {
            p.color = glm::vec4(0.85f + ctx.dist(ctx.rng) * 0.15f,
                                0.65f + ctx.dist(ctx.rng) * 0.20f,
                                0.20f + ctx.dist(ctx.rng) * 0.20f,
                                0.0f);
        }
        else if (leafChoice < 0.63f)
        {
            p.color = glm::vec4(0.85f + ctx.dist(ctx.rng) * 0.15f,
                                0.25f + ctx.dist(ctx.rng) * 0.20f,
                                0.15f + ctx.dist(ctx.rng) * 0.15f,
                                0.0f);
        }
        else if (leafChoice < 0.71f)
        {
            p.color = glm::vec4(0.60f + ctx.dist(ctx.rng) * 0.15f,
                                0.85f + ctx.dist(ctx.rng) * 0.15f,
                                0.30f + ctx.dist(ctx.rng) * 0.15f,
                                0.0f);
        }
        else if (leafChoice < 0.84f)
        {
            p.color = glm::vec4(0.95f + ctx.dist(ctx.rng) * 0.05f,
                                0.50f + ctx.dist(ctx.rng) * 0.15f,
                                0.15f + ctx.dist(ctx.rng) * 0.15f,
                                0.0f);
        }
        else if (leafChoice < 0.93f)
        {
            p.color = glm::vec4(0.55f + ctx.dist(ctx.rng) * 0.20f,
                                0.15f + ctx.dist(ctx.rng) * 0.15f,
                                0.20f + ctx.dist(ctx.rng) * 0.15f,
                                0.0f);
        }
        else
        {
            p.color = glm::vec4(0.75f + ctx.dist(ctx.rng) * 0.15f,
                                0.45f + ctx.dist(ctx.rng) * 0.15f,
                                0.15f + ctx.dist(ctx.rng) * 0.10f,
                                0.0f);
        }
        p.size = 4.5f + ctx.dist(ctx.rng) * 2.5f;
        p.lifetime = 10.0f + ctx.dist(ctx.rng) * 5.0f;
        p.maxLifetime = p.lifetime;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::DustMote>
{
    static constexpr float SpawnRate = 5.5f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 0.6f + p.phase) * 4.0f * ctx.deltaTime;
        p.position.x += std::cos(ctx.time * 0.4f + p.phase * 1.3f) * 3.0f * ctx.deltaTime;

        float twinkle = 0.7f + 0.3f * std::sin(ctx.time * 2.5f + p.phase);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.6f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = fadeIn * lifeFade * twinkle * ambience::AMBIENT_PARTICLE_ALPHA_CAP * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::DustMote;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = ctx.dist(ctx.rng) * 360.0f;
        // Neutral dust uses equal RGB channels; additive blending makes black invisible.
        const float greyChoice = ctx.dist(ctx.rng);
        float grey;
        if (greyChoice < 0.45f)
        {
            grey = 0.92f + ctx.dist(ctx.rng) * 0.08f;
        }
        else if (greyChoice < 0.80f)
        {
            grey = 0.65f + ctx.dist(ctx.rng) * 0.15f;
        }
        else
        {
            grey = 0.40f + ctx.dist(ctx.rng) * 0.15f;
        }
        p.color = glm::vec4(grey, grey, grey, 0.0f);
        p.size = 3.0f + ctx.dist(ctx.rng) * 1.5f;
        p.lifetime = 6.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Pollen>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // velocity.x selects the wind X sign for weather pollen entering from either edge.
        glm::vec2 wind = glm::normalize(ctx.windDir);
        if (p.velocity.x > 0.0f)
        {
            wind.x = -wind.x;
        }
        p.position += wind * (8.0f * 2.0f * ctx.windStrength) * ctx.deltaTime;
        p.position.y += std::sin(ctx.time * 0.9f + p.phase) * 3.0f * ctx.deltaTime;

        // Ambient pollen skips player avoidance.
        if (p.zoneIndex == ParticleSystem::WEATHER_ZONE_INDEX)
        {
            ApplyPlayerHitboxRepulsion(p, ctx);
        }

        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.7f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.2f);
        p.color.a = fadeIn * lifeFade * ambience::AMBIENT_PARTICLE_ALPHA_CAP * 0.9f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Pollen;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = ctx.dist(ctx.rng) * 360.0f;
        // Pollen supplies chromatic motes; DustMote supplies neutral ones.
        float speciesChoice = ctx.dist(ctx.rng);
        if (speciesChoice < 0.26f)
        {
            p.color = glm::vec4(
                1.0f, 0.95f + ctx.dist(ctx.rng) * 0.05f, 0.50f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else if (speciesChoice < 0.48f)
        {
            p.color = glm::vec4(
                1.0f, 0.70f + ctx.dist(ctx.rng) * 0.15f, 0.80f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        }
        else if (speciesChoice < 0.62f)
        {
            p.color = glm::vec4(
                0.80f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.65f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else if (speciesChoice < 0.74f)
        {
            p.color = glm::vec4(
                0.85f + ctx.dist(ctx.rng) * 0.10f, 0.70f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.0f);
        }
        else if (speciesChoice < 0.86f)
        {
            p.color = glm::vec4(
                1.0f, 0.65f + ctx.dist(ctx.rng) * 0.10f, 0.30f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else if (speciesChoice < 0.94f)
        {
            p.color = glm::vec4(
                1.0f, 0.55f + ctx.dist(ctx.rng) * 0.15f, 0.55f + ctx.dist(ctx.rng) * 0.15f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(
                1.0f, 0.35f + ctx.dist(ctx.rng) * 0.15f, 0.85f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        }
        p.size = 3.0f + ctx.dist(ctx.rng) * 1.5f;
        p.lifetime = 7.0f + ctx.dist(ctx.rng) * 5.0f;
        p.maxLifetime = p.lifetime;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::CherryBlossom>
{
    static constexpr float SpawnRate = 12.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Derive flutter amplitude and frequency from phase to keep neighboring blossoms out of
        // step.
        float ampX = 14.0f + 10.0f * std::abs(std::sin(p.phase * 0.7f));
        float ampY = 4.0f + 6.0f * std::abs(std::cos(p.phase * 0.9f));
        float freqX = 0.6f + 0.4f * std::sin(p.phase * 1.3f);
        float freqY = 0.4f + 0.3f * std::cos(p.phase * 1.7f);

        float driftX = std::sin(ctx.time * (0.7f + freqX) + p.phase) * ampX;
        float driftY = std::cos(ctx.time * (0.5f + freqY) + p.phase * 1.4f) * ampY;
        p.position.x += driftX * ctx.deltaTime;

        float fallSpeed = 14.0f + 14.0f * std::abs(std::sin(p.phase * 2.1f));
        p.position.y += (fallSpeed + driftY) * ctx.deltaTime;

        // Per-particle rotation rate (-65..+65 deg/s) derived from phase.
        float rotSpeed = (15.0f + 50.0f * std::abs(std::sin(p.phase * 1.7f))) *
                         (std::cos(p.phase * 0.9f) > 0.0f ? 1.0f : -1.0f);
        p.rotation += rotSpeed * ctx.deltaTime;

        // velocity.x stores peak alpha; the generic position integration still reads it.
        float peak = std::clamp(p.velocity.x, 0.2f, 1.0f);
        float fade = std::min(p.lifetime / 1.2f, (p.maxLifetime - p.lifetime) / 0.7f);
        float shimmer = 0.85f + 0.15f * std::sin(ctx.time * 1.8f + p.phase * 2.3f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * peak * shimmer;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        // Medium and large blossom tiers add a halo, so about 45% of spawns consume two particles.
        float tierRoll = ctx.dist(ctx.rng);

        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::CherryBlossom;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        // velocity.x stores peak alpha; velocity.y remains zero.
        p.velocity = glm::vec2(0.0f);
        p.lifetime = 8.0f + ctx.dist(ctx.rng) * 7.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = ctx.dist(ctx.rng) * 360.0f;
        p.additive = true;

        // Favor saturated pink petals; sparse pale variants preserve contrast without whitening
        // the flurry.
        float hueRoll = ctx.dist(ctx.rng);
        if (hueRoll < 0.40f)
        {
            p.color = glm::vec4(
                1.00f, 0.55f + ctx.dist(ctx.rng) * 0.10f, 0.78f + ctx.dist(ctx.rng) * 0.06f, 0.0f);
        }
        else if (hueRoll < 0.65f)
        {
            p.color = glm::vec4(
                1.00f, 0.40f + ctx.dist(ctx.rng) * 0.10f, 0.70f + ctx.dist(ctx.rng) * 0.08f, 0.0f);
        }
        else if (hueRoll < 0.80f)
        {
            p.color = glm::vec4(
                1.00f, 0.78f + ctx.dist(ctx.rng) * 0.08f, 0.86f + ctx.dist(ctx.rng) * 0.05f, 0.0f);
        }
        else if (hueRoll < 0.88f)
        {
            p.color = glm::vec4(
                1.00f, 0.72f + ctx.dist(ctx.rng) * 0.05f, 0.66f + ctx.dist(ctx.rng) * 0.06f, 0.0f);
        }
        else if (hueRoll < 0.94f)
        {
            p.color = glm::vec4(
                1.00f, 0.28f + ctx.dist(ctx.rng) * 0.10f, 0.45f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        }
        else if (hueRoll < 0.98f)
        {
            p.color = glm::vec4(0.85f + ctx.dist(ctx.rng) * 0.08f,
                                0.50f + ctx.dist(ctx.rng) * 0.08f,
                                0.95f + ctx.dist(ctx.rng) * 0.05f,
                                0.0f);
        }
        else
        {
            p.color = glm::vec4(
                1.00f, 0.92f + ctx.dist(ctx.rng) * 0.05f, 0.94f + ctx.dist(ctx.rng) * 0.04f, 0.0f);
        }

        // Match halo hue to the petal family so bloom preserves the selected color.
        glm::vec4 haloColor = glm::vec4(1.00f, 0.55f, 0.78f, 0.0f);
        if (hueRoll >= 0.88f && hueRoll < 0.94f)
            haloColor = glm::vec4(1.00f, 0.40f, 0.55f, 0.0f);
        else if (hueRoll >= 0.94f && hueRoll < 0.98f)
            haloColor = glm::vec4(0.85f, 0.55f, 1.00f, 0.0f);

        // Append a larger additive halo with a phase offset to separate its shimmer.
        auto appendHalo = [&](const Particle& source, float sizeMul, float peakAlpha)
        {
            Particle halo = source;
            halo.size = source.size * sizeMul;
            halo.velocity.x = peakAlpha;
            halo.color = haloColor;
            halo.phase = source.phase + 0.7f;
            ctx.particles.push_back(halo);
        };

        if (tierRoll < 0.55f)
        {
            p.size = 1.6f + ctx.dist(ctx.rng) * 1.6f;
            p.velocity.x = 0.65f + ctx.dist(ctx.rng) * 0.20f;
            ctx.particles.push_back(p);
        }
        else if (tierRoll < 0.88f)
        {
            p.size = 2.8f + ctx.dist(ctx.rng) * 1.8f;
            p.velocity.x = 0.85f + ctx.dist(ctx.rng) * 0.12f;
            ctx.particles.push_back(p);
            appendHalo(p, 1.7f, 0.15f + ctx.dist(ctx.rng) * 0.08f);
        }
        else
        {
            p.size = 4.5f + ctx.dist(ctx.rng) * 2.5f;
            p.velocity.x = 0.95f + ctx.dist(ctx.rng) * 0.05f;
            ctx.particles.push_back(p);
            appendHalo(p, 2.2f, 0.25f + ctx.dist(ctx.rng) * 0.12f);
        }
    }
};

template <>
struct ParticleBehavior<ParticleType::Ash>
{
    static constexpr float SpawnRate = 5.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float flutter = std::sin(ctx.time * 1.2f + p.phase) * 8.0f;
        p.position.x += flutter * ctx.deltaTime;

        // Wind strength scales ash drift; 0.5 gives about 10 px/s.
        p.position.x += ctx.windDir.x * 20.0f * ctx.windStrength * ctx.deltaTime;

        float fade = std::min(p.lifetime / 2.0f, (p.maxLifetime - p.lifetime) / 1.0f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * 0.75f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Ash;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 0.0f;
        p.velocity.y = 12.0f + ctx.dist(ctx.rng) * 10.0f;
        p.color = glm::vec4(0.70f, 0.70f, 0.72f + ctx.dist(ctx.rng) * 0.05f, 0.0f);
        p.size = 2.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 10.0f + ctx.dist(ctx.rng) * 10.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Ember>
{
    static constexpr float SpawnRate = 7.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float wobble = std::sin(ctx.time * 4.0f + p.phase) * 4.0f;
        p.position.x += wobble * ctx.deltaTime;

        float flicker = 0.6f + 0.4f * std::sin(ctx.time * 12.0f + p.phase * 2.0f);
        float life = std::min(p.lifetime / 0.6f, (p.maxLifetime - p.lifetime) / 0.3f);
        p.color.a = std::clamp(life, 0.0f, 1.0f) * flicker * 0.9f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Ember;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 8.0f;
        p.velocity.y = -(30.0f + ctx.dist(ctx.rng) * 30.0f);
        p.color = glm::vec4(
            0.95f, 0.45f + ctx.dist(ctx.rng) * 0.15f, 0.15f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        p.size = 4.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 1.5f + ctx.dist(ctx.rng) * 1.5f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Sand>
{
    static constexpr float SpawnRate = 25.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        float fade = std::min(p.lifetime / 0.2f, (p.maxLifetime - p.lifetime) / 0.1f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * 0.6f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Sand;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        // Sand always travels +X; weather spawn-edge selection assumes this direction.
        const float windScale = 2.0f * ctx.windStrength;
        p.velocity.x = (100.0f + ctx.dist(ctx.rng) * 100.0f) * windScale;
        p.velocity.y = 10.0f + ctx.dist(ctx.rng) * 15.0f;
        p.color = glm::vec4(0.85f, 0.72f, 0.45f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        p.size = 3.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 0.3f + ctx.dist(ctx.rng) * 0.5f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Rising smoke expands and bends with wind; it does not remain at a fixed ground point.
template <>
struct ParticleBehavior<ParticleType::Smoke>
{
    static constexpr float SpawnRate = 6.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Spawn velocity provides rise; phase noise and wind widen the plume with age.
        const float age = 1.0f - p.lifetime / p.maxLifetime;
        const glm::vec2 flow = FlowNoise(p.position, ctx.time, p.phase);
        p.position.x += (flow.x * (4.0f + 14.0f * age) + ctx.windDir.x * 14.0f * ctx.windStrength) *
                        ctx.deltaTime;
        p.position.y += flow.y * 2.5f * ctx.deltaTime;

        p.size += (3.0f + 2.0f * (0.5f + 0.5f * std::sin(p.phase))) * ctx.deltaTime;

        float fade = std::min(age / 0.12f, (1.0f - age) / 0.45f);
        float waver = 0.9f + 0.1f * std::sin(ctx.time * 0.8f + p.phase);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * waver * 0.5f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Smoke;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 4.0f;
        p.velocity.y = -(14.0f + ctx.dist(ctx.rng) * 10.0f);

        // A warm near-white tint avoids blue smoke over warm scenes; alpha stays below 0.5.
        float grey = 0.82f + ctx.dist(ctx.rng) * 0.13f;
        p.color = glm::vec4(grey * 1.04f, grey, grey * 0.98f, 0.0f);
        p.size = 8.0f + ctx.dist(ctx.rng) * 6.0f;
        p.lifetime = 6.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Steam rises faster and expires sooner than smoke.
template <>
struct ParticleBehavior<ParticleType::Steam>
{
    static constexpr float SpawnRate = 9.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const float age = 1.0f - p.lifetime / p.maxLifetime;
        const glm::vec2 flow = FlowNoise(p.position, ctx.time, p.phase);
        p.position.x += flow.x * (3.0f + 6.0f * age) * ctx.deltaTime;

        p.size += 5.0f * ctx.deltaTime;

        float fade = std::min(age / 0.10f, (1.0f - age) / 0.55f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * 0.45f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Steam;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 6.0f;
        p.velocity.y = -(28.0f + ctx.dist(ctx.rng) * 14.0f);
        float bright = 0.88f + ctx.dist(ctx.rng) * 0.12f;
        p.color = glm::vec4(bright, bright, bright, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 1.8f + ctx.dist(ctx.rng) * 1.2f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Aurora artwork supplies hue variants; time of day does not alter their visibility.

template <>
struct ParticleBehavior<ParticleType::Aurora>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.x += std::sin(ctx.time * 0.30f + p.phase) * 8.0f * ctx.deltaTime;
        p.position.y += std::cos(ctx.time * 0.22f + p.phase * 0.7f) * 4.0f * ctx.deltaTime;

        float wave = 0.55f + 0.45f * std::sin(ctx.time * 0.6f + p.phase * 1.9f);
        float lifeFade = std::min(1.0f, p.lifetime / (p.maxLifetime * 0.30f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 2.0f);
        p.color.a = wave * lifeFade * fadeIn * 0.34f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Aurora;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);

        // Use normal alpha blending so the sky remains visible through colored aurora motes.
        float cast = ctx.dist(ctx.rng);
        if (cast < 0.35f)
        {
            p.color = glm::vec4(0.70f, 0.88f, 0.78f, 0.0f);
        }
        else if (cast < 0.70f)
        {
            p.color = glm::vec4(0.68f, 0.80f, 0.90f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.80f, 0.72f, 0.90f, 0.0f);
        }
        p.size = 5.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 8.0f + ctx.dist(ctx.rng) * 6.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Damp random impulses while the strip plays one lifetime-mapped burst.
template <>
struct ParticleBehavior<ParticleType::Spark>
{
    static constexpr float SpawnRate = 10.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Random impulse scales by sqrt(dt) so random-walk variance is frame-rate independent.
        if (ctx.rng && ctx.dist)
        {
            const float impulse = 76.0f * std::sqrt(ctx.deltaTime);
            p.velocity.x += ((*ctx.dist)(*ctx.rng) - 0.5f) * impulse;
            p.velocity.y += ((*ctx.dist)(*ctx.rng) - 0.5f) * impulse;
        }
        p.velocity *= std::max(0.0f, 1.0f - 3.0f * ctx.deltaTime);

        float lifeRatio = 1.0f - p.lifetime / p.maxLifetime;
        float attack = std::min(1.0f, lifeRatio / 0.08f);
        float decay = 1.0f - lifeRatio;
        p.color.a = attack * decay * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Spark;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 80.0f;
        p.velocity.y = (ctx.dist(ctx.rng) - 0.5f) * 80.0f;

        if (ctx.dist(ctx.rng) < 0.5f)
        {
            p.color = glm::vec4(1.0f, 0.92f + ctx.dist(ctx.rng) * 0.08f, 0.55f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.70f, 0.85f + ctx.dist(ctx.rng) * 0.10f, 1.0f, 0.0f);
        }
        p.size = 3.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 0.4f + ctx.dist(ctx.rng) * 0.5f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Sprite variants supply the glitter color; shared motion adds a falling trail and twinkle.
template <>
struct ParticleBehavior<ParticleType::PixieDust>
{
    static constexpr float SpawnRate = 8.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.x += std::sin(ctx.time * 2.2f + p.phase) * 6.0f * ctx.deltaTime;

        // Let the twinkle reach full alpha so additive blending retains the variant color.
        float twinkle = 0.5f + 0.5f * std::abs(std::sin(ctx.time * 6.0f + p.phase * 2.0f));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.3f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.8f);
        p.color.a = twinkle * fadeIn * lifeFade;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::PixieDust;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 0.0f;
        p.velocity.y = 8.0f + ctx.dist(ctx.rng) * 8.0f;
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);  // Sprite variants supply color.
        p.size = 4.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 2.0f + ctx.dist(ctx.rng) * 2.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Orbit glyphs around the spawn point while moving the orbit upward.
template <>
struct ParticleBehavior<ParticleType::Arcane>
{
    static constexpr float SpawnRate = 5.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Integrate the circle derivative to orbit without storing a center.
        const float radius = 7.0f + 3.0f * std::sin(p.phase * 2.0f);
        const float omega = 1.6f + 0.3f * std::cos(p.phase);
        const float theta = ctx.time * omega + p.phase;
        p.position.x += -std::sin(theta) * radius * omega * ctx.deltaTime;
        p.position.y += (std::cos(theta) * radius * omega - 6.0f) * ctx.deltaTime;

        float pulse = 0.55f + 0.45f * std::sin(ctx.time * 1.1f + p.phase * 1.6f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.6f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = pulse * fadeIn * lifeFade * 0.7f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Arcane;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(
            0.82f + ctx.dist(ctx.rng) * 0.15f, 0.72f + ctx.dist(ctx.rng) * 0.12f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Decelerate the upward launch into a hover; continue a slow rise during fade-out.
template <>
struct ParticleBehavior<ParticleType::Enchant>
{
    static constexpr float SpawnRate = 6.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const float easeRate = std::min(1.0f, 1.4f * ctx.deltaTime);
        p.velocity.y += (-6.0f - p.velocity.y) * easeRate;
        p.position.x += std::sin(ctx.time * 2.4f + p.phase) * 4.0f * ctx.deltaTime;

        float shimmer = 0.85f + 0.15f * std::sin(ctx.time * 5.0f + p.phase * 2.0f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.2f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.9f);
        p.color.a = shimmer * fadeIn * lifeFade * 0.8f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Enchant;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 0.0f;
        p.velocity.y = -(30.0f + ctx.dist(ctx.rng) * 16.0f);
        p.color = glm::vec4(0.75f, 0.95f + ctx.dist(ctx.rng) * 0.05f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 2.5f;
        p.lifetime = 3.0f + ctx.dist(ctx.rng) * 2.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Runes>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y -= 3.0f * ctx.deltaTime;
        float rotSpeed = (std::fmod(p.phase, 2.0f) < 1.0f) ? 12.0f : -12.0f;
        p.rotation += rotSpeed * ctx.deltaTime;

        float pulse = 0.50f + 0.50f * std::sin(ctx.time * 0.8f + p.phase);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.8f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.2f);
        p.color.a = (0.25f + 0.55f * pulse) * fadeIn * lifeFade;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Runes;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(1.0f, 0.80f + ctx.dist(ctx.rng) * 0.10f, 0.45f, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Hex>
{
    static constexpr float SpawnRate = 5.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Reverse the Arcane orbit direction so adjacent effects remain visually distinct.
        const float radius = 9.0f + 3.0f * std::cos(p.phase * 1.4f);
        const float omega = 1.1f + 0.2f * std::sin(p.phase);
        const float theta = -(ctx.time * omega + p.phase);
        p.position.x += -std::sin(theta) * radius * omega * ctx.deltaTime;
        p.position.y += (std::cos(theta) * radius * omega - 3.0f) * ctx.deltaTime;

        float slowPulse = 0.5f + 0.5f * std::sin(ctx.time * 0.9f + p.phase);
        float fastPulse = 0.8f + 0.2f * std::sin(ctx.time * 3.7f + p.phase * 2.0f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.7f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = slowPulse * fastPulse * fadeIn * lifeFade * 0.65f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Hex;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);

        if (ctx.dist(ctx.rng) < 0.5f)
        {
            p.color = glm::vec4(0.60f + ctx.dist(ctx.rng) * 0.15f, 1.0f, 0.50f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.80f + ctx.dist(ctx.rng) * 0.10f, 0.50f, 1.0f, 0.0f);
        }
        p.size = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Dark curse tones require alpha blending; additive would remove the darkening.

template <>
struct ParticleBehavior<ParticleType::Curse>
{
    static constexpr float SpawnRate = 5.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.x += std::sin(ctx.time * 1.3f + p.phase) * 10.0f * ctx.deltaTime;
        p.position.y -= 8.0f * ctx.deltaTime;

        float flicker = (0.70f + 0.30f * std::sin(ctx.time * 6.3f + p.phase * 3.0f)) *
                        (0.75f + 0.25f * std::sin(ctx.time * 1.7f + p.phase));
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.5f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = flicker * fadeIn * lifeFade * 0.75f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Curse;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(
            0.35f + ctx.dist(ctx.rng) * 0.10f, 0.20f, 0.45f + ctx.dist(ctx.rng) * 0.10f, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 3.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Rotate and damp tangential velocity to produce an inward spiral that stalls near its center.
template <>
struct ParticleBehavior<ParticleType::Void>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Rotate and damp velocity for an inward spiral without a stored center.
        const float turn = 2.4f * ctx.deltaTime;
        const float cs = std::cos(turn);
        const float sn = std::sin(turn);
        p.velocity = glm::vec2(p.velocity.x * cs - p.velocity.y * sn,
                               p.velocity.x * sn + p.velocity.y * cs) *
                     std::max(0.0f, 1.0f - 0.55f * ctx.deltaTime);

        p.size = std::max(2.0f, p.size - 1.2f * ctx.deltaTime);

        float age = 1.0f - p.lifetime / p.maxLifetime;
        float fade = std::min(age / 0.25f, (1.0f - age) / 0.30f);
        float pulse = 0.85f + 0.15f * std::sin(ctx.time * 2.2f + p.phase);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * pulse * 0.8f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Void;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;

        float angle = ctx.dist(ctx.rng) * 6.28f;
        float speed = 25.0f + ctx.dist(ctx.rng) * 20.0f;
        p.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed;
        p.color = glm::vec4(0.30f, 0.18f + ctx.dist(ctx.rng) * 0.07f, 0.50f, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 3.0f + ctx.dist(ctx.rng) * 2.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Use a faster rising spiral with the same rotate-and-damp rule as Void.
template <>
struct ParticleBehavior<ParticleType::Vortex>
{
    static constexpr float SpawnRate = 6.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const float turn = 5.5f * ctx.deltaTime;
        const float cs = std::cos(turn);
        const float sn = std::sin(turn);
        p.velocity = glm::vec2(p.velocity.x * cs - p.velocity.y * sn,
                               p.velocity.x * sn + p.velocity.y * cs) *
                     std::max(0.0f, 1.0f - 0.25f * ctx.deltaTime);
        p.position.y -= 8.0f * ctx.deltaTime;

        float age = 1.0f - p.lifetime / p.maxLifetime;
        float fade = std::min(age / 0.15f, (1.0f - age) / 0.25f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * 0.7f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Vortex;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        float angle = ctx.dist(ctx.rng) * 6.28f;
        float speed = 40.0f + ctx.dist(ctx.rng) * 30.0f;
        p.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed;
        p.color = glm::vec4(0.80f, 0.95f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 2.0f + ctx.dist(ctx.rng) * 2.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Soul>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Combine two wander frequencies and a slow speed pulse to vary the rising path.
        p.position.x += (std::sin(ctx.time * 0.8f + p.phase) * 14.0f +
                         std::sin(ctx.time * 2.1f + p.phase * 2.3f) * 4.0f) *
                        ctx.deltaTime;
        float climb = 0.6f + 0.4f * std::sin(ctx.time * 0.5f + p.phase);
        p.position.y += p.velocity.y * (climb - 1.0f) * ctx.deltaTime;

        float breathe = 0.55f + 0.45f * std::sin(ctx.time * 1.4f + p.phase * 1.7f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 1.0f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.5f);
        p.color.a = breathe * fadeIn * lifeFade * 0.65f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Soul;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 0.0f;
        p.velocity.y = -(10.0f + ctx.dist(ctx.rng) * 8.0f);

        if (ctx.dist(ctx.rng) < 0.75f)
        {
            p.color = glm::vec4(0.78f, 0.95f, 1.0f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.80f, 1.0f, 0.85f, 0.0f);
        }
        p.size = 6.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 5.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Alternate tight hovering with short surges along a per-particle direction.
template <>
struct ParticleBehavior<ParticleType::Fairy>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // Cubed positive sine produces short dash windows.
        float gate = std::max(0.0f, std::sin(ctx.time * 0.7f + p.phase));
        gate = gate * gate * gate;
        const float dashDir = (std::sin(p.phase * 3.0f) >= 0.0f) ? 1.0f : -1.0f;

        p.position.x +=
            (std::sin(ctx.time * 2.6f + p.phase) * 18.0f + gate * 70.0f * dashDir) * ctx.deltaTime;
        p.position.y += std::cos(ctx.time * 3.1f + p.phase * 1.3f) * 14.0f * ctx.deltaTime;

        float glow = 0.60f + 0.40f * std::sin(ctx.time * 9.0f + p.phase * 2.0f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.5f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = glow * fadeIn * lifeFade * 0.85f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Fairy;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(1.0f, 0.95f + ctx.dist(ctx.rng) * 0.05f, 0.85f, 0.0f);
        p.size = 4.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Combine cruise motion, wing-beat bob and a slow wander; some spawns emit a pair.
template <>
struct ParticleBehavior<ParticleType::Butterfly>
{
    static constexpr float SpawnRate = 2.5f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += (std::sin(ctx.time * 5.5f + p.phase) * 10.0f +
                         std::cos(ctx.time * 0.6f + p.phase * 0.8f) * 8.0f) *
                        ctx.deltaTime;

        float dayFactor = 1.0f - ctx.nightFactor * 0.4f;
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.6f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.2f);
        p.color.a = fadeIn * lifeFade * dayFactor;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        auto spawnOne = [&](glm::vec2 offset)
        {
            Particle p;
            p.zoneIndex = zoneIndex;
            p.type = ParticleType::Butterfly;
            p.noProjection = zone.noProjection;
            p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x + offset.x;
            p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y + offset.y;
            float dir = (ctx.dist(ctx.rng) < 0.5f) ? -1.0f : 1.0f;
            p.velocity.x = dir * (12.0f + ctx.dist(ctx.rng) * 10.0f);
            p.velocity.y = 0.0f;
            p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
            p.size = 8.0f + ctx.dist(ctx.rng) * 4.0f;
            p.lifetime = 8.0f + ctx.dist(ctx.rng) * 6.0f;
            p.maxLifetime = p.lifetime;
            p.phase = ctx.dist(ctx.rng) * 6.28f;
            p.rotation = 0.0f;
            p.additive = false;
            ctx.particles.push_back(p);
        };
        spawnOne(glm::vec2(0.0f));

        if (ctx.dist(ctx.rng) < 0.25f)
        {
            spawnOne(
                glm::vec2((ctx.dist(ctx.rng) - 0.5f) * 24.0f, (ctx.dist(ctx.rng) - 0.5f) * 16.0f));
        }
    }
};

template <>
struct ParticleBehavior<ParticleType::Bat>
{
    static constexpr float SpawnRate = 2.5f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 2.3f + p.phase) * 38.0f * ctx.deltaTime;
        p.position.x += std::sin(ctx.time * 7.0f + p.phase * 2.0f) * 10.0f * ctx.deltaTime;

        float duskFactor = 0.30f + 0.70f * ctx.nightFactor;
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.5f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.0f);
        p.color.a = fadeIn * lifeFade * duskFactor * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Bat;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        float dir = (ctx.dist(ctx.rng) < 0.5f) ? -1.0f : 1.0f;
        p.velocity.x = dir * (45.0f + ctx.dist(ctx.rng) * 30.0f);
        p.velocity.y = 0.0f;
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 8.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 6.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Variant 0 loops while rising; the final 0.28 seconds use the life-mapped variant 1 pop strip.

template <>
struct ParticleBehavior<ParticleType::Bubble>
{
    static constexpr float SpawnRate = 5.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        if (p.variant == 1)
        {
            p.color.a = (p.lifetime / p.maxLifetime) * 0.9f;
            return;
        }

        p.position.x += std::sin(ctx.time * 3.0f + p.phase) * 7.0f * ctx.deltaTime;
        p.velocity.y -= 3.0f * ctx.deltaTime;

        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.3f);
        p.color.a = fadeIn * 0.55f;

        // Convert within the 0.1 s frame clamp so next-frame aging cannot skip the pop phase.
        const float popWindow = std::max(ctx.deltaTime, 0.1f);
        if (p.lifetime <= popWindow)
        {
            p.variant = 1;
            p.velocity = glm::vec2(0.0f);
            p.color = glm::vec4(0.90f, 0.97f, 1.0f, 0.9f);
            p.size *= 1.15f;
            p.lifetime = 0.28f;
            p.maxLifetime = p.lifetime;
        }
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Bubble;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = (ctx.dist(ctx.rng) - 0.5f) * 8.0f;
        p.velocity.y = -(15.0f + ctx.dist(ctx.rng) * 13.0f);
        p.color = glm::vec4(0.85f, 0.95f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 4.0f;

        p.lifetime = 3.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// The strip supplies the coin spin; position only adds a small vertical bob.
template <>
struct ParticleBehavior<ParticleType::Coin>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 2.0f + p.phase) * 5.0f * ctx.deltaTime;

        float glint = 0.85f + 0.15f * std::sin(ctx.time * 4.0f + p.phase * 2.0f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.3f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.5f);
        p.color.a = glint * fadeIn * lifeFade * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Coin;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 3.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Gem>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 1.3f + p.phase) * 4.0f * ctx.deltaTime;

        float sparkle = 0.75f + 0.25f * std::sin(ctx.time * 4.0f + p.phase * 3.0f);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.4f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.7f);
        p.color.a = sparkle * fadeIn * lifeFade * 0.9f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Gem;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 7.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 4.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// One spawn tick emits a fan of scraps. gravity ends the upward launch before flutter dominates.
template <>
struct ParticleBehavior<ParticleType::Confetti>
{
    // Rate counts whole bursts per second, not individual scraps.
    static constexpr float SpawnRate = 0.4f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.velocity.y = std::min(p.velocity.y + 220.0f * ctx.deltaTime, 40.0f);
        p.velocity.x *= std::max(0.0f, 1.0f - 1.8f * ctx.deltaTime);

        // Delay the wide zigzag until the launch loses speed.
        const float age = 1.0f - p.lifetime / p.maxLifetime;
        const float flutter = std::min(1.0f, age * 2.5f);
        p.position.x += std::sin(ctx.time * 3.3f + p.phase) * 26.0f * flutter * ctx.deltaTime;

        float rotSpeed = 180.0f + (p.phase / 6.28f) * 140.0f;
        if (std::fmod(p.phase, 2.0f) < 1.0f)
        {
            rotSpeed = -rotSpeed;
        }
        p.rotation += rotSpeed * ctx.deltaTime;

        float fadeIn = std::min(1.0f, age / 0.03f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.5f);
        p.color.a = fadeIn * lifeFade * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        // Use one origin for the burst so the pieces read as a single popper.
        const glm::vec2 origin(zone.position.x + ctx.dist(ctx.rng) * zone.size.x,
                               zone.position.y + ctx.dist(ctx.rng) * zone.size.y);
        const int burst = 14 + static_cast<int>(ctx.dist(ctx.rng) * 7.0f);  // 14-20
        for (int i = 0; i < burst; ++i)
        {
            Particle p;
            p.zoneIndex = zoneIndex;
            p.type = ParticleType::Confetti;
            p.noProjection = zone.noProjection;
            p.position = origin;
            // Upward launch cone spans about +/-55 degrees at 90-190 px/s.
            const float spread = (ctx.dist(ctx.rng) - 0.5f) * 1.92f;
            const float speed = 90.0f + ctx.dist(ctx.rng) * 100.0f;
            p.velocity.x = std::sin(spread) * speed;
            p.velocity.y = -std::cos(spread) * speed;
            static constexpr glm::vec3 kTints[6] = {
                {1.00f, 0.35f, 0.40f},
                {0.35f, 0.60f, 1.00f},
                {1.00f, 0.85f, 0.30f},
                {0.40f, 0.90f, 0.50f},
                {1.00f, 0.50f, 0.90f},
                {0.60f, 0.45f, 1.00f},
            };
            const int tint = std::min(5, static_cast<int>(ctx.dist(ctx.rng) * 6.0f));
            p.color = glm::vec4(kTints[tint], 0.0f);
            p.size = 4.0f + ctx.dist(ctx.rng) * 3.0f;
            p.lifetime = 2.2f + ctx.dist(ctx.rng) * 1.3f;
            p.maxLifetime = p.lifetime;
            p.phase = ctx.dist(ctx.rng) * 6.28f;
            p.rotation = ctx.dist(ctx.rng) * 360.0f;
            p.additive = false;
            ctx.particles.push_back(p);
        }
    }
};

// Ease the upward launch into a slow, swaying float.
template <>
struct ParticleBehavior<ParticleType::Heart>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const float easeRate = std::min(1.0f, 2.0f * ctx.deltaTime);
        p.velocity.y += (-8.0f - p.velocity.y) * easeRate;
        p.position.x += std::sin(ctx.time * 2.8f + p.phase) * 6.0f * ctx.deltaTime;
        p.size += 2.0f * ctx.deltaTime;

        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.15f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.6f);
        p.color.a = fadeIn * lifeFade * 0.95f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Heart;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 0.0f;
        p.velocity.y = -(26.0f + ctx.dist(ctx.rng) * 12.0f);
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 6.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 1.6f + ctx.dist(ctx.rng) * 1.2f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// A short lifetime, hard alpha strobe and position jitter produce the arc flash.
template <>
struct ParticleBehavior<ParticleType::Zap>
{
    static constexpr float SpawnRate = 8.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // sqrt(dt) keeps random-walk jitter independent of frame rate.
        if (ctx.rng && ctx.dist)
        {
            const float hop = 20.0f * std::sqrt(ctx.deltaTime);
            p.position.x += ((*ctx.dist)(*ctx.rng) - 0.5f) * hop;
            p.position.y += ((*ctx.dist)(*ctx.rng) - 0.5f) * hop;
        }

        float strobe = (std::sin(ctx.time * 22.0f + p.phase * 7.0f) > 0.2f) ? 0.95f : 0.15f;
        float lifeFade = std::min(1.0f, p.lifetime / 0.1f);
        p.color.a = strobe * lifeFade;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Zap;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(0.75f, 0.85f + ctx.dist(ctx.rng) * 0.10f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 0.35f + ctx.dist(ctx.rng) * 0.35f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

// Wind travels +X to match the weather spawn edge; rendering stretches the travel axis.

template <>
struct ParticleBehavior<ParticleType::Wind>
{
    static constexpr float SpawnRate = 6.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 4.0f + p.phase) * 6.0f * ctx.deltaTime;

        float fade = std::min(p.lifetime / 0.25f, (p.maxLifetime - p.lifetime) / 0.15f);
        p.color.a = std::clamp(fade, 0.0f, 1.0f) * 0.40f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Wind;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        const float windScale = 2.0f * ctx.windStrength;
        p.velocity.x = (110.0f + ctx.dist(ctx.rng) * 120.0f) * windScale;
        p.velocity.y = (ctx.dist(ctx.rng) - 0.5f) * 10.0f;
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 7.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 0.9f + ctx.dist(ctx.rng) * 0.7f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Ease upward and sideways while growing the sleep glyph.
template <>
struct ParticleBehavior<ParticleType::Zzz>
{
    static constexpr float SpawnRate = 2.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const float easeRate = std::min(1.0f, 1.2f * ctx.deltaTime);
        p.velocity.y += (-5.0f - p.velocity.y) * easeRate;
        p.position.x += std::sin(ctx.time * 1.6f + p.phase) * 3.0f * ctx.deltaTime;
        p.size += 2.5f * ctx.deltaTime;

        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.2f);
        float lifeFade = std::min(1.0f, p.lifetime / 0.5f);
        p.color.a = fadeIn * lifeFade * 0.9f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Zzz;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity.x = 6.0f + ctx.dist(ctx.rng) * 6.0f;
        p.velocity.y = -(20.0f + ctx.dist(ctx.rng) * 8.0f);
        p.color = glm::vec4(0.90f, 0.88f, 1.0f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 2.0f;
        p.lifetime = 2.5f + ctx.dist(ctx.rng) * 1.5f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Keep the constellation nearly stationary; night visibility controls the twinkle strength.
template <>
struct ParticleBehavior<ParticleType::Constellation>
{
    static constexpr float SpawnRate = 3.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.x += std::sin(ctx.time * 0.5f + p.phase) * 1.5f * ctx.deltaTime;

        float twinkle = 0.35f + 0.40f * std::abs(std::sin(ctx.time * 2.3f + p.phase * 1.6f));
        float nightBoost = 0.30f + 0.70f * ctx.nightFactor;
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 1.5f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.5f);
        p.color.a = twinkle * nightBoost * fadeIn * lifeFade;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Constellation;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(0.90f, 0.95f, 1.0f, 0.0f);
        p.size = 4.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 6.0f + ctx.dist(ctx.rng) * 6.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = true;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Planet>
{
    static constexpr float SpawnRate = 1.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.x += std::sin(ctx.time * 0.2f + p.phase) * 3.0f * ctx.deltaTime;
        p.position.y += std::cos(ctx.time * 0.15f + p.phase * 0.7f) * 1.5f * ctx.deltaTime;

        float pulse = 0.85f + 0.15f * std::sin(ctx.time * 0.7f + p.phase);
        float nightBoost = 0.25f + 0.75f * ctx.nightFactor;
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 2.0f);
        float lifeFade = std::min(1.0f, p.lifetime / 2.0f);
        p.color.a = pulse * nightBoost * fadeIn * lifeFade * 0.85f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Planet;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        p.size = 8.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 10.0f + ctx.dist(ctx.rng) * 6.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

template <>
struct ParticleBehavior<ParticleType::Moon>
{
    static constexpr float SpawnRate = 0.8f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        p.position.y += std::sin(ctx.time * 0.8f + p.phase) * 2.0f * ctx.deltaTime;

        float pulse = 0.80f + 0.20f * std::sin(ctx.time * 1.1f + p.phase);
        float nightBoost = 0.20f + 0.80f * ctx.nightFactor;
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 2.0f);
        float lifeFade = std::min(1.0f, p.lifetime / 2.0f);
        p.color.a = pulse * nightBoost * fadeIn * lifeFade * 0.9f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Moon;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);

        // Choose among warm, cool and rare lavender moon tints at spawn.
        float lunarRoll = ctx.dist(ctx.rng);
        if (lunarRoll < 0.35f)
        {
            p.color = glm::vec4(1.0f, 0.97f, 0.88f, 0.0f);
        }
        else if (lunarRoll < 0.60f)
        {
            p.color = glm::vec4(0.80f, 0.88f, 1.0f, 0.0f);
        }
        else if (lunarRoll < 0.80f)
        {
            p.color = glm::vec4(1.0f, 0.75f, 0.50f, 0.0f);
        }
        else if (lunarRoll < 0.92f)
        {
            p.color = glm::vec4(1.0f, 0.50f, 0.42f, 0.0f);
        }
        else
        {
            p.color = glm::vec4(0.88f, 0.78f, 1.0f, 0.0f);
        }
        p.size = 8.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 10.0f + ctx.dist(ctx.rng) * 6.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Move the ink blot slowly through the flow field while its size and alpha pulse.
template <>
struct ParticleBehavior<ParticleType::Ink>
{
    static constexpr float SpawnRate = 4.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        const glm::vec2 flow = FlowNoise(p.position, ctx.time, p.phase);
        p.position += flow * 3.0f * ctx.deltaTime;
        p.position.y -= 3.0f * ctx.deltaTime;

        float pulse = 0.75f + 0.25f * std::sin(ctx.time * 0.9f + p.phase);
        float fadeIn = std::min(1.0f, (p.maxLifetime - p.lifetime) / 0.8f);
        float lifeFade = std::min(1.0f, p.lifetime / 1.2f);
        p.color.a = pulse * fadeIn * lifeFade * 0.85f;
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::Ink;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(0.12f, 0.12f, 0.18f + ctx.dist(ctx.rng) * 0.05f, 0.0f);
        p.size = 5.0f + ctx.dist(ctx.rng) * 3.0f;
        p.lifetime = 4.0f + ctx.dist(ctx.rng) * 3.0f;
        p.maxLifetime = p.lifetime;
        p.phase = ctx.dist(ctx.rng) * 6.28f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Rain updates defer splash emission; Spawn also supports zones and console one-shots.

template <>
struct ParticleBehavior<ParticleType::RainSplash>
{
    static constexpr float SpawnRate = 8.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // The strip supplies splash motion; fade the final third of life to hide the last-frame
        // removal.
        const float lifeRatio = 1.0f - (p.lifetime / p.maxLifetime);  // 0 -> 1
        const float fade = (lifeRatio < 0.66f) ? 1.0f : std::max(0.0f, (1.0f - lifeRatio) / 0.34f);

        // Dim the impact against night backgrounds even when another weather overlay is active.
        p.color.a = ImpactSplashAlpha(fade, ctx.sceneNightFactor);
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::RainSplash;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(
            0.82f, 0.88f, 1.0f, 0.0f);  // The next Update replaces this alpha using scene darkness.
        p.size = 12.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 0.30f + ctx.dist(ctx.rng) * 0.10f;
        p.maxLifetime = p.lifetime;
        p.phase = 0.0f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// Snow updates defer puff emission; Spawn also supports zones and console one-shots.

template <>
struct ParticleBehavior<ParticleType::SnowSplash>
{
    static constexpr float SpawnRate = 8.0f;

    static void Update(Particle& p, const ParticleUpdateContext& ctx)
    {
        // The strip supplies puff motion; fade the final third of life to hide the last-frame
        // removal.
        const float lifeRatio = 1.0f - (p.lifetime / p.maxLifetime);  // 0 -> 1
        const float fade = (lifeRatio < 0.66f) ? 1.0f : std::max(0.0f, (1.0f - lifeRatio) / 0.34f);

        // Use scene night visibility so overlays cannot brighten a nighttime snow impact.
        p.color.a = ImpactSplashAlpha(fade, ctx.sceneNightFactor);
    }

    static void Spawn(int zoneIndex, const ParticleZone& zone, ParticleSpawnContext& ctx)
    {
        Particle p;
        p.zoneIndex = zoneIndex;
        p.type = ParticleType::SnowSplash;
        p.noProjection = zone.noProjection;
        p.position.x = zone.position.x + ctx.dist(ctx.rng) * zone.size.x;
        p.position.y = zone.position.y + ctx.dist(ctx.rng) * zone.size.y;
        p.velocity = glm::vec2(0.0f);
        p.color = glm::vec4(
            1.0f, 1.0f, 1.0f, 0.0f);  // The next Update replaces this alpha using scene darkness.
        p.size = 10.0f + ctx.dist(ctx.rng) * 4.0f;
        p.lifetime = 0.40f + ctx.dist(ctx.rng) * 0.15f;
        p.maxLifetime = p.lifetime;
        p.phase = 0.0f;
        p.rotation = 0.0f;
        p.additive = false;
        ctx.particles.push_back(p);
    }
};

// One table entry per ParticleType dispatches its specialized update and spawn functions.
using UpdateFn = void (*)(Particle&, const ParticleUpdateContext&);
using SpawnFn = void (*)(int, const ParticleZone&, ParticleSpawnContext&);

namespace
{

template <size_t... Is>
constexpr auto MakeSpawnRateTable(std::index_sequence<Is...>)
{
    return std::array<float, sizeof...(Is)>{
        ParticleBehavior<static_cast<ParticleType>(Is)>::SpawnRate...};
}

template <size_t... Is>
auto MakeUpdateTable(std::index_sequence<Is...>)
{
    return std::array<UpdateFn, sizeof...(Is)>{
        &ParticleBehavior<static_cast<ParticleType>(Is)>::Update...};
}

template <size_t... Is>
auto MakeSpawnTable(std::index_sequence<Is...>)
{
    return std::array<SpawnFn, sizeof...(Is)>{
        &ParticleBehavior<static_cast<ParticleType>(Is)>::Spawn...};
}

using Indices = std::make_index_sequence<EnumTraits<ParticleType>::Count>;

constexpr auto kSpawnRates = MakeSpawnRateTable(Indices{});
const auto kUpdateDispatch = MakeUpdateTable(Indices{});
const auto kSpawnDispatch = MakeSpawnTable(Indices{});

}  // namespace

ParticleSystem::ParticleSystem()
    : m_Zones(nullptr),
      m_Tilemap(nullptr),
      m_TileWidth(32),
      m_TileHeight(32),
      m_MaxParticlesPerZone(25),
      m_Time(0.0f),
      m_NightFactor(0.0f),
      m_Rng(std::random_device{}()),
      m_Dist01(0.0f, 1.0f),
      m_TexturesLoaded(false)
{
    // Reserve for calm scenes; heavy weather can grow the pool.
    m_Particles.reserve(1000);

    // At least one variant keeps headless spawn rolls valid before textures load.
    std::fill(std::begin(m_VariantCounts), std::end(m_VariantCounts), uint8_t{1});
}

bool ParticleSystem::LoadTextures(TextureStore& store, const ProjectManifest& manifest)
{
    m_Store = &store;
    BuildAtlas(manifest);
    m_TexturesLoaded = true;
    return true;
}

void ParticleSystem::BuildAtlas(const ProjectManifest& manifest)
{
    // Pack all variants into one 512-pixel-wide atlas to keep the particle pass on one texture.
    // Prefer horizontal strips, then static images, then a procedural fallback.
    struct TextureSource
    {
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
        int frameCount = 1;
        int typeIndex = 0;
        int variantIndex = 0;
    };

    // Normalize loaded pixels to RGBA before atlas copies; failure selects the next fallback.
    auto loadPng = [](const char* path, TextureSource& src) -> bool
    {
        Texture temp;
        if (temp.LoadFromFile(path))
        {
            src.width = temp.GetWidth();
            src.height = temp.GetHeight();
            int channels = temp.GetChannels();
            size_t pixelCount =
                static_cast<size_t>(temp.GetWidth()) * static_cast<size_t>(temp.GetHeight());

            if (channels == 4)
            {
                size_t dataSize = pixelCount * 4;
                src.pixels.resize(dataSize);
                if (!temp.GetImageData().empty())
                {
                    memcpy(src.pixels.data(), temp.GetImageData().data(), dataSize);
                }
                return true;
            }
            if (channels == 3 && !temp.GetImageData().empty())
            {
                src.pixels.resize(pixelCount * 4);
                const unsigned char* srcPx = temp.GetImageData().data();
                unsigned char* dst = src.pixels.data();
                for (size_t px = 0; px < pixelCount; ++px)
                {
                    dst[px * 4 + 0] = srcPx[px * 3 + 0];
                    dst[px * 4 + 1] = srcPx[px * 3 + 1];
                    dst[px * 4 + 2] = srcPx[px * 3 + 2];
                    dst[px * 4 + 3] = 255;
                }
                return true;
            }
        }
        return false;
    };

    // A radial alpha fallback avoids opaque squares when assets are missing.
    auto generateSoftCircle = [](TextureSource& src, int size, float falloffPow)
    {
        src.width = size;
        src.height = size;
        GeneratePixels(src.pixels,
                       size,
                       size,
                       [falloffPow](int x, int y, int w, int h) -> Pixel
                       {
                           float cx = w * 0.5f;
                           float cy = h * 0.5f;
                           float dx = (x - cx) / cx;
                           float dy = (y - cy) / cy;
                           float dist = std::sqrt(dx * dx + dy * dy);
                           float a = std::pow(std::max(0.0f, 1.0f - dist), falloffPow);
                           auto alpha = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
                           return Pixel{255, 255, 255, alpha};
                       });
    };

    // Raise source alpha peaks below 0.5 to about 0.9; behavior alpha controls final faintness.
    auto normalizeFaintAlpha = [](TextureSource& src)
    {
        uint8_t peak = 0;
        for (size_t i = 3; i < src.pixels.size(); i += 4)
        {
            peak = std::max(peak, src.pixels[i]);
        }
        if (peak == 0 || peak >= 128)
        {
            return;
        }
        const float scale = 230.0f / static_cast<float>(peak);
        for (size_t i = 3; i < src.pixels.size(); i += 4)
        {
            src.pixels[i] = static_cast<unsigned char>(
                std::min(255.0f, static_cast<float>(src.pixels[i]) * scale));
        }
    };

    // Collect one source per declared variant; procedural-only types still receive a valid atlas
    // slot.
    std::vector<TextureSource> sources;
    sources.reserve(EnumTraits<ParticleType>::Count * 2);
    for (size_t t = 0; t < EnumTraits<ParticleType>::Count; ++t)
    {
        int variantCount = 0;
        for (const char* base : kParticleVisuals[t].variants)
        {
            if (base == nullptr)
            {
                break;
            }
            TextureSource src;
            src.typeIndex = static_cast<int>(t);
            src.variantIndex = variantCount;

            // Derive static and _strip candidates from either manifest-linked filename.
            std::string stripPath;
            std::string staticPath;
            if (const auto link = manifest.particleSprites.find(base);
                link != manifest.particleSprites.end())
            {
                std::string stem = manifest.ResolvePathString(link->second);
                if (stem.ends_with(".png"))
                {
                    stem.resize(stem.size() - 4);
                }
                if (stem.ends_with("_strip"))
                {
                    stem.resize(stem.size() - 6);
                }
                stripPath = stem + "_strip.png";
                staticPath = stem + ".png";
            }
            else
            {
                Logger::ErrorF(LOG_SUBSYSTEM,
                               "Project manifest has no \"particles\" link for '{}'; using "
                               "soft-circle fallback",
                               base);
            }

            // Check optional strips before loading to avoid expected missing-file error logs.
            const bool stripLoaded = !stripPath.empty() && std::filesystem::exists(stripPath) &&
                                     loadPng(stripPath.c_str(), src);
            if (stripLoaded)
            {
                // 64x16 yields four horizontal frames; invalid ratios use one stretched frame.
                if (src.height > 0 && src.width > src.height && src.width % src.height == 0)
                {
                    src.frameCount = src.width / src.height;
                }
            }
            else if (!(!staticPath.empty() && std::filesystem::exists(staticPath) &&
                       loadPng(staticPath.c_str(), src)))
            {
                if (!staticPath.empty())
                {
                    Logger::ErrorF(LOG_SUBSYSTEM,
                                   "Missing particle asset for '{}' (tried {} and {}); using "
                                   "soft-circle fallback",
                                   base,
                                   stripPath,
                                   staticPath);
                }
                generateSoftCircle(src, 16, 1.5f);
            }
            // Reject sources wider than the atlas; otherwise uvs would clip silently.
            if (src.width > kParticleAtlasWidth)
            {
                Logger::ErrorF(LOG_SUBSYSTEM,
                               "Particle asset '{}' is {}px wide (atlas width is {}); using "
                               "soft-circle fallback",
                               base,
                               src.width,
                               kParticleAtlasWidth);
                src.pixels.clear();
                src.frameCount = 1;
                generateSoftCircle(src, 16, 1.5f);
            }
            normalizeFaintAlpha(src);
            sources.push_back(std::move(src));
            ++variantCount;
        }
        if (variantCount == 0)
        {
            TextureSource src;
            src.typeIndex = static_cast<int>(t);
            src.variantIndex = 0;
            switch (static_cast<ParticleType>(t))
            {
                case ParticleType::Lantern:
                    GenerateLanternPixels(src.pixels, src.width, src.height);
                    break;
                case ParticleType::Sunshine:
                    GenerateSunshinePixels(src.pixels, src.width, src.height);
                    break;
                default:
                    generateSoftCircle(src, 16, 1.5f);
                    break;
            }
            sources.push_back(std::move(src));
            variantCount = 1;
        }
        m_VariantCounts[t] = static_cast<uint8_t>(
            std::min<int>(variantCount, static_cast<int>(MAX_PARTICLE_VARIANTS)));
    }

    // Premeasure row packing so the atlas height fits every source.
    const int atlasWidth = kParticleAtlasWidth;
    int requiredHeight = 0;
    {
        int scanX = 0;
        int scanRowHeight = 0;
        for (const TextureSource& src : sources)
        {
            if (scanX + src.width > atlasWidth)
            {
                scanX = 0;
                requiredHeight += scanRowHeight + 1;
                scanRowHeight = 0;
            }
            scanX += src.width + 1;
            if (src.height > scanRowHeight)
            {
                scanRowHeight = src.height;
            }
        }
        requiredHeight += scanRowHeight;
    }
    const int atlasHeight = std::max(512, requiredHeight);
    std::vector<unsigned char> atlasPixels(atlasWidth * atlasHeight * 4, 0);

    int currentX = 0;
    int currentY = 0;
    int rowHeight = 0;

    for (const TextureSource& source : sources)
    {
        const int w = source.width;
        const int h = source.height;
        AtlasSlot& slot = m_AtlasSlots[source.typeIndex][source.variantIndex];
        slot.frameCount = source.frameCount;

        if (currentX + w > atlasWidth)
        {
            currentX = 0;
            currentY += rowHeight + 1;  // 1px padding
            rowHeight = 0;
        }

        if (currentY + h > atlasHeight)
        {
            Logger::ErrorF(LOG_SUBSYSTEM,
                           "Atlas overflow: type {} variant {} ({}x{}) does not fit at row {} "
                           "(atlas height={})",
                           source.typeIndex,
                           source.variantIndex,
                           w,
                           h,
                           currentY,
                           atlasHeight);
            // Degenerate uvs sample a corner pixel for a source that does not fit.
            slot.region.uvMin = glm::vec2(0.0f);
            slot.region.uvMax = glm::vec2(1.0f / atlasWidth, 1.0f / atlasHeight);
            slot.frameCount = 1;
            continue;
        }

        slot.region.uvMin = glm::vec2(static_cast<float>(currentX) / atlasWidth,
                                      static_cast<float>(currentY) / atlasHeight);
        slot.region.uvMax = glm::vec2(static_cast<float>(currentX + w) / atlasWidth,
                                      static_cast<float>(currentY + h) / atlasHeight);

        // File pixels are bottom-up; procedural pixels are top-down. The unflipped atlas retains
        // both conventions.
        for (int y = 0; y < h; y++)
        {
            int srcY = y;
            int dstY = currentY + y;
            if (dstY >= atlasHeight)
                continue;

            for (int x = 0; x < w; x++)
            {
                int dstX = currentX + x;
                if (dstX >= atlasWidth)
                    continue;

                int srcIdx = (srcY * w + x) * 4;
                int dstIdx = (dstY * atlasWidth + dstX) * 4;

                if (srcIdx + 3 < static_cast<int>(source.pixels.size()))
                {
                    atlasPixels[dstIdx + 0] = source.pixels[srcIdx + 0];
                    atlasPixels[dstIdx + 1] = source.pixels[srcIdx + 1];
                    atlasPixels[dstIdx + 2] = source.pixels[srcIdx + 2];
                    atlasPixels[dstIdx + 3] = source.pixels[srcIdx + 3];
                }
            }
        }

        currentX += w + 1;  // 1px padding
        rowHeight = std::max(rowHeight, h);
    }

    Texture atlas;
    atlas.LoadFromData(atlasPixels.data(), atlasWidth, atlasHeight, 4, false);
    m_AtlasHandle = m_Store->Adopt(std::move(atlas));

    Logger::InfoF(LOG_SUBSYSTEM,
                  "Atlas built: {}x{} ({} sprites across {} types)",
                  atlasWidth,
                  atlasHeight,
                  sources.size(),
                  EnumTraits<ParticleType>::Count);
}

void ParticleSystem::GenerateLanternPixels(std::vector<unsigned char>& pixels,
                                           int& width,
                                           int& height)
{
    width = 256;
    height = 256;
    GeneratePixels(pixels,
                   width,
                   height,
                   [](int x, int y, int w, int) -> Pixel
                   {
                       float center = w / 2.0f;
                       float dx = x - center;
                       float dy = y - center;
                       float dist = std::sqrt(dx * dx + dy * dy) / center;

                       float alpha = std::exp(-dist * dist * 1.2f);
                       float centerReduction = std::exp(-dist * dist * 8.0f) * 0.3f;
                       alpha = alpha * (1.0f - centerReduction);

                       if (dist > 0.6f)
                       {
                           float outerFade = 1.0f - (dist - 0.6f) / 0.4f;
                           outerFade = std::max(0.0f, outerFade);
                           outerFade = std::pow(outerFade, 0.4f);
                           alpha *= outerFade;
                       }

                       return {255,
                               static_cast<uint8_t>(220 + alpha * 35),
                               static_cast<uint8_t>(160 + alpha * 50),
                               static_cast<uint8_t>(alpha * 120)};
                   });
}

void ParticleSystem::GenerateSunshinePixels(std::vector<unsigned char>& pixels,
                                            int& width,
                                            int& height)
{
    width = 48;
    height = 192;
    GeneratePixels(pixels,
                   width,
                   height,
                   [](int x, int y, int w, int h) -> Pixel
                   {
                       float centerX = w / 2.0f;
                       float dx = std::abs(x - centerX) / centerX;
                       float dy = static_cast<float>(y) / static_cast<float>(h);

                       float beamWidth = 0.2f + dy * 0.55f;
                       float horizontalFalloff = 1.0f - std::min(1.0f, dx / beamWidth);
                       horizontalFalloff = std::pow(horizontalFalloff, 1.2f);
                       horizontalFalloff *= std::exp(-dx * dx * 1.5f);

                       float topFeather = std::min(1.0f, dy / 0.30f);
                       topFeather = std::pow(topFeather, 2.0f);
                       float bottomFeather = std::min(1.0f, (1.0f - dy) / 0.30f);
                       bottomFeather = std::pow(bottomFeather, 2.0f);

                       float verticalIntensity = 0.5f + 0.5f * std::sin(dy * rift::PiF);
                       float beamAlpha =
                           horizontalFalloff * verticalIntensity * topFeather * bottomFeather;

                       float groundGlowY = 1.0f - std::abs(dy - 0.78f) / 0.15f;
                       groundGlowY = std::max(0.0f, groundGlowY);
                       float groundGlowX = std::exp(-dx * dx * 1.5f);
                       float groundGlow = groundGlowY * groundGlowX * 0.35f * bottomFeather;

                       float alpha = std::min(1.0f, beamAlpha + groundGlow);

                       return {255, 255, 255, static_cast<uint8_t>(alpha * 140)};
                   });
}

void ParticleSystem::Update(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
{
    m_Time += deltaTime;
    const bool hasZones = (m_Zones && !m_Zones->empty());

    if (hasZones && m_ZoneSpawnTimers.size() < m_Zones->size())
    {
        m_ZoneSpawnTimers.resize(m_Zones->size(), 0.0f);
    }

    // Seed camera position on the first update to avoid a velocity spike from the origin.
    if (!m_HasPrevCameraPos)
    {
        m_PrevCameraPos = cameraPos;
        m_HasPrevCameraPos = true;
    }
    glm::vec2 rawCamDelta = cameraPos - m_PrevCameraPos;
    // Jumps beyond the viewport are scene cuts; do not rebase existing impact bands.
    if (std::abs(rawCamDelta.x) > viewSize.x || std::abs(rawCamDelta.y) > viewSize.y)
    {
        rawCamDelta = glm::vec2(0.0f);
    }
    const glm::vec2 rawCamVel = (deltaTime > 1e-4f) ? rawCamDelta / deltaTime : glm::vec2(0.0f);
    m_CameraVelocity = glm::mix(m_CameraVelocity, rawCamVel, 0.25f);
    m_PrevCameraPos = cameraPos;

    const float fogAlphaMul = m_CurrentWeatherDef ? m_CurrentWeatherDef->fogAlphaMultiplier : 1.0f;
    m_PendingSpawns.clear();
    const ParticleUpdateContext updateCtx{m_Time,
                                          deltaTime,
                                          m_NightFactor,
                                          m_SceneNightFactor,
                                          m_Zones,
                                          hasZones,
                                          fogAlphaMul,
                                          m_CameraVelocity,
                                          cameraPos,
                                          viewSize,
                                          m_PlayerPosition,
                                          &m_PendingSpawns,
                                          &m_Rng,
                                          &m_Dist01,
                                          m_WindDir,
                                          m_WindStrength,
                                          rawCamDelta};

    for (auto& p : m_Particles)
    {
        p.lifetime -= deltaTime;
        if (p.lifetime <= 0.0f)
        {
            continue;
        }

        // Nonnegative indices require a live zone; -1 deliberately has no zone.
        if (p.zoneIndex >= 0 && (!hasZones || p.zoneIndex >= static_cast<int>(m_Zones->size())))
        {
            p.lifetime = 0.0f;
            continue;
        }

        p.position += p.velocity * deltaTime;

        // Cull weather beyond overspray plus half a viewport to avoid stale particles after camera
        // motion.
        if (p.zoneIndex == WEATHER_ZONE_INDEX)
        {
            constexpr float kSpawnOverspray = 0.20f;
            const float marginX = viewSize.x * 0.5f;
            const float marginY = viewSize.y * 0.5f;
            const float spawnLeft = cameraPos.x - viewSize.x * kSpawnOverspray - marginX;
            const float spawnRight = cameraPos.x + viewSize.x * (1.0f + kSpawnOverspray) + marginX;
            const float spawnTop = cameraPos.y - viewSize.y * kSpawnOverspray - marginY;
            const float spawnBottom = cameraPos.y + viewSize.y * (1.0f + kSpawnOverspray) + marginY;
            if (p.position.x < spawnLeft || p.position.x > spawnRight || p.position.y < spawnTop ||
                p.position.y > spawnBottom)
            {
                p.lifetime = 0.0f;
                continue;
            }
        }

        int typeIndex = static_cast<int>(p.type);
        if (typeIndex >= 0 && typeIndex < static_cast<int>(kUpdateDispatch.size()))
        {
            kUpdateDispatch[typeIndex](p, updateCtx);
        }
        else
        {
            p.lifetime = 0.0f;
        }
    }

    // Merge deferred spawns and assign variants before removing dead particles.
    if (!m_PendingSpawns.empty())
    {
        const size_t firstMerged = m_Particles.size();
        m_Particles.insert(m_Particles.end(), m_PendingSpawns.begin(), m_PendingSpawns.end());
        AssignSpawnVariants(firstMerged);
        m_PendingSpawns.clear();
    }

    std::erase_if(m_Particles, [](const Particle& p) { return p.lifetime <= 0.0f; });

    if (hasZones)
    {
        m_ZoneParticleCounts.assign(m_Zones->size(), 0);
        for (const auto& p : m_Particles)
        {
            if (p.zoneIndex >= 0 && p.zoneIndex < static_cast<int>(m_ZoneParticleCounts.size()))
            {
                m_ZoneParticleCounts[p.zoneIndex]++;
            }
        }
    }

    UpdateAmbientSpawning(deltaTime, cameraPos, viewSize);

    UpdateWeatherSpawning(deltaTime, cameraPos, viewSize);

    if (!hasZones)
    {
        return;
    }

    for (size_t i = 0; i < m_Zones->size(); ++i)
    {
        const ParticleZone& zone = (*m_Zones)[i];
        if (!zone.enabled)
        {
            continue;
        }

        float margin = 80.0f;  // 80 world pixels of offscreen spawn margin.
        bool visible = !(zone.position.x + zone.size.x < cameraPos.x - margin ||
                         zone.position.x > cameraPos.x + viewSize.x + margin ||
                         zone.position.y + zone.size.y < cameraPos.y - margin ||
                         zone.position.y > cameraPos.y + viewSize.y + margin);

        if (!visible)
        {
            continue;
        }

        // Skip day lanterns to avoid flicker.
        if (zone.type == ParticleType::Lantern && m_NightFactor < 0.05f)
        {
            continue;
        }

        size_t zoneParticleCount = m_ZoneParticleCounts[i];

        int zoneTypeIndex = static_cast<int>(zone.type);
        if (zoneTypeIndex < 0 || zoneTypeIndex >= static_cast<int>(kSpawnRates.size()))
        {
            continue;
        }
        float spawnRate = kSpawnRates[zoneTypeIndex];

        float areaFactor = (zone.size.x * zone.size.y) / (64.0f * 64.0f);
        spawnRate *= std::max(0.5f, std::min(3.0f, areaFactor));

        m_ZoneSpawnTimers[i] += deltaTime;
        float spawnInterval = 1.0f / spawnRate;

        while (m_ZoneSpawnTimers[i] >= spawnInterval && zoneParticleCount < m_MaxParticlesPerZone)
        {
            m_ZoneSpawnTimers[i] -= spawnInterval;
            SpawnParticleInZone(static_cast<int>(i), zone);
            zoneParticleCount++;
        }
    }
}

void ParticleSystem::SpawnParticleInZone(int zoneIndex, const ParticleZone& zone)
{
    int typeIndex = static_cast<int>(zone.type);
    if (typeIndex < 0 || typeIndex >= static_cast<int>(kSpawnDispatch.size()))
    {
        return;
    }
    ParticleSpawnContext ctx{m_Rng, m_Dist01, m_Particles, m_WindDir, m_WindStrength};
    const size_t before = m_Particles.size();
    kSpawnDispatch[typeIndex](zoneIndex, zone, ctx);
    AssignSpawnVariants(before);
}

void ParticleSystem::AssignSpawnVariants(size_t firstIndex)
{
    for (size_t i = firstIndex; i < m_Particles.size(); ++i)
    {
        Particle& p = m_Particles[i];
        const auto typeIndex = static_cast<size_t>(p.type);
        if (typeIndex >= EnumTraits<ParticleType>::Count)
        {
            continue;
        }
        // Restrict spawn rolls to leading variants reserved for initial behavior states.
        int count = std::max<int>(1, m_VariantCounts[typeIndex]);
        const uint8_t spawnCount = kParticleVisuals[typeIndex].spawnVariantCount;
        if (spawnCount > 0)
        {
            count = std::min<int>(count, spawnCount);
        }
        p.variant = (count > 1) ? static_cast<uint8_t>(std::min<int>(
                                      count - 1, static_cast<int>(m_Dist01(m_Rng) * count)))
                                : uint8_t{0};
    }
}

namespace
{
// Periodic 24-hour bump; width is its half-width in hours.
float TimeOfDayBump(float timeOfDay, float center, float width)
{
    // Wrap-aware shortest distance on a 24h circle.
    float diff = std::abs(timeOfDay - center);
    if (diff > 12.0f)
    {
        diff = 24.0f - diff;
    }
    if (diff >= width)
    {
        return 0.0f;
    }
    float t = 1.0f - diff / width;
    return t * t * (3.0f - 2.0f * t);
}
}  // namespace

void ParticleSystem::UpdateAmbientSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
{
    int totalAmbient = 0;
    int countLeaf = 0, countDust = 0, countPollen = 0;
    for (const auto& p : m_Particles)
    {
        switch (p.type)
        {
            case ParticleType::DriftingLeaf:
                ++countLeaf;
                ++totalAmbient;
                break;
            case ParticleType::DustMote:
                ++countDust;
                ++totalAmbient;
                break;
            case ParticleType::Pollen:
                ++countPollen;
                ++totalAmbient;
                break;
            default:
                break;
        }
    }
    if (totalAmbient >= ambience::AMBIENT_PARTICLE_TOTAL_CAP)
    {
        return;
    }

    // Time-of-day biasing. Each curve peaks at 1.0 at the named hour.
    // Leaves: any daylight (peak midday, half-strength dawn/dusk).
    // Dust motes: dawn/midday sunbeams.
    // Pollen: golden hour only (dawn ~6h or dusk ~19h).
    float leafBias = TimeOfDayBump(m_TimeOfDay, 13.0f, 8.0f);
    float dustBias =
        std::max(TimeOfDayBump(m_TimeOfDay, 6.5f, 2.5f), TimeOfDayBump(m_TimeOfDay, 12.0f, 4.0f));
    float pollenBias =
        std::max(TimeOfDayBump(m_TimeOfDay, 6.5f, 1.5f), TimeOfDayBump(m_TimeOfDay, 19.0f, 1.5f));

    auto tickType = [&](ParticleType type, float ratePerSec, float bias)
    {
        int idx = static_cast<int>(type);
        m_AmbientSpawnTimers[idx] += deltaTime * std::max(0.0f, bias);
        float interval = (ratePerSec > 0.0f) ? (1.0f / ratePerSec) : 1e9f;
        while (m_AmbientSpawnTimers[idx] >= interval &&
               totalAmbient < ambience::AMBIENT_PARTICLE_TOTAL_CAP)
        {
            m_AmbientSpawnTimers[idx] -= interval;
            SpawnAmbientParticle(type, cameraPos, viewSize);
            ++totalAmbient;
        }
    };

    tickType(ParticleType::DriftingLeaf, ambience::AMBIENT_LEAF_SPAWN_PER_SEC, leafBias);
    tickType(ParticleType::DustMote, ambience::AMBIENT_DUST_SPAWN_PER_SEC, dustBias);
    tickType(ParticleType::Pollen, ambience::AMBIENT_POLLEN_SPAWN_PER_SEC, pollenBias);

    (void)countLeaf;
    (void)countDust;
    (void)countPollen;
}

void ParticleSystem::SpawnAmbientParticle(ParticleType type,
                                          glm::vec2 cameraPos,
                                          glm::vec2 viewSize)
{
    // Reuse the zone initializer over a viewport rectangle. zoneIndex -1 exempts ambient particles
    // from zone removal and per-zone caps.
    const float margin = ambience::AMBIENT_PARTICLE_SPAWN_MARGIN;
    ParticleZone fakeZone;
    fakeZone.position = cameraPos - glm::vec2(margin);
    fakeZone.size = viewSize + glm::vec2(margin * 2.0f);
    fakeZone.type = type;
    fakeZone.enabled = true;
    fakeZone.noProjection = false;
    SpawnParticleInZone(-1, fakeZone);
}

void ParticleSystem::SpawnOne(ParticleType type, glm::vec2 worldPos)
{
    // A 1x1 temporary zone preserves the type initializer and adds only sub-pixel position jitter.
    ParticleZone fakeZone;
    fakeZone.position = worldPos;
    fakeZone.size = glm::vec2(1.0f, 1.0f);
    fakeZone.type = type;
    fakeZone.enabled = true;
    fakeZone.noProjection = false;
    SpawnParticleInZone(-1, fakeZone);
}

void ParticleSystem::SetWeatherState(const WeatherDefinition* def, float intensity)
{
    m_CurrentWeatherDef = def;
    m_WeatherIntensity = std::clamp(intensity, 0.0f, 1.0f);
}

void ParticleSystem::SetWeatherTransition(const WeatherDefinition* outgoing,
                                          const WeatherDefinition* incoming,
                                          float weight)
{
    m_TransitionOut = outgoing;
    m_TransitionIn = incoming;
    m_TransitionWeight = std::clamp(weight, 0.0f, 1.0f);
}

void ParticleSystem::SetWeatherOverlay(const WeatherDefinition* def, float factor)
{
    m_OverlayWeatherDef = def;
    m_OverlayFactor = std::clamp(factor, 0.0f, 1.0f);
}

void ParticleSystem::SetWind(glm::vec2 direction, float strength)
{
    if (glm::length(direction) > 1e-4f)
    {
        m_WindDir = glm::normalize(direction);
    }
    m_WindStrength = std::max(0.0f, strength);
}

namespace
{
// Map WeatherParticleType -> concrete ParticleType. returns nullopt for None.
std::optional<ParticleType> ResolveWeatherParticle(WeatherParticleType wpt)
{
    switch (wpt)
    {
        case WeatherParticleType::None:
            return std::nullopt;
        case WeatherParticleType::Rain:
            return ParticleType::Rain;
        case WeatherParticleType::Snow:
            return ParticleType::Snow;
        case WeatherParticleType::Fog:
            return ParticleType::Fog;
        case WeatherParticleType::Leaf:
            return ParticleType::DriftingLeaf;
        case WeatherParticleType::Blossom:
            return ParticleType::CherryBlossom;
        case WeatherParticleType::Pollen:
            return ParticleType::Pollen;
        case WeatherParticleType::Ash:
            return ParticleType::Ash;
        case WeatherParticleType::Ember:
            return ParticleType::Ember;
        case WeatherParticleType::Sand:
            return ParticleType::Sand;
        case WeatherParticleType::Firefly:
            return ParticleType::Firefly;
        case WeatherParticleType::Wisp:
            return ParticleType::Wisp;
        case WeatherParticleType::Sunshine:
            return ParticleType::Sunshine;
        case WeatherParticleType::Smoke:
            return ParticleType::Smoke;
        case WeatherParticleType::Zap:
            return ParticleType::Zap;
        case WeatherParticleType::Wind:
            return ParticleType::Wind;
        case WeatherParticleType::Aurora:
            return ParticleType::Aurora;
        case WeatherParticleType::Constellation:
            return ParticleType::Constellation;
    }
    return std::nullopt;
}
}  // namespace

void ParticleSystem::UpdateWeatherSpawning(float deltaTime, glm::vec2 cameraPos, glm::vec2 viewSize)
{
    // Count each weather type once per frame, then increment its count for every spawn.
    std::array<int, EnumTraits<ParticleType>::Count> liveByType{};
    for (const auto& p : m_Particles)
    {
        const auto idx = static_cast<size_t>(p.type);
        if (p.zoneIndex == WEATHER_ZONE_INDEX && idx < liveByType.size())
        {
            ++liveByType[idx];
        }
    }

    if (m_CurrentWeatherDef != nullptr)
    {
        if (m_TransitionOut != nullptr && m_TransitionIn != nullptr)
        {
            // Transition streams use endpoint sizes and caps; the blended definition supplies
            // live-read effects.

            // Shared endpoint types use the smaller nonzero cap so neither stream overfills the
            // common population.
            const auto streamCap =
                [](const WeatherDefinition& other, WeatherParticleType type, int ownSlotCap)
            {
                const int otherCap = WeatherCapForType(other, type);
                if (otherCap == 0)
                {
                    return ownSlotCap;
                }
                if (ownSlotCap == 0)
                {
                    return otherCap;
                }
                return std::min(ownSlotCap, otherCap);
            };
            const WeatherDefinition& out = *m_TransitionOut;
            const WeatherDefinition& in = *m_TransitionIn;
            const float outWeight = 1.0f - m_TransitionWeight;
            SpawnWeatherType(out.particleType,
                             EffectiveRate(out.baseSpawnRate, viewSize) * outWeight,
                             streamCap(in, out.particleType, out.maxWeatherParticles),
                             m_WeatherSpawnTimerOut,
                             deltaTime,
                             cameraPos,
                             viewSize,
                             liveByType,
                             m_TransitionOut);
            SpawnWeatherType(
                out.secondaryParticleType,
                EffectiveRate(out.secondaryBaseSpawnRate, viewSize) * outWeight,
                streamCap(in, out.secondaryParticleType, out.secondaryMaxWeatherParticles),
                m_WeatherSpawnTimerOutSecondary,
                deltaTime,
                cameraPos,
                viewSize,
                liveByType,
                m_TransitionOut);
            SpawnWeatherType(in.particleType,
                             EffectiveRate(in.baseSpawnRate, viewSize) * m_TransitionWeight,
                             streamCap(out, in.particleType, in.maxWeatherParticles),
                             m_WeatherSpawnTimer,
                             deltaTime,
                             cameraPos,
                             viewSize,
                             liveByType,
                             m_TransitionIn);
            SpawnWeatherType(
                in.secondaryParticleType,
                EffectiveRate(in.secondaryBaseSpawnRate, viewSize) * m_TransitionWeight,
                streamCap(out, in.secondaryParticleType, in.secondaryMaxWeatherParticles),
                m_WeatherSpawnTimerSecondary,
                deltaTime,
                cameraPos,
                viewSize,
                liveByType,
                m_TransitionIn);
        }
        else
        {
            // Without a transition, the base definition supplies both stream sizes and spawn
            // rates.
            SpawnWeatherType(m_CurrentWeatherDef->particleType,
                             EffectiveRate(m_CurrentWeatherDef->baseSpawnRate, viewSize),
                             m_CurrentWeatherDef->maxWeatherParticles,
                             m_WeatherSpawnTimer,
                             deltaTime,
                             cameraPos,
                             viewSize,
                             liveByType,
                             m_CurrentWeatherDef);
            SpawnWeatherType(m_CurrentWeatherDef->secondaryParticleType,
                             EffectiveRate(m_CurrentWeatherDef->secondaryBaseSpawnRate, viewSize),
                             m_CurrentWeatherDef->secondaryMaxWeatherParticles,
                             m_WeatherSpawnTimerSecondary,
                             deltaTime,
                             cameraPos,
                             viewSize,
                             liveByType,
                             m_CurrentWeatherDef);
        }
    }

    // Overlay streams also run without base weather.
    if (m_OverlayWeatherDef != nullptr && m_OverlayFactor > 0.0f)
    {
        SpawnWeatherType(
            m_OverlayWeatherDef->particleType,
            EffectiveRate(m_OverlayWeatherDef->baseSpawnRate, viewSize) * m_OverlayFactor,
            m_OverlayWeatherDef->maxWeatherParticles,
            m_OverlaySpawnTimer,
            deltaTime,
            cameraPos,
            viewSize,
            liveByType,
            m_OverlayWeatherDef);
        SpawnWeatherType(
            m_OverlayWeatherDef->secondaryParticleType,
            EffectiveRate(m_OverlayWeatherDef->secondaryBaseSpawnRate, viewSize) * m_OverlayFactor,
            m_OverlayWeatherDef->secondaryMaxWeatherParticles,
            m_OverlaySpawnTimerSecondary,
            deltaTime,
            cameraPos,
            viewSize,
            liveByType,
            m_OverlayWeatherDef);
    }
}

float ParticleSystem::EffectiveRate(float baseSpawnRate, glm::vec2 viewSize) const
{
    // Scale density by visible area relative to 320x180, clamped to 0.25 through 4.
    constexpr float kReferenceArea = 320.0f * 180.0f;
    const float visibleArea = std::max(1.0f, viewSize.x * viewSize.y);
    const float zoomScale = std::clamp(visibleArea / kReferenceArea, 0.25f, 4.0f);
    return baseSpawnRate * m_WeatherIntensity * zoomScale;
}

void ParticleSystem::SpawnWeatherType(WeatherParticleType wpt,
                                      float effectiveRate,
                                      int maxWeatherParticles,
                                      float& spawnTimer,
                                      float deltaTime,
                                      glm::vec2 cameraPos,
                                      glm::vec2 viewSize,
                                      std::array<int, EnumTraits<ParticleType>::Count>& liveByType,
                                      const WeatherDefinition* streamDef)
{
    auto particleTypeOpt = ResolveWeatherParticle(wpt);
    if (!particleTypeOpt.has_value())
    {
        return;
    }
    if (effectiveRate <= 0.0f)
    {
        return;
    }

    // Each type uses its own shared live count; primary and secondary types do not compete.
    int& live = liveByType[static_cast<size_t>(*particleTypeOpt)];
    if (maxWeatherParticles > 0 && live >= maxWeatherParticles)
    {
        spawnTimer = 0.0f;
        return;
    }

    spawnTimer += deltaTime;
    float interval = 1.0f / effectiveRate;
    while (spawnTimer >= interval)
    {
        spawnTimer -= interval;
        const size_t before = m_Particles.size();
        SpawnWeatherParticle(*particleTypeOpt, cameraPos, viewSize, streamDef);
        live += static_cast<int>(m_Particles.size() - before);
        if (maxWeatherParticles > 0 && live >= maxWeatherParticles)
        {
            break;
        }
    }
}

void ParticleSystem::SpawnWeatherParticle(ParticleType type,
                                          glm::vec2 cameraPos,
                                          glm::vec2 viewSize,
                                          const WeatherDefinition* streamDef)
{
    // Spawn rect: viewport with 20% overspray. bias by particle type.
    const float overspray = 0.20f;
    glm::vec2 rectPos = cameraPos - viewSize * overspray;
    glm::vec2 rectSize = viewSize * (1.0f + 2.0f * overspray);

    bool leafOrPollenFromLeft = false;
    switch (type)
    {
        case ParticleType::Rain:
        case ParticleType::Snow:
        case ParticleType::Ash:

            rectSize.y *= 0.10f;
            break;
        case ParticleType::Sand:
        case ParticleType::Wind:

            // Sand and Wind move toward +X, so spawn them in the upwind left band.
            rectSize.x *= 0.10f;
            break;
        case ParticleType::Ember:

            rectPos.y += rectSize.y * 0.80f;
            rectSize.y *= 0.20f;
            break;
        case ParticleType::DriftingLeaf:
        case ParticleType::Pollen:
            // Left-edge leaf/pollen spawns reverse wind X; right-edge spawns use normal wind.
            leafOrPollenFromLeft = m_Dist01(m_Rng) < 0.5f;
            if (leafOrPollenFromLeft)
            {
                rectSize.x *= 0.10f;
            }
            else
            {
                rectPos.x += rectSize.x * 0.90f;
                rectSize.x *= 0.10f;
            }
            break;
        case ParticleType::Fog:
        case ParticleType::CherryBlossom:
        case ParticleType::Firefly:
        default:

            break;
    }

    ParticleZone fakeZone;
    fakeZone.position = rectPos;
    fakeZone.size = rectSize;
    fakeZone.type = type;
    fakeZone.enabled = true;
    fakeZone.noProjection = false;

    int typeIndex = static_cast<int>(type);
    if (typeIndex < 0 || typeIndex >= static_cast<int>(kSpawnDispatch.size()))
        return;
    ParticleSpawnContext ctx{m_Rng, m_Dist01, m_Particles, m_WindDir, m_WindStrength};
    size_t before = m_Particles.size();
    kSpawnDispatch[typeIndex](WEATHER_ZONE_INDEX, fakeZone, ctx);
    AssignSpawnVariants(before);

    // Apply endpoint size scaling to every particle the spawn routine appended, including
    // companions.
    const float sizeScale = streamDef ? streamDef->particleSizeScale : 1.0f;
    if (sizeScale != 1.0f)
    {
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            m_Particles[i].size *= sizeScale;
        }
    }

    // Ramp Snow from calm motion to shared wind-driven flurries over wind strength 0.3 to 0.9. The
    // strongest wind scales horizontal speed 7x and fall speed 3.5x.
    if (type == ParticleType::Snow)
    {
        const float ramp = glm::smoothstep(0.3f, 0.9f, m_WindStrength);
        const float boostX = glm::mix(1.0f, 7.0f, ramp);
        const float boostY = glm::mix(1.0f, 3.5f, ramp);
        const float dirSign = (m_WindDir.x < 0.0f) ? -1.0f : 1.0f;
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            m_Particles[i].velocity.x = std::abs(m_Particles[i].velocity.x) * boostX * dirSign;
            m_Particles[i].velocity.y *= boostY;
        }
    }

    // Tag left-edge wind reversal and extend weather lifetimes to cross the view.
    if (type == ParticleType::DriftingLeaf || type == ParticleType::Pollen)
    {
        constexpr float kWeatherLifetimeBoost = 1.5f;
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            if (leafOrPollenFromLeft)
                m_Particles[i].velocity.x = 1.0f;
            m_Particles[i].lifetime *= kWeatherLifetimeBoost;
            m_Particles[i].maxLifetime *= kWeatherLifetimeBoost;
        }
    }

    // Spread rain impacts across the visible Y range and size lifetime to reach the target.
    if (type == ParticleType::Rain)
    {
        const float minSplash = cameraPos.y + viewSize.y * 0.10f;
        const float maxSplash = cameraPos.y + viewSize.y * 1.05f;
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            Particle& rp = m_Particles[i];
            rp.bakedGroundY = minSplash + m_Dist01(m_Rng) * (maxSplash - minSplash);
            const float travel = std::max(0.0f, rp.bakedGroundY - rp.position.y);
            const float requiredTime = travel / std::max(1.0f, rp.velocity.y);
            rp.lifetime = std::max(2.0f, requiredTime * 1.2f);
            rp.maxLifetime = rp.lifetime;
        }
    }

    // Spread snow impacts across visible Y; the existing lifetime covers the travel distance.
    if (type == ParticleType::Snow)
    {
        const float minImpact = cameraPos.y + viewSize.y * 0.10f;
        const float maxImpact = cameraPos.y + viewSize.y * 1.05f;
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            m_Particles[i].bakedGroundY = minImpact + m_Dist01(m_Rng) * (maxImpact - minImpact);
        }
    }

    // Pre-age streaming particles along velocity so their travel columns populate immediately.

    // Drifting types retain full lifetime for a smooth alpha fade-in.
    const bool isStreaming = (type == ParticleType::Rain || type == ParticleType::Snow ||
                              type == ParticleType::Ash || type == ParticleType::Ember);
    if (isStreaming)
    {
        for (size_t i = before; i < m_Particles.size(); ++i)
        {
            Particle& p = m_Particles[i];
            const float ageFraction = m_Dist01(m_Rng) * 0.80f;
            const float ageTime = ageFraction * p.maxLifetime;
            p.position += p.velocity * ageTime;
            p.lifetime = p.maxLifetime * (1.0f - ageFraction);
        }
    }
}

ParticleSystem::ParticleRenderData ParticleSystem::MakeRenderData(const Particle& p) const
{
    ParticleRenderData data{};
    data.size = glm::vec2(p.size, p.size);
    data.color = p.color;
    data.rotation = p.rotation;
    data.phase = p.phase;
    data.lifeT =
        (p.maxLifetime > 1e-4f) ? std::clamp(1.0f - p.lifetime / p.maxLifetime, 0.0f, 1.0f) : 0.0f;
    data.additive = p.additive;
    data.type = p.type;
    data.variant = p.variant;
    return data;
}

bool ParticleSystem::ResolveNoProjection(const Particle& p) const
{
    // Live zones override noProjection; zoneless secondary particles retain the parent flag.
    if (m_Zones && p.zoneIndex >= 0 && p.zoneIndex < static_cast<int>(m_Zones->size()))
    {
        return (*m_Zones)[p.zoneIndex].noProjection;
    }
    return p.noProjection;
}

std::optional<ParticleSystem::ParticleSprite> ParticleSystem::ResolveSprite(
    const ParticleRenderData& data) const
{
    const int typeIndex = static_cast<int>(data.type);
    if (typeIndex < 0 || typeIndex >= static_cast<int>(EnumTraits<ParticleType>::Count))
    {
        return std::nullopt;
    }
    const uint8_t variantCount = std::max<uint8_t>(uint8_t{1}, m_VariantCounts[typeIndex]);
    const uint8_t variant = (data.variant < variantCount) ? data.variant : uint8_t{0};
    const AtlasSlot& slot = m_AtlasSlots[typeIndex][variant];

    // Loop strips use global time with phase jitter; one-shots map frames across lifetime.
    ParticleSprite sprite;
    sprite.uvMin = slot.region.uvMin;
    sprite.uvMax = slot.region.uvMax;
    if (slot.frameCount > 1)
    {
        const ParticleVisuals& vis = kParticleVisuals[typeIndex];
        // Bubble pop variant remaps its final lifetime onto one strip playback.
        const ParticleAnimMode animMode = (data.type == ParticleType::Bubble && variant == 1)
                                              ? ParticleAnimMode::LifeMapped
                                              : vis.animMode;
        int frame = 0;
        if (animMode == ParticleAnimMode::LifeMapped)
        {
            frame = std::min(slot.frameCount - 1,
                             static_cast<int>(data.lifeT * static_cast<float>(slot.frameCount)));
        }
        else
        {
            const float rate = vis.animFps * (0.85f + 0.30f * (data.phase / 6.2832f));
            const float cursor = m_Time * rate + data.phase * 2.0f;
            frame = static_cast<int>(cursor) % slot.frameCount;
        }
        const float frameWidth =
            (slot.region.uvMax.x - slot.region.uvMin.x) / static_cast<float>(slot.frameCount);
        // Quarter-texel inset avoids neighboring strip frames under rotation and scaling.
        const float inset = 0.25f / static_cast<float>(kParticleAtlasWidth);
        sprite.uvMin.x = slot.region.uvMin.x + frameWidth * static_cast<float>(frame) + inset;
        sprite.uvMax.x = slot.region.uvMin.x + frameWidth * static_cast<float>(frame + 1) - inset;
    }

    sprite.renderSize = data.size;

    if (data.type == ParticleType::Sunshine)
    {
        sprite.renderSize = glm::vec2(data.size.x, data.size.x * 4.0f);
    }

    else if (data.type == ParticleType::Rain)
    {
        const float stretch = 1.0f + 0.4f * (std::sin(data.phase) * 0.5f + 0.5f);
        sprite.renderSize = glm::vec2(data.size.x, data.size.x * stretch);
    }

    else if (data.type == ParticleType::Snow)
    {
        const float flipScale = std::cos(m_Time * 3.0f + data.phase);
        sprite.renderSize.x *= flipScale;
    }

    else if (data.type == ParticleType::Wind)
    {
        sprite.renderSize = glm::vec2(data.size.x * 2.2f, data.size.x * 0.75f);
    }
    return sprite;
}

void ParticleSystem::Render(IRenderer& renderer,
                            glm::vec2 cameraPos,
                            bool noProjectionOnly,
                            bool renderAll)
{
    if (!m_RenderEnabled)
    {
        m_LastDrawnCount = 0;
        return;
    }

    // noProjection particles skip viewport culling; regular particles use the padded view
    // rectangle.
    m_NoProjectionBatch.clear();
    m_RegularBatch.clear();

    const glm::vec2 viewSize = renderer.GetViewSize();

    for (const Particle& p : m_Particles)
    {
        const bool isNoProjection = ResolveNoProjection(p);

        if (!renderAll)
        {
            if (noProjectionOnly && !isNoProjection)
            {
                continue;
            }
            if (!noProjectionOnly && isNoProjection)
            {
                continue;
            }
        }

        ParticleRenderData data = MakeRenderData(p);

        data.screenPos = p.position - cameraPos;

        if (isNoProjection)
        {
            // Follow the structure mesh when covered; otherwise retain plain screen position.
            if (m_Tilemap)
            {
                glm::vec2 structureScreenPos;
                if (m_Tilemap->ProjectNoProjectionStructurePoint(
                        p.position, cameraPos, structureScreenPos))
                {
                    data.screenPos = structureScreenPos;
                }
            }

            m_NoProjectionBatch.push_back(data);
        }
        else
        {
            // Pad by sprite size so partially visible cards are not culled at the viewport edge.
            const float padding = std::max(data.size.x, data.size.y) * 2.0f + 50.0f;

            const bool outsideViewport =
                data.screenPos.x < -padding || data.screenPos.x > viewSize.x + padding ||
                data.screenPos.y < -padding || data.screenPos.y > viewSize.y + padding;

            if (outsideViewport)
            {
                continue;
            }

            m_RegularBatch.push_back(data);
        }
    }

    auto drawParticle = [&](const ParticleRenderData& data)
    {
        if (m_TexturesLoaded)
        {
            const std::optional<ParticleSprite> sprite = ResolveSprite(data);
            if (!sprite)
            {
                return;
            }
            const glm::vec2 centeredPos = data.screenPos - sprite->renderSize * 0.5f;
            renderer.DrawSpriteAtlas(m_Store->Get(m_AtlasHandle),
                                     centeredPos,
                                     sprite->renderSize,
                                     sprite->uvMin,
                                     sprite->uvMax,
                                     data.rotation,
                                     data.color,
                                     data.additive);
        }
        else
        {
            glm::vec2 size = data.size;
            if (data.type == ParticleType::Rain)
            {
                size = glm::vec2(1.0f, 8.0f);
            }
            renderer.DrawColoredRect(data.screenPos, size, data.color, data.additive);
        }
    };

    // Blend partitioning below is intentional; this unused comparator does not define render order.
    auto sortByBlendMode = [](const ParticleRenderData& a, const ParticleRenderData& b)
    { return a.additive < b.additive; };

    // Partition alpha before additive in O(n), without depth sorting.
    std::partition(m_NoProjectionBatch.begin(),
                   m_NoProjectionBatch.end(),
                   [](const ParticleRenderData& d) { return !d.additive; });
    std::partition(m_RegularBatch.begin(),
                   m_RegularBatch.end(),
                   [](const ParticleRenderData& d) { return !d.additive; });

    for (const auto& data : m_NoProjectionBatch)
    {
        drawParticle(data);
    }

    for (const auto& data : m_RegularBatch)
    {
        drawParticle(data);
    }

    m_LastDrawnCount = m_NoProjectionBatch.size() + m_RegularBatch.size();
}

// Classify scene cards; sprite appearance remains shared with the flat path.
//
// B0 ignores depth to match flat particles over all tile layers. facade decals alone
// test depth, with eye-ward wall bias. sheets use flat-viewport culling; ground props
// and zone cards use the frustum. noProjection particles bypass culling.
//
//   card         who                                    anchor A
//   -----------  -------------------------------------  ------------------------
//   sheet        weather, ambient and console spawns     the rig's ground focus
//   ground prop  zone types with no net vertical motion  the particle itself
//   zone card    zone types that fall or rise on screen  ClampToRect(focus, zone)
//   facade       noProjection over a Structure body      the body's run foot
void ParticleSystem::Render3D(IRenderer& renderer, const cameraRig::RigParams& rig)
{
    if (!m_RenderEnabled)
    {
        m_LastDrawnCount = 0;
        return;
    }

    m_FacadeBatch3D.clear();
    m_CardBatch3D.clear();

    const particleCards::Frame frame = particleCards::MakeFrame(rig);

    for (const Particle& p : m_Particles)
    {
        const bool isNoProjection = ResolveNoProjection(p);
        const ParticleRenderData data = MakeRenderData(p);

        if (isNoProjection && m_Tilemap)
        {
            const std::optional<Tilemap::StructureFacade> facade =
                m_Tilemap->FindStructureFacade(p.position);
            if (facade)
            {
                const billboard::Orientation& axes =
                    particleCards::FacadeAxes(frame, facade->widthTiles);
                m_FacadeBatch3D.push_back({data,
                                           particleCards::FacadePoint(frame,
                                                                      axes,
                                                                      facade->foot,
                                                                      facade->runCentreX,
                                                                      facade->baseSouthEdgeY,
                                                                      p.position),
                                           axes});
                continue;
            }
        }

        const int zone = (p.zoneIndex >= 0) ? p.zoneIndex : p.anchorZone;
        const bool hasZone = m_Zones && zone >= 0 && zone < static_cast<int>(m_Zones->size());

        glm::vec2 anchorWorld = frame.focusWorld;
        float anchorHeight = frame.focusHeight;
        if (hasZone)
        {
            const ParticleZone& zoneRect = (*m_Zones)[static_cast<size_t>(zone)];
            const bool ground =
                kParticleVisuals[static_cast<size_t>(p.type)].anchor == ParticleAnchor::Ground;
            anchorWorld = ground ? p.position
                                 : particleCards::ClampToRect(
                                       frame.focusWorld, zoneRect.position, zoneRect.size);
            anchorHeight = m_Tilemap ? m_Tilemap->SurfaceHeightAtWorldPos(anchorWorld) : 0.0f;
        }

        const glm::vec3 centre =
            particleCards::CardPoint(frame, anchorWorld, anchorHeight, p.position);

        if (!isNoProjection)
        {
            if (hasZone)
            {
                // Use a radius large enough for Sunshine, which renders at four times base size.
                const float radius = std::max(data.size.x, data.size.y) * 2.0f;
                if (!frustum::IntersectsSphere(frame.view, centre, radius))
                {
                    continue;
                }
            }
            else if (!particleCards::InsideSheetView(frame, p.position, data.size))
            {
                continue;
            }
        }

        m_CardBatch3D.push_back({data, centre, frame.sheet});
    }

    // Partition by blend mode within each pass; particle cards have no per-particle depth sort.
    const auto nonAdditiveFirst = [](const Particle3DQuad& q) { return !q.data.additive; };
    std::partition(m_FacadeBatch3D.begin(), m_FacadeBatch3D.end(), nonAdditiveFirst);
    std::partition(m_CardBatch3D.begin(), m_CardBatch3D.end(), nonAdditiveFirst);

    const Texture headlessTexture;
    size_t submitted = 0;

    const auto submit = [&](const Particle3DQuad& q, renderModes::DepthMode depth)
    {
        if (submitted >= MAX_PARTICLE_QUADS_3D)
        {
            return;
        }
        const renderModes::BlendMode blend =
            q.data.additive ? renderModes::BlendMode::Additive : renderModes::BlendMode::Alpha;
        glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT];

        if (m_TexturesLoaded)
        {
            const std::optional<ParticleSprite> sprite = ResolveSprite(q.data);
            if (!sprite)
            {
                return;
            }
            const Texture& atlas = m_Store->Get(m_AtlasHandle);
            const glm::vec2 dims(static_cast<float>(atlas.GetWidth()),
                                 static_cast<float>(atlas.GetHeight()));
            particleCards::MakeSpriteQuad(
                q.centre, sprite->renderSize, q.axes, q.data.rotation, corners);
            // flipY = false reproduces atlas V assignment for both file and procedural sprite
            // sources.
            renderer.DrawQuad3D(atlas,
                                corners,
                                sprite->uvMin * dims,
                                (sprite->uvMax - sprite->uvMin) * dims,
                                q.data.color,
                                blend,
                                depth,
                                false,
                                false,
                                false,
                                renderModes::LightMode::SelfLit);
        }
        else
        {
            // Retain headless geometry submission; backends discard the empty texture.
            particleCards::MakeSpriteQuad(q.centre, q.data.size, q.axes, q.data.rotation, corners);
            renderer.DrawQuad3D(headlessTexture,
                                corners,
                                glm::vec2(0.0f),
                                glm::vec2(1.0f),
                                q.data.color,
                                blend,
                                depth,
                                false,
                                false,
                                false,
                                renderModes::LightMode::SelfLit);
        }
        ++submitted;
    };

    // Draw depth-tested facade decals before cards that ignore depth.
    for (const Particle3DQuad& q : m_FacadeBatch3D)
    {
        submit(q, renderModes::DepthMode::TestOnly);
    }
    for (const Particle3DQuad& q : m_CardBatch3D)
    {
        submit(q, renderModes::DepthMode::None);
    }

    m_LastDrawnCount = submitted;
}

void ParticleSystem::OnZoneRemoved(int zoneIndex)
{
    if (zoneIndex < 0)
    {
        return;
    }

    // Splash anchorZone ties deletion to the parent zone.
    std::erase_if(m_Particles,
                  [zoneIndex](const Particle& p)
                  { return p.zoneIndex == zoneIndex || p.anchorZone == zoneIndex; });

    for (auto& p : m_Particles)
    {
        if (p.zoneIndex > zoneIndex)
        {
            p.zoneIndex--;
        }
        if (p.anchorZone > zoneIndex)
        {
            p.anchorZone--;
        }
    }

    if (zoneIndex < static_cast<int>(m_ZoneSpawnTimers.size()))
    {
        m_ZoneSpawnTimers.erase(m_ZoneSpawnTimers.begin() + zoneIndex);
    }
}
