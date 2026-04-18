#include "app/editor/hack/workflow/project_workflow_output_panel.h"

#include "absl/strings/str_format.h"
#include "app/editor/hack/workflow/workflow_activity_widgets.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze::editor::workflow {

void ProjectWorkflowOutputPanel::Draw(bool* p_open) {
  (void)p_open;

  const auto build_status = ContentRegistry::Context::build_workflow_status();
  const auto run_status = ContentRegistry::Context::run_workflow_status();
  const auto build_log = ContentRegistry::Context::build_workflow_log();
  const auto history = ContentRegistry::Context::workflow_history();
  const auto cancel_build =
      ContentRegistry::Context::cancel_build_workflow_callback();

  if (!build_status.visible && !run_status.visible && build_log.empty() &&
      history.empty()) {
    ImGui::TextDisabled("No workflow output available yet.");
    ImGui::TextWrapped(
        "Run a project build or output reload to populate workflow status and logs.");
    return;
  }

  if (build_status.visible) {
    DrawStatusCard(ICON_MD_BUILD, build_status);
    if (build_status.can_cancel && cancel_build) {
      if (ImGui::Button(ICON_MD_CANCEL " Cancel Build")) {
        cancel_build();
      }
    }
    ImGui::Spacing();
  }

  if (run_status.visible) {
    DrawStatusCard(ICON_MD_PLAY_ARROW, run_status);
    ImGui::Spacing();
  }

  if (!build_log.empty()) {
    ImGui::Separator();
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s Build Output",
                       ICON_MD_TERMINAL);
    ImGui::SameLine();
    DrawCopyCurrentLogButton(build_log);
    if (ImGui::BeginChild("##workflow_output_log", ImVec2(0, 240), true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      ImGui::TextUnformatted(build_log.c_str());
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }
    }
    ImGui::EndChild();
  }

  if (!history.empty()) {
    ImGui::Separator();
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s Recent History",
                       ICON_MD_HISTORY);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DELETE_SWEEP " Clear History")) {
      ContentRegistry::Context::ClearWorkflowHistory();
    }
    for (size_t i = 0; i < history.size(); ++i) {
      DrawHistoryEntry(history[i], static_cast<int>(i));
    }
  }
}

void ProjectWorkflowOutputPanel::DrawStatusCard(
    const char* fallback_icon, const ProjectWorkflowStatus& status) {
  ImGui::TextColored(WorkflowColor(status.state), "%s %s",
                     WorkflowIcon(status, fallback_icon),
                     status.summary.empty() ? status.label.c_str()
                                            : status.summary.c_str());
  if (!status.detail.empty()) {
    ImGui::TextWrapped("%s", status.detail.c_str());
  }
  if (!status.output_tail.empty()) {
    ImGui::TextWrapped("%s", status.output_tail.c_str());
  }
}

void ProjectWorkflowOutputPanel::DrawHistoryEntry(
    const ProjectWorkflowHistoryEntry& entry, int index) {
  ImGui::PushID(index);
  WorkflowActionCallbacks callbacks;
  callbacks.start_build = ContentRegistry::Context::start_build_workflow_callback();
  callbacks.run_project = ContentRegistry::Context::run_project_workflow_callback();
  const std::string title = absl::StrFormat(
      "%s %s - %s", entry.kind.c_str(), entry.status.summary.c_str(),
      FormatHistoryTime(entry.timestamp).c_str());
  if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawStatusCard(entry.kind == "Run" ? ICON_MD_PLAY_ARROW : ICON_MD_BUILD,
                   entry.status);
    DrawHistoryActionRow(entry, callbacks);

    if (!entry.output_log.empty()) {
      if (ImGui::BeginChild("##workflow_history_log", ImVec2(0, 120), true,
                            ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::TextUnformatted(entry.output_log.c_str());
      }
      ImGui::EndChild();
    }
  }
  ImGui::PopID();
}

}  // namespace yaze::editor::workflow
