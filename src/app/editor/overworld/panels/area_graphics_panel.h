#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_AREA_GRAPHICS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_AREA_GRAPHICS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class AreaGraphicsPanel
 * @brief Displays the current area's GFX sheet for preview
 *
 * Shows the graphics tileset used by the current overworld map,
 * useful for visual reference while editing.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class AreaGraphicsPanel : public EditorPanel {
 public:
  AreaGraphicsPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.area_graphics"; }
  std::string GetDisplayName() const override { return "Area Graphics"; }
  std::string GetIcon() const override { return ICON_MD_IMAGE; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  float GetPreferredWidth() const override {
    // 128px × 2.0 scale + padding = 276px
    return 128 * 2.0f + 20.0f;
  }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_AREA_GRAPHICS_PANEL_H
