#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class Tile8SelectorPanel
 * @brief Low-level 8x8 tile editing interface
 */
class Tile8SelectorPanel : public EditorPanel {
 public:
  explicit Tile8SelectorPanel(OverworldEditor* editor) : editor_(editor) {}

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

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_TILE8_SELECTOR_PANEL_H
