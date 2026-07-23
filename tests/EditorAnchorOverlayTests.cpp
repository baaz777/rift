// touching Structure bodies on different layers need separate anchor pairs.
#include "../src/CameraController.hpp"
#include "../src/Editor.hpp"
#include "../src/ParticleSystem.hpp"
#include "../src/Tilemap.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include <algorithm>
#include <vector>

namespace
{
// layer indices are zero-based; 2 and 3 are the object layers.
constexpr size_t kObjects = 2;
constexpr size_t kObjects2 = 3;
constexpr int kTileSize = 16;

// each cross has one horizontal bar of this size; its center gives the anchor position.
constexpr float kCrossBarLength = 12.0f;
constexpr float kCrossBarThickness = 2.0f;

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

// camera at the origin makes screen pixels equal world pixels; sort anchors west to east.
std::vector<glm::vec2> RenderedCrossCentres(Tilemap& tm)
{
    CameraState camera;
    entt::registry registry;
    MockRenderer renderer;
    ParticleSystem particles;
    const EditorContext ctx{
        nullptr, 800, 600, 50, 38, camera, tm, entt::null, registry, renderer, particles, ""};

    Editor editor;
    editor.SetDebugMode(true);  // arms the anchor pass
    editor.RenderNoProjectionAnchors(ctx);

    std::vector<glm::vec2> centres;
    for (const MockRenderer::Rect& rect : renderer.rects)
    {
        if (rect.size == glm::vec2(kCrossBarLength, kCrossBarThickness))
        {
            centres.push_back(rect.position +
                              glm::vec2(kCrossBarLength * 0.5f, kCrossBarThickness * 0.5f));
        }
    }
    std::sort(centres.begin(),
              centres.end(),
              [](const glm::vec2& a, const glm::vec2& b) { return a.x < b.x; });
    return centres;
}

glm::vec2 AnchorAt(int column, int row)
{
    return {static_cast<float>(column * kTileSize), static_cast<float>((row + 1) * kTileSize)};
}
}  // namespace

TEST(EditorAnchorOverlayTest, AdjacentBlocksOnDifferentLayersGetTwoAnchorPairs)
{
    // layer 2: columns 10-11; layer 3: columns 12-13. both occupy rows 10-11.
    Tilemap tm;
    tm.SetTilemapSize(40, 40, false);
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects2, 12, 10, 2, 2);

    const std::vector<glm::vec2> centres = RenderedCrossCentres(tm);
    ASSERT_EQ(centres.size(), 4u) << "one anchor pair per body";

    // all anchors lie on row 11's bottom edge; the middle pair coincides at the shared boundary.
    EXPECT_EQ(centres[0], AnchorAt(10, 11));
    EXPECT_EQ(centres[1], AnchorAt(12, 11));
    EXPECT_EQ(centres[2], AnchorAt(12, 11));
    EXPECT_EQ(centres[3], AnchorAt(14, 11));
}

TEST(EditorAnchorOverlayTest, AdjacentBlocksOnOneLayerShareOneAnchorPair)
{
    // touching blocks on one layer form one body and share an anchor pair.
    Tilemap tm;
    tm.SetTilemapSize(40, 40, false);
    PaintStructureBlock(tm, kObjects, 10, 10, 2, 2);
    PaintStructureBlock(tm, kObjects, 12, 10, 2, 2);

    const std::vector<glm::vec2> centres = RenderedCrossCentres(tm);
    ASSERT_EQ(centres.size(), 2u) << "touching bodies on one layer share one pair";
    EXPECT_EQ(centres[0], AnchorAt(10, 11));
    EXPECT_EQ(centres[1], AnchorAt(14, 11));
}
