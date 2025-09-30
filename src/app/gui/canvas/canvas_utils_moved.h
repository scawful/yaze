#ifndef YAZE_APP_GUI_CANVAS_CANVAS_UTILS_H
#define YAZE_APP_GUI_CANVAS_CANVAS_UTILS_H

#include <string>
#include <vector>
#include <functional>
#include "app/gfx/snes_palette.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace canvas {

/**
 * @brief Configuration for canvas display and interaction
 * 
 * Modern single-source-of-truth configuration structure.
 * Replaces the dual state management pattern.
 */
struct CanvasConfig {
  bool enable_grid = true;
  bool enable_hex_labels = false;
  bool enable_custom_labels = false;
  bool enable_context_menu = true;
  bool is_draggable = false;
  bool auto_resize = false;
  float grid_step = 32.0f;
  float global_scale = 1.0f;
  ImVec2 canvas_size = ImVec2(0, 0);
  ImVec2 content_size = ImVec2(0, 0);  // Size of actual content (bitmap, etc.)
  bool custom_canvas_size = false;
  ImVec2 scrolling = ImVec2(0, 0);
  
  // Modern callbacks for config updates
  std::function<void(const CanvasConfig&)> on_config_changed;
  std::function<void(const CanvasConfig&)> on_scale_changed;
};

/**
 * @brief Selection state for canvas interactions
 */
struct CanvasSelection {
  std::vector<ImVec2> selected_tiles;
  std::vector<ImVec2> selected_points;
  ImVec2 selected_tile_pos = ImVec2(-1, -1);
  bool select_rect_active = false;
  
  void Clear() {
    selected_tiles.clear();
    selected_points.clear();
    selected_tile_pos = ImVec2(-1, -1);
    select_rect_active = false;
  }
};

/**
 * @brief Palette management state for canvas
 */
struct CanvasPaletteManager {
  std::vector<gfx::SnesPalette> rom_palette_groups;
  std::vector<std::string> palette_group_names;
  gfx::SnesPalette original_palette;
  bool palettes_loaded = false;
  int current_group_index = 0;
  int current_palette_index = 0;
  
  void Clear() {
    rom_palette_groups.clear();
    palette_group_names.clear();
    original_palette.clear();
    palettes_loaded = false;
    current_group_index = 0;
    current_palette_index = 0;
  }
};

/**
 * @brief Context menu item configuration
 */
struct CanvasContextMenuItem {
  std::string label;
  std::string shortcut;
  std::function<void()> callback;
  std::function<bool()> enabled_condition = []() { return true; };
  std::vector<CanvasContextMenuItem> subitems;
};

/**
 * @brief Render context for canvas drawing operations
 */
struct CanvasRenderContext {
  ImDrawList* draw_list;
  ImVec2 canvas_p0;
  ImVec2 canvas_p1;
  ImVec2 scrolling;
  float global_scale;
  bool enable_grid;
  bool enable_hex_labels;
  float grid_step;
};

/**
 * @brief Utility functions for canvas operations
 * 
 * Stateless utilities for common canvas operations.
 * Following ImGui design pattern of pure helper functions.
 */
namespace CanvasUtils {

// ==================== Core Utilities (Stateless) ====================

/**
 * @brief Align position to grid
 * Pure function - no side effects
 */
ImVec2 AlignToGrid(ImVec2 pos, float grid_step);

/**
 * @brief Calculate effective scale for content
 */
float CalculateEffectiveScale(ImVec2 canvas_size, ImVec2 content_size, float global_scale);

/**
 * @brief Get tile ID from mouse position
 */
int GetTileIdFromPosition(ImVec2 mouse_pos, float tile_size, float scale, int tiles_per_row);

// ==================== Palette Management ====================

bool LoadROMPaletteGroups(Rom* rom, CanvasPaletteManager& palette_manager);
bool ApplyPaletteGroup(gfx::Bitmap* bitmap, const CanvasPaletteManager& palette_manager, 
                       int group_index, int palette_index);

// ==================== Drawing Utilities ====================

void DrawCanvasRect(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling, 
                   int x, int y, int w, int h, ImVec4 color, float global_scale);

void DrawCanvasText(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                   const std::string& text, int x, int y, float global_scale);

void DrawCanvasOutline(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                      int x, int y, int w, int h, uint32_t color = IM_COL32(255, 255, 255, 200));

void DrawCanvasOutlineWithColor(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                               int x, int y, int w, int h, ImVec4 color);

// ==================== Grid Utilities ====================

void DrawCanvasGridLines(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 canvas_p1, 
                        ImVec2 scrolling, float grid_step, float global_scale);

void DrawCustomHighlight(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                        int highlight_tile_id, float grid_step);

void DrawHexTileLabels(ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
                      ImVec2 canvas_sz, float grid_step, float global_scale);

// ==================== Layout Utilities ====================

ImVec2 CalculateCanvasSize(ImVec2 content_region, ImVec2 custom_size, bool use_custom);
ImVec2 CalculateScaledCanvasSize(ImVec2 canvas_size, float global_scale);
bool IsPointInCanvas(ImVec2 point, ImVec2 canvas_p0, ImVec2 canvas_p1);

// ==================== Size Reporting for Tables ====================

ImVec2 CalculateMinimumCanvasSize(ImVec2 content_size, float global_scale, float padding = 4.0f);
ImVec2 CalculatePreferredCanvasSize(ImVec2 content_size, float global_scale, float min_scale = 1.0f);
void ReserveCanvasSpace(ImVec2 canvas_size, const std::string& label = "");
void SetNextCanvasSize(ImVec2 size, bool auto_resize = false);

// ==================== Composite Operations ====================

void DrawCanvasGrid(const CanvasRenderContext& ctx, int highlight_tile_id = -1);
void DrawCanvasOverlay(const CanvasRenderContext& ctx, const ImVector<ImVec2>& points, 
                      const ImVector<ImVec2>& selected_points);
void DrawCanvasLabels(const CanvasRenderContext& ctx, const ImVector<ImVector<std::string>>& labels,
                     int current_labels, int tile_id_offset);

}  // namespace CanvasUtils

}  // namespace canvas

// ==================== Compatibility Aliases (gui namespace) ====================

// For backward compatibility, provide aliases in gui namespace
using CanvasConfig = canvas::CanvasConfig;
using CanvasSelection = canvas::CanvasSelection;
using CanvasPaletteManager = canvas::CanvasPaletteManager;
using CanvasContextMenuItem = canvas::CanvasContextMenuItem;

namespace CanvasUtils = canvas::CanvasUtils;

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_UTILS_H
