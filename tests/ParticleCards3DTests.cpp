// the sheet lies at focus depth, where one world pixel maps to one flat-frame
// pixel at every camera orientation. MockRenderer also checks particle pass
// selection and submission order.
#include "../src/CameraRig.hpp"
#include "../src/MathConstants.hpp"
#include "../src/ParticleCards.hpp"
#include "../src/ParticleSystem.hpp"
#include "../src/SceneMath.hpp"
#include "../src/Tilemap.hpp"
#include "MockRenderer.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace
{
constexpr float kTol = 1e-3f;
constexpr glm::vec2 kScreen{1520.0f, 855.0f};

constexpr float Degrees(float d)
{
    return d * rift::PiF / 180.0f;
}

cameraRig::RigParams MakeRig(cameraRig::Preset preset, float pitchDeg = 0.0f, float yawDeg = 0.0f)
{
    cameraRig::RigParams params;
    params.target = {1000.0f, 500.0f};
    params.visibleWorldSize = {320.0f, 180.0f};
    params.sceneRadius = 4096.0f;
    params.pitchRadians = Degrees(pitchDeg);
    params.yawRadians = Degrees(yawDeg);
    cameraRig::ApplyPreset(params, preset);
    return params;
}

// the camera set includes Classic, the DS preset, and two free orbits; pitch 12 stays above the
// clamp.
std::array<cameraRig::RigParams, 4> AllRigs()
{
    return {MakeRig(cameraRig::Preset::Classic),
            MakeRig(cameraRig::Preset::DS),
            MakeRig(cameraRig::Preset::Free, 35.0f, 45.0f),
            MakeRig(cameraRig::Preset::Free, 12.0f, 217.0f)};
}

void ExpectVec3Near(glm::vec3 actual, glm::vec3 expected, float tolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

// the update camera is centered on (1000, 500), matching the rigs so spawned zones stay visible.
constexpr glm::vec2 kFlatCam{840.0f, 410.0f};
constexpr glm::vec2 kFlatView{320.0f, 180.0f};

// 1280x960 world pixels at 16 px per tile place cell (62, 30) near (1000, 500).
Tilemap MakeMap80x60()
{
    Tilemap map;
    map.SetTilemapSize(80, 60, false);
    return map;
}

int QuadsWithDepth(const MockRenderer& renderer, renderModes::DepthMode depth)
{
    return static_cast<int>(std::count_if(renderer.quads3D.begin(),
                                          renderer.quads3D.end(),
                                          [depth](const MockRenderer::Quad3D& quad)
                                          { return quad.depth == depth; }));
}

// a parallelogram's center is the mean of its corners at any rotation.
glm::vec3 QuadCentre(const MockRenderer::Quad3D& quad)
{
    return (quad.corners[sceneMath::QUAD_TOP_LEFT] + quad.corners[sceneMath::QUAD_TOP_RIGHT] +
            quad.corners[sceneMath::QUAD_BOTTOM_RIGHT] +
            quad.corners[sceneMath::QUAD_BOTTOM_LEFT]) *
           0.25f;
}

void RunSpawnLoop(ParticleSystem& ps)
{
    for (int frame = 0; frame < 20; ++frame)
    {
        ps.Update(0.05f, kFlatCam, kFlatView);
    }
}

bool AnyParticleAtScenePoint(const ParticleSystem& ps, glm::vec3 centre, float height)
{
    return std::any_of(
        ps.GetParticles().begin(),
        ps.GetParticles().end(),
        [centre, height](const Particle& p)
        { return glm::distance(sceneMath::ToScene(p.position, height), centre) < kTol; });
}

bool AnyParticleOnCard(const ParticleSystem& ps,
                       const particleCards::Frame& frame,
                       glm::vec2 anchorWorld,
                       float anchorHeight,
                       glm::vec3 centre)
{
    return std::any_of(ps.GetParticles().begin(),
                       ps.GetParticles().end(),
                       [&frame, anchorWorld, anchorHeight, centre](const Particle& p)
                       {
                           return glm::distance(particleCards::CardPoint(
                                                    frame, anchorWorld, anchorHeight, p.position),
                                                centre) < kTol;
                       });
}

bool AnyParticleAtWorld(const ParticleSystem& ps, glm::vec2 world)
{
    return std::any_of(ps.GetParticles().begin(),
                       ps.GetParticles().end(),
                       [world](const Particle& p)
                       { return glm::distance(p.position, world) < kTol; });
}

int CountOfType(const ParticleSystem& ps, ParticleType type)
{
    return static_cast<int>(std::count_if(ps.GetParticles().begin(),
                                          ps.GetParticles().end(),
                                          [type](const Particle& p) { return p.type == type; }));
}
}  // namespace

TEST(ParticleCardsTest, SheetAxesEqualTheCameraBasis)
{
    // orient with damping {1, 1} gives right = (cos yaw, 0, -sin yaw), which is
    // Basis::right, and up = (-sy*sp, cp, -cy*sp), which is UpVector in closed
    // form. the dot with EyeDirection = (sy*cp, sp, cy*cp) is
    // -sp*cp*(sy^2 + cy^2) + cp*sp, identically zero: the card is exactly
    // perpendicular to the view ray.
    for (const cameraRig::RigParams& rig : AllRigs())
    {
        SCOPED_TRACE(::testing::Message()
                     << "yaw " << rig.yawRadians << " pitch " << rig.pitchRadians);

        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        const cameraRig::Basis basis = cameraRig::MakeBasis(rig);

        ExpectVec3Near(frame.sheet.right, basis.right, 1e-4f);
        ExpectVec3Near(
            frame.sheet.up, cameraRig::UpVector(rig.yawRadians, rig.pitchRadians), 1e-4f);
        EXPECT_NEAR(glm::dot(frame.sheet.up, frame.towardEye), 0.0f, 1e-4f);
    }
}

TEST(ParticleCardsTest, FacadeAxesFollowTheBodyWidth)
{
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Free, 35.0f, 45.0f));

    EXPECT_NE(frame.pivot.yawRadians, frame.wall.yawRadians);
    EXPECT_NEAR(frame.pivot.yawRadians, 0.5f * Degrees(45.0f), 1e-6f);
    EXPECT_NEAR(frame.wall.yawRadians, 0.0f, 1e-6f);

    EXPECT_NEAR(particleCards::FacadeAxes(frame, 1).yawRadians, frame.pivot.yawRadians, 1e-6f);
    EXPECT_NEAR(particleCards::FacadeAxes(frame, 2).yawRadians, frame.wall.yawRadians, 1e-6f);
}

