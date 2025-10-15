#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <array>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas.h"
#include "app/rom.h"
#include "zelda3/screen/dungeon_map.h"
#include "zelda3/screen/inventory.h"
#include "zelda3/screen/title_screen.h"
#include "zelda3/screen/overworld_map_screen.h"
#include "app/gui/app/editor_layout.h"
#include "imgui/imgui.h"

namespace yaze {
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
 */
class ScreenEditor : public Editor {
 public:
  explicit ScreenEditor(Rom* rom = nullptr) : rom_(rom) {
    screen_canvas_.SetCanvasSize(ImVec2(512, 512));
    type_ = EditorType::kScreen;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override { return absl::UnimplementedError("Save"); }
  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

  std::vector<zelda3::DungeonMap> dungeon_maps_;

 private:
  void DrawTitleScreenEditor();
  void DrawNamingScreenEditor();
  void DrawOverworldMapEditor();

  void DrawInventoryMenuEditor();
  void DrawInventoryItemIcons();
  void DrawToolset();
  void DrawDungeonMapToolset();
  void DrawInventoryToolset();

  // Title screen layer editing
  void DrawTitleScreenCompositeCanvas();
  void DrawTitleScreenBG1Canvas();
  void DrawTitleScreenBG2Canvas();
  void DrawTitleScreenBlocksetSelector();

  absl::Status LoadDungeonMapTile16(const std::vector<uint8_t>& gfx_data,
                                    bool bin_mode = false);
  absl::Status SaveDungeonMapTile16();

  void DrawDungeonMapScreen(int i);
  void DrawDungeonMapsTabs();
  void DrawDungeonMapsEditor();
  void DrawDungeonMapsRoomGfx();

  void LoadBinaryGfx();

  enum class EditingMode { DRAW, EDIT };

  EditingMode current_mode_ = EditingMode::DRAW;

  bool binary_gfx_loaded_ = false;

  uint8_t selected_room = 0;

  int selected_tile16_ = 0;
  int selected_tile8_ = 0;
  int selected_dungeon = 0;
  int floor_number = 1;

  bool copy_button_pressed = false;
  bool paste_button_pressed = false;

  zelda3::DungeonMapLabels dungeon_map_labels_;

  gfx::SnesPalette palette_;
  gfx::BitmapTable sheets_;
  gfx::Tilemap tile16_blockset_;
  gfx::Tilemap tile8_tilemap_;  // Tilemap for 8x8 tiles with on-demand caching
  std::array<gfx::TileInfo, 4> current_tile16_info;

  gui::Canvas current_tile_canvas_{"##CurrentTileCanvas", ImVec2(32, 32),
                                   gui::CanvasGridSize::k16x16, 2.0f};
  gui::Canvas screen_canvas_;
  gui::Canvas tilesheet_canvas_;
  gui::Canvas tilemap_canvas_{"##TilemapCanvas", ImVec2(128 + 2, (192) + 4),
                              gui::CanvasGridSize::k8x8, 2.f};

  // Title screen canvases
  // Title screen is 32x32 tiles at 8x8 pixels = 256x256 pixels total
  gui::Canvas title_bg1_canvas_{"##TitleBG1Canvas", ImVec2(256, 256),
                                gui::CanvasGridSize::k8x8, 2.0f};
  gui::Canvas title_bg2_canvas_{"##TitleBG2Canvas", ImVec2(256, 256),
                                gui::CanvasGridSize::k8x8, 2.0f};
  // Blockset is 128 pixels wide x 512 pixels tall (16x64 8x8 tiles)
  gui::Canvas title_blockset_canvas_{"##TitleBlocksetCanvas",
                                     ImVec2(128, 512),
                                     gui::CanvasGridSize::k8x8, 2.0f};

  zelda3::Inventory inventory_;
  zelda3::TitleScreen title_screen_;
  zelda3::OverworldMapScreen ow_map_screen_;

  // Title screen state
  int selected_title_tile16_ = 0;
  bool title_screen_loaded_ = false;
  bool title_h_flip_ = false;
  bool title_v_flip_ = false;
  int title_palette_ = 0;
  bool show_title_bg1_ = true;
  bool show_title_bg2_ = true;

  // Overworld map screen state
  int selected_ow_tile_ = 0;
  bool ow_map_loaded_ = false;
  bool ow_show_dark_world_ = false;

  // Overworld map canvases
  gui::Canvas ow_map_canvas_{"##OWMapCanvas", ImVec2(512, 512),
                             gui::CanvasGridSize::k8x8, 1.0f};
  gui::Canvas ow_tileset_canvas_{"##OWTilesetCanvas", ImVec2(128, 128),
                                 gui::CanvasGridSize::k8x8, 2.0f};

  Rom* rom_;
  absl::Status status_;
};

}  // namespace editor
}  // namespace yaze

#endif
