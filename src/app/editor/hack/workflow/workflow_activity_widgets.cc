#include "app/editor/hack/workflow/workflow_activity_widgets.h"

#include <algorithm>
#include <ctime>

#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"

namespace yaze::editor::workflow {
namespace {

WorkflowButtonRect LastItemRect() {
  WorkflowButtonRect rect;
  rect.visible = true;
  rect.id = ImGui::GetItemID();
  rect.min = ImGui::GetItemRectMin();
  rect.max = ImGui::GetItemRectMax();
  return rect;
}

bool DrawWorkflowActionButton(const char* button_label, const char* tooltip,
                              const std::function<void()>& callback,
                              WorkflowButtonRect* rect) {
  if (!callback) {
    return false;
  }
  if (ImGui::SmallButton(button_label)) {
    if (rect != nullptr) {
      *rect = LastItemRect();
    }
    callback();
    return true;
  }
  if (rect != nullptr) {
    *rect = LastItemRect();
  }
  ImGui::SetItemTooltip("%s", tooltip);
  return false;
}

}  // namespace

std::string FormatHistoryTime(
    std::chrono::system_clock::time_point timestamp) {
  const std::time_t raw = std::chrono::system_clock::to_time_t(timestamp);
  std::tm local_tm{};
#if defined(_WIN32)
  localtime_s(&local_tm, &raw);
#else
  localtime_r(&raw, &local_tm);
#endif
  char buffer[64];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
  return std::string(buffer);
}

ImVec4 WorkflowColor(ProjectWorkflowState state) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  switch (state) {
    case ProjectWorkflowState::kRunning:
      return gui::ConvertColorToImVec4(theme.info);
    case ProjectWorkflowState::kSuccess:
      return gui::ConvertColorToImVec4(theme.success);
    case ProjectWorkflowState::kFailure:
      return gui::ConvertColorToImVec4(theme.error);
    case ProjectWorkflowState::kIdle:
    default:
      return gui::GetTextSecondaryVec4();
  }
}

const char* WorkflowIcon(const ProjectWorkflowStatus& status,
                         const char* fallback_icon) {
  switch (status.state) {
    case ProjectWorkflowState::kRunning:
      return ICON_MD_SYNC;
    case ProjectWorkflowState::kSuccess:
      return ICON_MD_CHECK_CIRCLE;
    case ProjectWorkflowState::kFailure:
      return ICON_MD_ERROR;
    case ProjectWorkflowState::kIdle:
    default:
      return fallback_icon;
  }
}

std::string BuildCopyPayload(const ProjectWorkflowStatus& status,
                             const std::string& output_log) {
  if (!output_log.empty()) {
    return output_log;
  }

  std::string payload = status.summary.empty() ? status.label : status.summary;
  if (!status.detail.empty()) {
    if (!payload.empty()) {
      payload.append("\n");
    }
    payload.append(status.detail);
  }
  if (!status.output_tail.empty()) {
    if (!payload.empty()) {
      payload.append("\n");
    }
    payload.append(status.output_tail);
  }
  return payload;
}

WorkflowButtonRect DrawCopyCurrentLogButton(const std::string& build_log) {
  WorkflowButtonRect rect;
  if (build_log.empty()) {
    return rect;
  }
  if (ImGui::SmallButton("Copy Current Log##workflow_copy_current_log")) {
    rect = LastItemRect();
    ImGui::SetClipboardText(build_log.c_str());
    return rect;
  }
  rect = LastItemRect();
  return rect;
}

WorkflowActionRowResult DrawHistoryActionRow(
    const ProjectWorkflowHistoryEntry& entry,
    const WorkflowActionCallbacks& callbacks,
    const WorkflowActionRowOptions& options) {
  WorkflowActionRowResult result;
  bool drew_action = false;

  if (options.show_open_output && callbacks.show_output) {
    drew_action = true;
    DrawWorkflowActionButton(
        "Open Output##workflow_open_output",
        "Open the Workflow Output panel", callbacks.show_output,
        &result.open_output);
  }

  if (entry.kind == "Build" && callbacks.start_build) {
    if (drew_action) {
      ImGui::SameLine();
    }
    drew_action = true;
    DrawWorkflowActionButton(
        "Rebuild##workflow_rebuild", "Rebuild project",
        callbacks.start_build, &result.primary_action);
  } else if (entry.kind == "Run" && callbacks.run_project) {
    if (drew_action) {
      ImGui::SameLine();
    }
    drew_action = true;
    DrawWorkflowActionButton(
        "Run Again##workflow_run_again",
        "Run project output again", callbacks.run_project,
        &result.primary_action);
  }

  const std::string copy_payload =
      BuildCopyPayload(entry.status, entry.output_log);
  if (options.show_copy_log && !copy_payload.empty()) {
    if (drew_action) {
      ImGui::SameLine();
    }
    if (ImGui::SmallButton("Copy Log##workflow_copy_log")) {
      result.copy_log = LastItemRect();
      ImGui::SetClipboardText(copy_payload.c_str());
    } else {
      result.copy_log = LastItemRect();
    }
  }

  return result;
}

std::vector<ProjectWorkflowHistoryEntry> SelectWorkflowPreviewEntries(
    const std::vector<ProjectWorkflowHistoryEntry>& history,
    size_t max_entries) {
  if (max_entries == 0 || history.empty()) {
    return {};
  }
  const size_t count = std::min(history.size(), max_entries);
  return std::vector<ProjectWorkflowHistoryEntry>(history.begin(),
                                                  history.begin() + count);
}

}  // namespace yaze::editor::workflow
