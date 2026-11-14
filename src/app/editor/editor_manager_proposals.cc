#include "app/editor/editor_manager.h"

#include <filesystem>

#include "app/application.h"
#include "app/editor/shell/windows/proposal_workspace_panel.h"
#include "core/native_save_receipt.h"
#include "core/proposal_workspace.h"
#include "util/file_util.h"
#include "util/macro.h"

namespace yaze::editor {
namespace {
AppConfig ProposalStartup(const core::ProposalWorkspace& proposal) {
  AppConfig config;
  config.startup_editor =
      proposal.domain == "dungeon" ? "Dungeon" : "Overworld";
  config.jump_to_room = proposal.domain == "dungeon" ? proposal.target : -1;
  config.jump_to_map = proposal.domain == "overworld" ? proposal.target : -1;
  config.graphics_entrance = proposal.graphics_entrance_slot;
  config.open_panels = {"proposal.workbench"};
  return config;
}
}  // namespace

void EditorManager::InitializeProposalWorkspace() {
  window_manager_.RegisterWindowContent(
      std::make_unique<ProposalWorkspacePanel>(
          [this]() { return current_project_.filepath; },
          [this]() { return SaveProposalDraft(); },
          [this](const core::ProposalWorkspace& proposal) {
            ProcessStartupActions(ProposalStartup(proposal));
          },
          [this]() -> absl::Status {
            if (HasAnySessionPendingUnsavedWork() ||
                HasPendingUnsavedSessionAction())
              return absl::FailedPreconditionError(
                  "Save or close pending edits before opening a proposal");
            const auto selected = util::FileDialogWrapper::ShowOpenFileDialog(
                util::FileDialogOptions{{{"Proposal packet", "json"}}});
            if (selected.empty())
              return absl::CancelledError("Open cancelled");
            if (std::filesystem::path(selected).filename() != "proposal.json")
              return absl::InvalidArgumentError(
                  "Select the proposal.json packet");
            ASSIGN_OR_RETURN(
                auto proposal,
                core::LoadProposalWorkspace(
                    std::filesystem::path(selected).parent_path()));
            RETURN_IF_ERROR(OpenRomOrProjectInternal(
                proposal.project_file.string(), false));
            ProcessStartupActions(ProposalStartup(proposal));
            return absl::OkStatus();
          }));
}

absl::Status EditorManager::SaveProposalDraft() {
#if defined(__EMSCRIPTEN__) || !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "Proposal draft receipts require a native JSON-enabled editor");
#else
  CaptureActiveProjectEditingState();
  ASSIGN_OR_RETURN(auto workspace,
                   core::LoadProposalWorkspace(
                       std::filesystem::path(current_project_.filepath)
                           .parent_path()
                           .parent_path()));
  RETURN_IF_ERROR(
      core::ValidateProposalProjectBindings(current_project_, workspace));
  auto* rom = GetCurrentRom();
  if (!rom || !rom->is_loaded() || rom->filename().empty())
    return absl::FailedPreconditionError("No active proposal ROM session");
  std::error_code error;
  const auto editing_rom =
      std::filesystem::weakly_canonical(rom->filename(), error);
  if (error || editing_rom != workspace.rom_file)
    return absl::FailedPreconditionError(
        "The active ROM is not this proposal's isolated draft");
  const auto session = GetCurrentSessionId();
  if (rom_lifecycle_.HasPendingRomSave() || HasPendingUnsavedSessionAction())
    return absl::FailedPreconditionError(
        "Finish the pending save before saving proposal feedback");
  // Explicit Project Save creates this owner's receipt even for a clean clone.
  // Normal session Save also flushes ROM work for projects without source sheets.
  RETURN_IF_ERROR(SaveProject());
  RETURN_IF_ERROR(SaveCurrentSession(/*allow_dialogs=*/false));
  if (GetCurrentSessionId() != session || GetCurrentRom() != rom ||
      current_project_.filepath != workspace.project_file.string() ||
      rom_lifecycle_.HasPendingRomSave() ||
      SessionHasPendingUnsavedWork(GetCurrentSessionIndex()))
    return absl::FailedPreconditionError(
        "Proposal save did not complete in its owning session");
  RETURN_IF_ERROR(
      core::ValidateProposalProjectBindings(current_project_, workspace));
  const auto identity = core::ReadVerifiedCommittedNativeSaveIdentity(
      editing_rom.string(), workspace.project_file.string(),
      editing_rom.string());
  if (!identity.ok())
    return identity.status();
  return absl::OkStatus();
#endif
}
}  // namespace yaze::editor
