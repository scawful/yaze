#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace editor {

/**
 * @brief Panel for managing palettes applied to graphics sheets
 *
 * Provides palette group selection, quick presets, and
 * apply-to-sheet functionality.
 */
class PaletteControlsPanel : public EditorPanel {
 public:
  explicit PaletteControlsPanel(GraphicsEditorState* state, Rom* rom,
                                zelda3::GameData* game_data = nullptr)
      : state_(state), rom_(rom), game_data_(game_data) {}
  
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "graphics.palette_controls"; }
  std::string GetDisplayName() const override { return "Palette Controls"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 30; }

  // ==========================================================================
  // EditorPanel Lifecycle
  // ==========================================================================

  /**
   * @brief Initialize the panel
   */
  void Initialize();

  /**
   * @brief Draw the palette controls UI (EditorPanel interface)
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
  zelda3::GameData* game_data_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_H
