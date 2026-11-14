#include "app/editor/shell/windows/proposal_workspace_panel.h"

#include <filesystem>

#include "imgui/imgui.h"

namespace yaze::editor {

void ProposalWorkspacePanel::Draw(bool* p_open) {
  (void)p_open;
  if (ImGui::Button("Open proposal...")) {
    const auto opened = open_();
    result_ = opened.ok() ? "" : std::string(opened.message());
    if (opened.ok())
      loaded_project_.clear();
  }
  const auto project = project_path_();
  if (project.empty()) {
    ImGui::TextWrapped("Open an editable proposal project to begin.");
    if (!result_.empty())
      ImGui::TextWrapped("%s", result_.c_str());
    return;
  }
  if (project != loaded_project_) {
    loaded_project_ = project;
    result_.clear();
    auto workspace = core::LoadProposalWorkspace(
        std::filesystem::path(project).parent_path().parent_path());
    if (!workspace.ok()) {
      error_ = std::string(workspace.status().message());
    } else if (workspace->project_file != std::filesystem::path(project)) {
      error_ = "The open project is not this proposal's draft.";
    } else {
      workspace_ = *workspace;
      error_.clear();
    }
  }
  if (!error_.empty()) {
    ImGui::TextWrapped("%s", error_.c_str());
    if (ImGui::Button("Reload proposal"))
      loaded_project_.clear();
    return;
  }
  ImGui::TextUnformatted(workspace_.proposal_id.c_str());
  ImGui::TextDisabled("%s %03X",
                      workspace_.domain == "dungeon" ? "Room" : "Map",
                      workspace_.target);
  if (ImGui::Button("Edit target"))
    edit_(workspace_);
  ImGui::SameLine();
  if (ImGui::Button("Save draft")) {
    const auto status = save_();
    result_ = status.ok() ? "Draft saved." : std::string(status.message());
  }
  if (workspace_.unbound_references > 0) {
    ImGui::TextWrapped(
        "%d reference images still need native graphics binding.",
        workspace_.unbound_references);
  }
  auto& note = notes_[project];
  ImGui::InputTextMultiline("What changed?", note.data(), note.size(),
                            ImVec2(-1, ImGui::GetTextLineHeight() * 4));
  if (ImGui::Button("Save and export feedback")) {
    const auto saved = save_();
    if (!saved.ok()) {
      result_ = std::string(saved.message());
    } else {
      const auto exported =
          core::ExportProposalFeedback(workspace_, note.data());
      result_ = exported.ok()
                    ? "Feedback saved: " + exported->filename().string()
                    : std::string(exported.status().message());
    }
  }
  if (!result_.empty())
    ImGui::TextWrapped("%s", result_.c_str());
  if (ImGui::TreeNode("Baseline and draft")) {
    ImGui::TextWrapped("Original: %s", workspace_.baseline_rom_sha256.c_str());
    ImGui::TextWrapped("Your edits: %s",
                       workspace_.project_file.string().c_str());
    ImGui::TextWrapped(
        "Edits stay in this draft. Applying them to another project is a "
        "separate review.");
    ImGui::TreePop();
  }
}

}  // namespace yaze::editor
