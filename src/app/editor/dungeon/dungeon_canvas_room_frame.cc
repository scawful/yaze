#include "dungeon_canvas_viewer.h"

#include "app/editor/dungeon/ui_constants.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

using namespace dungeon_ui;  // NOLINT(google-build-using-namespace)

}  // namespace

bool DungeonCanvasViewer::ValidateRoomCanvasRequest(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return false;
  }

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return false;
  }

  return true;
}

gui::CanvasFrameOptions DungeonCanvasViewer::BuildRoomCanvasFrameOptions()
    const {
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = ImVec2(kDungeonRoomPixelSize, kDungeonRoomPixelSize);
  frame_opts.draw_grid = show_grid_;
  frame_opts.grid_step = static_cast<float>(custom_grid_size_);
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  return frame_opts;
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
