#include "app/editor/system/proposal_drawer.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "imgui/imgui.h"
#include "app/gui/icons.h"

namespace yaze {
namespace editor {

ProposalDrawer::ProposalDrawer() {
  RefreshProposals();
}

void ProposalDrawer::Draw() {
  if (!visible_) return;

  // Set drawer position on the right side
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - drawer_width_, 0), 
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(drawer_width_, io.DisplaySize.y), 
                           ImGuiCond_Always);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | 
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoCollapse;

  if (ImGui::Begin("Agent Proposals", &visible_, flags)) {
    if (needs_refresh_) {
      RefreshProposals();
      needs_refresh_ = false;
    }

    // Header with refresh button
    if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
      RefreshProposals();
    }
    ImGui::SameLine();
    DrawStatusFilter();

    ImGui::Separator();

    // Split view: proposal list on top, details on bottom
    float list_height = ImGui::GetContentRegionAvail().y * 0.4f;
    
    ImGui::BeginChild("ProposalList", ImVec2(0, list_height), true);
    DrawProposalList();
    ImGui::EndChild();

    if (selected_proposal_) {
      ImGui::Separator();
      ImGui::BeginChild("ProposalDetail", ImVec2(0, 0), true);
      DrawProposalDetail();
      ImGui::EndChild();
    }
  }
  ImGui::End();

  // Confirmation dialog
  if (show_confirm_dialog_) {
    ImGui::OpenPopup("Confirm Action");
    show_confirm_dialog_ = false;
  }

  if (ImGui::BeginPopupModal("Confirm Action", nullptr, 
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure you want to %s this proposal?", 
                confirm_action_.c_str());
    ImGui::Separator();

    if (ImGui::Button("Yes", ImVec2(120, 0))) {
      if (confirm_action_ == "accept") {
        AcceptProposal(confirm_proposal_id_);
      } else if (confirm_action_ == "reject") {
        RejectProposal(confirm_proposal_id_);
      } else if (confirm_action_ == "delete") {
        DeleteProposal(confirm_proposal_id_);
      }
      ImGui::CloseCurrentPopup();
      RefreshProposals();
    }
    ImGui::SameLine();
    if (ImGui::Button("No", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void ProposalDrawer::DrawProposalList() {
  if (proposals_.empty()) {
    ImGui::TextWrapped("No proposals found.");
    ImGui::TextWrapped("Run CLI command: z3ed agent run --prompt \"...\"");
    return;
  }

  ImGuiTableFlags flags = ImGuiTableFlags_Borders | 
                         ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_ScrollY;

  if (ImGui::BeginTable("ProposalsTable", 3, flags)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Prompt", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (const auto& proposal : proposals_) {
      ImGui::TableNextRow();
      
      // ID column
      ImGui::TableSetColumnIndex(0);
      bool is_selected = (proposal.id == selected_proposal_id_);
      if (ImGui::Selectable(proposal.id.c_str(), is_selected, 
                           ImGuiSelectableFlags_SpanAllColumns)) {
        SelectProposal(proposal.id);
      }

      // Status column
      ImGui::TableSetColumnIndex(1);
      switch (proposal.status) {
        case cli::ProposalRegistry::ProposalStatus::kPending:
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Pending");
          break;
        case cli::ProposalRegistry::ProposalStatus::kAccepted:
          ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Accepted");
          break;
        case cli::ProposalRegistry::ProposalStatus::kRejected:
          ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Rejected");
          break;
      }

      // Prompt column (truncated)
      ImGui::TableSetColumnIndex(2);
      std::string truncated = proposal.prompt;
      if (truncated.length() > 30) {
        truncated = truncated.substr(0, 27) + "...";
      }
      ImGui::TextWrapped("%s", truncated.c_str());
    }

    ImGui::EndTable();
  }
}

void ProposalDrawer::DrawProposalDetail() {
  if (!selected_proposal_) return;

  const auto& p = *selected_proposal_;

  // Metadata section
  if (ImGui::CollapsingHeader("Metadata", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("ID: %s", p.id.c_str());
    ImGui::Text("Sandbox: %s", p.sandbox_id.c_str());
    ImGui::Text("Created: %s", absl::FormatTime(p.created_at).c_str());
    if (p.reviewed_at.has_value()) {
      ImGui::Text("Reviewed: %s", absl::FormatTime(*p.reviewed_at).c_str());
    }
    ImGui::Text("Commands: %d", p.commands_executed);
    ImGui::Text("Bytes Changed: %d", p.bytes_changed);
    ImGui::Separator();
    ImGui::TextWrapped("Prompt: %s", p.prompt.c_str());
    ImGui::TextWrapped("Description: %s", p.description.c_str());
  }

  // Diff section
  if (ImGui::CollapsingHeader("Diff", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (diff_content_.empty() && std::filesystem::exists(p.diff_path)) {
      std::ifstream diff_file(p.diff_path);
      if (diff_file.is_open()) {
        std::stringstream buffer;
        buffer << diff_file.rdbuf();
        diff_content_ = buffer.str();
      }
    }

    if (!diff_content_.empty()) {
      ImGui::BeginChild("DiffContent", ImVec2(0, 150), true, 
                       ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::TextUnformatted(diff_content_.c_str());
      ImGui::EndChild();
    } else {
      ImGui::TextWrapped("No diff available");
    }
  }

  // Log section
  if (ImGui::CollapsingHeader("Execution Log")) {
    if (log_content_.empty() && std::filesystem::exists(p.log_path)) {
      std::ifstream log_file(p.log_path);
      if (log_file.is_open()) {
        std::stringstream buffer;
        std::string line;
        int line_count = 0;
        while (std::getline(log_file, line) && line_count < log_display_lines_) {
          buffer << line << "\n";
          line_count++;
        }
        if (line_count >= log_display_lines_) {
          buffer << "... (truncated, see " << p.log_path.string() << ")\n";
        }
        log_content_ = buffer.str();
      }
    }

    if (!log_content_.empty()) {
      ImGui::BeginChild("LogContent", ImVec2(0, 150), true, 
                       ImGuiWindowFlags_HorizontalScrollbar);
      ImGui::TextUnformatted(log_content_.c_str());
      ImGui::EndChild();
    } else {
      ImGui::TextWrapped("No log available");
    }
  }

  // Action buttons
  ImGui::Separator();
  DrawActionButtons();
}

void ProposalDrawer::DrawStatusFilter() {
  const char* filter_labels[] = {"All", "Pending", "Accepted", "Rejected"};
  int current_filter = static_cast<int>(status_filter_);

  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::Combo("Filter", &current_filter, filter_labels, 4)) {
    status_filter_ = static_cast<StatusFilter>(current_filter);
    RefreshProposals();
  }
}

void ProposalDrawer::DrawActionButtons() {
  if (!selected_proposal_) return;

  const auto& p = *selected_proposal_;
  bool is_pending = p.status == cli::ProposalRegistry::ProposalStatus::kPending;

  // Accept button (only for pending proposals)
  if (is_pending) {
    if (ImGui::Button(ICON_MD_CHECK " Accept", ImVec2(-1, 0))) {
      confirm_action_ = "accept";
      confirm_proposal_id_ = p.id;
      show_confirm_dialog_ = true;
    }

    // Reject button (only for pending proposals)
    if (ImGui::Button(ICON_MD_CLOSE " Reject", ImVec2(-1, 0))) {
      confirm_action_ = "reject";
      confirm_proposal_id_ = p.id;
      show_confirm_dialog_ = true;
    }
  }

  // Delete button (for all proposals)
  if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(-1, 0))) {
    confirm_action_ = "delete";
    confirm_proposal_id_ = p.id;
    show_confirm_dialog_ = true;
  }
}

void ProposalDrawer::RefreshProposals() {
  auto& registry = cli::ProposalRegistry::Instance();
  
  std::optional<cli::ProposalRegistry::ProposalStatus> filter;
  switch (status_filter_) {
    case StatusFilter::kPending:
      filter = cli::ProposalRegistry::ProposalStatus::kPending;
      break;
    case StatusFilter::kAccepted:
      filter = cli::ProposalRegistry::ProposalStatus::kAccepted;
      break;
    case StatusFilter::kRejected:
      filter = cli::ProposalRegistry::ProposalStatus::kRejected;
      break;
    case StatusFilter::kAll:
      filter = std::nullopt;
      break;
  }

  proposals_ = registry.ListProposals(filter);

  // Clear selection if proposal no longer exists
  if (!selected_proposal_id_.empty()) {
    bool found = false;
    for (const auto& p : proposals_) {
      if (p.id == selected_proposal_id_) {
        found = true;
        break;
      }
    }
    if (!found) {
      selected_proposal_id_.clear();
      selected_proposal_ = nullptr;
      diff_content_.clear();
      log_content_.clear();
    }
  }
}

void ProposalDrawer::SelectProposal(const std::string& proposal_id) {
  selected_proposal_id_ = proposal_id;
  selected_proposal_ = nullptr;
  diff_content_.clear();
  log_content_.clear();

  // Find the proposal in our list
  for (auto& p : proposals_) {
    if (p.id == proposal_id) {
      selected_proposal_ = &p;
      break;
    }
  }
}

absl::Status ProposalDrawer::AcceptProposal(const std::string& proposal_id) {
  auto& registry = cli::ProposalRegistry::Instance();
  auto status = registry.UpdateStatus(
      proposal_id, cli::ProposalRegistry::ProposalStatus::kAccepted);
  
  if (status.ok()) {
    // TODO: Merge changes into main ROM
    // This will require integration with the ROM editor
  }
  
  needs_refresh_ = true;
  return status;
}

absl::Status ProposalDrawer::RejectProposal(const std::string& proposal_id) {
  auto& registry = cli::ProposalRegistry::Instance();
  auto status = registry.UpdateStatus(
      proposal_id, cli::ProposalRegistry::ProposalStatus::kRejected);
  
  needs_refresh_ = true;
  return status;
}

absl::Status ProposalDrawer::DeleteProposal(const std::string& proposal_id) {
  auto& registry = cli::ProposalRegistry::Instance();
  auto status = registry.RemoveProposal(proposal_id);
  
  if (proposal_id == selected_proposal_id_) {
    selected_proposal_id_.clear();
    selected_proposal_ = nullptr;
    diff_content_.clear();
    log_content_.clear();
  }
  
  needs_refresh_ = true;
  return status;
}

}  // namespace editor
}  // namespace yaze
