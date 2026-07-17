#include "tile_object_handler.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <unordered_set>
#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gfx/resource/arena.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "util/log.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_stream_ordering.h"

#include "app/editor/dungeon/dungeon_snapping.h"

namespace yaze::editor {

namespace {
constexpr size_t kMaxLayerBatchMutation = 128;

struct LayerOrderEntry {
  zelda3::RoomObject object;
  bool selected = false;
};

int LayerBucketIndex(const zelda3::RoomObject& object) {
  return std::clamp(static_cast<int>(object.GetLayerValue()), 0, 2);
}

std::unordered_set<size_t> MakeValidIndexSet(const std::vector<size_t>& indices,
                                             size_t object_count) {
  std::unordered_set<size_t> index_set;
  for (size_t index : indices) {
    if (index < object_count) {
      index_set.insert(index);
    }
  }
  return index_set;
}

std::array<std::vector<LayerOrderEntry>, 3> BuildLayerBuckets(
    const std::vector<zelda3::RoomObject>& objects,
    const std::unordered_set<size_t>& selected_indices) {
  std::array<std::vector<LayerOrderEntry>, 3> buckets;
  for (size_t i = 0; i < objects.size(); ++i) {
    buckets[LayerBucketIndex(objects[i])].push_back(
        LayerOrderEntry{objects[i], selected_indices.count(i) > 0});
  }
  return buckets;
}

void RestoreObjectSelection(
    ObjectSelection* selection,
    const std::vector<size_t>& selected_indices_after_reorder) {
  if (!selection) {
    return;
  }
  selection->ClearSelection();
  for (size_t index : selected_indices_after_reorder) {
    selection->SelectObject(index, ObjectSelection::SelectionMode::Add);
  }
}

void FlattenLayerBuckets(std::vector<zelda3::RoomObject>& objects,
                         std::array<std::vector<LayerOrderEntry>, 3>& buckets,
                         ObjectSelection* selection) {
  std::vector<zelda3::RoomObject> reordered;
  std::vector<size_t> selected_indices_after_reorder;
  reordered.reserve(objects.size());
  for (auto& bucket : buckets) {
    for (auto& entry : bucket) {
      if (entry.selected) {
        selected_indices_after_reorder.push_back(reordered.size());
      }
      reordered.push_back(std::move(entry.object));
    }
  }
  objects = std::move(reordered);
  RestoreObjectSelection(selection, selected_indices_after_reorder);
}

struct GuideRect {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  int center_x = 0;
  int center_y = 0;
};

GuideRect GetGuideRect(const zelda3::RoomObject& object) {
  auto [x, y, width, height] =
      zelda3::DimensionService::Get().GetSelectionBoundsPixels(object);
  return GuideRect{x, y, x + width, y + height, x + width / 2, y + height / 2};
}

bool AddUniqueGuide(std::vector<int>& guides, int value) {
  constexpr int kDuplicateTolerancePx = 2;
  if (std::any_of(guides.begin(), guides.end(), [&](int guide) {
        return std::abs(guide - value) <= kDuplicateTolerancePx;
      })) {
    return false;
  }
  guides.push_back(value);
  return true;
}

void DrawDashedLine(ImDrawList* draw_list, ImVec2 start, ImVec2 end,
                    ImU32 color, float thickness) {
  constexpr float kDash = 6.0f;
  constexpr float kGap = 4.0f;
  const bool vertical = std::abs(start.x - end.x) < 0.5f;
  const float total =
      vertical ? std::abs(end.y - start.y) : std::abs(end.x - start.x);
  for (float offset = 0.0f; offset < total; offset += kDash + kGap) {
    const float segment_end = std::min(offset + kDash, total);
    if (vertical) {
      const float y0 = start.y + offset;
      const float y1 = start.y + segment_end;
      draw_list->AddLine(ImVec2(start.x, y0), ImVec2(start.x, y1), color,
                         thickness);
    } else {
      const float x0 = start.x + offset;
      const float x1 = start.x + segment_end;
      draw_list->AddLine(ImVec2(x0, start.y), ImVec2(x1, start.y), color,
                         thickness);
    }
  }
}

}  // namespace

TileObjectHandler::GhostCapacityState
TileObjectHandler::GetPlacementGhostCapacityState() const {
  if (!ctx_)
    return GhostCapacityState::kNormal;
  auto* room =
      const_cast<TileObjectHandler*>(this)->GetRoom(ctx_->current_room_id);
  const size_t current_obj_count = room ? room->GetTileObjects().size() : 0;
  return GetPlacementCapacityState(current_obj_count, zelda3::kMaxTileObjects);
}

zelda3::Room* TileObjectHandler::GetRoom(int room_id) {
  if (!ctx_ || !ctx_->rooms)
    return nullptr;
  if (room_id < 0 || room_id >= static_cast<int>(ctx_->rooms->size())) {
    return nullptr;
  }
  return &(*ctx_->rooms)[room_id];
}

void TileObjectHandler::NotifyChange(zelda3::Room* room) {
  if (!room || !ctx_)
    return;
  room->MarkTileObjectCollectionDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kTileObjects);
}

// ========================================================================
// BaseEntityHandler implementation
// ========================================================================

void TileObjectHandler::BeginPlacement() {
  object_placement_mode_ = true;
  RenderGhostPreviewBitmap();
}

void TileObjectHandler::CancelPlacement() {
  object_placement_mode_ = false;
  ghost_preview_buffer_.reset();
}

bool TileObjectHandler::HandleClick(int canvas_x, int canvas_y) {
  if (!HasValidContext())
    return false;

  if (object_placement_mode_) {
    auto [room_x, room_y] = CanvasToRoom(canvas_x, canvas_y);
    if (IsWithinBounds(canvas_x, canvas_y)) {
      PlaceObjectAt(ctx_->current_room_id, preview_object_, room_x, room_y);
    }
    return true;  // Placement click is handled even if outside bounds.
  }

  // Handle selection (click)
  if (!ctx_ || !ctx_->selection)
    return false;

  auto hovered = GetEntityAtPosition(canvas_x, canvas_y);
  if (!hovered.has_value())
    return false;

  const ImGuiIO& io = ImGui::GetIO();

  // Alt-click is reserved for caller-specific behaviors (inspector/etc). Treat
  // it as a handled click over an object without changing selection.
  if (io.KeyAlt)
    return true;

  ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;
  if (io.KeyShift) {
    mode = ObjectSelection::SelectionMode::Add;
  } else if (io.KeyCtrl || io.KeySuper) {
    mode = ObjectSelection::SelectionMode::Toggle;
  }

  ctx_->selection->SelectObject(*hovered, mode);
  return true;
}

void TileObjectHandler::InitDrag(const ImVec2& start_pos) {
  is_dragging_ = true;
  drag_start_ = snapping::SnapToTileGrid(start_pos);
  drag_current_ = drag_start_;
  drag_last_dx_ = 0;
  drag_last_dy_ = 0;
  drag_has_duplicated_ = false;
  drag_mutation_started_ = false;
}

void TileObjectHandler::BeginMarqueeSelection(const ImVec2& start_pos) {
  if (!ctx_ || !ctx_->selection)
    return;
  ctx_->selection->BeginRectangleSelection(static_cast<int>(start_pos.x),
                                           static_cast<int>(start_pos.y));
}

void TileObjectHandler::HandleMarqueeSelection(
    const ImVec2& mouse_pos, bool mouse_left_down, bool mouse_left_released,
    bool shift_down, bool toggle_down, bool alt_down, bool draw_box) {
  (void)shift_down;
  (void)toggle_down;
  if (!ctx_ || !ctx_->selection)
    return;
  if (!ctx_->selection->IsRectangleSelectionActive())
    return;

  // Update + draw while the drag is in progress.
  if (mouse_left_down) {
    ctx_->selection->UpdateRectangleSelection(static_cast<int>(mouse_pos.x),
                                              static_cast<int>(mouse_pos.y));
    if (draw_box) {
      if (ctx_->canvas) {
        ctx_->selection->DrawRectangleSelectionBox(ctx_->canvas);
      }
    }
  }

  // Finalize selection on release.
  if (mouse_left_released) {
    ctx_->selection->UpdateRectangleSelection(static_cast<int>(mouse_pos.x),
                                              static_cast<int>(mouse_pos.y));

    auto* room = GetRoom(ctx_->current_room_id);
    if (!room) {
      ctx_->selection->CancelRectangleSelection();
      return;
    }

    constexpr int kMinRectPixels = 6;
    if (alt_down || !ctx_->selection->IsRectangleLargeEnough(kMinRectPixels)) {
      ctx_->selection->CancelRectangleSelection();
      return;
    }

    ctx_->selection->EndRectangleSelection(
        room->GetTileObjects(), ObjectSelection::SelectionMode::Single);
  }
}

void TileObjectHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  (void)delta;
  if (!is_dragging_ || !ctx_ || !ctx_->selection)
    return;

  drag_current_ = snapping::SnapToTileGrid(current_pos);

  // Calculate total drag delta from start (snapped)
  ImVec2 drag_delta =
      ImVec2(drag_current_.x - drag_start_.x, drag_current_.y - drag_start_.y);
  drag_delta = ApplyDragModifiers(drag_delta);

  const int tile_dx = static_cast<int>(drag_delta.x) / 8;
  const int tile_dy = static_cast<int>(drag_delta.y) / 8;

  // Calculate incremental move
  const int inc_dx = tile_dx - drag_last_dx_;
  const int inc_dy = tile_dy - drag_last_dy_;

  if (inc_dx != 0 || inc_dy != 0) {
    if (!drag_mutation_started_) {
      ctx_->NotifyMutation(MutationDomain::kTileObjects);
      drag_mutation_started_ = true;
    }

    MoveObjects(ctx_->current_room_id, ctx_->selection->GetSelectedIndices(),
                inc_dx, inc_dy,
                /*notify_mutation=*/false);

    drag_last_dx_ = tile_dx;
    drag_last_dy_ = tile_dy;
  }
}

void TileObjectHandler::HandleRelease() {
  if (is_dragging_) {
    const bool had_mutation = drag_mutation_started_;
    is_dragging_ = false;
    drag_mutation_started_ = false;
    drag_has_duplicated_ = false;
    // Drag operations mutate incrementally while the mouse is held down. The
    // editor's undo capture wants to finalize after the drag ends (once the
    // interaction mode has returned to Select), so emit one more invalidation
    // on release if anything changed.
    if (had_mutation && ctx_) {
      ctx_->NotifyInvalidateCache(MutationDomain::kTileObjects);
    }
  }
}

ImVec2 TileObjectHandler::ApplyDragModifiers(const ImVec2& delta) const {
  return delta;
}

bool TileObjectHandler::HandleMouseWheel(float delta) {
  if (!HasValidContext() || !ctx_ || !ctx_->selection || delta == 0.0f)
    return false;

  auto indices = ctx_->selection->GetSelectedIndices();
  if (indices.empty())
    return false;

  int resize_delta = (delta > 0.0f) ? 1 : -1;
  ResizeObjects(ctx_->current_room_id, indices, resize_delta);
  return true;
}

void TileObjectHandler::DrawGhostPreview() {
  if (!object_placement_mode_ || preview_object_.id_ < 0 || !HasValidContext())
    return;

  const auto pointer_screen_pos = GetPointerScreenPosition();
  if (!pointer_screen_pos.has_value()) {
    return;
  }

  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();

  ImVec2 canvas_mouse_pos = ImVec2(pointer_screen_pos->x - canvas_pos.x,
                                   pointer_screen_pos->y - canvas_pos.y);
  auto [room_x, room_y] = CanvasToRoom(static_cast<int>(canvas_mouse_pos.x),
                                       static_cast<int>(canvas_mouse_pos.y));

  if (!IsWithinBounds(static_cast<int>(canvas_mouse_pos.x),
                      static_cast<int>(canvas_mouse_pos.y)))
    return;

  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvas(room_x, room_y);
  auto [obj_width, obj_height] = CalculateObjectBounds(preview_object_);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 preview_start(canvas_pos.x + snap_canvas_x * scale,
                       canvas_pos.y + snap_canvas_y * scale);
  ImVec2 preview_end(preview_start.x + obj_width * scale,
                     preview_start.y + obj_height * scale);

  zelda3::Room* room = GetRoom(ctx_->current_room_id);
  const size_t current_obj_count = room ? room->GetTileObjects().size() : 0;
  const auto capacity_state = GetPlacementGhostCapacityState();

  const auto& theme = AgentUI::GetTheme();
  const ImVec4 outline_color = GetPlacementAccentColor(
      theme, capacity_state, theme.dungeon_selection_primary);
  bool drew_bitmap = false;

  if (ghost_preview_buffer_) {
    auto& bitmap = ghost_preview_buffer_->bitmap();
    if (bitmap.texture()) {
      ImVec2 bitmap_end(preview_start.x + bitmap.width() * scale,
                        preview_start.y + bitmap.height() * scale);
      ImVec4 tint = capacity_state == GhostCapacityState::kNormal
                        ? theme.text_primary
                        : GetPlacementAccentColor(theme, capacity_state,
                                                  theme.text_primary);
      tint.w = 0.70f;
      draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                          preview_start, bitmap_end, ImVec2(0, 0), ImVec2(1, 1),
                          ImGui::GetColorU32(tint));
      draw_list->AddRect(preview_start, bitmap_end,
                         ImGui::GetColorU32(outline_color), 0.0f, 0, 2.0f);
      drew_bitmap = true;
    }
  }

  if (!drew_bitmap) {
    draw_list->AddRectFilled(
        preview_start, preview_end,
        ImGui::GetColorU32(
            ImVec4(outline_color.x, outline_color.y, outline_color.z, 0.25f)));
    draw_list->AddRect(preview_start, preview_end,
                       ImGui::GetColorU32(outline_color), 0.0f, 0, 2.0f);
  }

  // ID label
  std::string id_text = absl::StrFormat("0x%02X", preview_object_.id_);
  draw_list->AddText(ImVec2(preview_start.x + 2, preview_start.y + 1),
                     ImGui::GetColorU32(theme.text_primary), id_text.c_str());

  // Capacity tooltip while hovering — proactive warning before user clicks.
  if (capacity_state != GhostCapacityState::kNormal &&
      ImGui::IsMouseHoveringRect(preview_start, preview_end)) {
    ImGui::SetTooltip(tr("Objects: %zu/%zu\n%s"), current_obj_count,
                      zelda3::kMaxTileObjects,
                      GetPlacementCapacityTooltipSuffix(capacity_state).data());
  }

  const std::string badge_text = absl::StrFormat(
      "Objects %zu/%zu", current_obj_count, zelda3::kMaxTileObjects);
  DrawPlacementCapacityBadge(draw_list,
                             ImVec2(preview_start.x, preview_end.y + 6.0f),
                             theme, capacity_state, badge_text);
}

void TileObjectHandler::DrawSelectionHighlight() {
  if (!HasValidContext() || !ctx_->selection)
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  // Use ObjectSelection's rendering (handles pulsing border, corner handles)
  ctx_->selection->DrawSelectionHighlights(
      ctx_->canvas, room->GetTileObjects(), [](const zelda3::RoomObject& obj) {
        auto result = zelda3::DimensionService::Get().GetDimensions(obj);
        return std::make_tuple(result.offset_x_tiles * 8,
                               result.offset_y_tiles * 8, result.width_pixels(),
                               result.height_pixels());
      });

  if (is_dragging_) {
    DrawSmartGuides(room->GetTileObjects());
  }
}

std::optional<size_t> TileObjectHandler::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  auto* room =
      const_cast<TileObjectHandler*>(this)->GetRoom(ctx_->current_room_id);
  if (!room)
    return std::nullopt;

  const auto& objects = room->GetTileObjects();
  for (size_t i = objects.size(); i > 0; --i) {
    size_t index = i - 1;
    const auto& object = objects[index];

    // Respect layer filter if available in context
    if (ctx_ && ctx_->selection &&
        !ctx_->selection->PassesLayerFilterForObject(object)) {
      continue;
    }

    auto [obj_tile_x, obj_tile_y, width_tiles, height_tiles] =
        zelda3::DimensionService::Get().GetHitTestBounds(object);

    int obj_px = obj_tile_x * 8;
    int obj_py = obj_tile_y * 8;
    int w_px = width_tiles * 8;
    int h_px = height_tiles * 8;

    if (canvas_x >= obj_px && canvas_x < obj_px + w_px && canvas_y >= obj_py &&
        canvas_y < obj_py + h_px) {
      return index;
    }
  }
  return std::nullopt;
}

void TileObjectHandler::DrawSmartGuides(
    const std::vector<zelda3::RoomObject>& objects) const {
  if (!ctx_ || !ctx_->canvas || !ctx_->selection ||
      !ctx_->selection->HasSelection()) {
    return;
  }

  const auto selected = ctx_->selection->GetSelectedIndices();
  if (selected.empty()) {
    return;
  }

  constexpr int kAlignmentTolerancePx = 2;
  constexpr size_t kMaxGuidesPerAxis = 8;
  std::vector<int> vertical_guides;
  std::vector<int> horizontal_guides;

  for (size_t selected_index : selected) {
    if (selected_index >= objects.size()) {
      continue;
    }
    const GuideRect selected_rect = GetGuideRect(objects[selected_index]);
    const std::array<int, 3> selected_x = {
        selected_rect.left, selected_rect.center_x, selected_rect.right};
    const std::array<int, 3> selected_y = {
        selected_rect.top, selected_rect.center_y, selected_rect.bottom};

    for (size_t other_index = 0; other_index < objects.size(); ++other_index) {
      if (ctx_->selection->IsObjectSelected(other_index)) {
        continue;
      }
      const GuideRect other_rect = GetGuideRect(objects[other_index]);
      const std::array<int, 3> other_x = {other_rect.left, other_rect.center_x,
                                          other_rect.right};
      const std::array<int, 3> other_y = {other_rect.top, other_rect.center_y,
                                          other_rect.bottom};

      for (int sx : selected_x) {
        for (int ox : other_x) {
          if (std::abs(sx - ox) <= kAlignmentTolerancePx) {
            AddUniqueGuide(vertical_guides, ox);
          }
        }
      }
      for (int sy : selected_y) {
        for (int oy : other_y) {
          if (std::abs(sy - oy) <= kAlignmentTolerancePx) {
            AddUniqueGuide(horizontal_guides, oy);
          }
        }
      }
      if (vertical_guides.size() >= kMaxGuidesPerAxis &&
          horizontal_guides.size() >= kMaxGuidesPerAxis) {
        break;
      }
    }
  }

  if (vertical_guides.empty() && horizontal_guides.empty()) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  ImVec4 guide_color = theme.accent_color;
  guide_color.w = 0.78f;
  const ImU32 color = ImGui::GetColorU32(guide_color);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 canvas_pos = ctx_->canvas->zero_point();
  const float scale = ctx_->canvas->global_scale();
  const float width = dungeon_coords::kRoomPixelWidth * scale;
  const float height = dungeon_coords::kRoomPixelHeight * scale;

  const size_t vertical_count =
      std::min(vertical_guides.size(), kMaxGuidesPerAxis);
  for (size_t i = 0; i < vertical_count; ++i) {
    const float x = canvas_pos.x + vertical_guides[i] * scale;
    DrawDashedLine(draw_list, ImVec2(x, canvas_pos.y),
                   ImVec2(x, canvas_pos.y + height), color, 1.2f);
  }

  const size_t horizontal_count =
      std::min(horizontal_guides.size(), kMaxGuidesPerAxis);
  for (size_t i = 0; i < horizontal_count; ++i) {
    const float y = canvas_pos.y + horizontal_guides[i] * scale;
    DrawDashedLine(draw_list, ImVec2(canvas_pos.x, y),
                   ImVec2(canvas_pos.x + width, y), color, 1.2f);
  }
}

// ========================================================================
// Mutation Logic
// ========================================================================

void TileObjectHandler::MoveObjects(int room_id,
                                    const std::vector<size_t>& indices,
                                    int delta_x, int delta_y,
                                    bool notify_mutation) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (notify_mutation && ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].x_ =
          std::clamp(static_cast<int>(objects[index].x_ + delta_x), 0, 63);
      objects[index].y_ =
          std::clamp(static_cast<int>(objects[index].y_ + delta_y), 0, 63);
    }
  }

  NotifyChange(room);
}

void TileObjectHandler::UpdateObjectsId(int room_id,
                                        const std::vector<size_t>& indices,
                                        int16_t new_id) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      // Use the setter so derived flags + tile caches stay coherent.
      objects[index].set_id(new_id);
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::UpdateObjectsSize(int room_id,
                                          const std::vector<size_t>& indices,
                                          uint8_t new_size) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].size_ = new_size;
      objects[index].tiles_loaded_ = false;
    }
  }
  NotifyChange(room);
}

bool TileObjectHandler::UpdateObjectsLayer(int room_id,
                                           const std::vector<size_t>& indices,
                                           int new_layer) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return false;
  if (new_layer < 0 || new_layer > 2) {
    LOG_WARN("TileObjectHandler",
             "Rejected layer update with invalid target layer: %d", new_layer);
    return false;
  }
  auto& objects = room->GetTileObjects();
  std::vector<size_t> deduped_indices;
  deduped_indices.reserve(indices.size());
  std::unordered_set<size_t> seen_indices;
  for (size_t index : indices) {
    if (index >= objects.size()) {
      continue;
    }
    if (seen_indices.insert(index).second) {
      deduped_indices.push_back(index);
    }
  }
  if (deduped_indices.empty()) {
    return false;
  }

  if (deduped_indices.size() > kMaxLayerBatchMutation) {
    LOG_WARN("TileObjectHandler",
             "Rejected layer batch mutation of %zu objects (max %zu)",
             deduped_indices.size(), kMaxLayerBatchMutation);
    return false;
  }

  auto candidate_objects = objects;
  auto mutation = zelda3::ReassignObjectStorage(candidate_objects,
                                                deduped_indices, new_layer);
  if (!mutation.ok()) {
    LOG_WARN("TileObjectHandler", "Rejected object stream mutation: %s",
             std::string(mutation.status().message()).c_str());
    return false;
  }
  if (!mutation->changed) {
    return true;
  }

  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  objects = std::move(candidate_objects);
  RestoreObjectSelection(ctx_ ? ctx_->selection : nullptr,
                         mutation->selected_indices);
  NotifyChange(room);
  return true;
}

std::vector<size_t> TileObjectHandler::DuplicateObjects(
    int room_id, const std::vector<size_t>& indices, int delta_x, int delta_y,
    bool notify_mutation) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return {};
  if (notify_mutation && ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  auto& objects = room->GetTileObjects();
  std::vector<size_t> new_indices;

  const size_t base_index = objects.size();
  for (size_t index : indices) {
    if (index < objects.size()) {
      auto clone = objects[index].CopyForNewPlacement();
      clone.x_ = std::clamp(static_cast<int>(clone.x_ + delta_x), 0, 63);
      clone.y_ = std::clamp(static_cast<int>(clone.y_ + delta_y), 0, 63);
      objects.push_back(clone);
      new_indices.push_back(base_index + (new_indices.size()));
    }
  }

  NotifyChange(room);
  return new_indices;
}

void TileObjectHandler::DeleteObjects(int room_id,
                                      std::vector<size_t> indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  std::sort(indices.rbegin(), indices.rend());
  for (size_t index : indices) {
    room->RemoveTileObject(index);
  }

  NotifyChange(room);
}

