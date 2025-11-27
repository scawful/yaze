#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

/**
 * @brief Panel for managing palettes applied to graphics sheets
 *
 * Provides palette group selection, quick presets, and
 * apply-to-sheet functionality.
 */
class PaletteControlsPanel {
 public:
  explicit PaletteControlsPanel(GraphicsEditorState* state, Rom* rom)
      : state_(state), rom_(rom) {}

  /**
   * @brief Initialize the panel
   */
  void Initialize();

  /**
   * @brief Render the palette controls UI
   * @return Status of the render operation
   */
  absl::Status Update();

 private:
  /**
   * @brief Draw quick preset buttons
   */
  void DrawPresets();

  /**
   * @brief Draw palette group selection
   */
  void DrawPaletteGroupSelector();

  /**
   * @brief Draw the current palette display
   */
  void DrawPaletteDisplay();

  /**
   * @brief Draw apply buttons
   */
  void DrawApplyButtons();

  /**
   * @brief Apply current palette to specified sheet
   */
  void ApplyPaletteToSheet(uint16_t sheet_id);

  /**
   * @brief Apply current palette to all active sheets
   */
  void ApplyPaletteToAllSheets();

  GraphicsEditorState* state_;
  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
