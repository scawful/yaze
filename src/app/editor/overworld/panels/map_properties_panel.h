#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class MapPropertiesPanel
 * @brief Displays and edits properties for the current overworld map
 *
 * Shows settings like palette selection, graphics groups, large/small map,
 * parent map, message ID, and other per-map configuration.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class MapPropertiesPanel : public EditorPanel {
 public:
  MapPropertiesPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.properties"; }
  std::string GetDisplayName() const override { return "Map Properties"; }
  std::string GetIcon() const override { return ICON_MD_TUNE; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H