void TileObjectHandler::DeleteAllObjects(int room_id) {
  auto* room = GetRoom(room_id);
  if (!room)
    return;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  room->ClearTileObjects();
  NotifyChange(room);
}

void TileObjectHandler::SendToFront(int room_id,
                                    const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  auto& objects = room->GetTileObjects();
  auto selected_set = MakeValidIndexSet(indices, objects.size());
  if (selected_set.empty()) {
    return;
  }
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto buckets = BuildLayerBuckets(objects, selected_set);
  for (auto& bucket : buckets) {
    std::vector<LayerOrderEntry> other;
    std::vector<LayerOrderEntry> selected;
    other.reserve(bucket.size());
    selected.reserve(bucket.size());
    for (auto& entry : bucket) {
      if (entry.selected) {
        selected.push_back(std::move(entry));
      } else {
        other.push_back(std::move(entry));
      }
    }
    bucket = std::move(other);
    bucket.insert(bucket.end(), std::make_move_iterator(selected.begin()),
                  std::make_move_iterator(selected.end()));
  }
  FlattenLayerBuckets(objects, buckets, ctx_ ? ctx_->selection : nullptr);
  NotifyChange(room);
}

void TileObjectHandler::SendToBack(int room_id,
                                   const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  auto& objects = room->GetTileObjects();
  auto selected_set = MakeValidIndexSet(indices, objects.size());
  if (selected_set.empty()) {
    return;
  }
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto buckets = BuildLayerBuckets(objects, selected_set);
  for (auto& bucket : buckets) {
    std::vector<LayerOrderEntry> selected;
    std::vector<LayerOrderEntry> other;
    selected.reserve(bucket.size());
    other.reserve(bucket.size());
    for (auto& entry : bucket) {
      if (entry.selected) {
        selected.push_back(std::move(entry));
      } else {
        other.push_back(std::move(entry));
      }
    }
    bucket = std::move(selected);
    bucket.insert(bucket.end(), std::make_move_iterator(other.begin()),
                  std::make_move_iterator(other.end()));
  }
  FlattenLayerBuckets(objects, buckets, ctx_ ? ctx_->selection : nullptr);
  NotifyChange(room);
}

void TileObjectHandler::MoveForward(int room_id,
                                    const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  auto& objects = room->GetTileObjects();
  auto selected_set = MakeValidIndexSet(indices, objects.size());
  if (selected_set.empty()) {
    return;
  }
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto buckets = BuildLayerBuckets(objects, selected_set);
  for (auto& bucket : buckets) {
    if (bucket.size() < 2) {
      continue;
    }
    for (size_t i = bucket.size() - 1; i > 0; --i) {
      const size_t previous = i - 1;
      if (bucket[previous].selected && !bucket[i].selected) {
        std::swap(bucket[previous], bucket[i]);
      }
    }
  }
  FlattenLayerBuckets(objects, buckets, ctx_ ? ctx_->selection : nullptr);
  NotifyChange(room);
}

void TileObjectHandler::MoveBackward(int room_id,
                                     const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  auto& objects = room->GetTileObjects();
  auto selected_set = MakeValidIndexSet(indices, objects.size());
  if (selected_set.empty()) {
    return;
  }
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto buckets = BuildLayerBuckets(objects, selected_set);
  for (auto& bucket : buckets) {
    if (bucket.size() < 2) {
      continue;
    }
    for (size_t i = 1; i < bucket.size(); ++i) {
      const size_t previous = i - 1;
      if (bucket[i].selected && !bucket[previous].selected) {
        std::swap(bucket[i], bucket[previous]);
      }
    }
  }
  FlattenLayerBuckets(objects, buckets, ctx_ ? ctx_->selection : nullptr);
  NotifyChange(room);
}

void TileObjectHandler::ResizeObjects(int room_id,
                                      const std::vector<size_t>& indices,
                                      int delta) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      int new_size =
          std::clamp(static_cast<int>(objects[index].size_) + delta, 0, 15);
      objects[index].size_ = static_cast<uint8_t>(new_size);
      objects[index].tiles_loaded_ = false;
    }
  }
  NotifyChange(room);
}

bool TileObjectHandler::PlaceObjectAt(int room_id,
                                      const zelda3::RoomObject& object, int x,
                                      int y) {
  auto* room = GetRoom(room_id);
  if (!room) {
    placement_block_reason_ = PlacementBlockReason::kInvalidRoom;
    return false;
  }

  // Hard-stop: enforce ROM object limit before committing placement.
  if (room->GetTileObjects().size() >= zelda3::kMaxTileObjects) {
    placement_block_reason_ = PlacementBlockReason::kObjectLimit;
    return false;
  }

  placement_block_reason_ = PlacementBlockReason::kNone;
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  auto new_obj = object.CopyForNewPlacement();
  new_obj.x_ = std::clamp(x, 0, 63);
  new_obj.y_ = std::clamp(y, 0, 63);
  room->AddTileObject(new_obj);
  NotifyChange(room);
  TriggerSuccessToast();
  return true;
}

void TileObjectHandler::SetPreviewObject(const zelda3::RoomObject& object) {
  preview_object_ = object;
  if (object_placement_mode_) {
    RenderGhostPreviewBitmap();
  }
}

void TileObjectHandler::RenderGhostPreviewBitmap() {
  if (!ctx_ || !ctx_->rom || !ctx_->rom->is_loaded())
    return;

  auto* room = GetRoom(ctx_->current_room_id);
  if (!room || !room->IsLoaded())
    return;

  auto [width, height] = CalculateObjectBounds(preview_object_);
  width = std::max(width, 16);
  height = std::max(height, 16);

  ghost_preview_buffer_ =
      std::make_unique<gfx::BackgroundBuffer>(width, height);
  const uint8_t* gfx_data = room->get_gfx_buffer().data();

  zelda3::ObjectDrawer drawer(ctx_->rom, ctx_->current_room_id, gfx_data);
  drawer.InitializeDrawRoutines();

  auto status =
      drawer.DrawObject(preview_object_, *ghost_preview_buffer_,
                        *ghost_preview_buffer_, ctx_->current_palette_group);
  if (!status.ok()) {
    ghost_preview_buffer_.reset();
    return;
  }

  auto& bitmap = ghost_preview_buffer_->bitmap();
  if (bitmap.size() > 0) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
  }
}

std::pair<int, int> TileObjectHandler::CalculateObjectBounds(
    const zelda3::RoomObject& object) {
  return zelda3::DimensionService::Get().GetPixelDimensions(object);
}

// ========================================================================
// Clipboard Operations
// ========================================================================

void TileObjectHandler::CopyObjectsToClipboard(
    int room_id, const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;

  clipboard_.clear();
  const auto& objects = room->GetTileObjects();

  for (size_t idx : indices) {
    if (idx < objects.size()) {
      clipboard_.push_back(objects[idx]);
    }
  }
}

std::vector<size_t> TileObjectHandler::PasteFromClipboard(int room_id,
                                                          int offset_x,
                                                          int offset_y) {
  auto* room = GetRoom(room_id);
  if (!room || clipboard_.empty())
    return {};
  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);

  std::vector<size_t> new_indices;
  size_t base_index = room->GetTileObjects().size();

  for (auto obj : clipboard_) {
    obj = obj.CopyForNewPlacement();
    obj.x_ = std::clamp(obj.x_ + offset_x, 0, 63);
    obj.y_ = std::clamp(obj.y_ + offset_y, 0, 63);
    obj.tiles_loaded_ = false;
    room->AddTileObject(obj);
    new_indices.push_back(base_index++);
  }

  NotifyChange(room);
  return new_indices;
}

std::vector<size_t> TileObjectHandler::PasteFromClipboardAt(int room_id,
                                                            int target_x,
                                                            int target_y) {
  if (clipboard_.empty())
    return {};

  int offset_x = target_x - clipboard_[0].x_;
  int offset_y = target_y - clipboard_[0].y_;

  return PasteFromClipboard(room_id, offset_x, offset_y);
}

}  // namespace yaze::editor
