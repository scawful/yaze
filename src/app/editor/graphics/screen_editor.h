#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

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
#include "imgui/imgui.h"

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
  absl::Status LoadDungeonMapTile16(const std::vector<uint8_t>& gfx_data,
                                    bool bin_mode = false);
  absl::Status SaveDungeonMapTile16();
  void DrawDungeonMapsTabs();
  void DrawDungeonMapsEditor();

  void LoadBinaryGfx();

  bool dungeon_maps_loaded_ = false;
  bool binary_gfx_loaded_ = false;

  uint8_t selected_room = 0;
  uint8_t boss_room = 0;

  int selected_tile16_ = 0;
  int selected_dungeon = 0;
  int floor_number = 1;

  bool copy_button_pressed = false;
  bool paste_button_pressed = false;

  std::vector<uint8_t> all_gfx_;
  std::unordered_map<int, gfx::Bitmap> tile16_individual_;
  std::vector<zelda3::screen::DungeonMap> dungeon_maps_;
  std::vector<std::vector<std::array<std::string, 25>>> dungeon_map_labels_;
  std::array<uint16_t, 4> current_tile16_data_;

  absl::Status status_;

  gfx::SnesPalette palette_;
  gfx::BitmapTable sheets_;
  gfx::Tilesheet tile16_sheet_;
  gfx::InternalTile16 current_tile16_info;

  gui::Canvas current_tile_canvas_{"##CurrentTileCanvas"};
  gui::Canvas screen_canvas_;
  gui::Canvas tilesheet_canvas_;
  gui::Canvas tilemap_canvas_;

  zelda3::screen::Inventory inventory_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
