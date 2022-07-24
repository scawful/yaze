#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/overworld.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr unsigned int k4BPP = 4;
static constexpr unsigned int kByteSize = 3;
static constexpr unsigned int kMessageIdSize = 5;
static constexpr unsigned int kNumSheetsToLoad = 223;
static constexpr unsigned int kTile8DisplayHeight = 64;
static constexpr float kInputFieldSize = 30.f;

class OverworldEditor {
 public:
  void SetupROM(ROM &rom);
  void Update();

 private:
  void DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();
  void DrawTile16Selector();
  void DrawTile8Selector();
  void DrawAreaGraphics();

  void LoadGraphics();

  int current_world_ = 0;
  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";
  char staticgfx[16];
  bool isLoaded = false;
  bool doneLoaded = false;
  bool opt_enable_grid = true;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;

  ImVec4 current_palette_[8];

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;

  std::unordered_map<unsigned int, gfx::Bitmap> graphics_bin_;
  absl::flat_hash_map<int, gfx::Bitmap> graphics_bin_v2_;

  ROM rom_;
  zelda3::Overworld overworld_;

  gfx::SNESPalette palette_;
  gfx::Bitmap tile16_blockset_bmp_;  // pointer size 1048576
  gfx::Bitmap current_gfx_bmp_;      // pointer size 32768
  gfx::Bitmap all_gfx_bmp;           // pointer size 456704

  gui::Canvas overworld_map_canvas_;
  gui::Canvas current_gfx_canvas_;
  gui::Canvas blockset_canvas_;
  gui::Canvas graphics_bin_canvas_;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif