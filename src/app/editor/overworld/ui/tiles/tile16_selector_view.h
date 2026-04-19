#ifndef YAZE_APP_EDITOR_OVERWORLD_UI_TILES_TILE16_SELECTOR_VIEW_H
#define YAZE_APP_EDITOR_OVERWORLD_UI_TILES_TILE16_SELECTOR_VIEW_H

#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class Tile16SelectorView
 * @brief Displays the Tile16 palette for painting tiles on the overworld
 *
 * Provides a visual tile selector showing all available 16x16 tiles
 * that can be placed on the current overworld map.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class Tile16SelectorView : public WindowContent {
 public:
  Tile16SelectorView() = default;

  // WindowContent interface
  std::string GetId() const override { return "overworld.tile16_selector"; }
  std::string GetDisplayName() const override { return "Tile16 Selector"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  float GetPreferredWidth() const override {
    // 8 tiles × 16px × 2.0 scale, scroll gutter, and enough width for the
    // jump/range controls before they wrap.
    return 332.0f;
  }
  bool PreferAutoHideTabBar() const override { return true; }
  bool IsVisibleByDefault() const override { return true; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_UI_TILES_TILE16_SELECTOR_VIEW_H
