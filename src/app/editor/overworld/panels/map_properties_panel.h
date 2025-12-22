#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class MapPropertiesPanel
 * @brief Displays and edits properties for the current overworld map
 * 
 * Shows settings like palette selection, graphics groups, large/small map,
 * parent map, message ID, and other per-map configuration.
 */
class MapPropertiesPanel : public EditorPanel {
 public:
  explicit MapPropertiesPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.properties"; }
  std::string GetDisplayName() const override { return "Map Properties"; }
  std::string GetIcon() const override { return ICON_MD_TUNE; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_MAP_PROPERTIES_PANEL_H
