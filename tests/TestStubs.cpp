// link-only stubs for paths that require the full game. do not call renderer.set,
// Editor::Render, RenderPlacementPreview, or RenderParticleZoneOverlays in tests:
// the editor stubs return zero geometry. RenderNoProjectionAnchors is safe with
// MockRenderer. remove the three editor stubs if EditorInput.cpp is linked into
// rift_tests, or their real definitions will cause duplicate symbols.

#include "../src/Editor.hpp"
#include "../src/Game.hpp"
#include "../src/RendererAPI.hpp"

bool Game::SwitchRenderer(RendererAPI)
{
    return false;
}

void Editor::CalculateRotatedSourceTile(int, int, int& sourceDx, int& sourceDy) const
{
    sourceDx = 0;
    sourceDy = 0;
}

float Editor::GetCompensatedTileRotation() const
{
    return 0.0f;
}

Editor::TileZoneRect Editor::CalculateParticleZoneRect(float, float, int, int) const
{
    return TileZoneRect{};
}
