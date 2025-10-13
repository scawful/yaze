#ifndef YAZE_APP_EDITOR_SCREEN_EDITOR_H
#define YAZE_APP_EDITOR_SCREEN_EDITOR_H

#include <array>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/editor_card_manager.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "zelda3/screen/dungeon_map.h"
#include "zelda3/screen/inventory.h"
#include "app/gui/editor_layout.h"
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
  void DrawToolset();
  void DrawDungeonMapToolset();
  void DrawInventoryToolset();

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

  std::vector<gfx::Bitmap> tile8_individual_;
  zelda3::DungeonMapLabels dungeon_map_labels_;

  gfx::SnesPalette palette_;
  gfx::BitmapTable sheets_;
  gfx::Tilemap tile16_blockset_;
  std::array<gfx::TileInfo, 4> current_tile16_info;

  gui::Canvas current_tile_canvas_{"##CurrentTileCanvas", ImVec2(32, 32),
                                   gui::CanvasGridSize::k16x16, 2.0f};
  gui::Canvas screen_canvas_;
  gui::Canvas tilesheet_canvas_;
  gui::Canvas tilemap_canvas_{"##TilemapCanvas", ImVec2(128 + 2, (192) + 4),
                              gui::CanvasGridSize::k8x8, 2.f};

  zelda3::Inventory inventory_;
  Rom* rom_;
  absl::Status status_;
};

}  // namespace editor
}  // namespace yaze

#endif