TEST(ParticleCardsTest, CardPointReducesToTheWorldPositionUnderClassic)
{
    // under Classic right = (1, 0, 0) and up = (0, ~0, -1), so
    // A + right*dx - up*dy = (A.x + dx, ~0, A.y + dy) = (P.x, 0, P.y) whatever
    // the anchor is. cos(pi/2) is about -4.4e-8 in float, hence the tolerance
    // on scene Y rather than an equality.
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));
    const glm::vec2 particle{1234.0f, 321.0f};
    const std::array<glm::vec2, 3> anchors{
        glm::vec2{1000.0f, 500.0f}, particle, glm::vec2{0.0f, 0.0f}};

    for (const glm::vec2& anchor : anchors)
    {
        SCOPED_TRACE(::testing::Message() << "anchor " << anchor.x << ", " << anchor.y);
        ExpectVec3Near(particleCards::CardPoint(frame, anchor, 0.0f, particle),
                       glm::vec3(1234.0f, 0.0f, 321.0f),
                       kTol);
    }
}

TEST(ParticleCardsTest, SheetPointKeepsTheFlatPixelUnderEveryCamera)
{
    // on the sheet the camera-space offsets are dx along right and -dy along up
    // at depth D, and D * tan(fov/2) = visible.y / 2 = 90, so ndc.x = dx/160 and
    // ndc.y = -dy/90. pixel x = (30/320 + 0.5) * 1520 = 902.5, pixel
    // y = (0.5 - 50/180) * 855 = 190.0. under ortho the half-extents give the
    // same two numbers, so the pixel is independent of yaw, pitch, fov and
    // projection kind.
    for (const cameraRig::RigParams& rig : AllRigs())
    {
        SCOPED_TRACE(::testing::Message()
                     << "yaw " << rig.yawRadians << " pitch " << rig.pitchRadians);

        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        const glm::vec3 centre =
            particleCards::CardPoint(frame,
                                     frame.focusWorld,
                                     frame.focusHeight,
                                     frame.focusWorld + glm::vec2(30.0f, -50.0f));

        const std::optional<glm::vec2> pixel =
            cameraRig::WorldToScreen(centre, cameraRig::BuildViewProjection(rig), kScreen);
        ASSERT_TRUE(pixel.has_value());
        EXPECT_NEAR(pixel->x, 902.5f, 0.02f);
        EXPECT_NEAR(pixel->y, 190.0f, 0.02f);
    }
}

TEST(ParticleCardsTest, SheetPointsShareTheFocusDepth)
{
    // depth = D - dx*dot(right, towardEye) + dy*dot(up, towardEye), and both
    // dots are identically zero, so every sheet point sits at the focus depth
    // D = 90 / tan(7.5 deg) = 683.6179.
    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    const particleCards::Frame frame = particleCards::MakeFrame(rig);
    const cameraRig::Basis basis = cameraRig::MakeBasis(rig);
    const float focusDepth =
        cameraRig::DistanceForVisibleHeight(rig.visibleWorldSize.y, rig.fovYRadians);

    const std::array<glm::vec2, 4> offsets{glm::vec2{30.0f, -50.0f},
                                           glm::vec2{-140.0f, 80.0f},
                                           glm::vec2{0.0f, 0.0f},
                                           glm::vec2{150.0f, -89.0f}};

    for (const glm::vec2& offset : offsets)
    {
        SCOPED_TRACE(::testing::Message() << "offset " << offset.x << ", " << offset.y);

        const glm::vec3 centre = particleCards::CardPoint(
            frame, frame.focusWorld, frame.focusHeight, frame.focusWorld + offset);
        const float depth = glm::dot(centre - basis.eye, basis.forward);
        EXPECT_NEAR(depth, focusDepth, 1e-2f);
        EXPECT_NEAR(depth, 683.618f, 0.01f);
    }
}

