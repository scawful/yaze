#include "object_selection.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/agent_theme.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/dungeon/dimension_service.h"

namespace yaze::editor {

// ============================================================================
// Selection Operations
// ============================================================================

void ObjectSelection::SelectObject(size_t index, SelectionMode mode) {
  switch (mode) {
    case SelectionMode::Single:
      // Replace entire selection with single object
      selected_indices_.clear();
      selected_indices_.insert(index);
      break;

    case SelectionMode::Add:
      // Add to existing selection (Shift+click)
      selected_indices_.insert(index);
      break;

    case SelectionMode::Toggle:
      // Toggle object in selection (Ctrl+click)
      if (selected_indices_.count(index)) {
        selected_indices_.erase(index);
      } else {
        selected_indices_.insert(index);
      }
      break;

    case SelectionMode::Rectangle:
      // This shouldn't be used for single object selection
      LOG_ERROR("ObjectSelection",
                "Rectangle mode used for single object selection");
      selected_indices_.insert(index);
      break;
  }

  NotifySelectionChanged();
}

void ObjectSelection::SelectObjectsInRect(
    int room_min_x, int room_min_y, int room_max_x, int room_max_y,
    const std::vector<zelda3::RoomObject>& objects, SelectionMode mode) {
  // Normalize rectangle bounds
  int min_x = std::min(room_min_x, room_max_x);
  int max_x = std::max(room_min_x, room_max_x);
  int min_y = std::min(room_min_y, room_max_y);
  int max_y = std::max(room_min_y, room_max_y);

  // For Single mode, clear previous selection first
  if (mode == SelectionMode::Single) {
    selected_indices_.clear();
  }

  // Find all objects within rectangle
  for (size_t i = 0; i < objects.size(); ++i) {
    if (IsObjectInRectangle(objects[i], min_x, min_y, max_x, max_y)) {
      if (mode == SelectionMode::Toggle) {
        // Toggle each object
        if (selected_indices_.count(i)) {
          selected_indices_.erase(i);
        } else {
          selected_indices_.insert(i);
        }
      } else {
        // Add or Replace mode - just add
        selected_indices_.insert(i);
      }
    }
  }

  NotifySelectionChanged();
}

void ObjectSelection::SelectAll(size_t object_count) {
  selected_indices_.clear();
  for (size_t i = 0; i < object_count; ++i) {
    selected_indices_.insert(i);
  }
  NotifySelectionChanged();
}

void ObjectSelection::SelectAll(const std::vector<zelda3::RoomObject>& objects) {
  selected_indices_.clear();
  for (size_t i = 0; i < objects.size(); ++i) {
    // Only select objects that pass the layer filter
    if (PassesLayerFilter(objects[i])) {
      selected_indices_.insert(i);
    }
  }
  NotifySelectionChanged();
}

void ObjectSelection::ClearSelection() {
  if (selected_indices_.empty()) {
    return;  // No change
  }

  selected_indices_.clear();
  NotifySelectionChanged();
}

bool ObjectSelection::IsObjectSelected(size_t index) const {
  return selected_indices_.count(index) > 0;
}

std::vector<size_t> ObjectSelection::GetSelectedIndices() const {
  // Safely convert set to vector with bounds checking
  std::vector<size_t> result;
  result.reserve(selected_indices_.size());
  for (size_t idx : selected_indices_) {
    result.push_back(idx);
  }
  return result;
}

std::optional<size_t> ObjectSelection::GetPrimarySelection() const {
  if (selected_indices_.empty()) {
    return std::nullopt;
  }
  return *selected_indices_.begin();  // First element (lowest index)
}

// ============================================================================
// Rectangle Selection State
// ============================================================================

void ObjectSelection::BeginRectangleSelection(int canvas_x, int canvas_y) {
  rectangle_selection_active_ = true;
  rect_start_x_ = canvas_x;
  rect_start_y_ = canvas_y;
  rect_end_x_ = canvas_x;
  rect_end_y_ = canvas_y;
}

void ObjectSelection::UpdateRectangleSelection(int canvas_x, int canvas_y) {
  if (!rectangle_selection_active_) {
    LOG_ERROR("ObjectSelection",
              "UpdateRectangleSelection called when not active");
    return;
  }

  rect_end_x_ = canvas_x;
  rect_end_y_ = canvas_y;
}

void ObjectSelection::EndRectangleSelection(
    const std::vector<zelda3::RoomObject>& objects, SelectionMode mode) {
  if (!rectangle_selection_active_) {
    LOG_ERROR("ObjectSelection",
              "EndRectangleSelection called when not active");
    return;
  }

  // Convert canvas coordinates to room coordinates
  auto [start_room_x, start_room_y] =
      CanvasToRoomCoordinates(rect_start_x_, rect_start_y_);
  auto [end_room_x, end_room_y] =
      CanvasToRoomCoordinates(rect_end_x_, rect_end_y_);

  // Select objects in rectangle
  SelectObjectsInRect(start_room_x, start_room_y, end_room_x, end_room_y,
                      objects, mode);

  rectangle_selection_active_ = false;
}

void ObjectSelection::CancelRectangleSelection() {
  rectangle_selection_active_ = false;
}

std::tuple<int, int, int, int> ObjectSelection::GetRectangleSelectionBounds()
    const {
  int min_x = std::min(rect_start_x_, rect_end_x_);
  int max_x = std::max(rect_start_x_, rect_end_x_);
  int min_y = std::min(rect_start_y_, rect_end_y_);
  int max_y = std::max(rect_start_y_, rect_end_y_);
  return {min_x, min_y, max_x, max_y};
}

bool ObjectSelection::IsRectangleLargeEnough(int min_pixels) const {
  if (!rectangle_selection_active_) {
    return false;
  }

  auto [min_x, min_y, max_x, max_y] = GetRectangleSelectionBounds();
  const int width = std::abs(max_x - min_x);
  const int height = std::abs(max_y - min_y);
  return (width >= min_pixels) || (height >= min_pixels);
}

// ============================================================================
// Visual Rendering
// ============================================================================

void ObjectSelection::DrawSelectionHighlights(
    gui::Canvas* canvas, const std::vector<zelda3::RoomObject>& objects,
    std::function<std::tuple<int, int, int, int>(const zelda3::RoomObject&)>
        bounds_calculator) {
  if (selected_indices_.empty() || !canvas) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas->zero_point();
  float scale = canvas->global_scale();

  for (size_t index : selected_indices_) {
    if (index >= objects.size()) {
      continue;
    }

    const auto& object = objects[index];

    // Calculate object position in canvas coordinates
    auto [obj_x, obj_y] = RoomToCanvasCoordinates(object.x_, object.y_);

    int offset_x = 0;
    int offset_y = 0;
    int pixel_width = 0;
    int pixel_height = 0;
    if (bounds_calculator) {
      auto [dx, dy, w, h] = bounds_calculator(object);
      offset_x = dx;
      offset_y = dy;
      pixel_width = w;
      pixel_height = h;
    } else {
      // Fallback to old logic if no calculator provided
      auto [tile_x, tile_y, tile_width, tile_height] = GetObjectBounds(object);
      offset_x = (tile_x - object.x_) * 8;
      offset_y = (tile_y - object.y_) * 8;
      pixel_width = tile_width * 8;
      pixel_height = tile_height * 8;
    }

    obj_x += offset_x;
    obj_y += offset_y;

    // Apply scale and canvas offset
    ImVec2 obj_start(canvas_pos.x + obj_x * scale,
                     canvas_pos.y + obj_y * scale);
    ImVec2 obj_end(obj_start.x + pixel_width * scale,
                   obj_start.y + pixel_height * scale);

    // Expand selection box slightly for visibility
    constexpr float margin = 2.0f;
    obj_start.x -= margin;
    obj_start.y -= margin;
    obj_end.x += margin;
    obj_end.y += margin;

    // Get color based on theme
    const auto& theme = AgentUI::GetTheme();

    // Draw pulsing animated border
    float pulse =
        0.6f + 0.4f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
    ImVec4 pulsing_color = theme.selection_primary;
    pulsing_color.w = 0.4f + 0.4f * pulse;

    ImU32 border_color = ImGui::GetColorU32(theme.selection_primary);
    ImU32 pulse_color_u32 = ImGui::GetColorU32(pulsing_color);

    draw_list->AddRect(obj_start, obj_end, border_color, 0.0f, 0, 2.0f);
    draw_list->AddRect(obj_start, obj_end, pulse_color_u32, 0.0f, 0, 4.0f);

    // Draw corner handles with theme handle color
    float handle_size = 6.0f * std::max(1.0f, scale * 0.5f);
    ImU32 handle_color = ImGui::GetColorU32(theme.selection_handle);
    ImU32 handle_outline = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.8f));

