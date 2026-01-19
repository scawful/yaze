#ifndef YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H
#define YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

/**
 * @class UsageStatisticsPanel
 * @brief Displays tile usage statistics across all overworld maps
 *
 * Analyzes and shows which tiles are used most frequently,
 * helping identify patterns and potential optimization opportunities.
 *
 * Uses ContentRegistry::Context to access the current OverworldEditor.
 * Self-registers via REGISTER_PANEL macro.
 */
class UsageStatisticsPanel : public EditorPanel {
 public:
  UsageStatisticsPanel() = default;

  // EditorPanel interface
  std::string GetId() const override { return "overworld.usage_stats"; }
  std::string GetDisplayName() const override { return "Usage Statistics"; }
  std::string GetIcon() const override { return ICON_MD_ANALYTICS; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  void Draw(bool* p_open) override;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PANELS_USAGE_STATISTICS_PANEL_H
