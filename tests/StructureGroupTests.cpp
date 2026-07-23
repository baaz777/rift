// structure groups use four-way connectivity on one layer; touching bodies on different layers stay
// separate.

#include "../src/Tilemap.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

constexpr size_t kObjects = 2;
constexpr size_t kObjects2 = 3;

Tilemap MakeMap(int width = 40, int height = 40)
{
    Tilemap tm;
    tm.SetTilemapSize(width, height, false);
    return tm;
}

void PaintStructureBlock(Tilemap& tm, size_t layer, int leftX, int topY, int width, int height)
{
    for (int dy = 0; dy < height; ++dy)
    {
        for (int dx = 0; dx < width; ++dx)
        {
            tm.SetLayerTile(leftX + dx, topY + dy, layer, 1);
            tm.SetLayerStance(leftX + dx, topY + dy, layer, TileStance::Structure);
        }
    }
}

void ExpectBox(const Tilemap::StructureBounds& box, int minX, int maxX, int minY, int maxY)
{
    EXPECT_EQ(box.minX, minX) << "minX";
    EXPECT_EQ(box.maxX, maxX) << "maxX";
    EXPECT_EQ(box.minY, minY) << "minY";
    EXPECT_EQ(box.maxY, maxY) << "maxY";
}
}  // namespace

TEST(StructureGroupTest, AdjacentBlocksOnDifferentLayersStayTwoGroups)
{
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects2, 12, 10, 2, 2);

    const std::vector<Tilemap::StructureBounds> groups2 = tm.FindStructureGroups(kObjects);
    const std::vector<Tilemap::StructureBounds> groups3 = tm.FindStructureGroups(kObjects2);
    ASSERT_EQ(groups2.size(), 1u) << "layer 2 must hold exactly one group";
    ASSERT_EQ(groups3.size(), 1u) << "layer 3 must hold exactly one group";
    {
        SCOPED_TRACE("layer 2 box");
        ExpectBox(groups2[0], 10, 11, 10, 11);
    }
    {
        SCOPED_TRACE("layer 3 box");
        ExpectBox(groups3[0], 12, 13, 10, 11);
    }
    EXPECT_LT(groups2[0].maxX, groups3[0].minX) << "the layer-2 box swallowed the layer-3 body";
}

TEST(StructureGroupTest, AdjacentBlocksOnOneLayerAreOneGroup)
{
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects, 12, 10, 2, 2);

    const std::vector<Tilemap::StructureBounds> groups = tm.FindStructureGroups(kObjects);
    ASSERT_EQ(groups.size(), 1u) << "touching blocks on one layer must merge";
    ExpectBox(groups[0], 10, 13, 10, 11);
    EXPECT_TRUE(tm.FindStructureGroups(kObjects2).empty());
}

TEST(StructureGroupTest, AGapSeparatesBlocksOnOneLayer)
{
    // the row-major seed scan reports the western body first.
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects, 13, 10, 2, 2);

    const std::vector<Tilemap::StructureBounds> groups = tm.FindStructureGroups(kObjects);
    ASSERT_EQ(groups.size(), 2u);
    {
        SCOPED_TRACE("western box");
        ExpectBox(groups[0], 10, 11, 10, 11);
    }
    {
        SCOPED_TRACE("eastern box");
        ExpectBox(groups[1], 13, 14, 10, 11);
    }
}

TEST(StructureGroupTest, AMultiRowGroupReportsItsBottomRow)
{
    // anchors use (maxY + 1) * tileHeight, so bounds must include the body's southernmost row.
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 20, 8, 1, 3);

    const std::vector<Tilemap::StructureBounds> groups = tm.FindStructureGroups(kObjects);
    ASSERT_EQ(groups.size(), 1u);
    ExpectBox(groups[0], 20, 20, 8, 10);
    EXPECT_EQ(groups[0].maxY, 10) << "anchors must sit under the bottom row";
    EXPECT_EQ((groups[0].maxY + 1) * tm.GetTileHeight(), 176)
        << "bottom anchor pixel row for tile height 16";
}

TEST(StructureGroupTest, StackedBlocksOnTwoLayersAreOneGroupPerLayer)
{
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects2, 10, 10, 2, 2);

    const std::vector<Tilemap::StructureBounds> groups2 = tm.FindStructureGroups(kObjects);
    const std::vector<Tilemap::StructureBounds> groups3 = tm.FindStructureGroups(kObjects2);
    ASSERT_EQ(groups2.size(), 1u);
    ASSERT_EQ(groups3.size(), 1u);
    {
        SCOPED_TRACE("layer 2 box");
        ExpectBox(groups2[0], 10, 11, 10, 11);
    }
    {
        SCOPED_TRACE("layer 3 box");
        ExpectBox(groups3[0], 10, 11, 10, 11);
    }
    EXPECT_EQ(groups2.size() + groups3.size(), 2u) << "the editor must see one entry per layer";
    EXPECT_TRUE(tm.FindStructureGroups(0).empty()) << "an untouched layer reports nothing";
}

TEST(StructureGroupTest, WallAndPropNeverJoinAGroup)
{
    // only Structure cells contribute to a group; nearby props and walls remain separate.
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    tm.SetLayerTile(12, 10, kObjects, 1);
    tm.SetLayerStance(12, 10, kObjects, TileStance::Wall);
    tm.SetLayerTile(12, 11, kObjects, 1);
    tm.SetLayerStance(12, 11, kObjects, TileStance::Prop);

    const std::vector<Tilemap::StructureBounds> groups = tm.FindStructureGroups(kObjects);
    ASSERT_EQ(groups.size(), 1u) << "Wall/Prop cells must not seed groups of their own";
    ExpectBox(groups[0], 10, 11, 10, 11);
}

TEST(StructureGroupTest, ALayerPastTheStackHasNoGroups)
{
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);

    EXPECT_TRUE(tm.FindStructureGroups(tm.GetLayerCount()).empty());
    EXPECT_TRUE(tm.FindStructureGroups(tm.GetLayerCount() + 5).empty());
    ASSERT_EQ(tm.FindStructureGroups(kObjects).size(), 1u)
        << "a bad index must not disturb the scratch buffer for the next call";
}

TEST(StructureGroupTest, AGroupOnTheMapEdgeIsBoundedByTheMap)
{
    Tilemap tm = MakeMap();
    PaintStructureBlock(tm, kObjects, 0, 0, 2, 2);
    PaintStructureBlock(tm, kObjects, 38, 38, 2, 2);

    const std::vector<Tilemap::StructureBounds> groups = tm.FindStructureGroups(kObjects);
    ASSERT_EQ(groups.size(), 2u);
    {
        SCOPED_TRACE("north-west corner box");
        ExpectBox(groups[0], 0, 1, 0, 1);
    }
    {
        SCOPED_TRACE("south-east corner box");
        ExpectBox(groups[1], 38, 39, 38, 39);
    }
}
