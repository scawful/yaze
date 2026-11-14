#ifndef YAZE_APP_EDITOR_SHELL_WINDOWS_PROPOSAL_WORKSPACE_PANEL_H_
#define YAZE_APP_EDITOR_SHELL_WINDOWS_PROPOSAL_WORKSPACE_PANEL_H_

#include <array>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/proposal_workspace.h"

namespace yaze::editor {

class ProposalWorkspacePanel : public WindowContent {
 public:
  using ProjectPath = std::function<std::string()>;
  using SaveDraft = std::function<absl::Status()>;
  using EditTarget = std::function<void(const core::ProposalWorkspace&)>;
  using OpenProposal = std::function<absl::Status()>;
  ProposalWorkspacePanel(ProjectPath path, SaveDraft save, EditTarget edit,
                         OpenProposal open)
      : project_path_(std::move(path)),
        save_(std::move(save)),
        edit_(std::move(edit)),
        open_(std::move(open)) {}
  std::string GetId() const override { return "proposal.workbench"; }
  std::string GetDisplayName() const override { return "Proposal"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Project"; }
  WindowScope GetScope() const override { return WindowScope::kGlobal; }
  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::CrossEditor;
  }
  WindowPresentationPolicy GetPresentationPolicy() const override {
    return WindowPresentationPolicy::OptionalPopOut();
  }
  void Draw(bool* p_open) override;

 private:
  ProjectPath project_path_;
  SaveDraft save_;
  EditTarget edit_;
  OpenProposal open_;
  std::string loaded_project_;
  std::string error_;
  std::string result_;
  core::ProposalWorkspace workspace_;
  std::map<std::string, std::array<char, 4096>> notes_;
};

}  // namespace yaze::editor
#endif  // YAZE_APP_EDITOR_SHELL_WINDOWS_PROPOSAL_WORKSPACE_PANEL_H_
