// Structure tiles share a pivot and leaning axes. vertical slices must meet
// along the up axis; horizontal slices must retain their shared edges during camera rotation.
#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/SceneMath.hpp"
#include "../src/Tilemap.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
constexpr float kTol = 1e-3f;
constexpr int kTileSize = 16;

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

constexpr size_t kLayer = 0;

Tilemap MakeMap(int w = 40, int h = 40)
{
    Tilemap tm;
    tm.SetTilemapSize(w, h, false);
    return tm;
}

// use an oblique camera so an incorrect lift axis visibly separates the quads.
cameraRig::RigParams MakeRig(glm::vec2 target)
{
    cameraRig::RigParams rig;
    rig.target = target;
    rig.visibleWorldSize = {320.0f, 180.0f};
    rig.sceneRadius = 2048.0f;
    cameraRig::ApplyPreset(rig, cameraRig::Preset::DS);
    return rig;
}

void PaintUprightRow(Tilemap& tm, int leftX, int y, int columnCount)
{
    for (int i = 0; i < columnCount; ++i)
    {
        tm.SetLayerTile(leftX + i, y, kLayer, 1);
        tm.SetLayerStance(leftX + i, y, kLayer, TileStance::Structure);
    }
}

std::vector<MockRenderer::Quad3D> SortedByRunOrder(std::vector<MockRenderer::Quad3D> quads)
{
    std::sort(quads.begin(),
              quads.end(),
              [](const MockRenderer::Quad3D& a, const MockRenderer::Quad3D& b)
              {
                  return a.corners[sceneMath::QUAD_BOTTOM_LEFT].x <
                         b.corners[sceneMath::QUAD_BOTTOM_LEFT].x;
              });
    return quads;
}

void PaintUprightColumn(Tilemap& tm, int x, int topRow, int rowCount)
{
    for (int i = 0; i < rowCount; ++i)
    {
        const int y = topRow + i;
        tm.SetLayerTile(x, y, kLayer, 1);
        tm.SetLayerStance(x, y, kLayer, TileStance::Structure);
    }
}

void PaintStanceColumn(Tilemap& tm, int x, int topRow, int rowCount, TileStance stance)
{
    for (int i = 0; i < rowCount; ++i)
    {
        const int y = topRow + i;
        tm.SetLayerTile(x, y, kLayer, 1);
        tm.SetLayerStance(x, y, kLayer, stance);
    }
}

std::vector<MockRenderer::Quad3D> SortedByHeight(std::vector<MockRenderer::Quad3D> quads)
{
    std::sort(quads.begin(),
              quads.end(),
              [](const MockRenderer::Quad3D& a, const MockRenderer::Quad3D& b)
              {
                  return a.corners[sceneMath::QUAD_BOTTOM_LEFT].y <
                         b.corners[sceneMath::QUAD_BOTTOM_LEFT].y;
              });
    return quads;
}

// leave structureId at -1 to exercise automatic grouping.
void PaintStructureBlock(Tilemap& tm, int leftX, int topRow, int columnCount, int rowCount)
{
    for (int dy = 0; dy < rowCount; ++dy)
    {
        for (int dx = 0; dx < columnCount; ++dx)
        {
            tm.SetLayerTile(leftX + dx, topRow + dy, kLayer, 1);
            tm.SetLayerStance(leftX + dx, topRow + dy, kLayer, TileStance::Structure);
        }
    }
}

std::vector<MockRenderer::Quad3D> SortedByDepth(std::vector<MockRenderer::Quad3D> quads)
{
    std::sort(quads.begin(),
              quads.end(),
              [](const MockRenderer::Quad3D& a, const MockRenderer::Quad3D& b)
              {
                  return a.corners[sceneMath::QUAD_BOTTOM_LEFT].z <
                         b.corners[sceneMath::QUAD_BOTTOM_LEFT].z;
              });
    return quads;
}
}  // namespace

TEST(World3DStackingTest, UprightRunStacksIntoOneWall)
{
    Tilemap tm = MakeMap();

    PaintUprightColumn(tm, 20, 10, 3);

    MockRenderer renderer;
    const cameraRig::RigParams rig = MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize});
    tm.RenderWorld3D(renderer, rig);

    ASSERT_EQ(renderer.quads3D.size(), 3u);
    const auto quads = SortedByHeight(renderer.quads3D);

    // measure along the shared up axis: a three-tile wall must span exactly three
    // tiles, rather than advancing across three ground rows.
    const glm::vec3 foot = quads.front().corners[sceneMath::QUAD_BOTTOM_LEFT];
    const glm::vec3 head = quads.back().corners[sceneMath::QUAD_TOP_LEFT];
    EXPECT_NEAR(glm::distance(foot, head), 3.0f * kTileSize, kTol);

    const glm::vec3 alongWall = glm::normalize(head - foot);
    const glm::vec3 quadUp = glm::normalize(quads.front().corners[sceneMath::QUAD_TOP_LEFT] -
                                            quads.front().corners[sceneMath::QUAD_BOTTOM_LEFT]);
    EXPECT_NEAR(glm::dot(alongWall, quadUp), 1.0f, kTol);

    for (size_t i = 1; i < quads.size(); ++i)
    {
        EXPECT_NEAR(quads[i].corners[sceneMath::QUAD_BOTTOM_LEFT].x, foot.x, kTol)
            << "quad " << i << " drifted in X";
    }
}

TEST(World3DStackingTest, AdjacentTilesInARunShareAnEdgeExactly)
{
    // each tile's top edge must meet the next tile's bottom edge along the leaning up axis.
    Tilemap tm = MakeMap();
    PaintUprightColumn(tm, 20, 10, 4);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 4u);
    const auto quads = SortedByHeight(renderer.quads3D);

    for (size_t i = 0; i + 1 < quads.size(); ++i)
    {
        const glm::vec3 lowerTopLeft = quads[i].corners[sceneMath::QUAD_TOP_LEFT];
        const glm::vec3 lowerTopRight = quads[i].corners[sceneMath::QUAD_TOP_RIGHT];
        const glm::vec3 upperBottomLeft = quads[i + 1].corners[sceneMath::QUAD_BOTTOM_LEFT];
        const glm::vec3 upperBottomRight = quads[i + 1].corners[sceneMath::QUAD_BOTTOM_RIGHT];

        EXPECT_NEAR(glm::distance(lowerTopLeft, upperBottomLeft), 0.0f, kTol)
            << "gap at seam " << i << " (left)";
        EXPECT_NEAR(glm::distance(lowerTopRight, upperBottomRight), 0.0f, kTol)
            << "gap at seam " << i << " (right)";
    }
}

