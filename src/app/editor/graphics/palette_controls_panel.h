#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// Shared by the pixel strip and the existing Palette Controls panel.
void DrawGraphicsPreviewPaletteSelector(GraphicsEditorState& state);

/**
 * @brief Panel for managing palettes applied to graphics sheets
 *
 * Provides palette group selection, quick presets, and
 * apply-to-sheet functionality.
 */
class PaletteControlsPanel : public WindowContent {
 public:
  explicit PaletteControlsPanel(GraphicsEditorState* state) : state_(state) {}

  // ==========================================================================
  // WindowContent Identity
  // ==========================================================================

  std::string GetId() const override { return "graphics.palette_controls"; }
  std::string GetDisplayName() const override { return "Palette Controls"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 30; }

  // ==========================================================================
  // WindowContent Lifecycle
  // ==========================================================================

  /**
   * @brief Draw the palette controls UI (WindowContent interface)
   */
  void Draw(bool* p_open) override;

  /**
   * @brief Legacy Update method for backward compatibility
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

  GraphicsEditorState* state_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
