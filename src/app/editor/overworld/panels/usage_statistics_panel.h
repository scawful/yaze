#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

class OverworldEditor;

/**
 * @class UsageStatisticsPanel  
 * @brief Displays tile usage statistics across all overworld maps
 * 
 * Analyzes and shows which tiles are used most frequently,
 * helping identify patterns and potential optimization opportunities.
 */
class UsageStatisticsPanel : public EditorPanel {
 public:
  explicit UsageStatisticsPanel(OverworldEditor* editor) : editor_(editor) {}

  // EditorPanel interface
  std::string GetId() const override { return "overworld.usage_stats"; }
  std::string GetDisplayName() const override { return "Usage Statistics"; }
  std::string GetIcon() const override { return ICON_MD_ANALYTICS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;

 private:
  OverworldEditor* editor_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H
