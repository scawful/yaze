#ifndef YAZE_APP_EDITOR_GRAPHICS_EDITOR_H
#define YAZE_APP_EDITOR_GRAPHICS_EDITOR_H

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/pipeline.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/viewer/cgx_viewer.h"

namespace yaze {
namespace app {
namespace editor {
  // "99973","A3D80",

  // 

const std::string kSuperDonkeyTiles[] = {
    "97C05", "98219", "9871E", "98C00", "99084", "995AF", 
    "99DE0", "9A27E", "9A741", "9AC31", "9B07E", "9B55C", "9B963",
    "9BB99", "9C009", "9C4B4", "9C92B", "9CDD6", "9D2C2", "9E037",
    "9E527", "9EA56", "9EF65", "9FCD1", "A0193", "A059E", "A0B17",
    "A0FB6", "A14A5", "A1988", "A1E66", "A232B", "A27F0", "A2B6E",
    "A302C", "A3453", "A38CA",  "A42BB", "A470C", "A4BA9",
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

constexpr char* kPaletteGroupAddressesKeys[] = {
    "ow_main",        "ow_aux",       "ow_animated",  "hud",
    "global_sprites", "armors",       "swords",       "shields",
    "sprites_aux1",   "sprites_aux2", "sprites_aux3", "dungeon_main",
    "grass",          "3d_object",    "ow_mini_map",
};

static constexpr absl::string_view kGfxToolsetColumnNames[] = {
    "#memoryEditor",
    "##separator_gfx1",
};

constexpr ImGuiTableFlags kGfxEditFlags = ImGuiTableFlags_Reorderable |
                                          ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_SizingStretchSame;

class GraphicsEditor {
 public:
  absl::Status Update();
  void SetupROM(ROM& rom) { rom_ = rom; }

 private:
  absl::Status DrawToolset();
  absl::Status DrawCgxImport();
  absl::Status DrawFileImport();
  absl::Status DrawPaletteControls();
  absl::Status DrawClipboardImport();
  absl::Status DrawExperimentalFeatures();
  absl::Status DrawMemoryEditor();
  absl::Status DrawGraphicsBin();

  absl::Status DecompressImportData(int size);

  absl::Status DecompressSuperDonkey();

  int current_offset_ = 0;
  int current_size_ = 0;
  int current_palette_ = 0;
  int current_palette_index_ = 0;
  
  int num_sheets_to_load_ = 1;

  int clipboard_offset_ = 0;
  int clipboard_size_ = 0;

  bool open_memory_editor_ = false;
  bool gfx_loaded_ = false;
  bool is_open_ = false;
  bool super_donkey_ = false;
  bool col_file_ = false;
  bool cgx_loaded_ = false;
  bool scr_loaded_ = false;

  char file_path_[256] = "";
  char col_file_path_[256] = "";
  char col_file_name_[256] = "";

  char cgx_file_path_[256] = "";
  char cgx_file_name_[256] = "";

  ROM rom_;
  ROM temp_rom_;
  Bytes import_data_;
  Bytes graphics_buffer_;

  MemoryEditor cgx_memory_editor_;
  MemoryEditor col_memory_editor_;

  PaletteEditor palette_editor_;

  gfx::Bitmap cgx_bitmap_;

  gfx::Bitmap bitmap_;
  gui::Canvas import_canvas_;

  gui::Canvas super_donkey_canvas_;
  gfx::BitmapTable graphics_bin_;

  gfx::SNESPalette palette_;
  gfx::SNESPalette col_file_palette_;

  viewer::CgxViewer cgx_viewer_;

  absl::Status status_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_EDITOR_H