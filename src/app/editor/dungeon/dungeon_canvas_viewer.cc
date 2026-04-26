#include "dungeon_canvas_viewer.h"

#include <algorithm>
#include <vector>

#include "app/gui/core/agent_theme.h"
#include "zelda3/dungeon/dimension_service.h"

namespace yaze::editor {

namespace {

constexpr double kChangePingDurationSeconds = 0.85;
constexpr int kDungeonCanvasPixelSize = 512;
constexpr size_t kRecentRoomLimit = 5;

bool HasSameObjectIdentity(const zelda3::RoomObject& lhs,
                           const zelda3::RoomObject& rhs) {
  return lhs.id_ == rhs.id_ && lhs.x() == rhs.x() && lhs.y() == rhs.y() &&
         lhs.size() == rhs.size() && lhs.GetLayerValue() == rhs.GetLayerValue();
}

}  // namespace

void DungeonCanvasViewer::RecordVisitedRoom(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return;
  }

  recently_visited_rooms_.erase(
      std::remove(recently_visited_rooms_.begin(),
                  recently_visited_rooms_.end(), room_id),
      recently_visited_rooms_.end());
  recently_visited_rooms_.insert(recently_visited_rooms_.begin(), room_id);
  if (recently_visited_rooms_.size() > kRecentRoomLimit) {
    recently_visited_rooms_.resize(kRecentRoomLimit);
  }
}

void DungeonCanvasViewer::SetProject(const project::YazeProject* project) {
  project_ = project;
  ApplyTrackCollisionConfig();
}

void DungeonCanvasViewer::TriggerChangePing() {
  change_ping_rects_.clear();
  change_ping_start_time_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
}

void DungeonCanvasViewer::TriggerCanvasPingRect(int pixel_x, int pixel_y,
                                                int pixel_w, int pixel_h) {
  change_ping_rects_.clear();
  const int x = std::clamp(pixel_x, 0, kDungeonCanvasPixelSize - 1);
  const int y = std::clamp(pixel_y, 0, kDungeonCanvasPixelSize - 1);
  const int w =
      std::clamp(pixel_w, 1, std::max(1, kDungeonCanvasPixelSize - x));
  const int h =
      std::clamp(pixel_h, 1, std::max(1, kDungeonCanvasPixelSize - y));
  change_ping_rects_.push_back(ChangePingRect{x, y, w, h});
  change_ping_start_time_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
}

void DungeonCanvasViewer::TriggerObjectChangePing(
    const std::vector<zelda3::RoomObject>& previous_objects,
    const std::vector<zelda3::RoomObject>& next_objects) {
  change_ping_rects_.clear();

  auto append_object_rect = [this](const zelda3::RoomObject& object) {
    const auto [x, y, w, h] =
        zelda3::DimensionService::Get().GetSelectionBoundsPixels(object);
    change_ping_rects_.push_back(
        ChangePingRect{x, y, std::max(w, 8), std::max(h, 8)});
  };

  const size_t count = std::max(previous_objects.size(), next_objects.size());
  for (size_t i = 0; i < count; ++i) {
    const bool has_previous = i < previous_objects.size();
    const bool has_next = i < next_objects.size();
    if (has_previous && has_next &&
        HasSameObjectIdentity(previous_objects[i], next_objects[i])) {
      continue;
    }
    if (has_previous) {
      append_object_rect(previous_objects[i]);
    }
    if (has_next) {
      append_object_rect(next_objects[i]);
    }
  }

  if (change_ping_rects_.empty()) {
    change_ping_start_time_ = -1.0;
    return;
  }

  change_ping_start_time_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
}

