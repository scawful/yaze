#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDITOR_STATE_H
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDITOR_STATE_H

#include <cstdint>
#include <functional>
#include <set>
#include <stack>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Pixel editing tool types for the graphics editor
 */
enum class PixelTool {
  kSelect,       // Rectangle selection
  kLasso,        // Freeform selection
  kPencil,       // Single pixel drawing
  kBrush,        // Multi-pixel brush
  kEraser,       // Set pixels to transparent (index 0)
  kFill,         // Flood fill
  kLine,         // Line drawing
  kRectangle,    // Rectangle outline/fill
  kEyedropper,   // Color picker from canvas
};

/**
 * @brief Selection data for copy/paste operations
 */
struct PixelSelection {
  std::vector<uint8_t> pixel_data;   // Copied pixel indices
  gfx::SnesPalette palette;          // Associated palette
  int x = 0;                         // Selection origin X
  int y = 0;                         // Selection origin Y
  int width = 0;                     // Selection width
  int height = 0;                    // Selection height
  bool is_active = false;            // Whether selection exists
  bool is_floating = false;          // Floating vs committed

  void Clear() {
    pixel_data.clear();
    x = y = width = height = 0;
    is_active = false;
    is_floating = false;
  }
};

/**
 * @brief Snapshot for undo/redo operations
 */
struct PixelEditorSnapshot {
  uint16_t sheet_id;
  std::vector<uint8_t> pixel_data;
  gfx::SnesPalette palette;

  bool operator==(const PixelEditorSnapshot& other) const {
    return sheet_id == other.sheet_id && pixel_data == other.pixel_data;
  }
};

/**
 * @brief Shared state between GraphicsEditor panel components
 *
 * This class maintains the state that needs to be shared between the
 * Sheet Browser, Pixel Editor, and Palette Controls panels. It provides
 * a single source of truth for selection, current sheet, palette, and
 * editing state.
 */
class GraphicsEditorState {
 public:
  // --- Current Selection ---
  uint16_t current_sheet_id = 0;
  std::set<uint16_t> open_sheets;
  std::set<uint16_t> selected_sheets;  // Multi-select support

  // --- Editing State ---
  PixelTool current_tool = PixelTool::kPencil;
  uint8_t current_color_index = 1;  // Palette index (0 = transparent)
  ImVec4 current_color;             // RGBA for display
  uint8_t brush_size = 1;           // 1-8 pixel brush
  bool fill_contiguous = true;      // Fill tool: contiguous only

  // --- View State ---
  float zoom_level = 4.0f;          // 1x to 16x
  bool show_grid = true;            // 8x8 tile grid
  bool show_tile_boundaries = true; // 16x16 tile boundaries
  ImVec2 pan_offset = {0, 0};       // Canvas pan offset

  // --- Overlay State (for enhanced UX) ---
  bool show_cursor_crosshair = true;    // Crosshair at cursor position
  bool show_brush_preview = true;       // Preview circle for brush/eraser
  bool show_transparency_grid = true;   // Checkerboard for transparent pixels
  bool show_pixel_info_tooltip = true;  // Tooltip with pixel info on hover

  // --- Palette State ---
  uint64_t palette_group_index = 0;
  uint64_t palette_index = 0;
  uint64_t sub_palette_index = 0;
  bool refresh_graphics = false;

  // --- Selection State ---
  PixelSelection selection;
  ImVec2 selection_start;           // Drag start point
  bool is_selecting = false;        // Currently drawing selection

  // --- Undo/Redo ---
  std::vector<PixelEditorSnapshot> undo_stack;
  std::vector<PixelEditorSnapshot> redo_stack;
  static constexpr size_t kMaxUndoHistory = 50;

  // --- Modified Sheets Tracking ---
  std::set<uint16_t> modified_sheets;

  // --- Callbacks for cross-panel communication ---
  std::function<void(uint16_t)> on_sheet_selected;
  std::function<void()> on_palette_changed;
  std::function<void()> on_tool_changed;
  std::function<void(uint16_t)> on_sheet_modified;

  // --- Methods ---

  /**
   * @brief Mark a sheet as modified for save tracking
   */
  void MarkSheetModified(uint16_t sheet_id) {
    modified_sheets.insert(sheet_id);
    if (on_sheet_modified) {
      on_sheet_modified(sheet_id);
    }
  }

  /**
   * @brief Clear modification tracking (after save)
   */
  void ClearModifiedSheets() { modified_sheets.clear(); }

  /**
   * @brief Clear specific sheets from modification tracking
   */
  void ClearModifiedSheets(const std::set<uint16_t>& sheet_ids) {
    for (auto sheet_id : sheet_ids) {
      modified_sheets.erase(sheet_id);
    }
  }

  /**
   * @brief Check if any sheets have unsaved changes
   */
  bool HasUnsavedChanges() const { return !modified_sheets.empty(); }

  /**
   * @brief Push current state to undo stack before modification
   */
  void PushUndoState(uint16_t sheet_id, const std::vector<uint8_t>& pixel_data,
                     const gfx::SnesPalette& palette) {
    // Clear redo stack on new action
    redo_stack.clear();

    // Add to undo stack
    undo_stack.push_back({sheet_id, pixel_data, palette});

    // Limit stack size
    if (undo_stack.size() > kMaxUndoHistory) {
      undo_stack.erase(undo_stack.begin());
    }
  }

  /**
   * @brief Pop and return the last undo state
   */
  bool PopUndoState(PixelEditorSnapshot& out) {
    if (undo_stack.empty()) return false;
    out = undo_stack.back();
    redo_stack.push_back(out);
    undo_stack.pop_back();
    return true;
  }

  /**
   * @brief Pop and return the last redo state
   */
  bool PopRedoState(PixelEditorSnapshot& out) {
    if (redo_stack.empty()) return false;
    out = redo_stack.back();
    undo_stack.push_back(out);
    redo_stack.pop_back();
    return true;
  }

  bool CanUndo() const { return !undo_stack.empty(); }
  bool CanRedo() const { return !redo_stack.empty(); }

  /**
   * @brief Select a sheet for editing
   */
  void SelectSheet(uint16_t sheet_id) {
    current_sheet_id = sheet_id;
    open_sheets.insert(sheet_id);
    if (on_sheet_selected) {
      on_sheet_selected(sheet_id);
    }
  }

  /**
   * @brief Close a sheet tab
   */
  void CloseSheet(uint16_t sheet_id) { open_sheets.erase(sheet_id); }

  /**
   * @brief Set the current editing tool
   */
  void SetTool(PixelTool tool) {
    current_tool = tool;
    if (on_tool_changed) {
      on_tool_changed();
    }
  }

  /**
   * @brief Set zoom level with clamping
   */
  void SetZoom(float zoom) {
    zoom_level = std::clamp(zoom, 1.0f, 16.0f);
  }

  void ZoomIn() { SetZoom(zoom_level + 1.0f); }
  void ZoomOut() { SetZoom(zoom_level - 1.0f); }

  /**
   * @brief Get tool name for status display
   */
  const char* GetToolName() const {
    switch (current_tool) {
      case PixelTool::kSelect:     return "Select";
      case PixelTool::kLasso:      return "Lasso";
      case PixelTool::kPencil:     return "Pencil";
      case PixelTool::kBrush:      return "Brush";
      case PixelTool::kEraser:     return "Eraser";
      case PixelTool::kFill:       return "Fill";
      case PixelTool::kLine:       return "Line";
      case PixelTool::kRectangle:  return "Rectangle";
      case PixelTool::kEyedropper: return "Eyedropper";
      default:                     return "Unknown";
    }
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_EDITOR_STATE_H
