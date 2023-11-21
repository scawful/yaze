#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/core/editor.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace app {
namespace editor {

class DungeonEditor : public Editor,
                      public SharedROM,
                      public core::ExperimentFlags {
 public:
  absl::Status Update() override;
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }

 private:
  void DrawToolset();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);
  void DrawRoomGraphics();
  void DrawTileSelector();

  uint16_t current_room_id_ = 0;
  bool is_loaded_ = false;

  gfx::Bitmap room_gfx_bmp_;

  ImVector<int> active_rooms_;

  std::vector<zelda3::dungeon::Room> rooms_;
  zelda3::dungeon::DungeonObjectRenderer object_renderer_;

  gui::Canvas canvas_;
  gui::Canvas room_gfx_canvas_;
  ImGuiTableFlags toolset_table_flags_ = ImGuiTableFlags_SizingFixedFit |
                                         ImGuiTableFlags_Reorderable |
                                         ImGuiTableFlags_Resizable;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
