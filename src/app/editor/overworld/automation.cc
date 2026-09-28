#include "app/editor/overworld/overworld_editor.h"

#include "app/gui/canvas/canvas_automation_api.h"

namespace yaze {
namespace editor {

// ============================================================================
// Canvas Automation API Integration (Phase 4)
// ============================================================================

void OverworldEditor::SetupCanvasAutomation() {
  auto* api = ow_map_canvas_.GetAutomationAPI();

  // Set tile paint callback
  api->SetTilePaintCallback([this](int x, int y, int tile_id) {
    return AutomationSetTile(x, y, tile_id);
  });

  // Set tile query callback
  api->SetTileQueryCallback(
      [this](int x, int y) { return AutomationGetTile(x, y); });
}

bool OverworldEditor::AutomationSetTile(int x, int y, int tile_id) {
  if (!overworld_.is_loaded()) {
    return false;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return false;
  }

  // Set current world based on current_map_
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  // Set the tile in the overworld data structure
  overworld_.SetTile(x, y, static_cast<uint16_t>(tile_id));

  // Update the bitmap
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (!tile_data.empty()) {
    tile_painting_->RenderUpdatedMapBitmap(
        ImVec2(static_cast<float>(x * 16), static_cast<float>(y * 16)),
        tile_data);
    return true;
  }

  return false;
}

int OverworldEditor::AutomationGetTile(int x, int y) {
  if (!overworld_.is_loaded()) {
    return -1;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return -1;
  }

  // Set current world
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  return overworld_.GetTile(x, y);
}

}  // namespace editor
}  // namespace yaze
