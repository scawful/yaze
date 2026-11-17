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
  if (!overworld_.is_loaded() || tile_id < 0 ||
      tile_id >= zelda3::kNumTile16Individual) {
    return false;
  }

  if (IsUnifiedCanvas()) {
    if (x < 0 || y < 0) {
      return false;
    }
    const ImVec2 canvas_point(static_cast<float>(x * kTile16Size),
                              static_cast<float>(y * kTile16Size));
    const auto map_id = GetCanvasLayout().MapAt(canvas_point, current_world_,
                                                DisplayedMapCount());
    if (!map_id || !tile_painting_) {
      return false;
    }
    return tile_painting_->PlaceBrushAt({1, 1, {tile_id}}, x, y).ok();
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

  if (IsUnifiedCanvas()) {
    if (x < 0 || y < 0) {
      return -1;
    }
    const ImVec2 canvas_point(static_cast<float>(x * kTile16Size),
                              static_cast<float>(y * kTile16Size));
    const OverworldCanvasLayout layout = GetCanvasLayout();
    const auto map_id =
        layout.MapAt(canvas_point, current_world_, DisplayedMapCount());
    const auto world_point =
        layout.WorldLocalFromCanvas(canvas_point, current_world_);
    if (!map_id || !world_point) {
      return -1;
    }
    const int local_x =
        static_cast<int>(world_point->local_position.x / kTile16Size);
    const int local_y =
        static_cast<int>(world_point->local_position.y / kTile16Size);
    if (IsProjectTerrainMap(*map_id) && lyra_canvas_.loaded()) {
      const auto tile = lyra_canvas_.document()->tile(local_x, local_y - 128);
      return tile.ok() ? static_cast<int>(*tile) : -1;
    }
    if (!tile_painting_) {
      return -1;
    }
    const auto cell =
        tile_painting_->ReadMapCell(*map_id, local_x % 32, local_y % 32);
    return cell.ok() ? static_cast<int>(*cell) : -1;
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
