#ifndef YAZE_APP_EDITOR_HACK_WORKFLOW_WORKFLOW_ACTIVITY_WIDGETS_H_
#define YAZE_APP_EDITOR_HACK_WORKFLOW_WORKFLOW_ACTIVITY_WIDGETS_H_

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "app/editor/system/project_workflow_status.h"
#include "imgui/imgui.h"

namespace yaze::editor::workflow {

struct WorkflowActionCallbacks {
  std::function<void()> start_build;
  std::function<void()> run_project;
  std::function<void()> show_output;
  std::function<void()> cancel_build;
};

struct WorkflowButtonRect {
  bool visible = false;
  ImGuiID id = 0;
  ImVec2 min = ImVec2(0.0f, 0.0f);
  ImVec2 max = ImVec2(0.0f, 0.0f);

  ImVec2 Center() const {
    return ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
  }
};

struct WorkflowActionRowOptions {
  bool show_open_output = false;
  bool show_copy_log = true;
};

struct WorkflowActionRowResult {
  WorkflowButtonRect open_output;
  WorkflowButtonRect primary_action;
  WorkflowButtonRect copy_log;
};

std::string FormatHistoryTime(
    std::chrono::system_clock::time_point timestamp);
ImVec4 WorkflowColor(ProjectWorkflowState state);
const char* WorkflowIcon(const ProjectWorkflowStatus& status,
                         const char* fallback_icon);
std::string BuildCopyPayload(const ProjectWorkflowStatus& status,
                             const std::string& output_log);
WorkflowButtonRect DrawCopyCurrentLogButton(const std::string& build_log);
WorkflowActionRowResult DrawHistoryActionRow(
    const ProjectWorkflowHistoryEntry& entry,
    const WorkflowActionCallbacks& callbacks,
    const WorkflowActionRowOptions& options = {});
std::vector<ProjectWorkflowHistoryEntry> SelectWorkflowPreviewEntries(
    const std::vector<ProjectWorkflowHistoryEntry>& history,
    size_t max_entries);

}  // namespace yaze::editor::workflow

#endif  // YAZE_APP_EDITOR_HACK_WORKFLOW_WORKFLOW_ACTIVITY_WIDGETS_H_