TEST(World3DStackingTest, RunIsOrderedBottomRowLowest)
{
    // the southernmost map row forms the base so the artwork stays upright.
    Tilemap tm = MakeMap();
    PaintUprightColumn(tm, 20, 10, 3);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));
    ASSERT_EQ(renderer.quads3D.size(), 3u);

    // the north-to-south scan submits the top tile first.
    const float firstHeight = renderer.quads3D.front().corners[sceneMath::QUAD_BOTTOM_LEFT].y;
    const float lastHeight = renderer.quads3D.back().corners[sceneMath::QUAD_BOTTOM_LEFT].y;
    EXPECT_GT(firstHeight, lastHeight);

    EXPECT_NEAR(lastHeight, 0.0f, kTol);
}

TEST(World3DStackingTest, LoneUprightTileStandsOnItsOwnCell)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Structure);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    const glm::vec3 foot = renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT];
    EXPECT_NEAR(foot.y, 0.0f, kTol);

    EXPECT_NEAR(foot.z, static_cast<float>((12 + 1) * kTileSize), kTol);
}

TEST(World3DStackingTest, SeparateRunsDoNotMerge)
{
    Tilemap tm = MakeMap();
    PaintUprightColumn(tm, 20, 8, 2);   // rows 8-9
    PaintUprightColumn(tm, 20, 12, 2);  // rows 12-13

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 11 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 4u);

    int onGround = 0;
    for (const auto& quad : renderer.quads3D)
    {
        if (std::abs(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].y) < kTol)
        {
            ++onGround;
        }
    }
    EXPECT_EQ(onGround, 2);
}

TEST(World3DStackingTest, SideBySideTilesShareAnEdgeWhenTheCameraIsYawed)
{
    // independent pivots separate adjacent edges by (1 - cos yaw); a shared pivot keeps them
    // joined.
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 2);

    cameraRig::RigParams rig = MakeRig({21 * kTileSize, 12 * kTileSize});
    rig.yawRadians = Degrees(40.0f);  // the angle that used to break it

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, rig);

    ASSERT_EQ(renderer.quads3D.size(), 2u);
    const auto quads = SortedByRunOrder(renderer.quads3D);

    EXPECT_NEAR(glm::distance(quads[0].corners[sceneMath::QUAD_TOP_RIGHT],
                              quads[1].corners[sceneMath::QUAD_TOP_LEFT]),
                0.0f,
                kTol);
    EXPECT_NEAR(glm::distance(quads[0].corners[sceneMath::QUAD_BOTTOM_RIGHT],
                              quads[1].corners[sceneMath::QUAD_BOTTOM_LEFT]),
                0.0f,
                kTol);
}

TEST(World3DStackingTest, WideRunStaysWeldedAtEveryYaw)
{
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 4);

    for (int deg = -180; deg < 180; deg += 15)
    {
        cameraRig::RigParams rig = MakeRig({22 * kTileSize, 12 * kTileSize});
        rig.yawRadians = Degrees(static_cast<float>(deg));

        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        ASSERT_EQ(renderer.quads3D.size(), 4u) << "yaw " << deg;

        const auto quads = SortedByRunOrder(renderer.quads3D);
        for (size_t i = 0; i + 1 < quads.size(); ++i)
        {
            EXPECT_NEAR(glm::distance(quads[i].corners[sceneMath::QUAD_BOTTOM_RIGHT],
                                      quads[i + 1].corners[sceneMath::QUAD_BOTTOM_LEFT]),
                        0.0f,
                        kTol)
                << "seam " << i << " split at yaw " << deg;
        }
    }
}

TEST(World3DStackingTest, WideRunKeepsItsTotalWidth)
{
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 4);

    cameraRig::RigParams rig = MakeRig({22 * kTileSize, 12 * kTileSize});
    rig.yawRadians = Degrees(55.0f);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, rig);
    ASSERT_EQ(renderer.quads3D.size(), 4u);

    const auto quads = SortedByRunOrder(renderer.quads3D);
    EXPECT_NEAR(glm::distance(quads.front().corners[sceneMath::QUAD_BOTTOM_LEFT],
                              quads.back().corners[sceneMath::QUAD_BOTTOM_RIGHT]),
                4.0f * kTileSize,
                kTol);
}

TEST(World3DStackingTest, LoneUprightTileIsUnaffectedByTheRunPivot)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Structure);

    cameraRig::RigParams rig = MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize});
    rig.yawRadians = Degrees(70.0f);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, rig);
    ASSERT_EQ(renderer.quads3D.size(), 1u);

    const glm::vec3 mid = (renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT] +
                           renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                          0.5f;
    EXPECT_NEAR(mid.x, 20 * kTileSize + kTileSize * 0.5f, kTol);
    EXPECT_NEAR(mid.z, static_cast<float>((12 + 1) * kTileSize), kTol);
}

TEST(World3DStackingTest, WideStructureStaysAnchoredAsTheCameraOrbits)
{
    // wide structures keep fixed yaw so their footprint cannot move during an orbit.
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 4);

    cameraRig::RigParams reference = MakeRig({22 * kTileSize, 12 * kTileSize});
    reference.yawRadians = 0.0f;

    MockRenderer referenceRenderer;
    tm.RenderWorld3D(referenceRenderer, reference);
    ASSERT_EQ(referenceRenderer.quads3D.size(), 4u);
    const auto expected = SortedByRunOrder(referenceRenderer.quads3D);

    for (int deg = -180; deg < 180; deg += 20)
    {
        cameraRig::RigParams rig = reference;
        rig.yawRadians = Degrees(static_cast<float>(deg));

        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        ASSERT_EQ(renderer.quads3D.size(), 4u) << "yaw " << deg;
        const auto actual = SortedByRunOrder(renderer.quads3D);

        for (size_t q = 0; q < actual.size(); ++q)
        {
            for (int c = 0; c < sceneMath::QUAD_CORNER_COUNT; ++c)
            {
                EXPECT_NEAR(glm::distance(actual[q].corners[c], expected[q].corners[c]), 0.0f, kTol)
                    << "tile " << q << " corner " << c << " moved at yaw " << deg;
            }
        }
    }
}

TEST(World3DStackingTest, LoneTileStillTurnsTowardTheCameraButHoldsItsAnchor)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Structure);

    const glm::vec2 anchor{20 * kTileSize + kTileSize * 0.5f, (12 + 1) * kTileSize};

    auto footMidpointAtYaw = [&](float degrees)
    {
        cameraRig::RigParams rig = MakeRig(anchor);
        rig.yawRadians = Degrees(degrees);
        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        EXPECT_EQ(renderer.quads3D.size(), 1u);
        return renderer.quads3D;
    };

    const auto straight = footMidpointAtYaw(0.0f);
    const auto turned = footMidpointAtYaw(70.0f);
    ASSERT_EQ(straight.size(), 1u);
    ASSERT_EQ(turned.size(), 1u);

    EXPECT_GT(glm::distance(turned[0].corners[sceneMath::QUAD_BOTTOM_LEFT],
                            straight[0].corners[sceneMath::QUAD_BOTTOM_LEFT]),
              0.1f);

    for (const auto& quads : {straight, turned})
    {
        const glm::vec3 mid = (quads[0].corners[sceneMath::QUAD_BOTTOM_LEFT] +
                               quads[0].corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                              0.5f;
        EXPECT_NEAR(mid.x, anchor.x, kTol);
        EXPECT_NEAR(mid.z, anchor.y, kTol);
    }
}

TEST(World3DStackingTest, TallSingleColumnStillCountsAsAPole)
{
    // column span determines fixed yaw; a one-column, three-row tower may still turn.
    Tilemap tm = MakeMap();
    PaintUprightColumn(tm, 20, 10, 3);

    const cameraRig::RigParams straight =
        MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize});
    cameraRig::RigParams turned = straight;
    turned.yawRadians = Degrees(70.0f);

    MockRenderer a;
    MockRenderer b;
    tm.RenderWorld3D(a, straight);
    tm.RenderWorld3D(b, turned);
    ASSERT_EQ(a.quads3D.size(), 3u);
    ASSERT_EQ(b.quads3D.size(), 3u);

    EXPECT_GT(glm::distance(b.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT],
                            a.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT]),
              0.1f);
}

TEST(World3DStackingTest, WideStructureBaseSitsOnItsAuthoredFootprint)
{
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 3);

    cameraRig::RigParams rig = MakeRig({21 * kTileSize, 12 * kTileSize});
    rig.yawRadians = Degrees(50.0f);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, rig);
    ASSERT_EQ(renderer.quads3D.size(), 3u);
    const auto quads = SortedByRunOrder(renderer.quads3D);

    for (size_t i = 0; i < quads.size(); ++i)
    {
        const glm::vec3 mid = (quads[i].corners[sceneMath::QUAD_BOTTOM_LEFT] +
                               quads[i].corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                              0.5f;
        const float expectedX =
            static_cast<float>(20 + static_cast<int>(i)) * kTileSize + kTileSize * 0.5f;
        EXPECT_NEAR(mid.x, expectedX, kTol) << "tile " << i;
        EXPECT_NEAR(mid.z, static_cast<float>((12 + 1) * kTileSize), kTol) << "tile " << i;
    }
}

TEST(World3DStackingTest, GroundDrawsWithoutDepthSoCoplanarTilesCannotFight)
{
    // coplanar rotated ground quads can disagree in depth by float noise. disable
    // depth for ground and preserve layer order to prevent flicker.
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerRotation(20, 12, kLayer, 37.0f);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    EXPECT_EQ(renderer.quads3D[0].depth, renderModes::DepthMode::None);
}

TEST(World3DStackingTest, UprightArtworkStillUsesDepth)
{
    // upright artwork still needs depth against actors and other upright quads.
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Structure);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    EXPECT_EQ(renderer.quads3D[0].depth, renderModes::DepthMode::TestAndWrite);
}

TEST(World3DStackingTest, AllGroundIsSubmittedBeforeAnyUprightArtwork)
{
    // submit all ground before upright geometry; depth-free ground could otherwise cover walls.
    Tilemap tm = MakeMap();
    for (int x = 18; x <= 22; ++x)
    {
        tm.SetLayerTile(x, 12, kLayer, 1);  // ground
        tm.SetLayerTile(x, 11, kLayer, 1);  // more ground

        tm.SetLayerStance(x, 11, kLayer, (x % 2 == 0) ? TileStance::Structure : TileStance::Flat);
    }

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize, 12 * kTileSize}));
    ASSERT_GT(renderer.quads3D.size(), 4u);

    bool seenUpright = false;
    for (const auto& quad : renderer.quads3D)
    {
        if (quad.depth == renderModes::DepthMode::TestAndWrite)
        {
            seenUpright = true;
        }
        else
        {
            EXPECT_FALSE(seenUpright) << "ground submitted after upright artwork";
        }
    }
    EXPECT_TRUE(seenUpright) << "test painted no upright tiles";
}

TEST(World3DStackingTest, YSortFlagsAloneLeaveATileFlat)
{
    // y-sort flags change order only; an otherwise Flat tile stays in the ground pass.
    for (const bool useMinus : {false, true})
    {
        Tilemap tm = MakeMap();
        tm.SetLayerTile(20, 12, kLayer, 1);
        if (useMinus)
        {
            tm.SetLayerYSortMinus(20, 12, kLayer, true);
        }
        else
        {
            tm.SetLayerYSortPlus(20, 12, kLayer, true);
        }

        MockRenderer renderer;
        tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

        ASSERT_EQ(renderer.quads3D.size(), 1u) << "ySortMinus=" << useMinus;
        EXPECT_EQ(renderer.quads3D[0].depth, renderModes::DepthMode::None)
            << "ySortMinus=" << useMinus;

        for (const glm::vec3& corner : renderer.quads3D[0].corners)
        {
            EXPECT_NEAR(corner.y, 0.0f, kTol) << "ySortMinus=" << useMinus;
        }
    }
}

TEST(World3DStackingTest, AWallColumnRecedesAlongTheGroundInsteadOfStacking)
{
    // only Structure stacks vertically; a north-south Wall run stays on separate ground rows.
    Tilemap tm = MakeMap();
    PaintStanceColumn(tm, 20, 10, 3, TileStance::Wall);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 11 * kTileSize}));
    ASSERT_EQ(renderer.quads3D.size(), 3u);

    for (const auto& quad : renderer.quads3D)
    {
        EXPECT_NEAR(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].y, 0.0f, kTol)
            << "a fence panel was lifted into the air";
        // check scene-Y rise: a flat quad has the same edge length and could pass a length-only
        // check.
        EXPECT_GT(quad.corners[sceneMath::QUAD_TOP_LEFT].y,
                  quad.corners[sceneMath::QUAD_BOTTOM_LEFT].y)
            << "a fence panel is lying on the ground";

        EXPECT_NEAR(glm::distance(quad.corners[sceneMath::QUAD_TOP_LEFT],
                                  quad.corners[sceneMath::QUAD_BOTTOM_LEFT]),
                    static_cast<float>(kTileSize),
                    kTol);
    }

    std::vector<float> depths;
    depths.reserve(renderer.quads3D.size());
    for (const auto& quad : renderer.quads3D)
    {
        depths.push_back(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].z);
    }
    std::sort(depths.begin(), depths.end());
    for (size_t i = 0; i < depths.size(); ++i)
    {
        EXPECT_NEAR(depths[i], static_cast<float>((10 + static_cast<int>(i) + 1) * kTileSize), kTol)
            << "post " << i << " is not on its own row";
    }
}

TEST(World3DStackingTest, APropBelowABuildingIsNotItsGroundFloor)
{
    // base-row scans stop at non-Structure cells so a neighboring prop cannot lift the building.
    Tilemap tm = MakeMap();
    PaintUprightColumn(tm, 20, 10, 2);   // building, rows 10-11
    tm.SetLayerTile(20, 12, kLayer, 1);  // prop, row 12
    tm.SetLayerStance(20, 12, kLayer, TileStance::Prop);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 11 * kTileSize}));
    ASSERT_EQ(renderer.quads3D.size(), 3u);

    // the building base and separate prop each contribute one ground contact.
    int onGround = 0;
    for (const auto& quad : renderer.quads3D)
    {
        if (std::abs(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].y) < kTol)
        {
            ++onGround;
        }
    }
    EXPECT_EQ(onGround, 2);

    std::vector<float> groundDepths;
    groundDepths.reserve(renderer.quads3D.size());
    for (const auto& quad : renderer.quads3D)
    {
        if (std::abs(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].y) < kTol)
        {
            groundDepths.push_back(quad.corners[sceneMath::QUAD_BOTTOM_LEFT].z);
        }
    }
    ASSERT_EQ(groundDepths.size(), 2u);
    std::sort(groundDepths.begin(), groundDepths.end());
    EXPECT_NEAR(groundDepths[0], static_cast<float>((11 + 1) * kTileSize), kTol);
    EXPECT_NEAR(groundDepths[1], static_cast<float>((12 + 1) * kTileSize), kTol);
}

TEST(World3DStackingTest, APropDoesNotJoinAnAdjacentStructuresRun)
{
    // a neighboring Prop must turn independently of a wide Structure's fixed yaw.
    Tilemap tm = MakeMap();
    PaintUprightRow(tm, 20, 12, 2);      // building, columns 20-21
    tm.SetLayerTile(22, 12, kLayer, 1);  // post, column 22
    tm.SetLayerStance(22, 12, kLayer, TileStance::Prop);

    const glm::vec2 target{21 * kTileSize, 12 * kTileSize};

    auto quadsAtYaw = [&](float degrees)
    {
        cameraRig::RigParams rig = MakeRig(target);
        rig.yawRadians = Degrees(degrees);
        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        return SortedByRunOrder(renderer.quads3D);
    };

    const auto straight = quadsAtYaw(0.0f);
    const auto turned = quadsAtYaw(60.0f);
    ASSERT_EQ(straight.size(), 3u);
    ASSERT_EQ(turned.size(), 3u);

    for (size_t i = 0; i < 2; ++i)
    {
        for (int c = 0; c < sceneMath::QUAD_CORNER_COUNT; ++c)
        {
            EXPECT_NEAR(glm::distance(turned[i].corners[c], straight[i].corners[c]), 0.0f, kTol)
                << "wall tile " << i << " corner " << c << " moved";
        }
    }

    EXPECT_GT(glm::distance(turned[2].corners[sceneMath::QUAD_BOTTOM_LEFT],
                            straight[2].corners[sceneMath::QUAD_BOTTOM_LEFT]),
              0.1f)
        << "the post was absorbed into the wall's run and frozen with it";

    const glm::vec3 mid = (turned[2].corners[sceneMath::QUAD_BOTTOM_LEFT] +
                           turned[2].corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                          0.5f;
    EXPECT_NEAR(mid.x, 22 * kTileSize + kTileSize * 0.5f, kTol);
    EXPECT_NEAR(mid.z, static_cast<float>((12 + 1) * kTileSize), kTol);
}

TEST(World3DStackingTest, NorthSouthWallRunIsGridLocked)
{
    // Wall uses fixed yaw regardless of run direction; rotating each panel would
    // move its edges by +/-(tileW/2)*sin(yaw) in Z and open gaps.
    Tilemap tm = MakeMap();
    PaintStanceColumn(tm, 20, 10, 3, TileStance::Wall);

    const glm::vec2 target{20 * kTileSize + kTileSize * 0.5f, 11 * kTileSize};

    auto quadsAtYaw = [&](float degrees)
    {
        cameraRig::RigParams rig = MakeRig(target);
        rig.yawRadians = Degrees(degrees);
        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        return SortedByHeight(renderer.quads3D);
    };

    const auto straight = quadsAtYaw(0.0f);
    ASSERT_EQ(straight.size(), 3u);

    for (const float degrees : {-120.0f, -45.0f, 30.0f, 60.0f, 150.0f})
    {
        const auto turned = quadsAtYaw(degrees);
        ASSERT_EQ(turned.size(), 3u) << "yaw " << degrees;
        for (size_t i = 0; i < turned.size(); ++i)
        {
            for (int c = 0; c < sceneMath::QUAD_CORNER_COUNT; ++c)
            {
                EXPECT_NEAR(glm::distance(turned[i].corners[c], straight[i].corners[c]), 0.0f, kTol)
                    << "panel " << i << " corner " << c << " swung at yaw " << degrees;
            }
        }
    }
}

TEST(World3DStackingTest, AdjacentPropsBothStillTurn)
{
    // adjacent Props each keep their own rotating anchor; proximity must not lock their yaw.
    Tilemap tm = MakeMap();
    for (int x = 20; x <= 21; ++x)
    {
        tm.SetLayerTile(x, 12, kLayer, 1);
        tm.SetLayerStance(x, 12, kLayer, TileStance::Prop);
    }

    const glm::vec2 target{21 * kTileSize, 12 * kTileSize};

    auto quadsAtYaw = [&](float degrees)
    {
        cameraRig::RigParams rig = MakeRig(target);
        rig.yawRadians = Degrees(degrees);
        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);
        return SortedByRunOrder(renderer.quads3D);
    };

    const auto straight = quadsAtYaw(0.0f);
    const auto turned = quadsAtYaw(60.0f);
    ASSERT_EQ(straight.size(), 2u);
    ASSERT_EQ(turned.size(), 2u);

    for (size_t i = 0; i < turned.size(); ++i)
    {
        EXPECT_GT(glm::distance(turned[i].corners[sceneMath::QUAD_BOTTOM_LEFT],
                                straight[i].corners[sceneMath::QUAD_BOTTOM_LEFT]),
                  0.1f)
            << "prop " << i << " was frozen by its neighbour";

        const glm::vec3 mid = (turned[i].corners[sceneMath::QUAD_BOTTOM_LEFT] +
                               turned[i].corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                              0.5f;
        EXPECT_NEAR(mid.x, (20 + static_cast<int>(i)) * kTileSize + kTileSize * 0.5f, kTol);
        EXPECT_NEAR(mid.z, static_cast<float>((12 + 1) * kTileSize), kTol);
    }
}

TEST(World3DStackingTest, AdjacentWallsShareAnEdgeExactlyAtEveryYaw)
{
    // at yawFollow zero, world +X is the right axis; neighboring Walls derive
    // shared edges from identical grid expressions.
    Tilemap tm = MakeMap();
    for (int x = 20; x <= 21; ++x)
    {
        tm.SetLayerTile(x, 12, kLayer, 1);
        tm.SetLayerStance(x, 12, kLayer, TileStance::Wall);
    }

    for (const float degrees : {0.0f, 35.0f, 90.0f, -75.0f})
    {
        cameraRig::RigParams rig = MakeRig({21 * kTileSize, 12 * kTileSize});
        rig.yawRadians = Degrees(degrees);
        MockRenderer renderer;
        tm.RenderWorld3D(renderer, rig);

        const auto quads = SortedByRunOrder(renderer.quads3D);
        ASSERT_EQ(quads.size(), 2u) << "yaw " << degrees;
        EXPECT_NEAR(glm::distance(quads[0].corners[sceneMath::QUAD_TOP_RIGHT],
                                  quads[1].corners[sceneMath::QUAD_TOP_LEFT]),
                    0.0f,
                    kTol)
            << "top seam opened at yaw " << degrees;
        EXPECT_NEAR(glm::distance(quads[0].corners[sceneMath::QUAD_BOTTOM_RIGHT],
                                  quads[1].corners[sceneMath::QUAD_BOTTOM_LEFT]),
                    0.0f,
                    kTol)
            << "bottom seam opened at yaw " << degrees;
    }
}

TEST(World3DStackingTest, ElevationWithoutARoleDoesNotDisplaceTiles)
{
    // cell elevation drives physical surfaces; only a layer's ElevationRole lifts its artwork.
    Tilemap plain = MakeMap();
    plain.SetLayerTile(20, 12, kLayer, 1);

    Tilemap elevated = MakeMap();
    elevated.SetLayerTile(20, 12, kLayer, 1);
    elevated.SetElevation(20, 12, 5);

    const cameraRig::RigParams rig = MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize});

    MockRenderer plainRenderer;
    plain.RenderWorld3D(plainRenderer, rig);
    MockRenderer elevatedRenderer;
    elevated.RenderWorld3D(elevatedRenderer, rig);

    ASSERT_EQ(plainRenderer.quads3D.size(), 1u);
    ASSERT_EQ(elevatedRenderer.quads3D.size(), 1u);
    for (int c = 0; c < sceneMath::QUAD_CORNER_COUNT; ++c)
    {
        EXPECT_EQ(plainRenderer.quads3D[0].corners[c], elevatedRenderer.quads3D[0].corners[c])
            << "corner " << c << " moved without a role";
    }
}

TEST(World3DStackingTest, RaisedTilesSitAtTheirCellElevation)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetElevation(20, 12, 6);
    tm.SetLayerElevationRole(20, 12, kLayer, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    for (const glm::vec3& corner : renderer.quads3D[0].corners)
    {
        EXPECT_NEAR(corner.y, 6.0f, kTol);
    }
}

