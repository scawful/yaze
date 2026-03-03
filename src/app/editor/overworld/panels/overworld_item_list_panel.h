#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_ITEM_LIST_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_ITEM_LIST_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class OverworldItemListPanel
 * @brief Filterable list view for overworld items with quick selection/actions.
 */
class OverworldItemListPanel : public EditorPanel {
 public:
  OverworldItemListPanel() = default;

  std::string GetId() const override { return "overworld.item_list"; }
  std::string GetDisplayName() const override { return "Item List"; }
  std::string GetIcon() const override { return ICON_MD_LIST; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  std::string GetShortcutHint() const override { return "Ctrl+Shift+I"; }
  int GetPriority() const override { return 26; }
  float GetPreferredWidth() const override { return 420.0f; }

  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_OVERWORLD_ITEM_LIST_PANEL_H
