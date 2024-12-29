#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/tile16_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/zeml.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

constexpr uint k4BPP = 4;
constexpr uint kByteSize = 3;
constexpr uint kMessageIdSize = 5;
constexpr uint kNumSheetsToLoad = 223;
constexpr uint kTile8DisplayHeight = 64;
constexpr uint kOverworldMapSize = 0x200;
constexpr float kInputFieldSize = 30.f;
constexpr ImVec2 kOverworldCanvasSize(kOverworldMapSize * 8,
                                      kOverworldMapSize * 8);
constexpr ImVec2 kCurrentGfxCanvasSize(0x100 + 1, 0x10 * 0x40 + 1);
constexpr ImVec2 kBlocksetCanvasSize(0x100 + 1, 0x4000 + 1);
constexpr ImVec2 kGraphicsBinCanvasSize(0x100 + 1, kNumSheetsToLoad * 0x40 + 1);

constexpr ImGuiTableFlags kOWMapFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
constexpr ImGuiTableFlags kToolsetTableFlags = ImGuiTableFlags_SizingFixedFit;
constexpr ImGuiTableFlags kOWEditFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

static constexpr absl::string_view kToolsetColumnNames[] = {
    "#undoTool",      "#redoTool",   "#separator2",       "#zoomOutTool",
    "#zoomInTool",    "#separator",  "#drawTool",         "#history",
    "#entranceTool",  "#exitTool",   "#itemTool",         "#spriteTool",
    "#transportTool", "#musicTool",  "#separator3",       "#tilemapTool",
    "propertiesTool", "#separator4", "#experimentalTool", "#properties",
    "#separator5"};

constexpr absl::string_view kWorldList =
    "Light World\0Dark World\0Extra World\0";

constexpr absl::string_view kGamePartComboString = "Part 0\0Part 1\0Part 2\0";

constexpr absl::string_view kTileSelectorTab = "##TileSelectorTabBar";
constexpr absl::string_view kOWEditTable = "##OWEditTable";
constexpr absl::string_view kOWMapTable = "#MapSettingsTable";

constexpr int kEntranceTileTypePtrLow = 0xDB8BF;
constexpr int kEntranceTileTypePtrHigh = 0xDB917;
constexpr int kNumEntranceTileTypes = 0x2C;

class EntranceContext {
 public:
  absl::Status LoadEntranceTileTypes(Rom& rom) {
    int offset_low = kEntranceTileTypePtrLow;
    int offset_high = kEntranceTileTypePtrHigh;

    for (int i = 0; i < kNumEntranceTileTypes; i++) {
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
                        public EntranceContext,
                        public GfxContext,
                        public core::ExperimentFlags {
 public:
  OverworldEditor() { type_ = EditorType::kOverworld; }

  void InitializeZeml();

  absl::Status Update() final;
  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override {
    return absl::UnimplementedError("Find Unused Tiles");
  }
  absl::Status Save();

  auto overworld() { return &overworld_; }

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
  /**
   * @brief Draws the canvas, tile16 selector, and toolset in fullscreen
   */
  void DrawFullscreenCanvas();

  /**
   * @brief Toolset for entrances, exits, items, sprites, and transports.
   */
  void DrawToolset();

  /**
   * @brief Draws the overworld map settings. Graphics, palettes, etc.
   */
  void DrawOverworldMapSettings();

  /**
   * @brief Draw the overworld settings for ZSCustomOverworld.
   */
  void DrawCustomOverworldMapSettings();

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
                              const std::vector<uint8_t>& tile_data);

  /**
   * @brief Check for changes to the overworld map.
   *
   * This function either draws the tile painter with the current tile16 or
   * group of tile16 data with ow_map_canvas_ and DrawOverworldEdits or it
   * checks for left mouse button click/drag to select a tile16 or group of
   * tile16 data from the overworld map canvas. Similar to ZScream selection.
   */
  void CheckForOverworldEdits();

  /**
   * @brief Draw and create the tile16 IDs that are currently selected.
   */
  void CheckForSelectRectangle();

  /**
   * @brief Check for changes to the overworld map. Calls RefreshOverworldMap
   * and RefreshTile16Blockset on the current map if it is modified and is
   * actively being edited.
   */
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

  absl::Status UpdateUsageStats();
  void DrawUsageGrid();
  void DrawDebugWindow();

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

  enum OverworldProperty {
    LW_AREA_GFX,
    DW_AREA_GFX,
    LW_AREA_PAL,
    DW_AREA_PAL,
    LW_SPR_GFX_PART1,
    LW_SPR_GFX_PART2,
    DW_SPR_GFX_PART1,
    DW_SPR_GFX_PART2,
    LW_SPR_PAL_PART1,
    LW_SPR_PAL_PART2,
    DW_SPR_PAL_PART1,
    DW_SPR_PAL_PART2,
  };

  int current_world_ = 0;
  int current_map_ = 0;
  int current_parent_ = 0;
  int current_entrance_id_ = 0;
  int current_exit_id_ = 0;
  int current_item_id_ = 0;
  int current_sprite_id_ = 0;
  int current_blockset_ = 0;
  int game_state_ = 1;
  int current_tile16_ = 0;
  int selected_entrance_ = 0;
  int selected_usage_map_ = 0xFFFF;

  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;
  bool selected_tile_loaded_ = false;
  bool show_tile16_editor_ = false;
  bool overworld_canvas_fullscreen_ = false;
  bool middle_mouse_dragging_ = false;
  bool is_dragging_entity_ = false;

  std::vector<uint8_t> selected_tile_data_;
  std::vector<std::vector<uint8_t>> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  std::vector<std::vector<uint8_t>> tile8_individual_data_;
  std::vector<gfx::Bitmap> tile8_individual_;

  Tile16Editor tile16_editor_;
  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;

  gfx::SnesPalette palette_;

  gfx::Bitmap selected_tile_bmp_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;

  gfx::BitmapTable maps_bmp_;
  gfx::BitmapTable current_graphics_set_;
  gfx::BitmapTable sprite_previews_;

  zelda3::overworld::Overworld overworld_;
  zelda3::OWBlockset refresh_blockset_;

  zelda3::Sprite current_sprite_;

  zelda3::overworld::OverworldEntrance current_entrance_;
  zelda3::overworld::OverworldExit current_exit_;
  zelda3::overworld::OverworldItem current_item_;

  zelda3::GameEntity* current_entity_;
  zelda3::GameEntity* dragged_entity_;

  gui::Canvas ow_map_canvas_{"OwMap", kOverworldCanvasSize,
                             gui::CanvasGridSize::k64x64};
  gui::Canvas current_gfx_canvas_{"CurrentGfx", kCurrentGfxCanvasSize,
                                  gui::CanvasGridSize::k32x32};
  gui::Canvas blockset_canvas_{"OwBlockset", kBlocksetCanvasSize,
                               gui::CanvasGridSize::k32x32};
  gui::Canvas graphics_bin_canvas_{"GraphicsBin", kGraphicsBinCanvasSize,
                                   gui::CanvasGridSize::k16x16};
  gui::Canvas properties_canvas_;

  gui::Table toolset_table_{"##ToolsetTable0", 22, kToolsetTableFlags};

  gui::zeml::Node layout_node_;
  absl::Status status_;
};
}  // namespace editor
}  // namespace yaze

#endif
