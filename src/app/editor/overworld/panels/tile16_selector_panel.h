#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_SELECTOR_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_SELECTOR_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class Tile16SelectorPanel
 * @brief Displays the Tile16 palette for painting tiles on the overworld
 * 
 * Provides a visual tile selector showing all available 16x16 tiles
 * that can be placed on the current overworld map.
 */
class Tile16SelectorPanel : public EditorPanel {
 public:
  explicit Tile16SelectorPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.tile16_selector"; }
  std::string GetDisplayName() const override { return "Tile16 Selector"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  float GetPreferredWidth() const override {
    // 8 tiles × 16px × 2.0 scale + padding = 276px
    return 8 * 16 * 2.0f + 20.0f;
  }
  bool IsVisibleByDefault() const override { return true; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE16_SELECTOR_PANEL_H