TEST(ParticleCardsTest, ClampToRectHoldsAPointInsideItsZone)
{
    // clamp the focus into the zone so its card stays near the particles.
    const glm::vec2 pulled =
        particleCards::ClampToRect({1000.0f, 500.0f}, {1100.0f, 420.0f}, {48.0f, 48.0f});
    EXPECT_NEAR(pulled.x, 1100.0f, 1e-5f);
    EXPECT_NEAR(pulled.y, 468.0f, 1e-5f);

    const glm::vec2 inside =
        particleCards::ClampToRect({1120.0f, 440.0f}, {1100.0f, 420.0f}, {48.0f, 48.0f});
    EXPECT_NEAR(inside.x, 1120.0f, 1e-5f);
    EXPECT_NEAR(inside.y, 440.0f, 1e-5f);

    const glm::vec2 unmoved =
        particleCards::ClampToRect({1000.0f, 500.0f}, {900.0f, 400.0f}, {200.0f, 200.0f});
    EXPECT_NEAR(unmoved.x, 1000.0f, 1e-5f);
    EXPECT_NEAR(unmoved.y, 500.0f, 1e-5f);
}

TEST(ParticleCardsTest, FacadePointRidesTheStructurePlane)
{
    // Wall lean is 0.8 * 51.34 deg = 41.072 deg, so cl = 0.7538846 and
    // sl = 0.6570069 and axes.up = (0, cl, -sl). with localX = 4 and 24 px of
    // height above the foot: y = 24*cl + sin(51.34 deg) = 18.874097 and
    // z = 128 - 24*sl + cos(51.34 deg) = 112.856531.
    const particleCards::Frame frame = particleCards::MakeFrame(MakeRig(cameraRig::Preset::DS));
    const billboard::Orientation& axes = particleCards::FacadeAxes(frame, 3);
    EXPECT_EQ(&axes, &frame.wall);

    const glm::vec3 point = particleCards::FacadePoint(
        frame, axes, glm::vec3(184.0f, 0.0f, 128.0f), 184.0f, 128.0f, glm::vec2(188.0f, 104.0f));
    ExpectVec3Near(point, glm::vec3(188.0f, 18.8741f, 112.8565f), kTol);

    // the bias is 1 px along towardEye, and the wall normal (0, sl, cl) meets
    // that direction at cos(51.34 - 41.072 deg), so the decal clears its own
    // wall by 0.984 px under DepthMode::TestOnly.
    const glm::vec3 normal = glm::normalize(glm::cross(axes.right, axes.up));
    EXPECT_NEAR(glm::dot(normal, frame.towardEye), 0.98399f, kTol);
}

TEST(ParticleCardsTest, NegativeWidthMirrorsWithoutClamping)
{
    // Snow width changes sign with its cosine cycle; negative width swaps the corner pairs.
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));

    glm::vec3 positive[sceneMath::QUAD_CORNER_COUNT]{};
    glm::vec3 negative[sceneMath::QUAD_CORNER_COUNT]{};
    particleCards::MakeSpriteQuad({0.0f, 0.0f, 0.0f}, {10.0f, 6.0f}, frame.sheet, 0.0f, positive);
    particleCards::MakeSpriteQuad({0.0f, 0.0f, 0.0f}, {-10.0f, 6.0f}, frame.sheet, 0.0f, negative);

    ExpectVec3Near(negative[sceneMath::QUAD_TOP_LEFT], positive[sceneMath::QUAD_TOP_RIGHT], 1e-5f);
    ExpectVec3Near(negative[sceneMath::QUAD_TOP_RIGHT], positive[sceneMath::QUAD_TOP_LEFT], 1e-5f);
    ExpectVec3Near(
        negative[sceneMath::QUAD_BOTTOM_RIGHT], positive[sceneMath::QUAD_BOTTOM_LEFT], 1e-5f);
    ExpectVec3Near(
        negative[sceneMath::QUAD_BOTTOM_LEFT], positive[sceneMath::QUAD_BOTTOM_RIGHT], 1e-5f);
}

TEST(ParticleCardsTest, RotationMatchesTheFlatRotateCorners)
{
    // under Classic axisRight = (1, 0, 0) and axisDown = -up = (0, ~0, 1). with
    // cos 30 = 0.8660254 and sin 30 = 0.5: u = (0.8660254, 0, 0.5),
    // v = (-0.5, 0, 0.8660254), halfW = 5u = (4.330127, 0, 2.5) and
    // halfH = 3v = (-1.5, 0, 2.5980762). these are the numbers
    // IRenderer::RotateCorners produces in the flat Y-down frame.
    const particleCards::Frame frame =
        particleCards::MakeFrame(MakeRig(cameraRig::Preset::Classic));

    glm::vec3 corners[sceneMath::QUAD_CORNER_COUNT]{};
    particleCards::MakeSpriteQuad({0.0f, 0.0f, 0.0f}, {10.0f, 6.0f}, frame.sheet, 30.0f, corners);

    ExpectVec3Near(corners[sceneMath::QUAD_TOP_LEFT], {-2.8301f, 0.0f, -5.0981f}, kTol);
    ExpectVec3Near(corners[sceneMath::QUAD_TOP_RIGHT], {5.8301f, 0.0f, -0.0981f}, kTol);
    ExpectVec3Near(corners[sceneMath::QUAD_BOTTOM_RIGHT], {2.8301f, 0.0f, 5.0981f}, kTol);
    ExpectVec3Near(corners[sceneMath::QUAD_BOTTOM_LEFT], {-5.8301f, 0.0f, 0.0981f}, kTol);
}

