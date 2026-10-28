#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>

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

void DungeonCanvasViewer::UpdateRoomCanvasViewport(ImVec2 viewport_size) {
  const float fit_scale =
      std::max(0.001f, std::min(viewport_size.x, viewport_size.y) /
                           kDungeonRoomPixelSize);
  const float current_scale = canvas_.global_scale();
  if (room_viewport_last_scale_ < 0) {
    // The initial 1x Canvas value is its default. A supplied non-default zoom
    // is intentional and remains centered until Fit is requested.
    room_viewport_auto_fit_ = std::abs(current_scale - 1.0f) < 0.0001f;
  } else if (std::abs(current_scale - room_viewport_last_scale_) > 0.0001f) {
    room_viewport_auto_fit_ = false;
  }
  // Selection/find requests should reveal their tile without reintroducing
  // free pan. Retain explicit zoom when the tile is already visible; otherwise
  // return to Fit before BeginCanvas creates drawing and input geometry.
  if (!room_viewport_auto_fit_ && pending_scroll_target_) {
    const auto [tile_x, tile_y] = *pending_scroll_target_;
    constexpr float kTileSize = 8.0f;
    const float tile_size = kTileSize * current_scale;
    const ImVec2 centered(
        (viewport_size.x - kDungeonRoomPixelSize * current_scale) * 0.5f,
        (viewport_size.y - kDungeonRoomPixelSize * current_scale) * 0.5f);
    const ImVec2 target(centered.x + std::clamp(tile_x, 0, 63) * tile_size,
                        centered.y + std::clamp(tile_y, 0, 63) * tile_size);
    if (target.x < 0 || target.y < 0 ||
        target.x + tile_size > viewport_size.x ||
        target.y + tile_size > viewport_size.y) {
      room_viewport_auto_fit_ = true;
    }
  }
  if (room_viewport_auto_fit_) {
    canvas_.set_global_scale(fit_scale);
  }
  room_viewport_last_scale_ = canvas_.global_scale();
  const float room_size = kDungeonRoomPixelSize * room_viewport_last_scale_;
  canvas_.set_scrolling(ImVec2((viewport_size.x - room_size) * 0.5f,
                               (viewport_size.y - room_size) * 0.5f));
}

void DungeonCanvasViewer::HandleRoomCanvasNavigation(
    gui::CanvasRuntime& canvas_rt) {
  auto& io = ImGui::GetIO();
  if (canvas_rt.hovered && (io.MouseWheel != 0 || io.MouseWheelH != 0)) {
    const bool tool_active = crumble_floor_view_.rehearsing() ||
                             (minecart_track_panel_ &&
                              (minecart_track_panel_->canvas_editing() ||
                               minecart_track_panel_->collision_stamping() ||
                               minecart_track_panel_->placement_brush()));
    const bool resize_object =
        object_interaction_enabled_ && !tool_active &&
        (!object_interaction_.GetSelectedObjectIndices().empty() ||
         object_interaction_.entity_coordinator().IsPlacementActive());
    if (resize_object) {
      object_interaction_.HandleScrollWheelResize();
    }
    io.MouseWheel = 0;
    io.MouseWheelH = 0;
  }

  // Normal room view stays centered, including explicit zoom and restored
  // offsets. Free navigation belongs to the separate stitched-room overview.
  UpdateRoomCanvasViewport(canvas_rt.canvas_sz);
  canvas_rt.scrolling = canvas_.scrolling();
  canvas_rt.scale = canvas_.global_scale();
}

void DungeonCanvasViewer::SyncCanvasConfigFromViewerState() {
  auto canvas_config = canvas_.GetConfig();
  canvas_config.enable_grid = show_grid_;
  canvas_config.grid_step = static_cast<float>(custom_grid_size_);
  canvas_config.is_draggable = false;
  // Main dungeon canvas is an editable scratchpad: users paint tiles into
  // the room and manipulate object/sprite/door entities directly. Declaring
  // the role lets future canvas surfaces default appropriately without each
  // editor re-encoding the policy.
  canvas_config.role = gui::CanvasRole::kEditableScratchpad;
  canvas_.ApplyConfigSnapshot(canvas_config);
  canvas_.SetShowBuiltinContextMenu(false);
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
