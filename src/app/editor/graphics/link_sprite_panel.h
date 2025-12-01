#ifndef YAZE_APP_EDITOR_GRAPHICS_LINK_SPRITE_PANEL_H
#define YAZE_APP_EDITOR_GRAPHICS_LINK_SPRITE_PANEL_H

#include <array>
#include <string>

#include "absl/status/status.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/system/editor_panel.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/util/zspr_loader.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"

namespace yaze {

class Rom;

namespace editor {

/**
 * @brief Dedicated panel for editing Link's 14 graphics sheets
 *
 * Features:
 * - Sheet thumbnail grid (4x4 layout, 14 sheets)
 * - ZSPR import support
 * - Palette switcher (Green/Blue/Red/Bunny mail)
 * - Integration with main pixel editor
 * - Reset to vanilla option
 */
class LinkSpritePanel : public EditorPanel {
 public:
  static constexpr int kNumLinkSheets = 14;

  /**
   * @brief Link sprite palette types
   */
  enum class PaletteType {
    kGreenMail = 0,
    kBlueMail = 1,
    kRedMail = 2,
    kBunny = 3
  };

  LinkSpritePanel(GraphicsEditorState* state, Rom* rom);

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "graphics.link_sprite"; }
  std::string GetDisplayName() const override { return "Link Sprite"; }
  std::string GetIcon() const override { return ICON_MD_PERSON; }
  std::string GetEditorCategory() const override { return "Graphics"; }
  int GetPriority() const override { return 40; }

  // ==========================================================================
  // EditorPanel Lifecycle
  // ==========================================================================

  /**
   * @brief Initialize the panel and load Link sheets
   */
  void Initialize();

  /**
   * @brief Draw the panel UI (EditorPanel interface)
   */
  void Draw(bool* p_open) override;

  /**
   * @brief Legacy Update method for backward compatibility
   * @return Status of the render operation
   */
  absl::Status Update();

  /**
   * @brief Check if the panel has unsaved changes
   */
  bool HasUnsavedChanges() const { return has_unsaved_changes_; }

 private:
  /**
   * @brief Draw the toolbar with Import/Reset buttons
   */
  void DrawToolbar();

  /**
   * @brief Draw the 4x4 sheet selection grid
   */
  void DrawSheetGrid();

  /**
   * @brief Draw a single Link sheet thumbnail
   */
  void DrawSheetThumbnail(int sheet_index);

  /**
   * @brief Draw the preview canvas for selected sheet
   */
  void DrawPreviewCanvas();

  /**
   * @brief Draw the palette selector dropdown
   */
  void DrawPaletteSelector();

  /**
   * @brief Draw info panel with stats
   */
  void DrawInfoPanel();

  /**
   * @brief Handle ZSPR file import
   */
  void ImportZspr();

  /**
   * @brief Reset Link sheets to vanilla ROM data
   */
  void ResetToVanilla();

  /**
   * @brief Open selected sheet in the main pixel editor
   */
  void OpenSheetInPixelEditor();

  /**
   * @brief Load Link graphics sheets from ROM
   */
  absl::Status LoadLinkSheets();

  /**
   * @brief Apply the selected palette to Link sheets for display
   */
  void ApplySelectedPalette();

  /**
   * @brief Get the name of a palette type
   */
  static const char* GetPaletteName(PaletteType type);

  GraphicsEditorState* state_;
  Rom* rom_;

  // Link sheets loaded from ROM
  std::array<gfx::Bitmap, kNumLinkSheets> link_sheets_;
  bool sheets_loaded_ = false;

  // UI state
  int selected_sheet_ = 0;
  PaletteType selected_palette_ = PaletteType::kGreenMail;
  bool has_unsaved_changes_ = false;

  // Preview canvas
  gui::Canvas preview_canvas_;
  float preview_zoom_ = 4.0f;

  // Currently loaded ZSPR (if any)
  std::optional<gfx::ZsprData> loaded_zspr_;

  // Thumbnail size
  static constexpr float kThumbnailSize = 64.0f;
  static constexpr float kThumbnailPadding = 4.0f;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_LINK_SPRITE_PANEL_H