    auto draw_handle = [&](ImVec2 pos) {
      draw_list->AddRectFilled(
          ImVec2(pos.x - handle_size / 2, pos.y - handle_size / 2),
          ImVec2(pos.x + handle_size / 2, pos.y + handle_size / 2),
          handle_color, 2.0f);
      draw_list->AddRect(
          ImVec2(pos.x - handle_size / 2, pos.y - handle_size / 2),
          ImVec2(pos.x + handle_size / 2, pos.y + handle_size / 2),
          handle_outline, 2.0f, 0, 1.0f);
    };

    draw_handle(obj_start);
    draw_handle(ImVec2(obj_end.x, obj_start.y));
    draw_handle(ImVec2(obj_start.x, obj_end.y));
    draw_handle(obj_end);
  }
}

ImVec4 ObjectSelection::GetLayerTypeColor(const zelda3::RoomObject& object) const {
  const auto& theme = AgentUI::GetTheme();
  int layer = object.GetLayerValue();

  switch (layer) {
    case 0: return theme.dungeon_outline_layer0;
    case 1: return theme.dungeon_outline_layer1;
    case 2: return theme.dungeon_outline_layer2;
    default: return theme.status_inactive;
  }
}

void ObjectSelection::DrawRectangleSelectionBox(gui::Canvas* canvas) {
  if (!rectangle_selection_active_ || !canvas) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas->zero_point();
  float scale = canvas->global_scale();

  // Get normalized bounds
  auto [min_x, min_y, max_x, max_y] = GetRectangleSelectionBounds();

  // Apply scale and canvas offset
  ImVec2 box_start(canvas_pos.x + min_x * scale, canvas_pos.y + min_y * scale);
  ImVec2 box_end(canvas_pos.x + max_x * scale, canvas_pos.y + max_y * scale);

  // Draw selection box with theme selection color
  // Border: High-contrast at 0.85f alpha
  ImU32 border_color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(theme.selection_secondary.x, theme.selection_secondary.y, theme.selection_secondary.z,
             0.85f));
  // Fill: Subtle at 0.15f alpha
  ImU32 fill_color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(theme.selection_secondary.x, theme.selection_secondary.y, theme.selection_secondary.z,
             0.15f));

  draw_list->AddRectFilled(box_start, box_end, fill_color);
  draw_list->AddRect(box_start, box_end, border_color, 0.0f, 0, 2.0f);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::pair<int, int> ObjectSelection::RoomToCanvasCoordinates(int room_x,
                                                              int room_y) {
  // Dungeon tiles are 8x8 pixels
  return {room_x * 8, room_y * 8};
}

