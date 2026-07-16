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
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"
#include "util/log.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"

#include "app/editor/dungeon/dungeon_snapping.h"

namespace yaze::editor {

namespace {
constexpr size_t kMaxLayerBatchMutation = 128;
constexpr int kGhostPreviewBufferSize = 512;

bool ApplyRoomPaletteToGhost(const zelda3::Room& room, gfx::Bitmap& ghost) {
  const gfx::Bitmap* sources[] = {
      &room.bg1_buffer().bitmap(), &room.object_bg1_buffer().bitmap(),
      &room.bg2_buffer().bitmap(), &room.object_bg2_buffer().bitmap()};
  for (const gfx::Bitmap* source : sources) {
    SDL_Surface* surface = source->surface();
    SDL_Palette* palette = platform::GetSurfacePalette(surface);
    if (!palette || palette->ncolors <= 0) {
      continue;
    }

    std::vector<SDL_Color> colors(256, {0, 0, 0, 0});
    const int color_count = std::min(palette->ncolors, 256);
    std::copy_n(palette->colors, color_count, colors.begin());
    colors[255] = {0, 0, 0, 0};
    ghost.SetPalette(colors);
    if (ghost.surface()) {
      SDL_SetColorKey(ghost.surface(), SDL_TRUE, 255);
      SDL_SetSurfaceBlendMode(ghost.surface(), SDL_BLENDMODE_BLEND);
    }
    return true;
  }
  return false;
}

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

  const DungeonCanvasTransform transform = GetCanvasTransform();
  const auto [canvas_x, canvas_y] =
      transform.ScreenToRoomPixelCoordinates(*pointer_screen_pos);
  auto [room_x, room_y] = CanvasToRoom(canvas_x, canvas_y);

  if (!IsWithinBounds(canvas_x, canvas_y))
    return;

  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvas(room_x, room_y);
  const auto preview_geometry = CalculateGhostPreviewGeometry(preview_object_);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 preview_start = transform.RoomPixelsToScreen(ImVec2(
      static_cast<float>(snap_canvas_x + preview_geometry.offset_x_tiles * 8),
      static_cast<float>(snap_canvas_y + preview_geometry.offset_y_tiles * 8)));
  const ImVec2 preview_size = transform.RoomSizeToScreen(
      ImVec2(static_cast<float>(preview_geometry.width_pixels),
             static_cast<float>(preview_geometry.height_pixels)));
  const ImVec2 preview_end(preview_start.x + preview_size.x,
                           preview_start.y + preview_size.y);
  const ImVec2 bitmap_start = transform.RoomPixelsToScreen(
      ImVec2(static_cast<float>(snap_canvas_x -
                                preview_geometry.render_anchor_x_tiles * 8),
             static_cast<float>(snap_canvas_y -
                                preview_geometry.render_anchor_y_tiles * 8)));

  zelda3::Room* room = GetRoom(ctx_->current_room_id);
  const size_t current_obj_count = room ? room->GetTileObjects().size() : 0;
  const auto capacity_state = GetPlacementGhostCapacityState();

  const auto& theme = AgentUI::GetTheme();
  const ImVec4 outline_color = GetPlacementAccentColor(
      theme, capacity_state, theme.dungeon_selection_primary);
  bool drew_bitmap = false;

  if (ghost_preview_buffer_ && ghost_preview_bitmap_ready_) {
    auto& bitmap = ghost_preview_buffer_->bitmap();
    if (bitmap.texture()) {
      const int crop_width =
          std::min(preview_geometry.buffer_width_pixels, bitmap.width());
      const int crop_height =
          std::min(preview_geometry.buffer_height_pixels, bitmap.height());
      const ImVec2 bitmap_size = transform.RoomSizeToScreen(ImVec2(
          static_cast<float>(crop_width), static_cast<float>(crop_height)));
      const ImVec2 bitmap_end(bitmap_start.x + bitmap_size.x,
                              bitmap_start.y + bitmap_size.y);
      const ImVec2 uv_end(static_cast<float>(crop_width) / bitmap.width(),
                          static_cast<float>(crop_height) / bitmap.height());
      ImVec4 tint = capacity_state == GhostCapacityState::kNormal
                        ? theme.text_primary
                        : GetPlacementAccentColor(theme, capacity_state,
                                                  theme.text_primary);
      tint.w = 0.70f;
      draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(), bitmap_start,
                          bitmap_end, ImVec2(0, 0), uv_end,
                          ImGui::GetColorU32(tint));
      draw_list->AddRect(preview_start, preview_end,
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
  if (!HasValidContext() || !IsWithinBounds(canvas_x, canvas_y)) {
    return std::nullopt;
  }
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
  const DungeonCanvasTransform transform = GetCanvasTransform();
  const ImVec2 canvas_pos = transform.room_origin_screen();
  const ImVec2 room_size = transform.RoomSizeToScreen(ImVec2(
      dungeon_coords::kRoomPixelWidth, dungeon_coords::kRoomPixelHeight));

  const size_t vertical_count =
      std::min(vertical_guides.size(), kMaxGuidesPerAxis);
  for (size_t i = 0; i < vertical_count; ++i) {
    const float x =
        transform.RoomPixelsToScreen(ImVec2(vertical_guides[i], 0.0f)).x;
    DrawDashedLine(draw_list, ImVec2(x, canvas_pos.y),
                   ImVec2(x, canvas_pos.y + room_size.y), color, 1.2f);
  }

  const size_t horizontal_count =
      std::min(horizontal_guides.size(), kMaxGuidesPerAxis);
  for (size_t i = 0; i < horizontal_count; ++i) {
    const float y =
        transform.RoomPixelsToScreen(ImVec2(0.0f, horizontal_guides[i])).y;
    DrawDashedLine(draw_list, ImVec2(canvas_pos.x, y),
                   ImVec2(canvas_pos.x + room_size.x, y), color, 1.2f);
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

void TileObjectHandler::UpdateObjectsLayer(int room_id,
                                           const std::vector<size_t>& indices,
                                           int new_layer) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty())
    return;
  if (new_layer < 0 || new_layer > 2) {
    LOG_WARN("TileObjectHandler",
             "Rejected layer update with invalid target layer: %d", new_layer);
    return;
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
    return;
  }

  if (deduped_indices.size() > kMaxLayerBatchMutation) {
    LOG_WARN("TileObjectHandler",
             "Rejected layer batch mutation of %zu objects (max %zu)",
             deduped_indices.size(), kMaxLayerBatchMutation);
    return;
  }

  int current_bg3_count = 0;
  for (const auto& object : objects) {
    if (object.GetLayerValue() == 2) {
      ++current_bg3_count;
    }
  }

  int moving_to_bg3 = 0;
  int moving_from_bg3 = 0;
  auto target_layer = static_cast<zelda3::RoomObject::LayerType>(new_layer);
  for (size_t index : deduped_indices) {
    const auto& object = objects[index];
    const auto semantics = zelda3::GetObjectLayerSemantics(object);
    if (semantics.draws_to_both_bgs &&
        target_layer != zelda3::RoomObject::LayerType::BG1) {
      continue;
    }
    if (object.layer_ == target_layer) {
      continue;
    }
    if (object.GetLayerValue() == 2) {
      ++moving_from_bg3;
    }
    if (new_layer == 2) {
      ++moving_to_bg3;
    }
  }
  const int projected_bg3_count =
      current_bg3_count - moving_from_bg3 + moving_to_bg3;
  if (projected_bg3_count > zelda3::kMaxBg3Objects) {
    LOG_WARN("TileObjectHandler",
             "Rejected layer mutation: projected BG3 count %d exceeds max %d",
             projected_bg3_count, zelda3::kMaxBg3Objects);
    return;
  }

  const std::unordered_set<size_t> selected_set(deduped_indices.begin(),
                                                deduped_indices.end());
  std::array<std::vector<LayerOrderEntry>, 3> buckets;
  std::vector<LayerOrderEntry> moved_to_target_layer;
  bool changed = false;
  for (size_t index = 0; index < objects.size(); ++index) {
    auto object = objects[index];
    const bool selected = selected_set.count(index) > 0;
    if (!selected) {
      buckets[LayerBucketIndex(object)].push_back(
          LayerOrderEntry{std::move(object), false});
      continue;
    }
    const auto semantics = zelda3::GetObjectLayerSemantics(object);
    if (semantics.draws_to_both_bgs &&
        target_layer != zelda3::RoomObject::LayerType::BG1) {
      buckets[LayerBucketIndex(object)].push_back(
          LayerOrderEntry{std::move(object), true});
      continue;
    }
    if (object.layer_ == target_layer) {
      buckets[LayerBucketIndex(object)].push_back(
          LayerOrderEntry{std::move(object), true});
      continue;
    }
    object.layer_ = target_layer;
    moved_to_target_layer.push_back(LayerOrderEntry{std::move(object), true});
    changed = true;
  }
  if (!changed) {
    return;
  }

  for (auto& entry : moved_to_target_layer) {
    buckets[new_layer].push_back(std::move(entry));
  }

  if (ctx_)
    ctx_->NotifyMutation(MutationDomain::kTileObjects);
  FlattenLayerBuckets(objects, buckets, ctx_ ? ctx_->selection : nullptr);
  NotifyChange(room);
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
      auto clone = objects[index];
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
  auto new_obj = object;
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
  ghost_preview_bitmap_ready_ = false;
  if (!ctx_ || !ctx_->rom || !ctx_->rom->is_loaded())
    return;

  auto* room = GetRoom(ctx_->current_room_id);
  if (!room || !room->IsLoaded())
    return;

  const auto preview_geometry = CalculateGhostPreviewGeometry(preview_object_);

  // Keep one room-sized scratch texture for the handler lifetime. Replacing a
  // textured bitmap during ImGui frame construction can invalidate draw-list
  // texture handles, and repeatedly allocating previews leaks those handles.
  if (!ghost_preview_buffer_) {
    ghost_preview_buffer_ = std::make_unique<gfx::BackgroundBuffer>(
        kGhostPreviewBufferSize, kGhostPreviewBufferSize);
  }
  ghost_preview_buffer_->EnsureBitmapInitialized();
  ghost_preview_buffer_->ClearBuffer();
  auto& bitmap = ghost_preview_buffer_->bitmap();
  ApplyRoomPaletteToGhost(*room, bitmap);
  std::fill(bitmap.mutable_data().begin(), bitmap.mutable_data().end(), 255);
  bitmap.set_modified(true);
  const uint8_t* gfx_data = room->get_gfx_buffer().data();

  zelda3::ObjectDrawer drawer(ctx_->rom, ctx_->current_room_id, gfx_data);
  drawer.InitializeDrawRoutines();

  // Replay at the same safe anchor ObjectGeometry uses for measurement so
  // routines that draw upward or leftward do not clip against buffer origin.
  auto render_object = preview_object_;
  render_object.x_ = preview_geometry.render_anchor_x_tiles;
  render_object.y_ = preview_geometry.render_anchor_y_tiles;
  // A placement preview is a flattened visual. Force the replay through BG1
  // so a sampled BG2-stream object cannot mask pixels in the same scratch
  // buffer that it just rendered.
  render_object.layer_ = zelda3::RoomObject::LayerType::BG1;
  auto status =
      drawer.DrawObject(render_object, *ghost_preview_buffer_,
                        *ghost_preview_buffer_, ctx_->current_palette_group);
  if (!status.ok()) {
    return;
  }

  if (bitmap.size() > 0) {
    bitmap.UpdateSurfacePixels();
    if (bitmap.texture()) {
      ghost_preview_create_queued_ = false;
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &bitmap);
    } else if (!ghost_preview_create_queued_) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bitmap);
      ghost_preview_create_queued_ = true;
    }
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
    if (bitmap.texture()) {
      ghost_preview_create_queued_ = false;
    }
    ghost_preview_bitmap_ready_ = true;
  }
}

TileObjectHandler::GhostPreviewGeometry
TileObjectHandler::CalculateGhostPreviewGeometry(
    const zelda3::RoomObject& object) {
  const auto dimensions = zelda3::DimensionService::Get().GetDimensions(object);
  const auto [resolved_anchor_x, resolved_anchor_y] =
      zelda3::ObjectGeometry::Get().ResolveAnchor(object.id_, object.size_);
  // ResolveAnchor supplies routine-specific headroom. Dimension offsets cover
  // fallback/custom geometry too, so keep enough room for either source.
  const int anchor_x =
      std::max({0, resolved_anchor_x, -dimensions.offset_x_tiles});
  const int anchor_y =
      std::max({0, resolved_anchor_y, -dimensions.offset_y_tiles});
  const int leading_x_tiles = anchor_x + dimensions.offset_x_tiles;
  const int leading_y_tiles = anchor_y + dimensions.offset_y_tiles;
  return GhostPreviewGeometry{
      .render_anchor_x_tiles = anchor_x,
      .render_anchor_y_tiles = anchor_y,
      .offset_x_tiles = dimensions.offset_x_tiles,
      .offset_y_tiles = dimensions.offset_y_tiles,
      .width_pixels = dimensions.width_pixels(),
      .height_pixels = dimensions.height_pixels(),
      .buffer_width_pixels = (leading_x_tiles + dimensions.width_tiles) * 8,
      .buffer_height_pixels = (leading_y_tiles + dimensions.height_tiles) * 8,
  };
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
