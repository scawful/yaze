#ifndef YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_save_plan.h"
#include "app/editor/graphics/sheet_png_transfer.h"
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

class UndoManager;

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

  using LabelSetter =
      std::function<absl::Status(uint16_t sheet, const std::string& label)>;
  using LabelImporter =
      std::function<absl::StatusOr<int>(const std::string& csv_text)>;

  /**
   * @brief Project-label writers. Unset callbacks disable label editing.
   */
  void SetLabelCallbacks(LabelSetter setter, LabelImporter importer) {
    label_setter_ = std::move(setter);
    label_importer_ = std::move(importer);
  }

  /**
   * @brief Where PNG imports push their undo steps. Unset means no undo.
   */
  void SetUndoManager(UndoManager* undo_manager) {
    undo_manager_ = undo_manager;
  }

  using SavePlanner =
      std::function<absl::StatusOr<std::vector<GraphicsSavePlanEntry>>()>;
  /**
   * @brief Source of the "Pending graphics save" preflight list
   * (GraphicsEditor::PlanGraphicsSave). Unset hides the list.
   */
  void SetSavePlanner(SavePlanner planner) {
    save_planner_ = std::move(planner);
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

  // Project sheet labels.
  std::string SheetLabel(uint16_t sheet) const;
  void DrawSheetLabelEditor(uint16_t sheet_id);
  LabelSetter label_setter_;
  LabelImporter label_importer_;
  int label_edit_sheet_ = -1;
  std::string label_buffer_;
  std::string label_status_;

  // PNG export and import (sheet_png_transfer). Imports are previewed, then
  // applied to the Arena as one undo step per sheet; saving the ROM writes
  // them through GraphicsEditor::Save.
  void DrawPngTransfer(uint16_t sheet_id);
  absl::StatusOr<zelda3::SheetPalette> PngPalette();
  void SetPngStatus(std::string message, bool is_error);
  UndoManager* undo_manager_ = nullptr;
  int png_palette_mode_ = 0;  // 0 grayscale, 1 room background, 2 room sprite
  int png_room_ = 0;
  int png_palette_row_ = 2;
  std::vector<SheetPngImportPreview> png_pending_;
  std::string png_status_;
  bool png_status_is_error_ = false;

  // Pending graphics save: the preflight for the dirty sheets, computed on
  // request because it runs the writer on a copy of the ROM.
  void DrawPendingSave();
  std::string SheetUsageSummary(uint16_t sheet_id) const;
  SavePlanner save_planner_;
  std::vector<GraphicsSavePlanEntry> save_plan_;
  std::string save_plan_error_;
  std::set<uint16_t> save_plan_sheets_;  // the dirty set the plan describes
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SHEET_BROWSER_PANEL_H
