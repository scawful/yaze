#ifndef YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H

#include <functional>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "zelda3/gfx_sheet_inventory.h"

namespace yaze {
class Rom;
namespace zelda3 {
class GameData;
}  // namespace zelda3
namespace project {
struct YazeProject;
}  // namespace project

namespace editor {

/**
 * @brief WindowContent for browsing and selecting graphics sheets
 *
 * Displays a grid view of all 223 graphics sheets from the ROM.
 * Supports single/multi-select, search/filter, and batch operations.
 */
class SheetBrowserPanel : public WindowContent {
 public:
  explicit SheetBrowserPanel(GraphicsEditorState* state) : state_(state) {}

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "graphics.sheet_browser_v2"; }
  std::string GetDisplayName() const override { return "Sheet Browser"; }
  std::string GetIcon() const override { return ICON_MD_VIEW_LIST; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 10; }

  // ==========================================================================
  // WindowContent Lifecycle
  // ==========================================================================

  /**
   * @brief Initialize the panel
   */
  void Initialize();

  /**
   * @brief Draw the sheet browser UI
   */
  void Draw(bool* p_open) override;

  /**
   * @brief Legacy Update method for backward compatibility
   * @return Status of the render operation
   */
  absl::Status Update();

  /**
   * @brief Where free-block and "used by" data come from. The project
   * getter may return null (no project open).
   */
  void SetDataSources(
      Rom* rom, zelda3::GameData* game_data,
      std::function<const project::YazeProject*()> project_getter);

  /**
   * @brief Rebuild the sheet inventory from the ROM buffer. Runs lazily on
   * the first draw and when the ROM changes.
   */
  void RefreshInventory();

  const std::optional<zelda3::GfxSheetInventory>& inventory() const {
    return inventory_;
  }

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

  /**
   * @brief Storage, reserved state, free 16x16 blocks and "used by" for the
   * current sheet.
   */
  void DrawSelectedSheetInfo();

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

  // Inventory (free blocks, reserved, used by), built from the ROM buffer.
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::function<const project::YazeProject*()> project_getter_;
  std::optional<zelda3::GfxSheetInventory> inventory_;
  std::string inventory_error_;
  const Rom* inventory_rom_ = nullptr;
  bool show_free_blocks_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
