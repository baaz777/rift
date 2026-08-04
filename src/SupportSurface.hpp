#pragma once

#include <glm/glm.hpp>

#include <cstdint>

/**
 * @enum SupportSurface
 * @brief Distinguishes ground from a ramp or deck in the same footprint.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Surface identity and physical height are separate. Ground exists beneath elevated cells.
 * A collision cell blocks only actors on its support surface and exact authored height.
 *
 * | Surface   | Height in pixels   | Blocking cell elevation |
 * |-----------|--------------------|-------------------------|
 * | Ground    | Always zero.       | Zero.                   |
 * | Elevation | Authored, nonzero. | Equal to actor height.  |
 */
enum class SupportSurface : std::uint8_t
{
    Ground = 0,     ///< Implicit ground beneath every cell.
    Elevation = 1,  ///< Authored ramp or deck support.
};

/**
 * @struct SupportState
 * @brief Pairs support identity with its exact height.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Ground requires height 0. Entity state lives in Elevation::surface and Elevation::plane;
 * CharacterKinematics::CommitSupport writes them together.
 */
struct SupportState
{
    SupportSurface surface{SupportSurface::Ground};  ///< Ground vs. authored-elevation topology.
    int height{0};  ///< Exact support height in pixels; zero for ground.

    bool operator==(const SupportState&) const = default;
};

/**
 * @struct SurfaceTransition
 * @brief Candidate support from a movement probe.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * A disconnected result rejects the entire move. Its support can differ from the initial
 * support when an earlier sub-step succeeded; do not commit that partial result.
 */
struct SurfaceTransition
{
    SupportState support{};  ///< Last connected support reached by the probe.
    bool connected{true};    ///< True only when every crossed boundary connects.
};

/**
 * @struct CharacterCollisionBody
 * @brief Per-frame character collision snapshot.
 * @author Alex (<https://github.com/lextpf>)
 * @ingroup World
 *
 * Characters collide only when support identity and height both match.
 * Rebuild from live components; do not store on entities.
 */
struct CharacterCollisionBody
{
    glm::vec2 feet{0.0f};    ///< Bottom-center anchor in world pixels, with +Y pointing down.
    SupportState support{};  ///< Exact surface + height; must match for a collision to register.
};