void DungeonCanvasViewer::ApplyTrackCollisionConfig() {
  auto apply_list = [](std::array<bool, 256>& dest,
                       const std::vector<uint16_t>& values) {
    dest.fill(false);
    for (uint16_t value : values) {
      if (value < 256) {
        dest[value] = true;
      }
    }
  };
  auto ids_valid = [](const std::vector<uint16_t>& values) {
    std::array<bool, 256> seen{};
    for (uint16_t value : values) {
      if (value >= seen.size() || seen[value]) {
        return false;
      }
      seen[value] = true;
    }
    return true;
  };

  std::vector<uint16_t> default_track_tile_list;
  for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
    default_track_tile_list.push_back(tile);
  }
  const std::vector<uint16_t> default_stop_tile_list = {0xB7, 0xB8, 0xB9, 0xBA};
  const std::vector<uint16_t> default_switch_tile_list = {0xD0, 0xD1, 0xD2,
                                                          0xD3};

  const auto& track_tiles =
      (project_ && !project_->dungeon_overlay.track_tiles.empty())
          ? project_->dungeon_overlay.track_tiles
          : default_track_tile_list;
  apply_list(track_collision_config_.track_tiles, track_tiles);
  track_tile_order_ = track_tiles;

  const auto& stop_tiles =
      (project_ && !project_->dungeon_overlay.track_stop_tiles.empty())
          ? project_->dungeon_overlay.track_stop_tiles
          : default_stop_tile_list;
  apply_list(track_collision_config_.stop_tiles, stop_tiles);

  const auto& switch_tiles =
      (project_ && !project_->dungeon_overlay.track_switch_tiles.empty())
          ? project_->dungeon_overlay.track_switch_tiles
          : default_switch_tile_list;
  apply_list(track_collision_config_.switch_tiles, switch_tiles);
  switch_tile_order_ = switch_tiles;

  track_direction_map_enabled_ =
      (track_tile_order_.size() == default_track_tile_list.size()) &&
      (switch_tile_order_.size() == default_switch_tile_list.size()) &&
      ids_valid(track_tile_order_) && ids_valid(switch_tile_order_);

  minecart_sprite_ids_.reset();
  std::vector<uint16_t> minecart_ids = {0xA3};
  if (project_ && !project_->dungeon_overlay.minecart_sprite_ids.empty()) {
    minecart_ids = project_->dungeon_overlay.minecart_sprite_ids;
  }
  for (uint16_t id : minecart_ids) {
    if (id < minecart_sprite_ids_.size()) {
      minecart_sprite_ids_[id] = true;
    }
  }

  collision_overlay_cache_.clear();
}

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  current_room_id_ = room_id;
  if (!ValidateRoomCanvasRequest(room_id)) {
    return;
  }
  RecordVisitedRoom(room_id);

  ImGui::BeginGroup();
  const auto frame_opts = BuildRoomCanvasFrameOptions();
  SyncCanvasConfigFromViewerState();
  zelda3::Room* active_room = PrepareActiveRoomForCanvasFrame(room_id);
  DrawCompactLayerToggles(room_id);
  ImGui::EndGroup();

  PopulateCanvasContextMenu(room_id);

  auto canvas_rt = gui::BeginCanvas(canvas_, frame_opts);
  SyncCanvasCaptureRegion(canvas_rt);
  ConsumePendingCanvasScroll(canvas_rt);
  canvas_rt.scrolling = canvas_.scrolling();

  touch_handler_.ProcessForCanvas(canvas_rt.canvas_p0, canvas_rt.canvas_sz,
                                  canvas_rt.hovered);
  touch_handler_.Update();

  DrawHeaderHiddenMetadataHud(room_id);
  DrawPersistentDebugWindows(room_id);

  if (active_room != nullptr) {
    DrawRoomCanvasContent(canvas_rt, *active_room, room_id);
    DrawRoomCanvasOverlays(canvas_rt, *active_room, room_id);
    DrawChangePingOverlay(canvas_rt, *active_room);
  }

  DrawCoordinateOverlayHud(room_id);

  gui::EndCanvas(canvas_, canvas_rt, frame_opts);
  SyncViewerStateFromCanvasConfig();
}

