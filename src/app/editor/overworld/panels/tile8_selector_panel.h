#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class Tile8SelectorPanel
 * @brief Low-level 8x8 tile editing interface
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class Tile8SelectorPanel : public EditorPanel {
 public:
  Tile8SelectorPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.tile8_selector"; }
  std::string GetDisplayName() const override { return "Tile8 Selector"; }
  std::string GetIcon() const override { return ICON_MD_GRID_3X3; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  float GetPreferredWidth() const override {
    // Graphics bin width (256px) + padding = 276px
    return 256.0f + 20.0f;
  }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H
