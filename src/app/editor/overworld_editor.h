#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/core/common.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/modules/gfx_group_editor.h"
#include "app/editor/modules/tile16_editor.h"
#include "app/editor/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

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
    "#undoTool",      "#redoTool",   "#drawTool",   "#separator2",
    "#zoomOutTool",   "#zoomInTool", "#separator",  "#history",
    "#entranceTool",  "#exitTool",   "#itemTool",   "#spriteTool",
    "#transportTool", "#musicTool",  "#separator3", "#tilemapTool"};

static constexpr absl::string_view kOverworldSettingsColumnNames[] = {
    "##1stCol",    "##gfxCol",   "##palCol", "##sprgfxCol",
    "##sprpalCol", "##msgidCol", "##2ndCol"};

constexpr ImGuiTableFlags kOWMapFlags = ImGuiTableFlags_Borders;
constexpr ImGuiTableFlags kToolsetTableFlags = ImGuiTableFlags_SizingFixedFit;
constexpr ImGuiTableFlags kOWEditFlags = ImGuiTableFlags_Reorderable |
                                         ImGuiTableFlags_Resizable |
                                         ImGuiTableFlags_SizingStretchSame;

constexpr absl::string_view kWorldList = "Light World\0Dark World\0Extra World";

constexpr absl::string_view kTileSelectorTab = "##TileSelectorTabBar";
constexpr absl::string_view kOWEditTable = "##OWEditTable";
constexpr absl::string_view kOWMapTable = "#MapSettingsTable";

class OverworldEditor : public Editor,
                        public SharedROM,
                        public core::ExperimentFlags {
 public:
  absl::Status Update() final;
  absl::Status Undo() { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() { return absl::UnimplementedError("Paste"); }

  void Shutdown() {
    for (auto &bmp : tile16_individual_) {
      bmp.Cleanup();
    }
    for (auto &[i, bmp] : maps_bmp_) {
      bmp.Cleanup();
    }
    for (auto &[i, bmp] : graphics_bin_) {
      bmp.Cleanup();
    }
    for (auto &[i, bmp] : current_graphics_set_) {
      bmp.Cleanup();
    }
  }

 private:
  absl::Status DrawToolset();
  void DrawOverworldMapSettings();

  void DrawOverworldEntrances(ImVec2 canvas_p, ImVec2 scrolling);
  void DrawOverworldMaps();
  void DrawOverworldSprites();

  void DrawOverworldEdits();
  void RenderUpdatedMapBitmap(const ImVec2 &click_position,
                              const Bytes &tile_data);
  void QueueROMChanges(int index, ushort new_tile16);
  void DetermineActiveMap(const ImVec2 &mouse_position);

  void CheckForOverworldEdits();
  void DrawOverworldCanvas();

  void DrawTile8Selector();
  void DrawTileSelector();
  absl::Status LoadGraphics();
  absl::Status LoadSpriteGraphics();

  absl::Status DrawExperimentalModal();

  enum class EditingMode {
    DRAW_TILE,
    ENTRANCES,
    EXITS,
    ITEMS,
    SPRITES,
    TRANSPORTS,
    MUSIC
  };

  EditingMode current_mode = EditingMode::DRAW_TILE;

  int current_world_ = 0;
  int current_map_ = 0;
  int current_tile16_ = 0;
  int selected_tile_ = 0;
  int game_state_ = 0;
  char map_gfx_[3] = "";
  char map_palette_[3] = "";
  char spr_gfx_[3] = "";
  char spr_palette_[3] = "";
  char message_id_[5] = "";
  char staticgfx[16];

  uint32_t tilemap_file_offset_high_ = 0;
  uint32_t tilemap_file_offset_low_ = 0;
  uint32_t light_maps_to_load_ = 0x51;
  uint32_t dark_maps_to_load_ = 0x2A;
  uint32_t sp_maps_to_load_ = 0x07;

  bool opt_enable_grid = true;
  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;
  bool selected_tile_loaded_ = false;
  bool update_selected_tile_ = true;
  bool is_dragging_entrance_ = false;
  bool show_tile16_editor_ = false;
  bool show_gfx_group_editor_ = false;

  bool IsMouseHoveringOverEntrance(const zelda3::OverworldEntrance &entrance,
                                   ImVec2 canvas_p, ImVec2 scrolling);
  zelda3::OverworldEntrance *dragged_entrance_;

  bool show_experimental = false;
  std::string ow_tilemap_filename_ = "";
  std::string tile32_configuration_filename_ = "";

  Bytes selected_tile_data_;
  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  Tile16Editor tile16_editor_;
  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;
  zelda3::Overworld overworld_;

  gui::Canvas ow_map_canvas_;
  gui::Canvas current_gfx_canvas_;
  gui::Canvas blockset_canvas_;
  gui::Canvas graphics_bin_canvas_;

  gfx::SNESPalette palette_;
  gfx::Bitmap selected_tile_bmp_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;

  gfx::BitmapTable maps_bmp_;
  gfx::BitmapTable graphics_bin_;
  gfx::BitmapTable current_graphics_set_;
  gfx::BitmapTable sprite_previews_;

  ImGuiTableFlags ow_edit_flags = ImGuiTableFlags_Reorderable |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchSame;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif