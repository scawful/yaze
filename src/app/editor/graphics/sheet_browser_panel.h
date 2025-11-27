#ifndef YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"

namespace yaze {
namespace editor {

/**
 * @brief Panel for browsing and selecting graphics sheets
 *
 * Displays a grid view of all 223 graphics sheets from the ROM.
 * Supports single/multi-select, search/filter, and batch operations.
 */
class SheetBrowserPanel {
 public:
  explicit SheetBrowserPanel(GraphicsEditorState* state) : state_(state) {}

  /**
   * @brief Initialize the panel
   */
  void Initialize();

  /**
   * @brief Render the sheet browser UI
   * @return Status of the render operation
   */
  absl::Status Update();

 private:
  /**
   * @brief Draw the search/filter bar
   */
  void DrawSearchBar();

  /**
   * @brief Draw the sheet grid view
   */
  void DrawSheetGrid();

  /**
   * @brief Draw a single sheet thumbnail
   * @param sheet_id Sheet index (0-222)
   * @param bitmap The bitmap to display
   */
  void DrawSheetThumbnail(int sheet_id, gfx::Bitmap& bitmap);

  /**
   * @brief Draw batch operation buttons
   */
  void DrawBatchOperations();

  GraphicsEditorState* state_;
  gui::Canvas thumbnail_canvas_;

  // Search/filter state
  char search_buffer_[16] = {0};
  int filter_min_ = 0;
  int filter_max_ = 222;
  bool show_only_modified_ = false;

  // Grid layout
  float thumbnail_scale_ = 2.0f;
  int columns_ = 2;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