TEST(ParticleCardsTest, SheetCullMatchesTheFlatPad)
{
    // pad = max(size) * 2 + 50, so the kept limit on X is 160 + pad: 306 for the
    // smallest Fog sprite and 402 for the largest. offset 286 passes both and
    // 422 fails both, which is the flat viewport rule rephrased about the focus.
    const particleCards::Frame frame = particleCards::MakeFrame(MakeRig(cameraRig::Preset::DS));

    EXPECT_TRUE(particleCards::InsideSheetView(frame, {1286.0f, 500.0f}, {48.0f, 48.0f}));
    EXPECT_FALSE(particleCards::InsideSheetView(frame, {1422.0f, 500.0f}, {48.0f, 48.0f}));
    EXPECT_TRUE(particleCards::InsideSheetView(frame, {1286.0f, 500.0f}, {96.0f, 96.0f}));
    EXPECT_FALSE(particleCards::InsideSheetView(frame, {1422.0f, 500.0f}, {96.0f, 96.0f}));

    // the Y limit is 90 + 146 = 236 for a 48 px sprite.
    EXPECT_TRUE(particleCards::InsideSheetView(frame, {1000.0f, 735.0f}, {48.0f, 48.0f}));
    EXPECT_FALSE(particleCards::InsideSheetView(frame, {1000.0f, 737.0f}, {48.0f, 48.0f}));
}

TEST(ParticleSystem3DTest, ClassicSheetQuadMatchesTheFlatSprite)
{
    // a zoneless particle rides the sheet, and under Classic the sheet axes are
    // right (1, 0, 0) and -up (0, ~0, 1), so the quad's half-extents land on
    // world X and world Z. scene Y is cos(pi/2) times an offset, hence ~1e-7.
    ParticleSystem ps;
    ps.SpawnOne(ParticleType::Fog, {1000.0f, 500.0f});
    ASSERT_EQ(ps.GetParticles().size(), 1u);

    MockRenderer mock;
    ps.Render3D(mock, MakeRig(cameraRig::Preset::Classic));
    ASSERT_EQ(mock.quads3D.size(), 1u);

    const Particle& particle = ps.GetParticles()[0];
    const glm::vec2 pos = particle.position;
    const float half = particle.size * 0.5f;
    const MockRenderer::Quad3D& quad = mock.quads3D[0];

    ExpectVec3Near(
        quad.corners[sceneMath::QUAD_TOP_LEFT], {pos.x - half, 0.0f, pos.y - half}, kTol);
    ExpectVec3Near(
        quad.corners[sceneMath::QUAD_TOP_RIGHT], {pos.x + half, 0.0f, pos.y - half}, kTol);
    ExpectVec3Near(
        quad.corners[sceneMath::QUAD_BOTTOM_RIGHT], {pos.x + half, 0.0f, pos.y + half}, kTol);
    ExpectVec3Near(
        quad.corners[sceneMath::QUAD_BOTTOM_LEFT], {pos.x - half, 0.0f, pos.y + half}, kTol);

    EXPECT_EQ(quad.depth, renderModes::DepthMode::None);
    EXPECT_EQ(quad.blend, renderModes::BlendMode::Alpha);
    EXPECT_EQ(quad.light, renderModes::LightMode::SelfLit);
    EXPECT_FALSE(quad.flipY);
    EXPECT_EQ(ps.GetLastDrawnCount(), 1u);
}

TEST(ParticleSystem3DTest, SheetQuadKeepsScreenSizeUnderDs)
{
    // the sheet sits at constant depth D = 683.6179, where one world unit
    // perpendicular to the view axis is (855 / 2) / (D * tan 7.5 deg) = 4.75
    // screen px; 1520 / 320 gives the same 4.75 horizontally. at yaw 0 the
    // sheet's right projects to screen +X and its up to screen -Y, so the
    // sprite stays an axis-aligned rectangle on screen.
    ParticleSystem ps;
    ps.SpawnOne(ParticleType::Fog, {1000.0f, 500.0f});
    ASSERT_EQ(ps.GetParticles().size(), 1u);

    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    MockRenderer mock;
    ps.Render3D(mock, rig);
    ASSERT_EQ(mock.quads3D.size(), 1u);

    const glm::mat4 viewProjection = cameraRig::BuildViewProjection(rig);
    std::array<glm::vec2, sceneMath::QUAD_CORNER_COUNT> pixels{};
    for (size_t corner = 0; corner < sceneMath::QUAD_CORNER_COUNT; ++corner)
    {
        const std::optional<glm::vec2> pixel =
            cameraRig::WorldToScreen(mock.quads3D[0].corners[corner], viewProjection, kScreen);
        ASSERT_TRUE(pixel.has_value());
        pixels[corner] = *pixel;
    }

    const float expected = ps.GetParticles()[0].size * 4.75f;
    EXPECT_NEAR(
        pixels[sceneMath::QUAD_TOP_RIGHT].x - pixels[sceneMath::QUAD_TOP_LEFT].x, expected, 1e-2f);
    EXPECT_NEAR(pixels[sceneMath::QUAD_BOTTOM_LEFT].y - pixels[sceneMath::QUAD_TOP_LEFT].y,
                expected,
                1e-2f);
    EXPECT_NEAR(pixels[sceneMath::QUAD_TOP_LEFT].y, pixels[sceneMath::QUAD_TOP_RIGHT].y, 1e-2f);
    EXPECT_NEAR(pixels[sceneMath::QUAD_TOP_LEFT].x, pixels[sceneMath::QUAD_BOTTOM_LEFT].x, 1e-2f);
}

TEST(ParticleSystem3DTest, HoverZoneParticleStandsAtItsOwnGroundPoint)
{
    // Sparkles has no net vertical motion, so it anchors on itself:
    // CardPoint(P, h, P) collapses to ToScene(P, h) whatever the camera.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1040.0f, 460.0f}, glm::vec2{32.0f, 32.0f}, ParticleType::Sparkles);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    MockRenderer mock;
    ps.Render3D(mock, MakeRig(cameraRig::Preset::DS));
    ASSERT_FALSE(mock.quads3D.empty());

    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        EXPECT_TRUE(AnyParticleAtScenePoint(ps, QuadCentre(quad), 0.0f));
    }
}

TEST(ParticleSystem3DTest, FallingZoneParticleRidesTheZoneCard)
{
    // ClampToRect((1000, 500), (1100, 420), 48x48) gives (1100, 468).
    // use Ash because it emits no ground-anchored splashes.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1100.0f, 420.0f}, glm::vec2{48.0f, 48.0f}, ParticleType::Ash);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    MockRenderer mock;
    ps.Render3D(mock, rig);
    ASSERT_FALSE(mock.quads3D.empty());

    const particleCards::Frame frame = particleCards::MakeFrame(rig);
    const glm::vec2 anchor{1100.0f, 468.0f};
    const glm::vec3 anchorScene = sceneMath::ToScene(anchor, 0.0f);

    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        const glm::vec3 offset = QuadCentre(quad) - anchorScene;

        EXPECT_NEAR(glm::dot(offset, frame.towardEye), 0.0f, kTol);

        // world X maps along card right and world Y along card down, reconstructing the source
        // position.
        const glm::vec2 world = anchor + glm::vec2(glm::dot(offset, frame.sheet.right),
                                                   -glm::dot(offset, frame.sheet.up));
        EXPECT_TRUE(AnyParticleAtWorld(ps, world));
    }
}

TEST(ParticleSystem3DTest, ZoneContainingTheFocusCoincidesWithTheSheet)
{
    // an interior focus stays unchanged, so the zone card matches the sheet.
    // Ash emits no ground-anchored children.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{960.0f, 460.0f}, glm::vec2{80.0f, 80.0f}, ParticleType::Ash);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    const cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    MockRenderer mock;
    ps.Render3D(mock, rig);
    ASSERT_FALSE(mock.quads3D.empty());

    const particleCards::Frame frame = particleCards::MakeFrame(rig);
    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        EXPECT_TRUE(
            AnyParticleOnCard(ps, frame, frame.focusWorld, frame.focusHeight, QuadCentre(quad)));
    }
}

TEST(ParticleSystem3DTest, ClassicCardAndSheetCoincide)
{
    // under Classic right = (1, 0, 0) and up = (0, ~0, -1), so
    // A + right*dx - up*dy collapses to (P.x, ~0, P.y) for every anchor: the
    // anchor choice is invisible under the top-down ortho.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1100.0f, 420.0f}, glm::vec2{48.0f, 48.0f}, ParticleType::Rain);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    MockRenderer mock;
    ps.Render3D(mock, MakeRig(cameraRig::Preset::Classic));
    ASSERT_FALSE(mock.quads3D.empty());

    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        EXPECT_TRUE(AnyParticleAtScenePoint(ps, QuadCentre(quad), 0.0f));
    }
}

TEST(ParticleSystem3DTest, RaisedDeckLiftsTheAnchor)
{
    // ground props use surface height at their anchor to avoid a southward offset on raised decks.
    Tilemap map = MakeMap80x60();
    for (int tileX = 62; tileX <= 64; ++tileX)
    {
        for (int tileY = 26; tileY <= 28; ++tileY)
        {
            map.SetLayerTile(tileX, tileY, 0, 1);
            map.SetElevation(tileX, tileY, 8);
            map.SetLayerElevationRole(tileX, tileY, 0, ElevationRole::Raised);
        }
    }

    ParticleSystem ps;
    ps.SetTilemap(&map);
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1000.0f, 424.0f}, glm::vec2{24.0f, 24.0f}, ParticleType::Sparkles);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    MockRenderer mock;
    ps.Render3D(mock, MakeRig(cameraRig::Preset::DS));
    ASSERT_FALSE(mock.quads3D.empty());

    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        const glm::vec3 centre = QuadCentre(quad);

        EXPECT_NEAR(centre.y, 8.0f, kTol);
        EXPECT_TRUE(AnyParticleAtScenePoint(ps, centre, 8.0f));
    }
}

TEST(ParticleSystem3DTest, NoProjectionParticleRidesTheFacade)
{
    // a noProjection decal sits on the facade plane with a 1 px eye-ward depth bias.
    Tilemap map = MakeMap80x60();
    for (int tileX = 62; tileX <= 64; ++tileX)
    {
        for (int tileY = 30; tileY <= 32; ++tileY)
        {
            map.SetLayerTile(tileX, tileY, 0, 1);
            map.SetLayerStance(tileX, tileY, 0, TileStance::Structure);
        }
    }

    ParticleSystem ps;
    ps.SetTilemap(&map);
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1000.0f, 490.0f}, glm::vec2{24.0f, 24.0f}, ParticleType::Sparkles);
    zones.back().noProjection = true;
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    rig.target = {1016.0f, 500.0f};

    MockRenderer mock;
    ps.Render3D(mock, rig);
    ASSERT_GT(QuadsWithDepth(mock, renderModes::DepthMode::TestOnly), 0);
    EXPECT_EQ(QuadsWithDepth(mock, renderModes::DepthMode::TestOnly),
              static_cast<int>(mock.quads3D.size()));

    // runCentreX = (62 + 64 + 1) * 0.5 * 16 and baseSouthEdgeY = (32 + 1) * 16.
    const std::optional<Tilemap::StructureFacade> facade =
        map.FindStructureFacade({1010.0f, 500.0f});
    ASSERT_TRUE(facade.has_value());
    ASSERT_EQ(facade->widthTiles, 3);

    const particleCards::Frame frame = particleCards::MakeFrame(rig);
    const billboard::Orientation& axes = particleCards::FacadeAxes(frame, facade->widthTiles);
    const glm::vec3 normal = glm::normalize(glm::cross(axes.right, axes.up));

    MockRenderer tilePass;
    map.RenderWorld3D(tilePass, rig);
    bool wallOnPlane = false;
    for (const MockRenderer::Quad3D& quad : tilePass.quads3D)
    {
        if (quad.depth != renderModes::DepthMode::TestAndWrite)
        {
            continue;
        }
        const glm::vec3 bottomCentre = (quad.corners[sceneMath::QUAD_BOTTOM_LEFT] +
                                        quad.corners[sceneMath::QUAD_BOTTOM_RIGHT]) *
                                       0.5f;
        if (glm::distance(bottomCentre, facade->foot) < kTol)
        {
            wallOnPlane = true;
        }
    }
    EXPECT_TRUE(wallOnPlane) << "no tile quad stands on the reported facade foot";

    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        const glm::vec3 centre = QuadCentre(quad);
        // cos(51.34 - 41.072 deg) of the 1 px eye-ward bias.
        EXPECT_NEAR(glm::dot(centre - facade->foot, normal), 0.98399f, kTol);

        const glm::vec3 onWall = centre - facade->foot - frame.towardEye;
        const float localX = glm::dot(onWall, axes.right);
        const float heightAboveFoot = glm::dot(onWall, axes.up);
        EXPECT_TRUE(AnyParticleAtWorld(
            ps, {facade->runCentreX + localX, facade->baseSouthEdgeY - heightAboveFoot}));
    }
}

TEST(ParticleSystem3DTest, NoProjectionWithoutStructureIsNeverCulled)
{
    // noProjection bypasses culling. without a facade, the particle uses its normal card in pass
    // B0.
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1040.0f, 460.0f}, glm::vec2{32.0f, 32.0f}, ParticleType::Sparkles);
    zones.back().noProjection = true;

    ParticleSystem ps;
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    cameraRig::RigParams farRig = MakeRig(cameraRig::Preset::DS);
    farRig.target = {5000.0f, 5000.0f};

    MockRenderer mock;
    ps.Render3D(mock, farRig);
    ASSERT_FALSE(mock.quads3D.empty());
    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        EXPECT_EQ(quad.depth, renderModes::DepthMode::None);
    }

    std::vector<ParticleZone> projected;
    projected.emplace_back(
        glm::vec2{1040.0f, 460.0f}, glm::vec2{32.0f, 32.0f}, ParticleType::Sparkles);

    ParticleSystem projectedPs;
    projectedPs.SetZones(&projected);
    projectedPs.SetTimeOfDay(2.0f);
    RunSpawnLoop(projectedPs);
    ASSERT_FALSE(projectedPs.GetParticles().empty());

    MockRenderer projectedMock;
    projectedPs.Render3D(projectedMock, farRig);
    EXPECT_TRUE(projectedMock.quads3D.empty());
}

TEST(ParticleSystem3DTest, SheetCullMatchesTheFlatRule)
{
    // pad = max(size) * 2 + 50 with Fog's size in [48, 96) puts the kept limit
    // between 306 and 402 px from the focus, so 286 is inside for every Fog
    // sprite and 422 is outside for every Fog sprite. Fog is the only type
    // whose whole size range makes both halves unconditional.
    for (const cameraRig::Preset preset : {cameraRig::Preset::Classic, cameraRig::Preset::DS})
    {
        SCOPED_TRACE(::testing::Message() << "preset " << static_cast<int>(preset));

        ParticleSystem ps;
        ps.SpawnOne(ParticleType::Fog, {1286.0f, 500.0f});
        ps.SpawnOne(ParticleType::Fog, {1422.0f, 500.0f});
        ASSERT_EQ(ps.GetParticles().size(), 2u);

        const cameraRig::RigParams rig = MakeRig(preset);
        const particleCards::Frame frame = particleCards::MakeFrame(rig);
        MockRenderer mock;
        ps.Render3D(mock, rig);
        ASSERT_EQ(mock.quads3D.size(), 1u);

        const glm::vec3 offset =
            QuadCentre(mock.quads3D[0]) - sceneMath::ToScene(frame.focusWorld, frame.focusHeight);
        EXPECT_NEAR(frame.focusWorld.x + glm::dot(offset, frame.sheet.right), 1286.5f, 1.0f);
    }
}

TEST(ParticleSystem3DTest, FacadeCardsDrawBeforeSheetCardsAlphaBeforeAdditive)
{
    // pass B1 depth-tests facade decals before B0 cards; each pass draws non-additive particles
    // first.
    Tilemap map = MakeMap80x60();
    for (int tileX = 62; tileX <= 64; ++tileX)
    {
        for (int tileY = 30; tileY <= 32; ++tileY)
        {
            map.SetLayerTile(tileX, tileY, 0, 1);
            map.SetLayerStance(tileX, tileY, 0, TileStance::Structure);
        }
    }

    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1000.0f, 490.0f}, glm::vec2{24.0f, 24.0f}, ParticleType::Fog);
    zones.back().noProjection = true;
    zones.emplace_back(glm::vec2{1000.0f, 490.0f}, glm::vec2{24.0f, 24.0f}, ParticleType::Sparkles);
    zones.back().noProjection = true;
    zones.emplace_back(glm::vec2{1040.0f, 540.0f}, glm::vec2{24.0f, 24.0f}, ParticleType::Fog);

    ParticleSystem ps;
    ps.SetTilemap(&map);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    cameraRig::RigParams rig = MakeRig(cameraRig::Preset::DS);
    rig.target = {1016.0f, 500.0f};

    MockRenderer mock;
    ps.Render3D(mock, rig);

    ASSERT_GT(QuadsWithDepth(mock, renderModes::DepthMode::TestOnly), 0);
    ASSERT_GT(QuadsWithDepth(mock, renderModes::DepthMode::None), 0);
    EXPECT_GT(std::count_if(mock.quads3D.begin(),
                            mock.quads3D.end(),
                            [](const MockRenderer::Quad3D& quad)
                            { return quad.blend == renderModes::BlendMode::Alpha; }),
              0);
    EXPECT_GT(std::count_if(mock.quads3D.begin(),
                            mock.quads3D.end(),
                            [](const MockRenderer::Quad3D& quad)
                            { return quad.blend == renderModes::BlendMode::Additive; }),
              0);

    bool sawCard = false;
    bool sawAdditiveInPass = false;
    renderModes::DepthMode passDepth = mock.quads3D.front().depth;
    for (const MockRenderer::Quad3D& quad : mock.quads3D)
    {
        if (quad.depth != passDepth)
        {
            passDepth = quad.depth;
            sawAdditiveInPass = false;
        }
        EXPECT_FALSE(sawCard && quad.depth == renderModes::DepthMode::TestOnly)
            << "a facade decal was submitted after a card";
        EXPECT_FALSE(sawAdditiveInPass && quad.blend == renderModes::BlendMode::Alpha)
            << "an alpha quad was submitted after an additive one in the same pass";

        sawCard = sawCard || quad.depth == renderModes::DepthMode::None;
        sawAdditiveInPass = sawAdditiveInPass || quad.blend == renderModes::BlendMode::Additive;
    }
}

TEST(ParticleSystem3DTest, RenderDisabledSubmitsNothingAndReportsZero)
{
    ParticleSystem ps;
    for (int i = 0; i < 8; ++i)
    {
        ps.SpawnOne(ParticleType::Fog, {1000.0f, 500.0f});
    }
    ASSERT_EQ(ps.GetParticles().size(), 8u);

    MockRenderer mock;
    ps.SetRenderEnabled(false);
    ps.Render3D(mock, MakeRig(cameraRig::Preset::DS));
    EXPECT_TRUE(mock.quads3D.empty());
    EXPECT_EQ(ps.GetLastDrawnCount(), 0u);

    ps.SetRenderEnabled(true);
    ps.Render3D(mock, MakeRig(cameraRig::Preset::DS));
    EXPECT_FALSE(mock.quads3D.empty());
    EXPECT_EQ(ps.GetLastDrawnCount(), mock.quads3D.size());
}

