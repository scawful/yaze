#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>

#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui_constants.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

using namespace dungeon_ui;  // NOLINT(google-build-using-namespace)

}  // namespace

bool DungeonCanvasViewer::ValidateRoomCanvasRequest(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    ImGui::Text(tr("Invalid room ID: %d"), room_id);
    return false;
  }

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text(tr("ROM not loaded"));
    return false;
  }

  return true;
}

gui::CanvasFrameOptions DungeonCanvasViewer::BuildRoomCanvasFrameOptions()
    const {
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = ImVec2(kDungeonRoomPixelSize, kDungeonRoomPixelSize);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  frame_opts.viewport_size =
      ImVec2(std::max(1.0f, available.x), std::max(1.0f, available.y));
  frame_opts.draw_grid = show_grid_;
  frame_opts.grid_step = static_cast<float>(custom_grid_size_);
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  return frame_opts;
}

void DungeonCanvasViewer::HandleRoomCanvasNavigation(
    gui::CanvasRuntime& canvas_rt) {
  auto& io = ImGui::GetIO();
  ImVec2 scroll = canvas_.scrolling();
  if (canvas_rt.hovered && (io.MouseWheel != 0 || io.MouseWheelH != 0)) {
    const bool tool_active = crumble_floor_view_.rehearsing() ||
                             (minecart_track_panel_ &&
                              (minecart_track_panel_->canvas_editing() ||
                               minecart_track_panel_->collision_stamping()));
    const bool resize_object =
        object_interaction_enabled_ && !tool_active &&
        (!object_interaction_.GetSelectedObjectIndices().empty() ||
         object_interaction_.entity_coordinator().IsPlacementActive());
    if (resize_object) {
      object_interaction_.HandleScrollWheelResize();
    } else {
      constexpr float kWheelPanPixels = 24.0f;
      scroll.x += io.MouseWheelH * kWheelPanPixels;
      scroll.y += io.MouseWheel * kWheelPanPixels;
    }
    io.MouseWheel = 0;
    io.MouseWheelH = 0;
  }

  // Also constrain right-drag, zoom, and restored offsets. When a room fits,
  // preserve centering within the spare space; otherwise expose only its
  // overflow. The room can never be panned entirely outside the viewport.
  const float room_size = kDungeonRoomPixelSize * canvas_.global_scale();
  auto clamp_axis = [room_size](float offset, float viewport) {
    const float spare = viewport - room_size;
    return std::clamp(offset, std::min(0.0f, spare), std::max(0.0f, spare));
  };
  canvas_.set_scrolling(ImVec2(clamp_axis(scroll.x, canvas_rt.canvas_sz.x),
                               clamp_axis(scroll.y, canvas_rt.canvas_sz.y)));
  canvas_rt.scrolling = canvas_.scrolling();
}

void DungeonCanvasViewer::SyncCanvasConfigFromViewerState() {
  auto canvas_config = canvas_.GetConfig();
  canvas_config.enable_grid = show_grid_;
  canvas_config.grid_step = static_cast<float>(custom_grid_size_);
  // Main dungeon canvas is an editable scratchpad: users paint tiles into
  // the room and manipulate object/sprite/door entities directly. Declaring
  // the role lets future canvas surfaces default appropriately without each
  // editor re-encoding the policy.
  canvas_config.role = gui::CanvasRole::kEditableScratchpad;
  canvas_.ApplyConfigSnapshot(canvas_config);
  canvas_.SetShowBuiltinContextMenu(true);
}

void DungeonCanvasViewer::SyncViewerStateFromCanvasConfig() {
  const auto& config = canvas_.GetConfig();
  show_grid_ = config.enable_grid;
  set_custom_grid_size(static_cast<int>(config.grid_step));
}

void DungeonCanvasViewer::RefreshActiveRoomCanvasState(zelda3::Room& room,
                                                       int room_id) {
  if (prev_blockset_ != room.blockset() || prev_palette_ != room.palette() ||
      prev_layout_ != room.layout_id() || prev_spriteset_ != room.spriteset()) {
    if (room.rom() && room.rom()->is_loaded()) {
      room.ReloadGraphics(current_entrance_blockset_);
    }

    prev_blockset_ = room.blockset();
    prev_palette_ = room.palette();
    prev_layout_ = room.layout_id();
    prev_spriteset_ = room.spriteset();
  }

  if (header_visible_) {
    DrawRoomHeader(room, room_id);
  }
}

zelda3::Room* DungeonCanvasViewer::PrepareActiveRoomForCanvasFrame(
    int room_id) {
  if (!rooms_) {
    return nullptr;
  }

  auto* room_ptr = rooms_->TryEnsureRoom(room_id);
  if (!room_ptr) {
    return nullptr;
  }
  auto& room = *room_ptr;
  RefreshActiveRoomCanvasState(room, room_id);
  return &room;
}

}  // namespace yaze::editor