TEST(World3DStackingTest, LayersAtOneCellRiseIndependently)
{
    // water on layer 0 stays at ground height while the deck on layer 2 rises.
    Tilemap tm = MakeMap();
    tm.SetElevation(20, 12, 6);
    tm.SetLayerTile(20, 12, 0, 1);
    tm.SetLayerTile(20, 12, 2, 1);
    tm.SetLayerElevationRole(20, 12, 2, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 2u);
    const auto quads = SortedByHeight(renderer.quads3D);
    EXPECT_NEAR(quads.front().corners[sceneMath::QUAD_BOTTOM_LEFT].y, 0.0f, kTol)
        << "the unmarked layer was lifted with the cell";
    EXPECT_NEAR(quads.back().corners[sceneMath::QUAD_BOTTOM_LEFT].y, 6.0f, kTol);
}

TEST(World3DStackingTest, ARampRunMeetsTheDeckWithNoSeam)
{
    // along X: ground 0 -> ramp 2 -> ramp 4 -> deck 6.
    // each high edge must meet the next low edge.
    Tilemap tm = MakeMap();
    for (int x = 20; x <= 24; ++x)
    {
        tm.SetLayerTile(x, 12, kLayer, 1);
    }
    tm.SetElevation(21, 12, 2);
    tm.SetElevation(22, 12, 4);
    tm.SetElevation(23, 12, 6);
    tm.SetElevation(24, 12, 6);
    tm.SetLayerElevationRole(21, 12, kLayer, ElevationRole::Ramp);
    tm.SetLayerElevationRole(22, 12, kLayer, ElevationRole::Ramp);
    tm.SetLayerElevationRole(23, 12, kLayer, ElevationRole::Raised);
    tm.SetLayerElevationRole(24, 12, kLayer, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({22 * kTileSize, 12 * kTileSize}));

    const std::vector<MockRenderer::Quad3D> quads = SortedByRunOrder(renderer.quads3D);
    ASSERT_EQ(quads.size(), 5u);

    const auto westY = [](const MockRenderer::Quad3D& q)
    { return q.corners[sceneMath::QUAD_TOP_LEFT].y; };
    const auto eastY = [](const MockRenderer::Quad3D& q)
    { return q.corners[sceneMath::QUAD_TOP_RIGHT].y; };

    EXPECT_NEAR(westY(quads[0]), 0.0f, kTol);  // x=20, bare ground
    EXPECT_NEAR(eastY(quads[0]), 0.0f, kTol);
    EXPECT_NEAR(westY(quads[1]), 0.0f, kTol);  // x=21, ramp 0 -> 3
    EXPECT_NEAR(eastY(quads[1]), 3.0f, kTol);
    EXPECT_NEAR(westY(quads[2]), 3.0f, kTol);  // x=22, ramp 3 -> 6
    EXPECT_NEAR(eastY(quads[2]), 6.0f, kTol);
    EXPECT_NEAR(westY(quads[3]), 6.0f, kTol);  // x=23, deck, level
    EXPECT_NEAR(eastY(quads[3]), 6.0f, kTol);
    EXPECT_NEAR(westY(quads[4]), 6.0f, kTol);  // x=24, deck, level
    EXPECT_NEAR(eastY(quads[4]), 6.0f, kTol);

    for (size_t i = 0; i + 1 < quads.size(); ++i)
    {
        EXPECT_NEAR(eastY(quads[i]), westY(quads[i + 1]), kTol) << "seam " << i << " opened";
    }
}

TEST(World3DStackingTest, ARampReadsNeighboursOnItsOwnLayerOnly)
{
    // ramp edges read the neighboring role on the same layer; raw elevation
    // cannot raise a neighbor whose role is Ground.
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetLayerTile(21, 12, kLayer, 1);
    tm.SetElevation(20, 12, 6);  // neighbour cell IS elevated...
    tm.SetElevation(21, 12, 4);

    tm.SetLayerElevationRole(21, 12, kLayer, ElevationRole::Ramp);

    tm.SetLayerTile(22, 12, kLayer, 1);
    tm.SetElevation(22, 12, 6);
    tm.SetLayerElevationRole(22, 12, kLayer, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({21 * kTileSize, 12 * kTileSize}));

    const std::vector<MockRenderer::Quad3D> quads = SortedByRunOrder(renderer.quads3D);
    ASSERT_EQ(quads.size(), 3u);

    EXPECT_NEAR(quads[1].corners[sceneMath::QUAD_TOP_LEFT].y, 0.0f, kTol)
        << "the ramp read the neighbour's cell elevation instead of its layer role";

    EXPECT_NEAR(quads[1].corners[sceneMath::QUAD_TOP_RIGHT].y, 6.0f, kTol)
        << "the ramp failed to climb toward a participating neighbour";
}

TEST(World3DStackingTest, ARampRunMeetsTheDeckWithNoSeamGoingNorthSouth)
{
    // the Y-axis ramp must select ElevationAxis::Y and sample neighbors along scene Z.
    Tilemap tm = MakeMap();
    for (int y = 20; y <= 23; ++y)
    {
        tm.SetLayerTile(20, y, kLayer, 1);
    }
    tm.SetElevation(20, 21, 2);
    tm.SetElevation(20, 22, 4);
    tm.SetElevation(20, 23, 6);
    tm.SetLayerElevationRole(20, 21, kLayer, ElevationRole::Ramp);
    tm.SetLayerElevationRole(20, 22, kLayer, ElevationRole::Ramp);
    tm.SetLayerElevationRole(20, 23, kLayer, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 22 * kTileSize}));

    // sort by scene Z because this run advances south in one column.
    const std::vector<MockRenderer::Quad3D> quads = SortedByDepth(renderer.quads3D);
    ASSERT_EQ(quads.size(), 4u);

    // along Z, TOP_LEFT/TOP_RIGHT form the north edge; BOTTOM_LEFT/BOTTOM_RIGHT form the south
    // edge.
    const auto northY = [](const MockRenderer::Quad3D& q)
    { return q.corners[sceneMath::QUAD_TOP_LEFT].y; };
    const auto southY = [](const MockRenderer::Quad3D& q)
    { return q.corners[sceneMath::QUAD_BOTTOM_LEFT].y; };

    EXPECT_NEAR(northY(quads[0]), 0.0f, kTol);  // y=20, bare ground
    EXPECT_NEAR(southY(quads[0]), 0.0f, kTol);
    EXPECT_NEAR(northY(quads[1]), 0.0f, kTol);  // y=21, ramp 0 -> 3
    EXPECT_NEAR(southY(quads[1]), 3.0f, kTol);
    EXPECT_NEAR(northY(quads[2]), 3.0f, kTol);  // y=22, ramp 3 -> 6
    EXPECT_NEAR(southY(quads[2]), 6.0f, kTol);
    EXPECT_NEAR(northY(quads[3]), 6.0f, kTol);  // y=23, deck, level
    EXPECT_NEAR(southY(quads[3]), 6.0f, kTol);

    for (size_t i = 0; i + 1 < quads.size(); ++i)
    {
        EXPECT_NEAR(southY(quads[i]), northY(quads[i + 1]), kTol) << "seam " << i << " opened";
    }
}

