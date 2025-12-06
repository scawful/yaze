#ifndef YAZE_APP_EDITOR_OVERWORLDEDITOR_H
#define YAZE_APP_EDITOR_OVERWORLDEDITOR_H

#include <chrono>
#include <optional>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/gfx_group_editor.h"
#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "app/editor/overworld/overworld_sidebar.h"
#include "app/editor/overworld/overworld_toolbar.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/editor/overworld/usage_statistics_card.h"
#include "app/editor/palette/palette_editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld.h"

// =============================================================================
// Overworld Editor - UI Layer
// =============================================================================
//
// ARCHITECTURE OVERVIEW:
// ----------------------
// The OverworldEditor is the main UI class for editing overworld maps.
// It orchestrates several subsystems:
//
//   1. TILE EDITING SYSTEM
//      - Tile16Editor: Popup for editing individual 16x16 tiles
//      - Tile selection and painting on the main canvas
//      - Undo/Redo stack for paint operations
//
//   2. ENTITY SYSTEM
//      - OverworldEntityRenderer: Draws entities on the canvas
//      - entity_operations.cc: Insertion/deletion logic
//      - overworld_entity_interaction.cc: Drag/drop and click handling
//
//   3. MAP PROPERTIES SYSTEM
//      - MapPropertiesSystem: Toolbar and context menus
//      - OverworldSidebar: Property editing tabs
//      - Graphics, palettes, music per area
//
//   4. CANVAS SYSTEM
//      - ow_map_canvas_: Main overworld display (4096x4096)
//      - blockset_canvas_: Tile16 selector
//      - scratch_canvas_: Layout workspace
//
// EDITING MODES:
// --------------
// - DRAW_TILE: Left-click paints tiles, right-click opens tile16 editor
// - MOUSE: Left-click selects entities, right-click opens context menus
//
// KEY WORKFLOWS:
// --------------
// See README.md in this directory for complete workflow documentation.
//
// SUBSYSTEM ORGANIZATION:
// -----------------------
// The class is organized into logical sections marked with comment blocks.
// Each section groups related methods and state for a specific subsystem.
// =============================================================================

namespace yaze {
namespace editor {

// =============================================================================
// Constants
// =============================================================================

constexpr unsigned int k4BPP = 4;
constexpr unsigned int kByteSize = 3;
constexpr unsigned int kMessageIdSize = 5;
constexpr unsigned int kNumSheetsToLoad = 223;
constexpr unsigned int kOverworldMapSize = 0x200;
constexpr ImVec2 kOverworldCanvasSize(kOverworldMapSize * 8,
                                      kOverworldMapSize * 8);
constexpr ImVec2 kCurrentGfxCanvasSize(0x100 + 1, 0x10 * 0x40 + 1);
constexpr ImVec2 kBlocksetCanvasSize(0x100 + 1, 0x4000 + 1);
constexpr ImVec2 kGraphicsBinCanvasSize(0x100 + 1, kNumSheetsToLoad * 0x40 + 1);

constexpr ImGuiTableFlags kOWMapFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

constexpr absl::string_view kWorldList =
    "Light World\0Dark World\0Extra World\0";

constexpr absl::string_view kGamePartComboString = "Part 0\0Part 1\0Part 2\0";

// Zoom/pan constants - centralized for consistency across all zoom controls
constexpr float kOverworldMinZoom = 0.1f;
constexpr float kOverworldMaxZoom = 5.0f;
constexpr float kOverworldZoomStep = 0.25f;

constexpr absl::string_view kOWMapTable = "#MapSettingsTable";

/**
 * @class OverworldEditor
 * @brief Main UI class for editing overworld maps in A Link to the Past.
 *
 * The OverworldEditor orchestrates tile editing, entity management, and
 * map property configuration. It coordinates between the data layer
 * (zelda3::Overworld) and the UI components (canvas, panels, popups).
 *
 * Key subsystems:
 * - Tile16Editor: Individual tile editing with pending changes workflow
 * - MapPropertiesSystem: Toolbar, context menus, property panels
 * - Entity system: Entrances, exits, items, sprites
 * - Canvas system: Main map display and tile selection
 *
 * @see README.md in this directory for architecture documentation
 * @see zelda3/overworld/overworld.h for the data layer
 * @see tile16_editor.h for tile editing details
 */
class OverworldEditor : public Editor, public gfx::GfxContext {
 public:
  // ===========================================================================
  // Construction and Initialization
  // ===========================================================================
  
  explicit OverworldEditor(Rom* rom) : rom_(rom) {
    type_ = EditorType::kOverworld;
    gfx_group_editor_.SetRom(rom);
    // MapPropertiesSystem will be initialized after maps_bmp_ and canvas are
    // ready
  }

  explicit OverworldEditor(Rom* rom, const EditorDependencies& deps)
      : OverworldEditor(rom) {
    dependencies_ = deps;
  }

  void SetGameData(zelda3::GameData* game_data) override {
    game_data_ = game_data;
    overworld_.SetGameData(game_data);
    tile16_editor_.SetGameData(game_data);
    gfx_group_editor_.SetGameData(game_data);
  }

  // ===========================================================================
  // Editor Interface Implementation
  // ===========================================================================
  
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() final;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }
  absl::Status Save() override;
  absl::Status Clear() override;
  
  /// @brief Access the underlying Overworld data
  zelda3::Overworld& overworld() { return overworld_; }

  // ===========================================================================
  // ZSCustomOverworld ASM Patching
  // ===========================================================================
  // These methods handle upgrading vanilla ROMs to support expanded features.
  // See overworld_version_helper.h for version detection and feature matrix.

  /// @brief Apply ZSCustomOverworld ASM patch to upgrade ROM version
  /// @param target_version Target version (2 for v2, 3 for v3)
  absl::Status ApplyZSCustomOverworldASM(int target_version);

  /// @brief Update ROM version markers and feature flags after ASM patching
  absl::Status UpdateROMVersionMarkers(int target_version);

  int jump_to_tab() { return jump_to_tab_; }
  int jump_to_tab_ = -1;

  // ===========================================================================
  // ROM State
  // ===========================================================================
  
  bool IsRomLoaded() const override { return rom_ && rom_->is_loaded(); }
  std::string GetRomStatus() const override {
    if (!rom_)
      return "No ROM loaded";
    if (!rom_->is_loaded())
      return "ROM failed to load";
    return absl::StrFormat("ROM loaded: %s", rom_->title());
  }

  Rom* rom() const { return rom_; }

  /// @brief Set the current map for editing (also updates world)
  void set_current_map(int map_id) {
    if (map_id >= 0 && map_id < zelda3::kNumOverworldMaps) {
      // Finalize any pending paint operation before switching maps
      FinalizePaintOperation();
      current_map_ = map_id;
      current_world_ =
          map_id / 0x40;  // Calculate which world the map belongs to
      overworld_.set_current_map(current_map_);
      overworld_.set_current_world(current_world_);
    }
  }

  // ===========================================================================
  // Graphics Loading
  // ===========================================================================

  /**
   * @brief Load the Bitmap objects for each OverworldMap.
   *
   * Calls the Overworld class to load the image data and palettes from the Rom,
   * then renders the area graphics and tile16 blockset Bitmap objects before
   * assembling the OverworldMap Bitmap objects.
   */
  absl::Status LoadGraphics();

  // ===========================================================================
  // Entity System - Insertion and Editing
  // ===========================================================================
  // Entity operations are delegated to entity_operations.cc helper functions.
  // Entity rendering is handled by OverworldEntityRenderer.
  // Entity interaction (drag/drop) is in overworld_entity_interaction.cc.

  /// @brief Handle entity insertion from context menu
  /// @param entity_type Type: "entrance", "hole", "exit", "item", "sprite"
  void HandleEntityInsertion(const std::string& entity_type);

  /// @brief Process any pending entity insertion request
  /// Called from Update() - needed because ImGui::OpenPopup() doesn't work
  /// correctly when called from within another popup's callback.
  void ProcessPendingEntityInsertion();

  /// @brief Handle tile16 editing from context menu (MOUSE mode)
  /// Gets the tile16 under the cursor and opens the Tile16Editor focused on it.
  void HandleTile16Edit();

  // ===========================================================================
  // Keyboard Shortcuts
  // ===========================================================================
  
  void ToggleBrushTool();
  void ActivateFillTool();
  void CycleTileSelection(int delta);

  /// @brief Handle keyboard shortcuts for the Overworld Editor
  /// Shortcuts: 1-2 (modes), 3-8 (entities), F11 (fullscreen),
  /// Ctrl+L (map lock), Ctrl+T (Tile16 editor), Ctrl+Z/Y (undo/redo)
  void HandleKeyboardShortcuts();

  // ===========================================================================
  // Panel Drawing Methods
  // ===========================================================================
  // These are called by the panel wrapper classes in the panels/ subdirectory.
  
  absl::Status DrawAreaGraphics();
  absl::Status DrawTile16Selector();
  void DrawMapProperties();

  /// @brief Invalidate cached graphics for a specific map or all maps
  /// @param map_id The map to invalidate (-1 to invalidate all maps)
  /// Call this when palette or graphics settings change.
  void InvalidateGraphicsCache(int map_id = -1);
  absl::Status DrawScratchSpace();
  void DrawTile8Selector();
  absl::Status UpdateGfxGroupEditor();
  void DrawV3Settings();
  
  /// @brief Access usage statistics card for panel
  UsageStatisticsCard* usage_stats_card() { return usage_stats_card_.get(); }
  
  /// @brief Access debug window card for panel
  DebugWindowCard* debug_window_card() { return debug_window_card_.get(); }

  /// @brief Access the Tile16 Editor for panel integration
  Tile16Editor& tile16_editor() { return tile16_editor_; }

  /// @brief Draw the main overworld canvas
  void DrawOverworldCanvas();

 private:
  // ===========================================================================
  // Canvas Drawing
  // ===========================================================================
  
  void DrawFullscreenCanvas();
  void DrawOverworldMaps();
  void DrawOverworldEdits();
  void DrawOverworldProperties();

  // ===========================================================================
  // Entity Interaction System
  // ===========================================================================
  // Handles mouse interactions with entities in MOUSE mode.
  
  void HandleEntityEditingShortcuts();
  void HandleUndoRedoShortcuts();

  /// @brief Handle entity interaction in MOUSE mode
  /// Includes: right-click context menus, double-click navigation, popups
  void HandleEntityInteraction();

  /// @brief Handle right-click context menus for entities
  void HandleEntityContextMenus(zelda3::GameEntity* hovered_entity);

  /// @brief Handle double-click actions on entities (e.g., jump to room)
  void HandleEntityDoubleClick(zelda3::GameEntity* hovered_entity);

  /// @brief Draw entity editor popups and update entity data
  void DrawEntityEditorPopups();

  // ===========================================================================
  // Map Refresh System
  // ===========================================================================
  // Methods to refresh map graphics after property changes.
  
  void RefreshChildMap(int map_index);
  void RefreshOverworldMap();
  void RefreshOverworldMapOnDemand(int map_index);
  void RefreshChildMapOnDemand(int map_index);
  void RefreshMultiAreaMapsSafely(int map_index, zelda3::OverworldMap* map);
  absl::Status RefreshMapPalette();
  void RefreshMapProperties();
  absl::Status RefreshTile16Blockset();
  void UpdateBlocksetWithPendingTileChanges();
  void ForceRefreshGraphics(int map_index);
  void RefreshSiblingMapGraphics(int map_index, bool include_self = false);

  // ===========================================================================
  // Tile Editing System
  // ===========================================================================
  // Handles tile painting and selection on the main canvas.

  void RenderUpdatedMapBitmap(const ImVec2& click_position,
                              const std::vector<uint8_t>& tile_data);

  /// @brief Check for tile edits - handles painting and selection
  void CheckForOverworldEdits();

  /// @brief Draw and create the tile16 IDs that are currently selected
  void CheckForSelectRectangle();

  /// @brief Check for map changes and refresh if needed
  absl::Status CheckForCurrentMap();
  
  void CheckForMousePan();
  void UpdateBlocksetSelectorState();
  void HandleMapInteraction();

  /// @brief Scroll the blockset canvas to show the current selected tile16
  void ScrollBlocksetCanvasToCurrentTile();

  // ===========================================================================
  // Texture and Graphics Loading
  // ===========================================================================

  absl::Status LoadSpriteGraphics();

  /// @brief Create textures for deferred map bitmaps on demand
  void ProcessDeferredTextures();

  /// @brief Ensure a specific map has its texture created
  void EnsureMapTexture(int map_index);

  // ===========================================================================
  // Canvas Navigation
  // ===========================================================================

  void HandleOverworldPan();
  void HandleOverworldZoom();  // No-op, use ZoomIn/ZoomOut instead
  void ZoomIn();
  void ZoomOut();
  void ClampOverworldScroll();  // Re-clamp scroll to valid bounds
  void ResetOverworldView();
  void CenterOverworldView();

  // ===========================================================================
  // Canvas Automation API
  // ===========================================================================
  // Integration with automation testing system.

  void SetupCanvasAutomation();
  gui::Canvas* GetOverworldCanvas() { return &ow_map_canvas_; }
  bool AutomationSetTile(int x, int y, int tile_id);
  int AutomationGetTile(int x, int y);

  // ===========================================================================
  // Scratch Space System
  // ===========================================================================
  // Workspace for planning tile layouts before placing them on the map.

  absl::Status SaveCurrentSelectionToScratch();
  absl::Status LoadScratchToSelection();
  absl::Status ClearScratchSpace();
  void DrawScratchSpaceEdits();
  void DrawScratchSpacePattern();
  void DrawScratchSpaceSelection();
  void UpdateScratchBitmapTile(int tile_x, int tile_y, int tile_id);

  // ===========================================================================
  // Background Map Pre-loading
  // ===========================================================================
  // Optimization to load adjacent maps before they're needed.

  /// @brief Queue adjacent maps for background pre-loading
  void QueueAdjacentMapsForPreload(int center_map);

  /// @brief Process one map from the preload queue (called each frame)
  void ProcessPreloadQueue();

  // ===========================================================================
  // Undo/Redo System
  // ===========================================================================
  // Tracks tile paint operations for undo/redo functionality.
  // Operations within 500ms are batched to reduce undo point count.

  struct OverworldUndoPoint {
    int map_id = 0;
    int world = 0;  // 0=Light, 1=Dark, 2=Special
    std::vector<std::pair<std::pair<int, int>, int>>
        tile_changes;  // ((x,y), old_tile_id)
    std::chrono::steady_clock::time_point timestamp;
  };

  void CreateUndoPoint(int map_id, int world, int x, int y, int old_tile_id);
  void FinalizePaintOperation();
  void ApplyUndoPoint(const OverworldUndoPoint& point);
  auto& GetWorldTiles(int world);

  // ===========================================================================
  // Editing Mode State
  // ===========================================================================
  
  EditingMode current_mode = EditingMode::DRAW_TILE;
  EditingMode previous_mode = EditingMode::DRAW_TILE;
  EntityEditMode entity_edit_mode_ = EntityEditMode::NONE;

  enum OverworldProperty {
    LW_AREA_GFX, DW_AREA_GFX, LW_AREA_PAL, DW_AREA_PAL,
    LW_SPR_GFX_PART1, LW_SPR_GFX_PART2, DW_SPR_GFX_PART1, DW_SPR_GFX_PART2,
    LW_SPR_PAL_PART1, LW_SPR_PAL_PART2, DW_SPR_PAL_PART1, DW_SPR_PAL_PART2,
  };

  // ===========================================================================
  // Current Selection State
  // ===========================================================================

  int current_world_ = 0;           // 0=Light, 1=Dark, 2=Special
  int current_map_ = 0;             // Current map index (0-159)
  int current_parent_ = 0;          // Parent map for multi-area
  int current_entrance_id_ = 0;
  int current_exit_id_ = 0;
  int current_item_id_ = 0;
  int current_sprite_id_ = 0;
  int current_blockset_ = 0;
  int game_state_ = 1;              // 0=Beginning, 1=Pendants, 2=Crystals
  int current_tile16_ = 0;          // Selected tile16 for painting
  int selected_entrance_ = 0;
  int selected_usage_map_ = 0xFFFF;
  
  // Selected tile IDs for rectangle selection
  std::vector<int> selected_tile16_ids_;

  // ===========================================================================
  // Loading State
  // ===========================================================================

  bool all_gfx_loaded_ = false;
  bool map_blockset_loaded_ = false;

  // ===========================================================================
  // Canvas Interaction State
  // ===========================================================================

  bool overworld_canvas_fullscreen_ = false;
  bool is_dragging_entity_ = false;
  bool dragged_entity_free_movement_ = false;
  bool current_map_lock_ = false;

  // ===========================================================================
  // Property Windows (Standalone, Not PanelManager)
  // ===========================================================================

  bool show_custom_bg_color_editor_ = false;
  bool show_overlay_editor_ = false;
  bool show_map_properties_panel_ = false;
  bool show_overlay_preview_ = false;

  // ===========================================================================
  // Performance Optimization State
  // ===========================================================================

  // Hover optimization - debounce map building during rapid hover
  int last_hovered_map_ = -1;
  float hover_time_ = 0.0f;
  static constexpr float kHoverBuildDelay = 0.15f;

  // Background pre-loading for adjacent maps
  std::vector<int> preload_queue_;
  static constexpr float kPreloadStartDelay = 0.3f;

  // ===========================================================================
  // UI Subsystem Components
  // ===========================================================================

  std::unique_ptr<MapPropertiesSystem> map_properties_system_;
  std::unique_ptr<OverworldSidebar> sidebar_;
  std::unique_ptr<OverworldEntityRenderer> entity_renderer_;
  std::unique_ptr<OverworldToolbar> toolbar_;

  // ===========================================================================
  // Scratch Space System (Unified Single Workspace)
  // ===========================================================================

  struct ScratchSpace {
    gfx::Bitmap scratch_bitmap;
    std::array<std::array<int, 32>, 32> tile_data{};
    bool in_use = false;
    std::string name = "Scratch Space";
    int width = 16;
    int height = 16;
    std::vector<ImVec2> selected_tiles;
    std::vector<ImVec2> selected_points;
    bool select_rect_active = false;
  };
  ScratchSpace scratch_space_;

  // ===========================================================================
  // Core Data References
  // ===========================================================================

  gfx::Tilemap tile16_blockset_;

  Rom* rom_;
  zelda3::GameData* game_data_ = nullptr;
  gfx::IRenderer* renderer_;
  
  // Sub-editors
  Tile16Editor tile16_editor_{rom_, &tile16_blockset_};
  GfxGroupEditor gfx_group_editor_;
  PaletteEditor palette_editor_;

  // ===========================================================================
  // Graphics Data
  // ===========================================================================

  gfx::SnesPalette palette_;
  gfx::Bitmap selected_tile_bmp_;
  gfx::Bitmap tile16_blockset_bmp_;
  gfx::Bitmap current_gfx_bmp_;
  gfx::Bitmap all_gfx_bmp;
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps> maps_bmp_;
  gfx::BitmapTable current_graphics_set_;
  std::vector<gfx::Bitmap> sprite_previews_;

  // ===========================================================================
  // Overworld Data Layer
  // ===========================================================================

  zelda3::Overworld overworld_{rom_};
  zelda3::OverworldBlockset refresh_blockset_;

  // ===========================================================================
  // Entity State
  // ===========================================================================

  zelda3::Sprite current_sprite_;
  zelda3::OverworldEntrance current_entrance_;
  zelda3::OverworldExit current_exit_;
  zelda3::OverworldItem current_item_;
  zelda3::OverworldEntranceTileTypes entrance_tiletypes_ = {};

  zelda3::GameEntity* current_entity_ = nullptr;
  zelda3::GameEntity* dragged_entity_ = nullptr;

  // Deferred entity insertion (needed for popup flow from context menu)
  std::string pending_entity_insert_type_;
  ImVec2 pending_entity_insert_pos_;
  std::string entity_insert_error_message_;

  // ===========================================================================
  // Canvas Components
  // ===========================================================================

  gui::Canvas ow_map_canvas_{"OwMap", kOverworldCanvasSize,
                             gui::CanvasGridSize::k64x64};
  gui::Canvas current_gfx_canvas_{"CurrentGfx", kCurrentGfxCanvasSize,
                                  gui::CanvasGridSize::k32x32};
  gui::Canvas blockset_canvas_{"OwBlockset", kBlocksetCanvasSize,
                               gui::CanvasGridSize::k32x32};
  std::unique_ptr<gui::TileSelectorWidget> blockset_selector_;
  gui::Canvas graphics_bin_canvas_{"GraphicsBin", kGraphicsBinCanvasSize,
                                   gui::CanvasGridSize::k16x16};
  gui::Canvas properties_canvas_;
  gui::Canvas scratch_canvas_{"ScratchSpace", ImVec2(320, 480),
                              gui::CanvasGridSize::k32x32};

  // ===========================================================================
  // Panel Cards
  // ===========================================================================

  std::unique_ptr<UsageStatisticsCard> usage_stats_card_;
  std::unique_ptr<DebugWindowCard> debug_window_card_;

  absl::Status status_;

  // ===========================================================================
  // Undo/Redo State
  // ===========================================================================

  std::vector<OverworldUndoPoint> undo_stack_;
  std::vector<OverworldUndoPoint> redo_stack_;
  std::optional<OverworldUndoPoint> current_paint_operation_;
  std::chrono::steady_clock::time_point last_paint_time_;
  static constexpr size_t kMaxUndoHistory = 50;
  static constexpr auto kPaintBatchTimeout = std::chrono::milliseconds(500);

  // ===========================================================================
  // Event Listeners
  // ===========================================================================

  int palette_listener_id_ = -1;
};
}  // namespace editor
}  // namespace yaze

#endif
