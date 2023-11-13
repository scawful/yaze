#ifndef YAZE_APP_EDITOR_DUNGEONEDITOR_H
#define YAZE_APP_EDITOR_DUNGEONEDITOR_H

#include <imgui/imgui.h>

#include "app/core/common.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace editor {

class DungeonEditor : public SharedROM {
 public:
  void Update();

 private:
  void DrawToolset();

  void DrawDungeonTabView();
  void DrawDungeonCanvas(int room_id);
  void DrawRoomGraphics();
  void DrawTileSelector();

  bool is_loaded_ = false;

  gfx::Bitmap room_gfx_bmp_;

  ImVector<int> active_rooms_;

  std::vector<zelda3::dungeon::Room> rooms_;

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