void DungeonCanvasViewer::DrawChangePingOverlay(
    const gui::CanvasRuntime& canvas_rt, const zelda3::Room& room) {
  if (!canvas_rt.draw_list || change_ping_start_time_ < 0.0 ||
      !ImGui::GetCurrentContext()) {
    return;
  }

  const double elapsed = ImGui::GetTime() - change_ping_start_time_;
  if (elapsed >= kChangePingDurationSeconds) {
    change_ping_start_time_ = -1.0;
    change_ping_rects_.clear();
    return;
  }

  std::vector<ChangePingRect> rects = change_ping_rects_;
  if (rects.empty()) {
    const auto& objects = room.GetTileObjects();
    for (size_t index : object_interaction_.GetSelectedObjectIndices()) {
      if (index >= objects.size()) {
        continue;
      }
      const auto [x, y, w, h] =
          zelda3::DimensionService::Get().GetSelectionBoundsPixels(
              objects[index]);
      rects.push_back(ChangePingRect{x, y, std::max(w, 8), std::max(h, 8)});
    }

    auto selected_entities =
        object_interaction_.entity_coordinator().GetSelectedEntities();
    if (selected_entities.empty() && object_interaction_.HasEntitySelection()) {
      selected_entities.push_back(object_interaction_.GetSelectedEntity());
    }
    for (const SelectedEntity entity : selected_entities) {
      switch (entity.type) {
        case EntityType::Door: {
          const auto& doors = room.GetDoors();
          if (entity.index < doors.size()) {
            const auto [x, y, w, h] = doors[entity.index].GetEditorBounds();
            rects.push_back(
                ChangePingRect{x, y, std::max(w, 8), std::max(h, 8)});
          }
          break;
        }
        case EntityType::Sprite: {
          const auto& sprites = room.GetSprites();
          if (entity.index < sprites.size()) {
            rects.push_back(ChangePingRect{sprites[entity.index].x() * 16,
                                           sprites[entity.index].y() * 16, 16,
                                           16});
          }
          break;
        }
        case EntityType::Item: {
          const auto& pot_items = room.GetPotItems();
          if (entity.index < pot_items.size()) {
            rects.push_back(ChangePingRect{pot_items[entity.index].GetPixelX(),
                                           pot_items[entity.index].GetPixelY(),
                                           16, 16});
          }
          break;
        }
        case EntityType::Object:
        case EntityType::None:
          break;
      }
    }
  }

  if (rects.empty()) {
    rects.push_back(
        ChangePingRect{0, 0, kDungeonCanvasPixelSize, kDungeonCanvasPixelSize});
  }

  const float progress =
      static_cast<float>(elapsed / kChangePingDurationSeconds);
  const float remaining = std::clamp(1.0f - progress, 0.0f, 1.0f);
  const float expand = 4.0f + 12.0f * progress;
  const auto& theme = AgentUI::GetTheme();
  ImVec4 fill = theme.dungeon_selection_pulsing;
  fill.w = 0.18f * remaining;
  ImVec4 border = theme.dungeon_selection_primary;
  border.w = 0.95f * remaining;
  const ImU32 fill_color = ImGui::GetColorU32(fill);
  const ImU32 border_color = ImGui::GetColorU32(border);

  const ImVec2 viewport_min = canvas_rt.canvas_p0;
  const ImVec2 viewport_max(canvas_rt.canvas_p0.x + canvas_rt.canvas_sz.x,
                            canvas_rt.canvas_p0.y + canvas_rt.canvas_sz.y);

  for (const ChangePingRect& rect : rects) {
    const ImVec2 min(canvas_rt.canvas_p0.x + canvas_rt.scrolling.x +
                         static_cast<float>(rect.x) * canvas_rt.scale - expand,
                     canvas_rt.canvas_p0.y + canvas_rt.scrolling.y +
                         static_cast<float>(rect.y) * canvas_rt.scale - expand);
    const ImVec2 max(
        min.x + static_cast<float>(rect.w) * canvas_rt.scale + expand * 2.0f,
        min.y + static_cast<float>(rect.h) * canvas_rt.scale + expand * 2.0f);

    const bool visible = max.x >= viewport_min.x && min.x <= viewport_max.x &&
                         max.y >= viewport_min.y && min.y <= viewport_max.y;
    if (visible) {
      canvas_rt.draw_list->AddRectFilled(min, max, fill_color, 2.0f);
      canvas_rt.draw_list->AddRect(min, max, border_color, 2.0f, 0,
                                   2.0f + 2.0f * remaining);
      continue;
    }

    const ImVec2 center(
        canvas_rt.canvas_p0.x + canvas_rt.scrolling.x +
            (static_cast<float>(rect.x) + static_cast<float>(rect.w) * 0.5f) *
                canvas_rt.scale,
        canvas_rt.canvas_p0.y + canvas_rt.scrolling.y +
            (static_cast<float>(rect.y) + static_cast<float>(rect.h) * 0.5f) *
                canvas_rt.scale);
    const float marker_min_x = viewport_min.x + 8.0f;
    const float marker_min_y = viewport_min.y + 8.0f;
    const float marker_max_x = std::max(marker_min_x, viewport_max.x - 8.0f);
    const float marker_max_y = std::max(marker_min_y, viewport_max.y - 8.0f);
    const ImVec2 clamped(std::clamp(center.x, marker_min_x, marker_max_x),
                         std::clamp(center.y, marker_min_y, marker_max_y));
    canvas_rt.draw_list->AddCircleFilled(clamped, 5.0f + 4.0f * remaining,
                                         fill_color);
    canvas_rt.draw_list->AddCircle(clamped, 7.0f + 6.0f * remaining,
                                   border_color, 18, 2.0f);
  }
}

}  // namespace yaze::editor