TEST(ParticleSystem3DTest, SplashChildRecordsItsParentsZone)
{
    // splashes use zoneIndex -1 to survive orphan cleanup and anchorZone for culling
    // and height. observe 20 s because impact spawning uses a ~30% random throttle.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{900.0f, 420.0f}, glm::vec2{64.0f, 64.0f}, ParticleType::Rain);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);

    int splashesSeen = 0;
    for (int frame = 0; frame < 200; ++frame)
    {
        ps.Update(0.1f, kFlatCam, kFlatView);
        for (const Particle& p : ps.GetParticles())
        {
            if (p.type != ParticleType::RainSplash)
            {
                continue;
            }
            ++splashesSeen;
            EXPECT_EQ(p.zoneIndex, -1);
            EXPECT_EQ(p.anchorZone, 0);
        }
    }
    EXPECT_GT(splashesSeen, 0);
}

TEST(ParticleSystem3DTest, AnchorZoneSurvivesOnZoneRemoved)
{
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{1040.0f, 460.0f}, glm::vec2{32.0f, 32.0f}, ParticleType::Sparkles);
    zones.emplace_back(glm::vec2{900.0f, 420.0f}, glm::vec2{64.0f, 64.0f}, ParticleType::Rain);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);

    bool haveSplash = false;
    for (int frame = 0; frame < 200 && !haveSplash; ++frame)
    {
        ps.Update(0.1f, kFlatCam, kFlatView);
        haveSplash =
            std::any_of(ps.GetParticles().begin(),
                        ps.GetParticles().end(),
                        [](const Particle& p)
                        { return p.type == ParticleType::RainSplash && p.anchorZone == 1; });
    }
    ASSERT_TRUE(haveSplash);

    const int before = CountOfType(ps, ParticleType::RainSplash);
    ps.OnZoneRemoved(0);
    zones.erase(zones.begin());

    EXPECT_EQ(CountOfType(ps, ParticleType::RainSplash), before);
    for (const Particle& p : ps.GetParticles())
    {
        if (p.type == ParticleType::RainSplash)
        {
            EXPECT_EQ(p.anchorZone, 0);
        }
    }

    ps.OnZoneRemoved(0);
    EXPECT_EQ(CountOfType(ps, ParticleType::RainSplash), 0);
}

TEST(ParticleSystem3DTest, SubmissionCapHolds)
{
    ParticleSystem ps;
    while (ps.GetParticles().size() <= ParticleSystem::MAX_PARTICLE_QUADS_3D)
    {
        ps.SpawnOne(ParticleType::Sparkles, {1000.0f, 500.0f});
    }
    ASSERT_GT(ps.GetParticles().size(), ParticleSystem::MAX_PARTICLE_QUADS_3D);

    MockRenderer mock;
    ps.Render3D(mock, MakeRig(cameraRig::Preset::Classic));
    EXPECT_EQ(mock.quads3D.size(), ParticleSystem::MAX_PARTICLE_QUADS_3D);
    EXPECT_EQ(ps.GetLastDrawnCount(), ParticleSystem::MAX_PARTICLE_QUADS_3D);
}

TEST(ParticleSystem3DTest, FlatRenderIsUnchangedByTheExtraction)
{
    // without an atlas, the flat path submits colored rectangles that MockRenderer records.
    ParticleSystem ps;
    std::vector<ParticleZone> zones;
    zones.emplace_back(glm::vec2{860.0f, 430.0f}, glm::vec2{48.0f, 48.0f}, ParticleType::Fog);
    zones.emplace_back(glm::vec2{960.0f, 430.0f}, glm::vec2{48.0f, 48.0f}, ParticleType::Rain);
    zones.emplace_back(glm::vec2{1040.0f, 460.0f}, glm::vec2{32.0f, 32.0f}, ParticleType::Sparkles);
    ps.SetZones(&zones);
    ps.SetTimeOfDay(2.0f);
    RunSpawnLoop(ps);

    MockRenderer mock;
    mock.SetViewSize(kFlatView);
    ps.Render(mock, kFlatCam);
    ASSERT_FALSE(mock.rects.empty());
    EXPECT_EQ(mock.rects.size(), ps.GetLastDrawnCount());

    bool sawAdditive = false;
    bool sawAlpha = false;
    for (const MockRenderer::Rect& rect : mock.rects)
    {
        const Particle* match = nullptr;
        for (const Particle& p : ps.GetParticles())
        {
            if (glm::distance(p.position - kFlatCam, rect.position) < kTol)
            {
                match = &p;
                break;
            }
        }
        ASSERT_NE(match, nullptr) << "a rect was drawn at a position no particle occupies";

        const glm::vec2 expected = (match->type == ParticleType::Rain)
                                       ? glm::vec2(1.0f, 8.0f)
                                       : glm::vec2(match->size, match->size);
        EXPECT_NEAR(rect.size.x, expected.x, kTol);
        EXPECT_NEAR(rect.size.y, expected.y, kTol);
        EXPECT_EQ(rect.additive, match->additive);

        EXPECT_FALSE(sawAdditive && !rect.additive)
            << "a non-additive rect was drawn after an additive one";
        sawAdditive = sawAdditive || rect.additive;
        sawAlpha = sawAlpha || !rect.additive;
    }
    EXPECT_TRUE(sawAdditive);
    EXPECT_TRUE(sawAlpha);
}
