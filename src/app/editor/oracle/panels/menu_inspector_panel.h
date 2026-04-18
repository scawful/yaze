#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_MENU_INSPECTOR_PANEL_H_
#define YAZE_APP_EDITOR_ORACLE_PANELS_MENU_INSPECTOR_PANEL_H_

#include <string>
#include <vector>

#include "app/editor/core/content_registry.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/oracle_menu_registry.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze::editor {

class OracleMenuInspectorPanel : public WindowContent {
 public:
  std::string GetId() const override { return "oracle.menu_inspector"; }
  std::string GetDisplayName() const override { return "Menu Inspector"; }
  std::string GetIcon() const override { return ICON_MD_MENU_BOOK; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Assets"; }
  std::string GetWorkflowDescription() const override {
    return "Inspect menu registries and generated menu assets for the project";
  }
  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::CrossEditor;
  }
  float GetPreferredWidth() const override { return 520.0f; }

  void Draw(bool* p_open) override;

 private:
  void EnsureProjectPath();
  void RefreshRegistry();
  void DrawToolbar();
  void DrawSummary() const;
  void DrawBinsTab();
  void DrawDrawRoutinesTab();
  void DrawComponentsTab();
  bool MatchesFilter(const std::string& value, const std::string& filter) const;

  std::string project_path_;
  bool attempted_initial_load_ = false;
  bool has_registry_ = false;
  core::OracleMenuRegistry registry_;
  std::string last_error_;
  std::string status_message_;

  bool bins_missing_only_ = false;
  std::string draw_filter_;
  std::string component_table_filter_;

  std::vector<const core::OracleMenuComponent*> filtered_components_;
  int selected_component_list_index_ = -1;
  int edit_row_ = 0;
  int edit_col_ = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_MENU_INSPECTOR_PANEL_H_