std::pair<int, int> ObjectSelection::CanvasToRoomCoordinates(int canvas_x,
                                                              int canvas_y) {
  // Convert pixels back to tiles (round down)
  return {canvas_x / 8, canvas_y / 8};
}

std::tuple<int, int, int, int> ObjectSelection::GetObjectBounds(
    const zelda3::RoomObject& object) {
  return zelda3::DimensionService::Get().GetHitTestBounds(object);
}

// ============================================================================
// Private Helper Functions
// ============================================================================

void ObjectSelection::NotifySelectionChanged() {
  if (selection_changed_callback_) {
    selection_changed_callback_();
  }
}

bool ObjectSelection::IsObjectInRectangle(const zelda3::RoomObject& object,
                                          int min_x, int min_y, int max_x,
                                          int max_y) const {
  // Check layer filter first
  if (!PassesLayerFilter(object)) {
    return false;
  }

  // Get object bounds
  auto [obj_x, obj_y, obj_width, obj_height] = GetObjectBounds(object);

  // Check if object's bounding box intersects with selection rectangle
  // Object is selected if ANY part of it is within the rectangle
  int obj_min_x = obj_x;
  int obj_max_x = obj_x + obj_width - 1;
  int obj_min_y = obj_y;
  int obj_max_y = obj_y + obj_height - 1;

  // Rectangle intersection test
  bool x_overlap = (obj_min_x <= max_x) && (obj_max_x >= min_x);
  bool y_overlap = (obj_min_y <= max_y) && (obj_max_y >= min_y);

  return x_overlap && y_overlap;
}

bool ObjectSelection::PassesLayerFilter(const zelda3::RoomObject& object) const {
  // If no layer filter is active, all objects pass
  if (active_layer_filter_ == kLayerAll) {
    return true;
  }

  // Mask mode: only allow BG2/Layer 1 objects (overlay content like platforms)
  if (active_layer_filter_ == kMaskLayer) {
    return object.GetLayerValue() == kLayer2;  // Layer 1 = BG2 = overlay
  }

  // Check if the object's layer matches the filter
  return object.GetLayerValue() == static_cast<uint8_t>(active_layer_filter_);
}

}  // namespace yaze::editor
