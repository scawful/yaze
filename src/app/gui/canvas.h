#ifndef YAZE_GUI_CANVAS_H
#define YAZE_GUI_CANVAS_H

#include "gfx/tilemap.h"
#define IMGUI_DEFINE_MATH_OPERATORS

#include <cstdint>
#include <string>
#include <functional>
#include <memory>

#include "app/gfx/bitmap.h"
#include "app/rom.h"
#include "app/gui/canvas_utils.h"
#include "app/gui/enhanced_palette_editor.h"
#include "app/gfx/bpp_format_manager.h"
#include "app/gui/bpp_format_ui.h"
#include "app/gui/canvas/canvas_modals.h"
#include "app/gui/canvas/canvas_context_menu.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "app/gui/canvas/canvas_performance_integration.h"
#include "app/gui/canvas/canvas_interaction_handler.h"
#include "imgui/imgui.h"

namespace yaze {

/**
 * @namespace yaze::gui
 * @brief Graphical User Interface (GUI) components for the application.
 */
namespace gui {

using gfx::Bitmap;
using gfx::BitmapTable;

enum class CanvasType { kTile, kBlock, kMap };
enum class CanvasMode { kPaint, kSelect };
enum class CanvasGridSize { k8x8, k16x16, k32x32, k64x64 };

/**
 * @class Canvas
 * @brief Modern, robust canvas for drawing and manipulating graphics.
 *
 * Following ImGui design patterns, this Canvas class provides:
 * - Modular configuration through CanvasConfig
 * - Separate selection state management  
 * - Enhanced palette management integration
 * - Performance-optimized rendering
 * - Comprehensive context menu system
 */
class Canvas {
 public:
  Canvas() = default;
  
  explicit Canvas(const std::string& id) 
    : canvas_id_(id), context_id_(id + "Context") {
    InitializeDefaults();
  }
  
  explicit Canvas(const std::string& id, ImVec2 canvas_size)
    : canvas_id_(id), context_id_(id + "Context") {
    InitializeDefaults();
    config_.canvas_size = canvas_size;
    config_.custom_canvas_size = true;
  }
  
  explicit Canvas(const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size)
    : canvas_id_(id), context_id_(id + "Context") {
    InitializeDefaults();
    config_.canvas_size = canvas_size;
    config_.custom_canvas_size = true;
    SetGridSize(grid_size);
  }
  
  explicit Canvas(const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size, float global_scale)
    : canvas_id_(id), context_id_(id + "Context") {
    InitializeDefaults();
    config_.canvas_size = canvas_size;
    config_.custom_canvas_size = true;
    config_.global_scale = global_scale;
    SetGridSize(grid_size);
  }

  void SetGridSize(CanvasGridSize grid_size) {
    switch (grid_size) {
      case CanvasGridSize::k8x8:
        config_.grid_step = 8.0f;
        break;
      case CanvasGridSize::k16x16:
        config_.grid_step = 16.0f;
        break;
      case CanvasGridSize::k32x32:
        config_.grid_step = 32.0f;
        break;
      case CanvasGridSize::k64x64:
        config_.grid_step = 64.0f;
        break;
    }
  }

  // Legacy compatibility
  void SetCanvasGridSize(CanvasGridSize grid_size) { SetGridSize(grid_size); }

  void UpdateColorPainter(gfx::Bitmap &bitmap, const ImVec4 &color,
                          const std::function<void()> &event, int tile_size,
                          float scale = 1.0f);

  void UpdateInfoGrid(ImVec2 bg_size, float grid_size = 64.0f,
                      int label_id = 0);

  // ==================== Modern ImGui-Style Interface ====================
  
  /**
   * @brief Begin canvas rendering (ImGui-style)
   * 
   * Modern alternative to DrawBackground(). Handles:
   * - Background and border rendering
   * - Size calculation
   * - Scroll/drag setup
   * - Context menu
   * 
   * Usage:
   * ```cpp
   * canvas.Begin();
   * canvas.DrawBitmap(bitmap);
   * if (canvas.DrawTilePainter(tile, 16)) { ... }
   * canvas.End();  // Draws grid and overlay
   * ```
   */
  void Begin(ImVec2 canvas_size = ImVec2(0, 0));
  
  /**
   * @brief End canvas rendering (ImGui-style)
   * 
   * Modern alternative to manual DrawGrid() + DrawOverlay().
   * Automatically draws grid and overlay if enabled.
   */
  void End();
  
  // ==================== Legacy Interface (Backward Compatible) ====================

  // Background for the Canvas represents region without any content drawn to
  // it, but can be controlled by the user.
  void DrawBackground(ImVec2 canvas_size = ImVec2(0, 0));

  // Context Menu refers to what happens when the right mouse button is pressed
  // This routine also handles the scrolling for the canvas.
  void DrawContextMenu();
  
  // Context menu system for consumers to add their own menu elements
  struct ContextMenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> callback;
    std::function<bool()> enabled_condition = []() { return true; };
    std::vector<ContextMenuItem> subitems;
    
    // Helper constructor for simple items
    ContextMenuItem() = default;
    ContextMenuItem(const std::string& lbl, std::function<void()> cb, 
                   const std::string& sc = "")
        : label(lbl), shortcut(sc), callback(std::move(cb)) {}
    
    // Helper to create disabled item
    static ContextMenuItem Disabled(const std::string& lbl) {
      ContextMenuItem item;
      item.label = lbl;
      item.enabled_condition = []() { return false; };
      return item;
    }
    
    // Helper to create conditional item
    static ContextMenuItem Conditional(const std::string& lbl, std::function<void()> cb,
                                      std::function<bool()> condition) {
      ContextMenuItem item;
      item.label = lbl;
      item.callback = std::move(cb);
      item.enabled_condition = std::move(condition);
      return item;
    }
  };
  
  // BPP format UI components
  std::unique_ptr<gui::BppFormatUI> bpp_format_ui_;
  std::unique_ptr<gui::BppConversionDialog> bpp_conversion_dialog_;
  std::unique_ptr<gui::BppComparisonTool> bpp_comparison_tool_;
  
  // Enhanced canvas components
  std::unique_ptr<canvas::CanvasModals> modals_;
  std::unique_ptr<canvas::CanvasContextMenu> context_menu_;
  std::shared_ptr<canvas::CanvasUsageTracker> usage_tracker_;
  std::shared_ptr<canvas::CanvasPerformanceIntegration> performance_integration_;
  canvas::CanvasInteractionHandler interaction_handler_;
  
  void AddContextMenuItem(const ContextMenuItem& item);
  void ClearContextMenuItems();
  void SetContextMenuEnabled(bool enabled) { context_menu_enabled_ = enabled; }
  
  // Enhanced view and edit operations
  void ShowAdvancedCanvasProperties();
  void ShowScalingControls();
  void SetZoomToFit(const gfx::Bitmap& bitmap);
  void ResetView();
  void ApplyConfigSnapshot(const canvas::CanvasConfig& snapshot);
  void ApplyScaleSnapshot(const canvas::CanvasConfig& snapshot);
  
  // Modular component access
  CanvasConfig& GetConfig() { return config_; }
  const CanvasConfig& GetConfig() const { return config_; }
  CanvasSelection& GetSelection() { return selection_; }
  const CanvasSelection& GetSelection() const { return selection_; }
  
  // Enhanced palette management
  void InitializePaletteEditor(Rom* rom);
  void ShowPaletteEditor();
  void ShowColorAnalysis();
  bool ApplyROMPalette(int group_index, int palette_index);
  
  // BPP format management
  void ShowBppFormatSelector();
  void ShowBppAnalysis();
  void ShowBppConversionDialog();
  bool ConvertBitmapFormat(gfx::BppFormat target_format);
  gfx::BppFormat GetCurrentBppFormat() const;
  
  // Enhanced canvas management
  void InitializeEnhancedComponents();
  void SetUsageMode(canvas::CanvasUsage usage);
  canvas::CanvasUsage GetUsageMode() const;
  void RecordCanvasOperation(const std::string& operation_name, double time_ms);
  void ShowPerformanceUI();
  void ShowUsageReport();
  
  // Interaction handler access
  canvas::CanvasInteractionHandler& GetInteractionHandler() { return interaction_handler_; }
  const canvas::CanvasInteractionHandler& GetInteractionHandler() const { return interaction_handler_; }
  
  // Initialization and cleanup
  void InitializeDefaults();
  void Cleanup();
  
  // Size reporting for ImGui table integration
  ImVec2 GetMinimumSize() const;
  ImVec2 GetPreferredSize() const;
  ImVec2 GetCurrentSize() const { return config_.canvas_size; }
  void SetAutoResize(bool auto_resize) { config_.auto_resize = auto_resize; }
  bool IsAutoResize() const { return config_.auto_resize; }
  
  // Table integration helpers
  void ReserveTableSpace(const std::string& label = "");
  bool BeginTableCanvas(const std::string& label = "");
  void EndTableCanvas();
  
  // Improved interaction detection
  bool HasValidSelection() const;
  bool WasClicked(ImGuiMouseButton button = ImGuiMouseButton_Left) const;
  bool WasDoubleClicked(ImGuiMouseButton button = ImGuiMouseButton_Left) const;
  ImVec2 GetLastClickPosition() const;
  
 private:
  void DrawContextMenuItem(const ContextMenuItem& item);

  // Tile painter shows a preview of the currently selected tile
  // and allows the user to left click to paint the tile or right
  // click to select a new tile to paint with.
  // (Moved to public section)

  // Draws a tile on the canvas at the specified position
  // (Moved to public section)

  // These methods are now public - see public section above

 public:
  // Tile painter methods
  bool DrawTilePainter(const Bitmap &bitmap, int size, float scale = 1.0f);
  bool DrawSolidTilePainter(const ImVec4 &color, int size);
  void DrawTileOnBitmap(int tile_size, gfx::Bitmap *bitmap, ImVec4 color);
  
  void DrawOutline(int x, int y, int w, int h);
  void DrawOutlineWithColor(int x, int y, int w, int h, ImVec4 color);
  void DrawOutlineWithColor(int x, int y, int w, int h, uint32_t color);

  void DrawRect(int x, int y, int w, int h, ImVec4 color);

  void DrawText(std::string text, int x, int y);
  void DrawGridLines(float grid_step);

  void DrawInfoGrid(float grid_step = 64.0f, int tile_id_offset = 8,
                    int label_id = 0);

  void DrawLayeredElements();

  int GetTileIdFromMousePos() {
    float x = mouse_pos_in_canvas_.x;
    float y = mouse_pos_in_canvas_.y;
    int num_columns = (canvas_sz_.x / global_scale_) / custom_step_;
    int num_rows = (canvas_sz_.y / global_scale_) / custom_step_;
    int tile_id = (x / custom_step_) + (y / custom_step_) * num_columns;
    tile_id = tile_id / global_scale_;
    if (tile_id >= num_columns * num_rows) {
      tile_id = -1;  // Invalid tile ID
    }
    return tile_id;
  }
  void DrawCustomHighlight(float grid_step);
  bool IsMouseHovering() const { return is_hovered_; }
  void ZoomIn() { global_scale_ += 0.25f; }
  void ZoomOut() { global_scale_ -= 0.25f; }

  // Points accessors - points_ is maintained separately for custom overlay drawing
  const ImVector<ImVec2>& points() const { return points_; }
  ImVector<ImVec2>* mutable_points() { return &points_; }
  auto push_back(ImVec2 pos) { points_.push_back(pos); }
  auto draw_list() const { return draw_list_; }
  auto zero_point() const { return canvas_p0_; }
  auto scrolling() const { return scrolling_; }
  void set_scrolling(ImVec2 scroll) { scrolling_ = scroll; }
  auto drawn_tile_position() const { return drawn_tile_pos_; }
  auto canvas_size() const { return canvas_sz_; }
  void set_global_scale(float scale) { global_scale_ = scale; }
  void set_draggable(bool draggable) { draggable_ = draggable; }
  
  // Modern accessors using modular structure
  bool IsSelectRectActive() const { return select_rect_active_; }
  const std::vector<ImVec2>& GetSelectedTiles() const { return selected_tiles_; }
  ImVec2 GetSelectedTilePos() const { return selected_tile_pos_; }
  void SetSelectedTilePos(ImVec2 pos) { selected_tile_pos_ = pos; }
  
  // Configuration accessors  
  void SetCanvasSize(ImVec2 canvas_size) { 
    config_.canvas_size = canvas_size; 
    config_.custom_canvas_size = true; 
  }
  float GetGlobalScale() const { return config_.global_scale; }
  void SetGlobalScale(float scale) { config_.global_scale = scale; }
  bool* GetCustomLabelsEnabled() { return &config_.enable_custom_labels; }
  float GetGridStep() const { return config_.grid_step; }
  
  // Rectangle selection boundary control (prevents wrapping in large maps)
  void SetClampRectToLocalMaps(bool clamp) { config_.clamp_rect_to_local_maps = clamp; }
  bool GetClampRectToLocalMaps() const { return config_.clamp_rect_to_local_maps; }
  float GetCanvasWidth() const { return config_.canvas_size.x; }
  float GetCanvasHeight() const { return config_.canvas_size.y; }
  
  // Legacy compatibility accessors
  auto select_rect_active() const { return select_rect_active_; }
  auto selected_tiles() const { return selected_tiles_; }
  auto selected_tile_pos() const { return selected_tile_pos_; }
  void set_selected_tile_pos(ImVec2 pos) { selected_tile_pos_ = pos; }
  auto global_scale() const { return config_.global_scale; }
  auto custom_labels_enabled() { return &config_.enable_custom_labels; }
  auto custom_step() const { return config_.grid_step; }
  auto width() const { return config_.canvas_size.x; }
  auto height() const { return config_.canvas_size.y; }
  
  // Public accessors for methods that need to be accessed externally
  auto canvas_id() const { return canvas_id_; }
  
  // Public methods for drawing operations
  void DrawBitmap(Bitmap &bitmap, int border_offset, float scale);
  void DrawBitmap(Bitmap &bitmap, int x_offset, int y_offset, float scale = 1.0f, int alpha = 255);
  void DrawBitmap(Bitmap &bitmap, ImVec2 dest_pos, ImVec2 dest_size, ImVec2 src_pos, ImVec2 src_size);
  void DrawBitmapTable(const BitmapTable &gfx_bin);
  /**
   * @brief Draw group of bitmaps for multi-tile selection preview
   * @param group Vector of tile IDs to draw
   * @param tilemap Tilemap containing the tiles
   * @param tile_size Size of each tile (default 16)
   * @param scale Rendering scale (default 1.0)
   * @param local_map_size Size of local map in pixels (default 512 for standard maps)
   * @param total_map_size Total map size for boundary clamping (default 4096x4096)
   */
  void DrawBitmapGroup(std::vector<int> &group, gfx::Tilemap &tilemap, 
                      int tile_size, float scale = 1.0f,
                      int local_map_size = 0x200,
                      ImVec2 total_map_size = ImVec2(0x1000, 0x1000));
  bool DrawTilemapPainter(gfx::Tilemap &tilemap, int current_tile);
  void DrawSelectRect(int current_map, int tile_size = 0x10, float scale = 1.0f);
  bool DrawTileSelector(int size, int size_y = 0);
  void DrawGrid(float grid_step = 64.0f, int tile_id_offset = 8);
  void DrawOverlay();
  auto labels(int i) {
    if (i >= labels_.size()) {
      labels_.push_back(ImVector<std::string>());
    }
    return labels_[i];
  }
  auto mutable_labels(int i) {
    if (i >= labels_.size()) {
      int x = i;
      while (x >= labels_.size()) {
        labels_.push_back(ImVector<std::string>());
        x--;
      }
      labels_.push_back(ImVector<std::string>());
    }
    return &labels_[i];
  }

  auto set_current_labels(int i) { current_labels_ = i; }
  auto set_highlight_tile_id(int i) { highlight_tile_id = i; }

  auto mutable_selected_tiles() { return &selected_tiles_; }
  auto selected_points() const { return selected_points_; }

  auto hover_mouse_pos() const { return mouse_pos_in_canvas_; }

  void set_rom(Rom *rom) { rom_ = rom; }
  Rom *rom() const { return rom_; }

 private:
  // Modular configuration and state
  CanvasConfig config_;
  CanvasSelection selection_;
  std::unique_ptr<EnhancedPaletteEditor> palette_editor_;
  
  // Core canvas state
  bool is_hovered_ = false;
  bool refresh_graphics_ = false;

  // Context menu system
  std::vector<ContextMenuItem> context_menu_items_;
  bool context_menu_enabled_ = true;

  // Legacy members (to be gradually replaced)
  int current_labels_ = 0;
  int highlight_tile_id = -1;
  uint16_t edit_palette_index_ = 0;
  uint64_t edit_palette_group_name_index_ = 0;
  uint64_t edit_palette_sub_index_ = 0;

  // Core canvas state
  Bitmap *bitmap_ = nullptr;
  Rom *rom_ = nullptr;
  ImDrawList *draw_list_ = nullptr;

  // Canvas geometry and interaction state
  ImVec2 scrolling_;
  ImVec2 canvas_sz_;
  ImVec2 canvas_p0_;
  ImVec2 canvas_p1_;
  ImVec2 drawn_tile_pos_;
  ImVec2 mouse_pos_in_canvas_;

  // Drawing and labeling
  // NOTE: points_ synchronized from interaction_handler_ for backward compatibility
  ImVector<ImVec2> points_;
  ImVector<ImVector<std::string>> labels_;

  // Identification
  std::string canvas_id_ = "Canvas";
  std::string context_id_ = "CanvasContext";
  
  // Legacy compatibility (gradually being replaced by selection_)
  std::vector<ImVec2> selected_tiles_;
  ImVector<ImVec2> selected_points_;
  ImVec2 selected_tile_pos_ = ImVec2(-1, -1);
  bool select_rect_active_ = false;
  float custom_step_ = 32.0f;
  float global_scale_ = 1.0f;
  bool enable_grid_ = true;
  bool enable_hex_tile_labels_ = false;
  bool enable_custom_labels_ = false;
  bool enable_context_menu_ = true;
  bool custom_canvas_size_ = false;
  bool draggable_ = false;
};

void BeginCanvas(Canvas &canvas, ImVec2 child_size = ImVec2(0, 0));
void EndCanvas(Canvas &canvas);

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, BitmapTable &graphics_bin);

void BitmapCanvasPipeline(gui::Canvas &canvas, gfx::Bitmap &bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id);

// Table-optimized canvas pipeline with automatic sizing
void TableCanvasPipeline(gui::Canvas &canvas, gfx::Bitmap &bitmap, 
                        const std::string& label = "", bool auto_resize = true);

/**
 * @class ScopedCanvas
 * @brief RAII wrapper for Canvas (ImGui-style)
 * 
 * Automatically calls Begin() on construction and End() on destruction,
 * preventing forgotten End() calls and ensuring proper cleanup.
 * 
 * Usage:
 * ```cpp
 * {
 *   gui::ScopedCanvas canvas("MyCanvas", ImVec2(512, 512));
 *   canvas->DrawBitmap(bitmap);
 *   if (canvas->DrawTilePainter(tile, 16)) {
 *     HandlePaint(canvas->drawn_tile_position());
 *   }
 * } // Automatic End() and cleanup
 * ```
 * 
 * Or wrap existing canvas:
 * ```cpp
 * Canvas my_canvas("Editor");
 * {
 *   ScopedCanvas scoped(my_canvas);
 *   scoped->DrawBitmap(bitmap);
 * } // Automatic End()
 * ```
 */
class ScopedCanvas {
 public:
  /**
   * @brief Construct and begin a new canvas
   */
  explicit ScopedCanvas(const std::string& id, ImVec2 canvas_size = ImVec2(0, 0))
      : canvas_(new Canvas(id, canvas_size)), owned_(true), active_(true) {
    canvas_->Begin();
  }
  
  /**
   * @brief Wrap existing canvas with RAII
   */
  explicit ScopedCanvas(Canvas& canvas) 
      : canvas_(&canvas), owned_(false), active_(true) {
    canvas_->Begin();
  }
  
  /**
   * @brief Destructor automatically calls End()
   */
  ~ScopedCanvas() {
    if (active_ && canvas_) {
      canvas_->End();
    }
    if (owned_) {
      delete canvas_;
    }
  }
  
  // No copy, move only
  ScopedCanvas(const ScopedCanvas&) = delete;
  ScopedCanvas& operator=(const ScopedCanvas&) = delete;
  
  ScopedCanvas(ScopedCanvas&& other) noexcept 
      : canvas_(other.canvas_), owned_(other.owned_), active_(other.active_) {
    other.active_ = false;
    other.canvas_ = nullptr;
  }
  
  /**
   * @brief Arrow operator for clean syntax: scoped->DrawBitmap(...)
   */
  Canvas* operator->() { return canvas_; }
  const Canvas* operator->() const { return canvas_; }
  
  /**
   * @brief Dereference operator for direct access: (*scoped).DrawBitmap(...)
   */
  Canvas& operator*() { return *canvas_; }
  const Canvas& operator*() const { return *canvas_; }
  
  /**
   * @brief Get underlying canvas
   */
  Canvas* get() { return canvas_; }
  const Canvas* get() const { return canvas_; }
  
 private:
  Canvas* canvas_;
  bool owned_;
  bool active_;
};

}  // namespace gui
}  // namespace yaze

#endif
