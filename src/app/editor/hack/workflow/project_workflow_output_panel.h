#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_PROJECT_WORKFLOW_OUTPUT_PANEL_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_PROJECT_WORKFLOW_OUTPUT_PANEL_H_

#include <string>

#include "app/editor/core/content_registry.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze::editor::workflow {

class ProjectWorkflowOutputPanel : public WindowContent {
 public:
  std::string GetId() const override { return "workflow.output"; }
  std::string GetDisplayName() const override { return "Workflow Output"; }
  std::string GetIcon() const override { return ICON_MD_TERMINAL; }
  std::string GetEditorCategory() const override { return "Agent"; }
  WindowScope GetScope() const override { return WindowScope::kGlobal; }
  std::string GetWorkflowGroup() const override { return "Build & Run"; }
  std::string GetWorkflowDescription() const override {
    return "Review build/run status and the latest workflow output log";
  }
  int GetWorkflowPriority() const override { return 15; }
  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::CrossEditor;
  }

  void Draw(bool* p_open) override;

 private:
  void DrawStatusCard(const char* fallback_icon,
                      const ProjectWorkflowStatus& status);
  void DrawHistoryEntry(const ProjectWorkflowHistoryEntry& entry, int index);
};

}  // namespace yaze::editor::workflow

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_PROJECT_WORKFLOW_OUTPUT_PANEL_H_