TEST(World3DStackingTest, TileRotationReachesTheQuad)
{
    Tilemap plain = MakeMap();
    plain.SetLayerTile(20, 12, kLayer, 1);

    Tilemap turned = MakeMap();
    turned.SetLayerTile(20, 12, kLayer, 1);
    turned.SetLayerRotation(20, 12, kLayer, 90.0f);

    const cameraRig::RigParams rig = MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize});

    MockRenderer plainRenderer;
    MockRenderer turnedRenderer;
    plain.RenderWorld3D(plainRenderer, rig);
    turned.RenderWorld3D(turnedRenderer, rig);

    ASSERT_EQ(plainRenderer.quads3D.size(), 1u);
    ASSERT_EQ(turnedRenderer.quads3D.size(), 1u);

    for (int i = 0; i < sceneMath::QUAD_CORNER_COUNT; ++i)
    {
        const int next = (i + 1) % sceneMath::QUAD_CORNER_COUNT;
        EXPECT_NEAR(glm::distance(turnedRenderer.quads3D[0].corners[i],
                                  plainRenderer.quads3D[0].corners[next]),
                    0.0f,
                    kTol)
            << "corner " << i;
    }
}

TEST(World3DStackingTest, GroundTilesStayFlatAndInPlace)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    for (const glm::vec3& corner : renderer.quads3D[0].corners)
    {
        EXPECT_NEAR(corner.y, 0.0f, kTol) << "ground tile is not flat";
    }
    EXPECT_NEAR(renderer.quads3D[0].corners[sceneMath::QUAD_TOP_LEFT].z,
                static_cast<float>(12 * kTileSize),
                kTol);
}

TEST(World3DStackingTest, UprightFeetRiseToTheDeck)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetElevation(20, 12, 6);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Wall);
    tm.SetLayerElevationRole(20, 12, kLayer, ElevationRole::Raised);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    EXPECT_NEAR(renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT].y, 6.0f, kTol);

    EXPECT_NEAR(glm::distance(renderer.quads3D[0].corners[sceneMath::QUAD_TOP_LEFT],
                              renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT]),
                static_cast<float>(kTileSize),
                kTol);
}

TEST(World3DStackingTest, AnUnmarkedUprightTileStillStandsOnZero)
{
    Tilemap tm = MakeMap();
    tm.SetLayerTile(20, 12, kLayer, 1);
    tm.SetElevation(20, 12, 6);
    tm.SetLayerStance(20, 12, kLayer, TileStance::Wall);

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({20 * kTileSize + kTileSize * 0.5f, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 1u);
    EXPECT_NEAR(renderer.quads3D[0].corners[sceneMath::QUAD_BOTTOM_LEFT].y, 0.0f, kTol);
}

TEST(World3DStackingTest, ARaisedStructureRunStaysRigid)
{
    // one foot height keeps all Structure slices joined. footColumn = (20+21)/2 = 20,
    // so the shared height is 6, not column 21's 10 or their average.
    Tilemap tm = MakeMap();
    for (int x = 20; x <= 21; ++x)
    {
        tm.SetLayerTile(x, 12, kLayer, 1);
        tm.SetLayerStance(x, 12, kLayer, TileStance::Structure);
        tm.SetLayerElevationRole(x, 12, kLayer, ElevationRole::Raised);
    }
    tm.SetElevation(20, 12, 6);
    tm.SetElevation(21, 12, 10);  // deliberately uneven

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({21 * kTileSize, 12 * kTileSize}));

    ASSERT_EQ(renderer.quads3D.size(), 2u);
    const auto quads = SortedByRunOrder(renderer.quads3D);
    EXPECT_NEAR(quads[0].corners[sceneMath::QUAD_BOTTOM_LEFT].y,
                quads[1].corners[sceneMath::QUAD_BOTTOM_LEFT].y,
                kTol)
        << "the structure tilted instead of staying rigid";
    EXPECT_NEAR(quads[0].corners[sceneMath::QUAD_BOTTOM_LEFT].y, 6.0f, kTol)
        << "the shared foot did not land on the run's centre-column elevation";
}

TEST(StructureFacadeTest, SurfaceHeightReadsOnlyOptedInLayers)
{
    Tilemap tm = MakeMap(80, 60);
    ASSERT_GE(tm.GetLayerCount(), 2u);

    // Cell (10, 10) spans world x 160..176 and y 160..176, so (168, 168) is inside it.
    tm.SetLayerTile(10, 10, kLayer, 1);
    tm.SetElevation(10, 10, 8);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({168.0f, 168.0f}), 0.0f, kTol)
        << "elevation alone lifted the surface without a role opting in";

    tm.SetLayerElevationRole(10, 10, kLayer, ElevationRole::Raised);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({168.0f, 168.0f}), 8.0f, kTol);

    tm.SetLayerTile(10, 10, 1, 1);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({168.0f, 168.0f}), 8.0f, kTol);

    tm.SetLayerTile(10, 10, kLayer, -1);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({168.0f, 168.0f}), 0.0f, kTol);
}

