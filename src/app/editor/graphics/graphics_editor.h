#ifndef YAZE_APP_EDITOR_GRAPHICS_EDITOR_H
#define YAZE_APP_EDITOR_GRAPHICS_EDITOR_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/link_sprite_panel.h"
#include "app/editor/graphics/palette_controls_panel.h"
#include "app/editor/graphics/paletteset_editor_panel.h"
#include "app/editor/graphics/pixel_editor_panel.h"
#include "app/editor/graphics/polyhedral_editor_panel.h"
#include "app/editor/graphics/sheet_browser_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

// Super Donkey prototype graphics offsets (from leaked dev materials)
const std::string kSuperDonkeyTiles[] = {
    "97C05", "98219", "9871E", "98C00", "99084", "995AF", "99DE0", "9A27E",
    "9A741", "9AC31", "9B07E", "9B55C", "9B963", "9BB99", "9C009", "9C4B4",
    "9C92B", "9CDD6", "9D2C2", "9E037", "9E527", "9EA56", "9EF65", "9FCD1",
    "A0193", "A059E", "A0B17", "A0FB6", "A14A5", "A1988", "A1E66", "A232B",
    "A27F0", "A2B6E", "A302C", "A3453", "A38CA", "A42BB", "A470C", "A4BA9",
    "A5089", "A5385", "A5742", "A5BCC", "A6017", "A6361", "A66F8"};

const std::string kSuperDonkeySprites[] = {
    "A8E5D", "A9435", "A9934", "A9D83", "AA2F1", "AA6D4", "AABE4", "AB127",
    "AB65A", "ABBDD", "AC38D", "AC797", "ACCC8", "AD0AE", "AD245", "AD554",
    "ADAAC", "ADECC", "AE453", "AE9D2", "AEF40", "AF3C9", "AF92E", "AFE9D",
    "B03D2", "B09AC", "B0F0C", "B1430", "B1859", "B1E01", "B229A", "B2854",
    "B2D27", "B31D7", "B3B58", "B40B5", "B45A5", "B4D64", "B5031", "B555F",
    "B5F30", "B6858", "B70DD", "B7526", "B79EC", "B7C83", "B80F7", "B85CC",
    "B8A3F", "B8F97", "B94F2", "B9A20", "B9E9A", "BA3A2", "BA8F6", "BACDC",
    "BB1F9", "BB781", "BBCCA", "BC26D", "BC7D4", "BCBB0", "BD082", "BD5FC",
    "BE115", "BE5C2", "BEB63", "BF0CB", "BF607", "BFA55", "BFD71", "C017D",
    "C0567", "C0981", "C0BA7", "C116D", "C166A", "C1FE0", "C24CE", "C2B19"};

/**
 * @class GraphicsEditor
 * @brief Allows the user to edit graphics sheets from the game or view
 * prototype graphics.
 *
 * The GraphicsEditor class is responsible for providing functionality to edit
 * graphics sheets from the game or view prototype graphics of Link to the Past
 * from the CGX, SCR, and OBJ formats. It provides various methods to update
 * different components of the graphics editor, such as the graphics edit tab,
 * link graphics view, and prototype graphics viewer. It also includes import
 * functions for different file formats, as well as other utility functions for
 * drawing toolsets, palette controls, clipboard imports, experimental features,
 * and memory editor.
 */
class GraphicsEditor : public Editor {
 public:
  explicit GraphicsEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kGraphics;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Save() override;
  absl::Status Update() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  // Set the ROM pointer
  void set_rom(Rom* rom) { rom_ = rom; }
  
  // Set the game data pointer
  void SetGameData(zelda3::GameData* game_data) override {
    game_data_ = game_data;
    if (palette_controls_panel_) {
      palette_controls_panel_->SetGameData(game_data);
    }
  }

  // Editor shortcuts
  void NextSheet();
  void PrevSheet();

  // Get the ROM pointer
  Rom* rom() const { return rom_; }

 private:
  // Editor-level shortcut handling
  void HandleEditorShortcuts();

  // --- Panel-Based Architecture ---
  GraphicsEditorState state_;
  std::unique_ptr<SheetBrowserPanel> sheet_browser_panel_;
  std::unique_ptr<PixelEditorPanel> pixel_editor_panel_;
  std::unique_ptr<PaletteControlsPanel> palette_controls_panel_;
  std::unique_ptr<LinkSpritePanel> link_sprite_panel_;
  std::unique_ptr<PolyhedralEditorPanel> polyhedral_panel_;
  std::unique_ptr<GfxGroupEditor> gfx_group_panel_;
  std::unique_ptr<PalettesetEditorPanel> paletteset_panel_;

  // --- Prototype Viewer (Super Donkey / Dev Format Imports) ---
  void DrawPrototypeViewer();
  absl::Status DrawCgxImport();
  absl::Status DrawScrImport();
  absl::Status DrawFileImport();
  absl::Status DrawObjImport();
  absl::Status DrawTilemapImport();
  absl::Status DrawPaletteControls();
  absl::Status DrawClipboardImport();
  absl::Status DrawExperimentalFeatures();
  absl::Status DrawMemoryEditor();
  absl::Status DecompressImportData(int size);
  absl::Status DecompressSuperDonkey();

  // Prototype viewer state
  int current_palette_ = 0;
  uint64_t current_offset_ = 0;
  uint64_t current_palette_index_ = 0;
  int current_bpp_ = 0;
  int scr_mod_value_ = 0;
  uint64_t num_sheets_to_load_ = 1;
  uint64_t bin_size_ = 0;
  uint64_t clipboard_offset_ = 0;
  uint64_t clipboard_size_ = 0;
  bool refresh_graphics_ = false;
  bool open_memory_editor_ = false;
  bool gfx_loaded_ = false;
  bool is_open_ = false;
  bool super_donkey_ = false;
  bool col_file_ = false;
  bool cgx_loaded_ = false;
  bool scr_loaded_ = false;
  bool obj_loaded_ = false;
  bool tilemap_loaded_ = false;

  std::string file_path_;
  std::string col_file_path_;
  std::string col_file_name_;
  std::string cgx_file_path_;
  std::string cgx_file_name_;
  std::string scr_file_path_;
  std::string scr_file_name_;
  std::string obj_file_path_;
  std::string tilemap_file_path_;
  std::string tilemap_file_name_;

  Rom temp_rom_;
  Rom tilemap_rom_;
  std::vector<uint8_t> import_data_;
  std::vector<uint8_t> decoded_cgx_;
  std::vector<uint8_t> cgx_data_;
  std::vector<uint8_t> extra_cgx_data_;
  std::vector<SDL_Color> decoded_col_;
  std::vector<uint8_t> scr_data_;
  std::vector<uint8_t> decoded_scr_data_;
  gfx::Bitmap cgx_bitmap_;
  gfx::Bitmap scr_bitmap_;
  gfx::Bitmap bin_bitmap_;
  std::array<gfx::Bitmap, zelda3::kNumGfxSheets> gfx_sheets_;
  gfx::PaletteGroup col_file_palette_group_;
  gfx::SnesPalette z3_rom_palette_;
  gfx::SnesPalette col_file_palette_;
  gui::Canvas import_canvas_;
  gui::Canvas scr_canvas_;
  gui::Canvas super_donkey_canvas_;

  // Status tracking
  absl::Status status_;

  // Core references
  Rom* rom_;
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_EDITOR_H
