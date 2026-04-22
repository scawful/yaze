#include "dungeon_canvas_viewer.h"

namespace yaze::editor {

void DungeonCanvasViewer::SetProject(const project::YazeProject* project) {
  project_ = project;
  ApplyTrackCollisionConfig();
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
  }

  DrawCoordinateOverlayHud(room_id);

  gui::EndCanvas(canvas_, canvas_rt, frame_opts);
  SyncViewerStateFromCanvasConfig();
}

}  // namespace yaze::editor
