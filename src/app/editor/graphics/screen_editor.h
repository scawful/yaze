#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <imgui/imgui.h>

#include <array>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/editor/utils/editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gfx/tilesheet.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/screen/dungeon_map.h"
#include "app/zelda3/screen/inventory.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @brief The ScreenEditor class allows the user to edit a variety of screens in
 * the game or create a custom menu.
 *
 * This class is currently a work in progress (WIP) and provides functionality
 * for updating the screens, saving dungeon maps, drawing different types of
 * screens, loading dungeon maps, and managing various properties related to the
 * editor.
 *
 * The screens that can be edited include the title screen, naming screen,
 * overworld map, inventory menu, and more.
 *
 * The class inherits from the SharedRom class.
 */
class ScreenEditor : public SharedRom, public Editor {
 public:
  ScreenEditor() {
    screen_canvas_.SetCanvasSize(ImVec2(512, 512));
    type_ = EditorType::kScreen;
  }

  absl::Status Update() override;

  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  absl::Status SaveDungeonMaps();

 private:
  void DrawTitleScreenEditor();
  void DrawNamingScreenEditor();
  void DrawOverworldMapEditor();

  void DrawInventoryMenuEditor();
  void DrawToolset();
  void DrawInventoryToolset();

  absl::Status LoadDungeonMaps();
  absl::Status LoadDungeonMapTile16();
  void DrawDungeonMapsTabs();
  void DrawDungeonMapsEditor();

  std::vector<zelda3::screen::DungeonMap> dungeon_maps_;
  std::vector<std::vector<std::array<std::string, 25>>> dungeon_map_labels_;

  std::unordered_map<int, gfx::Bitmap> tile16_individual_;

  bool dungeon_maps_loaded_ = false;

  int selected_tile16_ = 0;
  int selected_dungeon = 0;
  uint8_t selected_room = 0;
  uint8_t boss_room = 0;
  int floor_number = 1;

  bool copy_button_pressed = false;
  bool paste_button_pressed = false;

  Bytes all_gfx_;
  zelda3::screen::Inventory inventory_;
  gfx::SnesPalette palette_;
  gui::Canvas screen_canvas_;
  gui::Canvas tilesheet_canvas_;
  gui::Canvas tilemap_canvas_;

  gfx::BitmapTable sheets_;

  gfx::Tilesheet tile16_sheet_;

  absl::Status status_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif