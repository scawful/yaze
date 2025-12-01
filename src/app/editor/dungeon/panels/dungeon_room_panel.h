#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_PANEL_H_

#include <functional>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_loader.h"
#include "app/editor/system/resource_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonRoomPanel
 * @brief ResourcePanel for editing individual dungeon rooms
 *
 * This panel provides a tabbed view for editing a specific dungeon room.
 * Multiple rooms can be open simultaneously (up to kMaxRoomPanels).
 * Each panel shows the room canvas with objects, sprites, and layers.
 *
 * @section Features
 * - Room canvas with pan/zoom
 * - Object selection and manipulation
 * - Sprite editing
 * - Layer visibility toggles
 * - Lazy loading of room data
 *
 * @see ResourcePanel - Base class for resource-bound panels
 * @see DungeonCanvasViewer - The canvas rendering component
 */
class DungeonRoomPanel : public ResourcePanel {
 public:
  /**
   * @brief Construct a room panel
   * @param session_id The session this room belongs to
   * @param room_id The room ID (0-295)
   * @param room Pointer to the room data (must outlive panel)
   * @param canvas_viewer Pointer to canvas viewer for rendering
   * @param room_loader Pointer to room loader for lazy loading
   */
  DungeonRoomPanel(size_t session_id, int room_id, zelda3::Room* room,
                   DungeonCanvasViewer* canvas_viewer,
                   DungeonRoomLoader* room_loader)
      : room_id_(room_id),
        room_(room),
        canvas_viewer_(canvas_viewer),
        room_loader_(room_loader) {
    session_id_ = session_id;
  }

  // ==========================================================================
  // ResourcePanel Identity
  // ==========================================================================

  int GetResourceId() const override { return room_id_; }
  std::string GetResourceType() const override { return "room"; }

  std::string GetResourceName() const override {
    if (room_id_ >= 0 &&
        static_cast<size_t>(room_id_) < std::size(zelda3::kRoomNames)) {
      return absl::StrFormat("[%03X] %s", room_id_,
                             zelda3::kRoomNames[room_id_].data());
    }
    return absl::StrFormat("Room %03X", room_id_);
  }

  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "dungeon"; }
  int GetPriority() const override { return 100 + room_id_; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!room_ || !canvas_viewer_) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Room data unavailable");
      return;
    }

    // Lazy load room data
    if (!room_->IsLoaded() && room_loader_) {
      auto status = room_loader_->LoadRoom(room_id_, *room_);
      if (!status.ok()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load room: %s",
                           status.message().data());
        return;
      }
    }

    // Initialize room graphics if needed
    if (room_->IsLoaded()) {
      bool needs_render = false;

      if (room_->blocks().empty()) {
        room_->LoadRoomGraphics(room_->blockset);
        needs_render = true;
      }

      if (room_->GetTileObjects().empty()) {
        room_->LoadObjects();
        needs_render = true;
      }

      auto& bg1_bitmap = room_->bg1_buffer().bitmap();
      if (needs_render || !bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
        room_->RenderRoomGraphics();
      }
    }

    // Room status header
    if (room_->IsLoaded()) {
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                         ICON_MD_CHECK " Loaded");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         ICON_MD_PENDING " Loading...");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Objects: %zu", room_->GetTileObjects().size());

    ImGui::Separator();

    // Draw the room canvas
    canvas_viewer_->DrawDungeonCanvas(room_id_);
  }

  // ==========================================================================
  // ResourcePanel Lifecycle
  // ==========================================================================

  void OnResourceModified() override {
    // Re-render room when modified externally
    if (room_ && room_->IsLoaded()) {
      room_->RenderRoomGraphics();
    }
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  zelda3::Room* room() const { return room_; }
  int room_id() const { return room_id_; }

 private:
  int room_id_ = 0;
  zelda3::Room* room_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  DungeonRoomLoader* room_loader_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_ROOM_PANEL_H_
