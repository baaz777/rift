#include <gtest/gtest.h>

#include "../src/AnimationState.hpp"
#include "../src/CharacterDirection.hpp"
#include "../src/Elevation.hpp"
#include "../src/Facing.hpp"
#include "../src/NpcAiSystem.hpp"
#include "../src/NpcIdle.hpp"
#include "../src/Patrol.hpp"
#include "../src/PatrolRoute.hpp"
#include "../src/Speed.hpp"
#include "../src/Tilemap.hpp"
#include "../src/Transform.hpp"

#include <glm/glm.hpp>

#include <random>
#include <vector>

// a fixed std::mt19937 seed makes the no-patrol idle branch deterministic.

namespace
{
// each no-route update consumes one direction draw from the seeded engine.
std::vector<CharacterDirection> LookAroundSequence(unsigned seed, int picks)
{
    std::mt19937 rng(seed);
    Tilemap tilemap;
    tilemap.SetTilemapSize(10, 10, false);

    Transform xf;
    Elevation elev;
    Facing facing;
    AnimationState anim;
    NpcIdle idle;
    Patrol patrol;
    PatrolRoute route;
    Speed speed;

    idle.standingStill = true;
    idle.randomStandStillTimer = 0.0f;

    std::vector<CharacterDirection> seq;
    seq.reserve(static_cast<std::size_t>(picks));
    for (int i = 0; i < picks; ++i)
    {
        idle.lookAroundTimer = 0.0f;  // force a pick on every call
        NpcAiSystem::Update(
            xf, elev, facing, anim, idle, patrol, route, speed, 0.016f, &tilemap, nullptr, rng);
        seq.push_back(facing.dir);
    }
    return seq;
}
}  // namespace

TEST(NpcAiRng, LookAroundIsDeterministicForSameSeed)
{
    EXPECT_EQ(LookAroundSequence(0xC0FFEEu, 16), LookAroundSequence(0xC0FFEEu, 16));
}

TEST(NpcAiRng, EngineIsActuallyConsumed)
{
    // sixteen draws from four directions must vary; the seed fixes the sequence.
    const std::vector<CharacterDirection> seq = LookAroundSequence(0x1234u, 16);
    bool allSame = true;
    for (const CharacterDirection dir : seq)
    {
        if (dir != seq.front())
        {
            allSame = false;
            break;
        }
    }
    EXPECT_FALSE(allSame);
}
