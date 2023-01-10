#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/palette_editor.h"
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
  absl::Status Update();
  absl::Status Undo() const { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() const { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() const { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() const { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() const { return absl::UnimplementedError("Paste"); }
  void SetupROM(ROM &rom) { rom_ = rom; }

 private:
  absl::Status DrawToolset();
  void DrawOverworldMapSettings();

  void DrawOverworldEntrances();
  void DrawOverworldMaps();
  void DrawOverworldEdits();
  void DrawOverworldCanvas();

  void DrawTile16Selector();
  void DrawTile8Selector();
  void DrawAreaGraphics();
  void DrawTileSelector();
  absl::Status LoadGraphics();

  int current_world_ = 0;
  int current_map_ = 0;
  int current_tile16_ = 0;
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

  ImGuiTableFlags toolset_table_flags = ImGuiTableFlags_SizingFixedFit;
  ImGuiTableFlags ow_map_flags = ImGuiTableFlags_Borders;
  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;

  Bytes selected_tile_data_;
  std::unordered_map<int, gfx::Bitmap> graphics_bin_;
  std::unordered_map<int, gfx::Bitmap> current_graphics_set_;
  std::unordered_map<int, gfx::Bitmap> maps_bmp_;
  std::unordered_map<int, gfx::Bitmap> sprite_previews_;

  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  ROM rom_;
  PaletteEditor palette_editor_;
  zelda3::Overworld overworld_;

  gfx::SNESPalette palette_;
  gfx::Bitmap selected_tile_bmp_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;

  gui::Canvas ow_map_canvas_;
  gui::Canvas current_gfx_canvas_;
  gui::Canvas blockset_canvas_;
  gui::Canvas graphics_bin_canvas_;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif