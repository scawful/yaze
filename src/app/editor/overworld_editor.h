#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include "imgui/imgui.h"

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/core/common.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/tile16_editor.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/utils/editor.h"
#include "app/editor/utils/gfx_context.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/zeml.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

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
    "#undoTool",      "#redoTool",  "#separator2", "#zoomOutTool",
    "#zoomInTool",    "#separator", "#drawTool",   "#history",
    "#entranceTool",  "#exitTool",  "#itemTool",   "#spriteTool",
    "#transportTool", "#musicTool", "#separator3", "#tilemapTool",
    "propertiesTool"};

constexpr ImGuiTableFlags kOWMapFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
constexpr ImGuiTableFlags kToolsetTableFlags = ImGuiTableFlags_SizingFixedFit;
constexpr ImGuiTableFlags kOWEditFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

constexpr absl::string_view kWorldList =
    "Light World\0Dark World\0Extra World\0";

constexpr absl::string_view kGamePartComboString = "Part 0\0Part 1\0Part 2\0";

constexpr absl::string_view kTileSelectorTab = "##TileSelectorTabBar";
constexpr absl::string_view kOWEditTable = "##OWEditTable";
constexpr absl::string_view kOWMapTable = "#MapSettingsTable";

class EntranceContext {
 public:
  absl::Status LoadEntranceTileTypes(Rom& rom) {
    int offset_low = 0xDB8BF;
    int offset_high = 0xDB917;

    for (int i = 0; i < 0x2C; i++) {
      // Load entrance tile types
      ASSIGN_OR_RETURN(auto value_low, rom.ReadWord(offset_low + i));
      entrance_tile_types_low_.push_back(value_low);
      ASSIGN_OR_RETURN(auto value_high, rom.ReadWord(offset_high + i));
      entrance_tile_types_low_.push_back(value_high);
    }

    return absl::OkStatus();
  }

 private:
  std::vector<uint16_t> entrance_tile_types_low_;
  std::vector<uint16_t> entrance_tile_types_high_;
};

/**
 * @class OverworldEditor
 * @brief Manipulates the Overworld and OverworldMap data in a Rom.
 *
 * The `OverworldEditor` class is responsible for managing the editing and
 * manipulation of the overworld in a game. The user can drag and drop tiles,
 * modify OverworldEntrance, OverworldExit, Sprite, and OverworldItem
 * as well as change the gfx and palettes used in each overworld map.
 *
 * The Overworld itself is a series of bitmap images which exist inside each
 * OverworldMap object. The drawing of the overworld is done using the Canvas
 * class in conjunction with these underlying Bitmap objects.
 *
 * Provides access to the GfxGroupEditor and Tile16Editor through popup windows.
 *
 */
class OverworldEditor : public Editor,
                        public SharedRom,
                        public context::GfxContext,
                        public EntranceContext,
                        public core::ExperimentFlags {
 public:
  OverworldEditor() { type_ = EditorType::kOverworld; }

  void InitializeZeml();

  absl::Status Update() final;
  absl::Status Undo() { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() { return absl::UnimplementedError("Paste"); }
  absl::Status Find() { return absl::UnimplementedError("Find Unused Tiles"); }

  auto overworld() { return &overworld_; }

  /**
   * @brief
   */
  int jump_to_tab() { return jump_to_tab_; }
  int jump_to_tab_ = -1;

  /**
   * @brief Load the Bitmap objects for each OverworldMap.
   *
   * Calls the Overworld class to load the image data and palettes from the Rom,
   * then renders the area graphics and tile16 blockset Bitmap objects before
   * assembling the OverworldMap Bitmap objects.
   */
  absl::Status LoadGraphics();

 private:
  absl::Status UpdateFullscreenCanvas();

  absl::Status DrawToolset();
  void DrawOverworldMapSettings();

  void RefreshChildMap(int i);
  void RefreshOverworldMap();
  absl::Status RefreshMapPalette();
  void RefreshMapProperties();
  absl::Status RefreshTile16Blockset();

  void DrawOverworldEntrances(ImVec2 canvas_p, ImVec2 scrolling,
                              bool holes = false);
  void DrawOverworldExits(ImVec2 zero, ImVec2 scrolling);
  void DrawOverworldItems();
  void DrawOverworldSprites();

  void DrawOverworldMaps();
  void DrawOverworldEdits();
  void RenderUpdatedMapBitmap(const ImVec2& click_position,
                              const Bytes& tile_data);
  void CheckForOverworldEdits();
  void CheckForSelectRectangle();
  absl::Status CheckForCurrentMap();
  void CheckForMousePan();

  /**
   * @brief Allows the user to make changes to the overworld map.
   */
  void DrawOverworldCanvas();

  absl::Status DrawTile16Selector();
  void DrawTile8Selector();
  absl::Status DrawAreaGraphics();
  absl::Status DrawTileSelector();

  absl::Status LoadSpriteGraphics();

  void DrawOverworldProperties();

  absl::Status DrawExperimentalModal();

  absl::Status UpdateUsageStats();
  void DrawUsageGrid();
  void CalculateUsageStats();

  absl::Status LoadAnimatedMaps();
  void DrawDebugWindow();

  auto gfx_group_editor() const { return gfx_group_editor_; }

  enum class EditingMode {
    DRAW_TILE,
    ENTRANCES,
    EXITS,
    ITEMS,
    SPRITES,
    TRANSPORTS,
    MUSIC,
    PAN
  };

  EditingMode current_mode = EditingMode::DRAW_TILE;
  EditingMode previous_mode = EditingMode::DRAW_TILE;

  int current_world_ = 0;
  int current_map_ = 0;
  int current_parent_ = 0;
  int game_state_ = 1;
  int current_tile16_ = 0;
  int selected_tile_ = 0;

  int current_blockset_ = 0;

  int selected_entrance_ = 0;
  int selected_usage_map_ = 0xFFFF;

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
  bool overworld_canvas_fullscreen_ = false;
  bool middle_mouse_dragging_ = false;

  bool is_dragging_entity_ = false;
  zelda3::OverworldEntity* dragged_entity_;
  zelda3::OverworldEntity* current_entity_;

  int current_entrance_id_ = 0;
  zelda3::overworld::OverworldEntrance current_entrance_;
  int current_exit_id_ = 0;
  zelda3::overworld::OverworldExit current_exit_;
  int current_item_id_ = 0;
  zelda3::overworld::OverworldItem current_item_;
  int current_sprite_id_ = 0;
  zelda3::Sprite current_sprite_;

  bool show_experimental = false;
  std::string ow_tilemap_filename_ = "";
  std::string tile32_configuration_filename_ = "";

  Bytes selected_tile_data_;
  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  std::vector<Bytes> tile8_individual_data_;
  std::vector<gfx::Bitmap> tile8_individual_;

  Tile16Editor tile16_editor_;
  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;
  zelda3::overworld::Overworld overworld_;

  gui::Canvas ow_map_canvas_{"owMapCanvas", ImVec2(0x200 * 8, 0x200 * 8),
                             gui::CanvasGridSize::k64x64};
  gui::Canvas current_gfx_canvas_{"customGfxCanvas",
                                  ImVec2(0x100 + 1, 0x10 * 0x40 + 1),
                                  gui::CanvasGridSize::k32x32};
  gui::Canvas blockset_canvas_{"blocksetCanvas", ImVec2(0x100 + 1, 0x2000 + 1),
                               gui::CanvasGridSize::k32x32};
  gui::Canvas graphics_bin_canvas_{
      "graphicsBinCanvas", ImVec2(0x100 + 1, kNumSheetsToLoad * 0x40 + 1),
      gui::CanvasGridSize::k16x16};
  gui::Canvas properties_canvas_;

  gfx::SnesPalette palette_;
  gfx::Bitmap selected_tile_bmp_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;

  gfx::BitmapTable maps_bmp_;
  gfx::BitmapTable current_graphics_set_;
  gfx::BitmapTable sprite_previews_;
  gfx::BitmapTable animated_maps_;

  OWBlockset refresh_blockset_;

  gui::zeml::Node layout_node_;
  absl::Status status_;
};
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif