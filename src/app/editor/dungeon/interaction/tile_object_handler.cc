#include "tile_object_handler.h"
#include <algorithm>
#include <cmath>
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/gfx/resource/arena.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/object_drawer.h"
#include "app/editor/dungeon/object_selection.h"

#include "app/editor/dungeon/dungeon_snapping.h"

namespace yaze::editor {

zelda3::Room* TileObjectHandler::GetRoom(int room_id) {
  if (!ctx_ || !ctx_->rooms || room_id < 0 || room_id >= 296) return nullptr;
  return &(*ctx_->rooms)[room_id];
}

void TileObjectHandler::NotifyChange(zelda3::Room* room) {
  if (!room || !ctx_) return;
  room->MarkObjectsDirty();
  ctx_->NotifyInvalidateCache();
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
  if (!HasValidContext()) return false;

  if (object_placement_mode_) {
    auto [room_x, room_y] = CanvasToRoom(canvas_x, canvas_y);
    if (IsWithinBounds(canvas_x, canvas_y)) {
      PlaceObjectAt(ctx_->current_room_id, preview_object_, room_x, room_y);
    }
    return true;  // Placement click is handled even if outside bounds.
  }

  // Handle selection (click)
  if (!ctx_ || !ctx_->selection) return false;

  auto hovered = GetEntityAtPosition(canvas_x, canvas_y);
  if (!hovered.has_value()) return false;

  const ImGuiIO& io = ImGui::GetIO();

  // Alt-click is reserved for caller-specific behaviors (inspector/etc). Treat
  // it as a handled click over an object without changing selection.
  if (io.KeyAlt) return true;

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
  if (!ctx_ || !ctx_->selection) return;
  ctx_->selection->BeginRectangleSelection(static_cast<int>(start_pos.x),
                                           static_cast<int>(start_pos.y));
}

void TileObjectHandler::HandleMarqueeSelection(const ImVec2& mouse_pos,
                                               bool mouse_left_down,
                                               bool mouse_left_released,
                                               bool shift_down,
                                               bool toggle_down,
                                               bool alt_down,
                                               bool draw_box) {
  if (!ctx_ || !ctx_->selection) return;
  if (!ctx_->selection->IsRectangleSelectionActive()) return;

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

    ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;
    if (shift_down) {
      mode = ObjectSelection::SelectionMode::Add;
    } else if (toggle_down) {
      mode = ObjectSelection::SelectionMode::Toggle;
    }

    ctx_->selection->EndRectangleSelection(room->GetTileObjects(), mode);
  }
}

void TileObjectHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  (void)delta;
  if (!is_dragging_ || !ctx_ || !ctx_->selection) return;

  drag_current_ = snapping::SnapToTileGrid(current_pos);
  const bool alt_down = ImGui::GetIO().KeyAlt;
  
  // Calculate total drag delta from start (snapped)
  ImVec2 drag_delta = ImVec2(drag_current_.x - drag_start_.x,
                             drag_current_.y - drag_start_.y);
  drag_delta = ApplyDragModifiers(drag_delta);

  const int tile_dx = static_cast<int>(drag_delta.x) / 8;
  const int tile_dy = static_cast<int>(drag_delta.y) / 8;

  // Option-drag (Alt) duplicates once
  if (alt_down && !drag_has_duplicated_) {
    if (!drag_mutation_started_) {
      ctx_->NotifyMutation();
      drag_mutation_started_ = true;
    }
    
    // Duplicate objects at current position (delta 0 initially)
    auto new_indices = DuplicateObjects(
        ctx_->current_room_id, ctx_->selection->GetSelectedIndices(),
        /*delta_x=*/0, /*delta_y=*/0,
        /*notify_mutation=*/false);
        
    // Update selection to the new clones
    ctx_->selection->ClearSelection();
    for (size_t idx : new_indices) {
      ctx_->selection->SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
    drag_has_duplicated_ = true;
  }

  // Calculate incremental move
  const int inc_dx = tile_dx - drag_last_dx_;
  const int inc_dy = tile_dy - drag_last_dy_;

  if (inc_dx != 0 || inc_dy != 0) {
    if (!drag_mutation_started_) {
      ctx_->NotifyMutation();
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
    is_dragging_ = false;
    drag_mutation_started_ = false;
    drag_has_duplicated_ = false;
  }
}

ImVec2 TileObjectHandler::ApplyDragModifiers(const ImVec2& delta) const {
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyShift) return delta;
  if (std::abs(delta.x) >= std::abs(delta.y)) return ImVec2(delta.x, 0.0f);
  return ImVec2(0.0f, delta.y);
}


bool TileObjectHandler::HandleMouseWheel(float delta) {
  if (!HasValidContext() || !ctx_ || !ctx_->selection || delta == 0.0f) return false;
  
  auto indices = ctx_->selection->GetSelectedIndices();
  if (indices.empty()) return false;
  
  int resize_delta = (delta > 0.0f) ? 1 : -1;
  ResizeObjects(ctx_->current_room_id, indices, resize_delta);
  return true;
}

void TileObjectHandler::DrawGhostPreview() {
  if (!object_placement_mode_ || preview_object_.id_ < 0 || !HasValidContext()) return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  ImVec2 mouse_pos = io.MousePos;
  float scale = GetCanvasScale();

  ImVec2 canvas_mouse_pos = ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
  auto [room_x, room_y] = CanvasToRoom(static_cast<int>(canvas_mouse_pos.x), static_cast<int>(canvas_mouse_pos.y));

  if (!IsWithinBounds(static_cast<int>(canvas_mouse_pos.x), static_cast<int>(canvas_mouse_pos.y))) return;

  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvas(room_x, room_y);
  auto [obj_width, obj_height] = CalculateObjectBounds(preview_object_);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 preview_start(canvas_pos.x + snap_canvas_x * scale, canvas_pos.y + snap_canvas_y * scale);
  ImVec2 preview_end(preview_start.x + obj_width * scale, preview_start.y + obj_height * scale);

  const auto& theme = AgentUI::GetTheme();
  bool drew_bitmap = false;

  if (ghost_preview_buffer_) {
    auto& bitmap = ghost_preview_buffer_->bitmap();
    if (bitmap.texture()) {
      ImVec2 bitmap_end(preview_start.x + bitmap.width() * scale, preview_start.y + bitmap.height() * scale);
      ImVec4 tint = theme.text_primary;
      tint.w = 0.70f;
      draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(), preview_start,
                          bitmap_end, ImVec2(0, 0), ImVec2(1, 1),
                          ImGui::GetColorU32(tint));
      draw_list->AddRect(preview_start, bitmap_end, ImGui::GetColorU32(theme.dungeon_selection_primary), 0.0f, 0, 2.0f);
      drew_bitmap = true;
    }
  }

  if (!drew_bitmap) {
    draw_list->AddRectFilled(preview_start, preview_end, ImGui::GetColorU32(ImVec4(theme.dungeon_selection_primary.x, theme.dungeon_selection_primary.y, theme.dungeon_selection_primary.z, 0.25f)));
    draw_list->AddRect(preview_start, preview_end, ImGui::GetColorU32(theme.dungeon_selection_primary), 0.0f, 0, 2.0f);
  }

  // ID and Crosshair
  std::string id_text = absl::StrFormat("0x%02X", preview_object_.id_);
  draw_list->AddText(ImVec2(preview_start.x + 2, preview_start.y + 1), ImGui::GetColorU32(theme.text_primary), id_text.c_str());
}

void TileObjectHandler::DrawSelectionHighlight() {
  if (!HasValidContext() || !ctx_->selection) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  // Use ObjectSelection's rendering (handles pulsing border, corner handles)
  ctx_->selection->DrawSelectionHighlights(
      ctx_->canvas, room->GetTileObjects(), [](const zelda3::RoomObject& obj) {
        auto result = zelda3::DimensionService::Get().GetDimensions(obj);
        return std::make_tuple(result.offset_x_tiles * 8,
                               result.offset_y_tiles * 8,
                               result.width_pixels(), result.height_pixels());
      });
}

std::optional<size_t> TileObjectHandler::GetEntityAtPosition(int canvas_x, int canvas_y) const {
  auto* room = const_cast<TileObjectHandler*>(this)->GetRoom(ctx_->current_room_id);
  if (!room) return std::nullopt;

  const auto& objects = room->GetTileObjects();
  for (size_t i = objects.size(); i > 0; --i) {
    size_t index = i - 1;
    const auto& object = objects[index];

    // Respect layer filter if available in context
    if (ctx_ && ctx_->selection && !ctx_->selection->PassesLayerFilterForObject(object)) {
      continue;
    }

    auto [obj_tile_x, obj_tile_y, width_tiles, height_tiles] = zelda3::DimensionService::Get().GetHitTestBounds(object);
    
    int obj_px = obj_tile_x * 8;
    int obj_py = obj_tile_y * 8;
    int w_px = width_tiles * 8;
    int h_px = height_tiles * 8;

    if (canvas_x >= obj_px && canvas_x < obj_px + w_px &&
        canvas_y >= obj_py && canvas_y < obj_py + h_px) {
      return index;
    }
  }
  return std::nullopt;
}

// ========================================================================
// Mutation Logic
// ========================================================================

void TileObjectHandler::MoveObjects(int room_id,
                                    const std::vector<size_t>& indices,
                                    int delta_x, int delta_y,
                                    bool notify_mutation) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (notify_mutation && ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].x_ = std::clamp(static_cast<int>(objects[index].x_ + delta_x), 0, 63);
      objects[index].y_ = std::clamp(static_cast<int>(objects[index].y_ + delta_y), 0, 63);
    }
  }

  NotifyChange(room);
}

void TileObjectHandler::UpdateObjectsId(int room_id, const std::vector<size_t>& indices, int16_t new_id) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      // Use the setter so derived flags + tile caches stay coherent.
      objects[index].set_id(new_id);
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::UpdateObjectsSize(int room_id, const std::vector<size_t>& indices, uint8_t new_size) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].size_ = new_size;
      objects[index].tiles_loaded_ = false;
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::UpdateObjectsLayer(int room_id, const std::vector<size_t>& indices, int new_layer) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  auto layer = static_cast<zelda3::RoomObject::LayerType>(new_layer);
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].layer_ = layer;
    }
  }
  NotifyChange(room);
}

std::vector<size_t> TileObjectHandler::DuplicateObjects(
    int room_id, const std::vector<size_t>& indices, int delta_x, int delta_y,
    bool notify_mutation) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return {};
  if (notify_mutation && ctx_) ctx_->NotifyMutation();

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

void TileObjectHandler::DeleteObjects(int room_id, std::vector<size_t> indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  std::sort(indices.rbegin(), indices.rend());
  for (size_t index : indices) {
    room->RemoveTileObject(index);
  }

  NotifyChange(room);
}

void TileObjectHandler::DeleteAllObjects(int room_id) {
  auto* room = GetRoom(room_id);
  if (!room) return;
  if (ctx_) ctx_->NotifyMutation();
  room->ClearTileObjects();
  NotifyChange(room);
}

void TileObjectHandler::SendToFront(int room_id, const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  std::vector<zelda3::RoomObject> selected, other;

  for (size_t i = 0; i < objects.size(); ++i) {
    if (std::find(indices.begin(), indices.end(), i) != indices.end())
      selected.push_back(objects[i]);
    else
      other.push_back(objects[i]);
  }

  objects = std::move(other);
  objects.insert(objects.end(), selected.begin(), selected.end());
  NotifyChange(room);
}

void TileObjectHandler::SendToBack(int room_id, const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  std::vector<zelda3::RoomObject> selected, other;

  for (size_t i = 0; i < objects.size(); ++i) {
    if (std::find(indices.begin(), indices.end(), i) != indices.end())
      selected.push_back(objects[i]);
    else
      other.push_back(objects[i]);
  }

  objects = std::move(selected);
  objects.insert(objects.end(), other.begin(), other.end());
  NotifyChange(room);
}

void TileObjectHandler::MoveForward(int room_id, const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  auto sorted_indices = indices;
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());

  for (size_t idx : sorted_indices) {
    if (idx < objects.size() - 1) {
      std::swap(objects[idx], objects[idx + 1]);
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::MoveBackward(int room_id, const std::vector<size_t>& indices) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();

  auto& objects = room->GetTileObjects();
  auto sorted_indices = indices;
  std::sort(sorted_indices.begin(), sorted_indices.end());

  for (size_t idx : sorted_indices) {
    if (idx > 0) {
      std::swap(objects[idx], objects[idx - 1]);
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::ResizeObjects(int room_id, const std::vector<size_t>& indices, int delta) {
  auto* room = GetRoom(room_id);
  if (!room || indices.empty()) return;
  if (ctx_) ctx_->NotifyMutation();
  auto& objects = room->GetTileObjects();
  for (size_t index : indices) {
    if (index < objects.size()) {
       int new_size = std::clamp(static_cast<int>(objects[index].size_) + delta, 0, 15);
       objects[index].size_ = static_cast<uint8_t>(new_size);
       objects[index].tiles_loaded_ = false;
    }
  }
  NotifyChange(room);
}

void TileObjectHandler::PlaceObjectAt(int room_id, const zelda3::RoomObject& object, int x, int y) {
  auto* room = GetRoom(room_id);
  if (!room) return;
  if (ctx_) ctx_->NotifyMutation();
  auto new_obj = object;
  new_obj.x_ = std::clamp(x, 0, 63);
  new_obj.y_ = std::clamp(y, 0, 63);
  room->AddTileObject(new_obj);
  NotifyChange(room);
}


void TileObjectHandler::SetPreviewObject(const zelda3::RoomObject& object) {
  preview_object_ = object;
  if (object_placement_mode_) {
    RenderGhostPreviewBitmap();
  }
}

void TileObjectHandler::RenderGhostPreviewBitmap() {
  if (!ctx_ || !ctx_->rom || !ctx_->rom->is_loaded()) return;
  
  auto* room = GetRoom(ctx_->current_room_id);
  if (!room || !room->IsLoaded()) return;

  auto [width, height] = CalculateObjectBounds(preview_object_);
  width = std::max(width, 16);
  height = std::max(height, 16);

  ghost_preview_buffer_ = std::make_unique<gfx::BackgroundBuffer>(width, height);
  const uint8_t* gfx_data = room->get_gfx_buffer().data();

  zelda3::ObjectDrawer drawer(ctx_->rom, ctx_->current_room_id, gfx_data);
  drawer.InitializeDrawRoutines();

  auto status = drawer.DrawObject(preview_object_, *ghost_preview_buffer_, *ghost_preview_buffer_, ctx_->current_palette_group);
  if (!status.ok()) {
    ghost_preview_buffer_.reset();
    return;
  }

  auto& bitmap = ghost_preview_buffer_->bitmap();
  if (bitmap.size() > 0) {
    gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                          &bitmap);
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
  if (!room || indices.empty()) return;

  clipboard_.clear();
  const auto& objects = room->GetTileObjects();
  
  for (size_t idx : indices) {
    if (idx < objects.size()) {
      clipboard_.push_back(objects[idx]);
    }
  }
}

std::vector<size_t> TileObjectHandler::PasteFromClipboard(
    int room_id, int offset_x, int offset_y) {
  auto* room = GetRoom(room_id);
  if (!room || clipboard_.empty()) return {};
  if (ctx_) ctx_->NotifyMutation();

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

std::vector<size_t> TileObjectHandler::PasteFromClipboardAt(
    int room_id, int target_x, int target_y) {
  if (clipboard_.empty()) return {};
  
  int offset_x = target_x - clipboard_[0].x_;
  int offset_y = target_y - clipboard_[0].y_;
  
  return PasteFromClipboard(room_id, offset_x, offset_y);
}

} // namespace yaze::editor