TEST(StructureFacadeTest, SurfaceHeightIsZeroOffMapAndOnEmptyCells)
{
    const Tilemap tm = MakeMap(80, 60);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({-8.0f, -8.0f}), 0.0f, kTol);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({1e6f, 1e6f}), 0.0f, kTol);
    EXPECT_NEAR(tm.SurfaceHeightAtWorldPos({168.0f, 168.0f}), 0.0f, kTol);
}

TEST(StructureFacadeTest, FacadeReportsTheAutoFloodFillBody)
{
    Tilemap tm = MakeMap(80, 60);
    PaintStructureBlock(tm, 62, 30, 3, 3);

    // (1010, 500) is cell (63, 31), the middle of the block.
    const auto facade = tm.FindStructureFacade({1010.0f, 500.0f});
    ASSERT_TRUE(facade.has_value());

    // runCentreX = (62 + 64 + 1) * 0.5 * 16 and baseSouthEdgeY = (32 + 1) * 16.
    // every cell is elevation 0 on a Ground role, so the foot stays on the ground.
    EXPECT_EQ(facade->widthTiles, 3);
    EXPECT_NEAR(facade->runCentreX, 1016.0f, kTol);
    EXPECT_NEAR(facade->baseSouthEdgeY, 528.0f, kTol);
    EXPECT_NEAR(facade->foot.x, 1016.0f, kTol);
    EXPECT_NEAR(facade->foot.y, 0.0f, kTol);
    EXPECT_NEAR(facade->foot.z, 528.0f, kTol);
}

TEST(StructureFacadeTest, FacadeAgreesWithTheTilePass)
{
    Tilemap tm = MakeMap(80, 60);
    PaintStructureBlock(tm, 62, 30, 3, 3);

    const auto facade = tm.FindStructureFacade({1010.0f, 500.0f});
    ASSERT_TRUE(facade.has_value());

    MockRenderer renderer;
    tm.RenderWorld3D(renderer, MakeRig({1016.0f, 500.0f}));

    std::vector<MockRenderer::Quad3D> upright;
    for (const MockRenderer::Quad3D& quad : renderer.quads3D)
    {
        if (quad.depth == renderModes::DepthMode::TestAndWrite)
        {
            upright.push_back(quad);
        }
    }
    ASSERT_EQ(upright.size(), 9u);

    // billboard::MakeQuad anchors at the feet; the base-row center tile has zero
    // slice offset and lift, so its bottom midpoint is the decal anchor.
    bool matched = false;
    for (const MockRenderer::Quad3D& quad : upright)
    {
        const glm::vec3 bottomCentre = (quad.corners[sceneMath::QUAD_BOTTOM_LEFT] +
                                        quad.corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                                       0.5f;
        if (glm::distance(bottomCentre, facade->foot) < kTol)
        {
            matched = true;
        }
    }
    EXPECT_TRUE(matched) << "no tile quad stands on the reported facade foot";
}

TEST(StructureFacadeTest, FacadeIsFoundFromAboveButNotBeyondTheWalk)
{
    Tilemap tm = MakeMap(80, 60);
    PaintStructureBlock(tm, 62, 30, 3, 3);

    const auto fromAbove = tm.FindStructureFacade({1010.0f, 424.0f});
    ASSERT_TRUE(fromAbove.has_value());
    EXPECT_NEAR(fromAbove->runCentreX, 1016.0f, kTol);
    EXPECT_NEAR(fromAbove->baseSouthEdgeY, 528.0f, kTol);

    EXPECT_FALSE(tm.FindStructureFacade({1010.0f, 328.0f}).has_value());

    EXPECT_FALSE(tm.FindStructureFacade({100.0f, 100.0f}).has_value());
}

TEST(StructureFacadeTest, FacadeUsesTheAuthoredStructureBounds)
{
    Tilemap tm = MakeMap(80, 60);
    PaintStructureBlock(tm, 20, 10, 3, 3);

    const int id = tm.AddNoProjectionStructure({320.0f, 208.0f}, {368.0f, 208.0f});
    for (int y = 10; y <= 12; ++y)
    {
        for (int x = 20; x <= 22; ++x)
        {
            tm.SetTileStructureId(x, y, 1, id);
        }
    }

    // an extra automatic cell would extend the contiguity scan; width 3 confirms
    // that the authored group bounds take precedence.
    tm.SetLayerTile(23, 11, kLayer, 1);
    tm.SetLayerStance(23, 11, kLayer, TileStance::Structure);

    const auto facade = tm.FindStructureFacade({336.0f, 176.0f});
    ASSERT_TRUE(facade.has_value());
    EXPECT_EQ(facade->widthTiles, 3);
    EXPECT_NEAR(facade->runCentreX, 344.0f, kTol);
    EXPECT_NEAR(facade->baseSouthEdgeY, 208.0f, kTol);
    EXPECT_NEAR(facade->foot.x, 344.0f, kTol);
    EXPECT_NEAR(facade->foot.y, 0.0f, kTol);
    EXPECT_NEAR(facade->foot.z, 208.0f, kTol);
}

TEST(StructureFacadeTest, FacadeLiftsWithTheBodysFootHeight)
{
    Tilemap tm = MakeMap(80, 60);
    PaintStructureBlock(tm, 62, 30, 3, 3);

    // foot height comes from center column (62 + 64) / 2 = 63 at base row 32.
    tm.SetElevation(63, 32, 8);
    tm.SetLayerElevationRole(63, 32, kLayer, ElevationRole::Raised);

    const auto facade = tm.FindStructureFacade({1010.0f, 500.0f});
    ASSERT_TRUE(facade.has_value());
    EXPECT_NEAR(facade->foot.y, 8.0f, kTol);
    EXPECT_NEAR(facade->foot.x, 1016.0f, kTol);
    EXPECT_NEAR(facade->foot.z, 528.0f, kTol);
}
