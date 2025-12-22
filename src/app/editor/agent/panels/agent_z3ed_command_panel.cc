#include "app/editor/agent/panels/agent_z3ed_command_panel.h"

#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentZ3EDCommandPanel::Draw(AgentUIContext* context,
                                 const Z3EDCommandCallbacks& callbacks,
                                 ToastManager* toast_manager) {
  auto& state = context->z3ed_command_state();

  ImGui::PushID("Z3EDCmdPanel");
  ImVec4 command_color = ImVec4(1.0f, 0.647f, 0.0f, 1.0f);

  // Dense header (no collapsing)
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.12f, 0.18f, 0.95f));
  ImGui::BeginChild("Z3ED_CommandsChild", ImVec2(0, 100), true);

  ImGui::TextColored(command_color, ICON_MD_TERMINAL " Commands");
  ImGui::Separator();

  ImGui::SetNextItemWidth(-60);
  ImGui::InputTextWithHint(
      "##z3ed_cmd", "Command...", state.command_input_buffer,
      IM_ARRAYSIZE(state.command_input_buffer));
  ImGui::SameLine();
  ImGui::BeginDisabled(state.command_running);
  if (ImGui::Button(ICON_MD_PLAY_ARROW "##z3ed_run", ImVec2(50, 0))) {
    if (callbacks.run_agent_task) {
      std::string command = state.command_input_buffer;
      state.command_running = true;
      auto status = callbacks.run_agent_task(command);
      state.command_running = false;
      if (status.ok() && toast_manager) {
        toast_manager->Show("Task started", ToastType::kSuccess, 2.0f);
      }
    }
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Run command");
  }

  // Compact action buttons (inline)
  if (ImGui::SmallButton(ICON_MD_PREVIEW)) {
    if (callbacks.list_proposals) {
      auto result = callbacks.list_proposals();
      if (result.ok()) {
        const auto& proposals = *result;
        state.command_output = absl::StrJoin(proposals, "\n");
      }
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("List");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_DIFFERENCE)) {
    if (callbacks.diff_proposal) {
      auto result = callbacks.diff_proposal("");
      if (result.ok())
        state.command_output = *result;
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Diff");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_CHECK)) {
    if (callbacks.accept_proposal) {
      callbacks.accept_proposal("");
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Accept");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_CLOSE)) {
    if (callbacks.reject_proposal) {
      callbacks.reject_proposal("");
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Reject");

  if (!state.command_output.empty()) {
    ImGui::Separator();
    ImGui::TextDisabled(
        "%s", state.command_output.substr(0, 100).c_str());
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();

  ImGui::PopID();
}

}  // namespace editor
}  // namespace yaze
