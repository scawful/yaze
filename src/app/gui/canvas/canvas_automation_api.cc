#include "app/gui/canvas/canvas_automation_api.h"

#include <algorithm>
#include <cmath>

#include "app/gui/canvas/canvas.h"

namespace yaze {
namespace gui {

CanvasAutomationAPI::CanvasAutomationAPI(Canvas* canvas) : canvas_(canvas) {}

// ============================================================================
// Tile Operations
// ============================================================================

bool CanvasAutomationAPI::SetTileAt(int x, int y, int tile_id) {
  if (!IsInBounds(x, y)) {
    return false;
  }

  if (tile_paint_callback_) {
    return tile_paint_callback_(x, y, tile_id);
  }

  // Default behavior: add to canvas points for drawing
  // Note: Actual tile painting depends on the editor's canvas integration
  ImVec2 canvas_pos = TileToCanvas(x, y);
  canvas_->mutable_points()->push_back(canvas_pos);
  return true;
}

int CanvasAutomationAPI::GetTileAt(int x, int y) const {
  if (!IsInBounds(x, y)) {
    return -1;
  }

  if (tile_query_callback_) {
    return tile_query_callback_(x, y);
  }

  // Default: return -1 (requires callback for actual implementation)
  return -1;
}

bool CanvasAutomationAPI::SetTiles(
    const std::vector<std::tuple<int, int, int>>& tiles) {
  bool all_success = true;
  for (const auto& [x, y, tile_id] : tiles) {
    if (!SetTileAt(x, y, tile_id)) {
      all_success = false;
    }
  }
  return all_success;
}

// ============================================================================
// Selection Operations
// ============================================================================

void CanvasAutomationAPI::SelectTile(int x, int y) {
  if (!IsInBounds(x, y)) {
    return;
  }

  ImVec2 canvas_pos = TileToCanvas(x, y);
  canvas_->mutable_selected_points()->clear();
  canvas_->mutable_selected_points()->push_back(canvas_pos);
  canvas_->mutable_selected_points()->push_back(canvas_pos);
}

void CanvasAutomationAPI::SelectTileRect(int x1, int y1, int x2, int y2) {
  // Ensure x1 <= x2 and y1 <= y2
  if (x1 > x2)
    std::swap(x1, x2);
  if (y1 > y2)
    std::swap(y1, y2);

  if (!IsInBounds(x1, y1) || !IsInBounds(x2, y2)) {
    return;
  }

  ImVec2 start = TileToCanvas(x1, y1);
  ImVec2 end = TileToCanvas(x2, y2);

  canvas_->mutable_selected_points()->clear();
  canvas_->mutable_selected_points()->push_back(start);
  canvas_->mutable_selected_points()->push_back(end);
}

CanvasAutomationAPI::SelectionState CanvasAutomationAPI::GetSelection() const {
  SelectionState state;

  const auto& selected_points = canvas_->selected_points();
  if (selected_points.size() >= 2) {
    state.has_selection = true;
    state.selection_start = selected_points[0];
    state.selection_end = selected_points[1];

    // Convert canvas positions back to tile coordinates
    ImVec2 tile_start = CanvasToTile(state.selection_start);
    ImVec2 tile_end = CanvasToTile(state.selection_end);

    // Ensure proper ordering
    int min_x =
        std::min(static_cast<int>(tile_start.x), static_cast<int>(tile_end.x));
    int max_x =
        std::max(static_cast<int>(tile_start.x), static_cast<int>(tile_end.x));
    int min_y =
        std::min(static_cast<int>(tile_start.y), static_cast<int>(tile_end.y));
    int max_y =
        std::max(static_cast<int>(tile_start.y), static_cast<int>(tile_end.y));

    // Generate all tiles in selection rectangle
    for (int y = min_y; y <= max_y; ++y) {
      for (int x = min_x; x <= max_x; ++x) {
        state.selected_tiles.push_back(
            ImVec2(static_cast<float>(x), static_cast<float>(y)));
      }
    }
  }

  return state;
}

void CanvasAutomationAPI::ClearSelection() {
  canvas_->mutable_selected_points()->clear();
}

// ============================================================================
// View Operations
// ============================================================================

void CanvasAutomationAPI::ScrollToTile(int x, int y, bool center) {
  if (!IsInBounds(x, y)) {
    return;
  }

  if (center) {
    CenterOn(x, y);
    return;
  }

  // Check if tile is already visible
  if (IsTileVisible(x, y)) {
    return;
  }

  // Scroll to make tile visible
  ImVec2 tile_canvas_pos = TileToCanvas(x, y);

  // Get current scroll and canvas size
  ImVec2 current_scroll = canvas_->scrolling();
  ImVec2 canvas_size = canvas_->canvas_size();

  // Calculate new scroll to make tile visible at top-left
  float new_scroll_x = -tile_canvas_pos.x;
  float new_scroll_y = -tile_canvas_pos.y;

  canvas_->set_scrolling(ImVec2(new_scroll_x, new_scroll_y));
}

void CanvasAutomationAPI::CenterOn(int x, int y) {
  if (!IsInBounds(x, y)) {
    return;
  }

  ImVec2 tile_canvas_pos = TileToCanvas(x, y);
  ImVec2 canvas_size = canvas_->canvas_size();

  // Center the tile in the canvas view
  float new_scroll_x = -(tile_canvas_pos.x - canvas_size.x / 2.0f);
  float new_scroll_y = -(tile_canvas_pos.y - canvas_size.y / 2.0f);

  canvas_->set_scrolling(ImVec2(new_scroll_x, new_scroll_y));
}

void CanvasAutomationAPI::SetZoom(float zoom) {
  // Clamp zoom to reasonable range
  zoom = std::max(0.25f, std::min(zoom, 4.0f));
  canvas_->set_global_scale(zoom);
}

float CanvasAutomationAPI::GetZoom() const {
  return canvas_->global_scale();
}

// ============================================================================
// Query Operations
// ============================================================================

CanvasAutomationAPI::Dimensions CanvasAutomationAPI::GetDimensions() const {
  Dimensions dims;

  // Get canvas size in pixels
  ImVec2 canvas_size = canvas_->canvas_size();
  float scale = canvas_->global_scale();

  // Determine tile size from canvas grid size
  int tile_size = 16;  // Default
  switch (canvas_->grid_size()) {
    case CanvasGridSize::k8x8:
      tile_size = 8;
      break;
    case CanvasGridSize::k16x16:
      tile_size = 16;
      break;
    case CanvasGridSize::k32x32:
      tile_size = 32;
      break;
    case CanvasGridSize::k64x64:
      tile_size = 64;
      break;
  }

  dims.tile_size = tile_size;
  dims.width_tiles = static_cast<int>(canvas_size.x / (tile_size * scale));
  dims.height_tiles = static_cast<int>(canvas_size.y / (tile_size * scale));

  return dims;
}

CanvasAutomationAPI::VisibleRegion CanvasAutomationAPI::GetVisibleRegion()
    const {
  VisibleRegion region;

  ImVec2 scroll = canvas_->scrolling();
  ImVec2 canvas_size = canvas_->canvas_size();
  float scale = canvas_->global_scale();
  int tile_size = GetDimensions().tile_size;

  // Top-left corner of visible region
  ImVec2 top_left = CanvasToTile(ImVec2(-scroll.x, -scroll.y));

  // Bottom-right corner of visible region
  ImVec2 bottom_right = CanvasToTile(
      ImVec2(-scroll.x + canvas_size.x, -scroll.y + canvas_size.y));

  region.min_x = std::max(0, static_cast<int>(top_left.x));
  region.min_y = std::max(0, static_cast<int>(top_left.y));

  Dimensions dims = GetDimensions();
  region.max_x =
      std::min(dims.width_tiles - 1, static_cast<int>(bottom_right.x));
  region.max_y =
      std::min(dims.height_tiles - 1, static_cast<int>(bottom_right.y));

  return region;
}

bool CanvasAutomationAPI::IsTileVisible(int x, int y) const {
  if (!IsInBounds(x, y)) {
    return false;
  }

  VisibleRegion region = GetVisibleRegion();
  return x >= region.min_x && x <= region.max_x && y >= region.min_y &&
         y <= region.max_y;
}

bool CanvasAutomationAPI::IsInBounds(int x, int y) const {
  if (x < 0 || y < 0) {
    return false;
  }

  Dimensions dims = GetDimensions();
  return x < dims.width_tiles && y < dims.height_tiles;
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

ImVec2 CanvasAutomationAPI::TileToCanvas(int x, int y) const {
  int tile_size = GetDimensions().tile_size;
  float scale = canvas_->global_scale();

  float canvas_x = x * tile_size * scale;
  float canvas_y = y * tile_size * scale;

  return ImVec2(canvas_x, canvas_y);
}

ImVec2 CanvasAutomationAPI::CanvasToTile(ImVec2 canvas_pos) const {
  int tile_size = GetDimensions().tile_size;
  float scale = canvas_->global_scale();

  float tile_x = canvas_pos.x / (tile_size * scale);
  float tile_y = canvas_pos.y / (tile_size * scale);

  return ImVec2(std::floor(tile_x), std::floor(tile_y));
}

// ============================================================================
// Callback Registration
// ============================================================================

void CanvasAutomationAPI::SetTilePaintCallback(TilePaintCallback callback) {
  tile_paint_callback_ = std::move(callback);
}

void CanvasAutomationAPI::SetTileQueryCallback(TileQueryCallback callback) {
  tile_query_callback_ = std::move(callback);
}

}  // namespace gui
}  // namespace yaze
