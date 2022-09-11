#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr uint k4BPP = 4;
static constexpr uint kByteSize = 3;
static constexpr uint kMessageIdSize = 5;
static constexpr uint kNumSheetsToLoad = 223;
static constexpr uint kTile8DisplayHeight = 64;
static constexpr float kInputFieldSize = 30.f;

static constexpr absl::string_view kToolsetColumnNames[] = {
    "#undoTool",      "#redoTool",   "#drawTool",  "#separator2",
    "#zoomOutTool",   "#zoomInTool", "#separator", "#history",
    "#entranceTool",  "#exitTool",   "#itemTool",  "#spriteTool",
    "#transportTool", "#musicTool"};

static constexpr absl::string_view kOverworldSettingsColumnNames[] = {
    "##1stCol",    "##gfxCol",   "##palCol", "##sprgfxCol",
    "##sprpalCol", "##msgidCol", "##2ndCol"};

class OverworldEditor {
 public:
  void SetupROM(ROM &rom);
  absl::Status Update();
  absl::Status Undo() { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() { return absl::UnimplementedError("Paste"); }

 private:
  absl::Status DrawToolset();
  void DrawOverworldMapSettings();
  void DrawOverworldCanvas();
  void DrawTileSelector();
  void DrawTile16Selector();
  void DrawTile8Selector();
  void DrawAreaGraphics();
  void LoadGraphics();

  int current_world_ = 0;
  int current_map_ = 0;
  int selected_tile_ = 0;
  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";
  char staticgfx[16];

  bool opt_enable_grid = true;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;
  bool selected_tile_loaded_ = false;
  bool update_selected_tile_ = true;

  ImVec4 current_palette_[8];

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;

  std::unordered_map<int, gfx::Bitmap> graphics_bin_;
  std::unordered_map<int, gfx::Bitmap> current_graphics_set_;
  std::unordered_map<int, gfx::Bitmap> maps_bmp_;

  ROM rom_;
  zelda3::Overworld overworld_;

  gfx::SNESPalette palette_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;
  gfx::Bitmap selected_tile_bmp_;

  gui::Canvas overworld_map_canvas_;
  gui::Canvas current_gfx_canvas_;
  gui::Canvas blockset_canvas_;
  gui::Canvas graphics_bin_canvas_;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif