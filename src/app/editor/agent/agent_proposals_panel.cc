#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/agent/agent_proposals_panel.h"

#include <fstream>
#include <sstream>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

namespace {

std::string FormatRelativeTime(absl::Time timestamp) {
  if (timestamp == absl::InfinitePast()) {
    return "—";
  }
  absl::Duration delta = absl::Now() - timestamp;
  if (delta < absl::Seconds(60)) {
    return "just now";
  }
  if (delta < absl::Minutes(60)) {
    return absl::StrFormat("%dm ago",
                           static_cast<int>(delta / absl::Minutes(1)));
  }
  if (delta < absl::Hours(24)) {
    return absl::StrFormat("%dh ago", static_cast<int>(delta / absl::Hours(1)));
  }
  return absl::FormatTime("%b %d", timestamp, absl::LocalTimeZone());
}

std::string ReadFileContents(const std::filesystem::path& path,
                             int max_lines = 100) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return "";
  }

  std::ostringstream content;
  std::string line;
  int line_count = 0;
  while (std::getline(file, line) && line_count < max_lines) {
    content << line << "\n";
    ++line_count;
  }

  if (line_count >= max_lines) {
    content << "\n... (truncated)\n";
  }

  return content.str();
}

}  // namespace

AgentProposalsPanel::AgentProposalsPanel() {
  proposals_.reserve(16);
}

void AgentProposalsPanel::SetContext(AgentUIContext* context) {
  context_ = context;
}

void AgentProposalsPanel::SetToastManager(ToastManager* toast_manager) {
  toast_manager_ = toast_manager;
}

void AgentProposalsPanel::SetRom(Rom* rom) {
  rom_ = rom;
}

void AgentProposalsPanel::SetProposalCallbacks(
    const ProposalCallbacks& callbacks) {
  proposal_callbacks_ = callbacks;
}

void AgentProposalsPanel::Draw(float available_height) {
  if (needs_refresh_) {
    RefreshProposals();
  }

  const auto& theme = AgentUI::GetTheme();

  // Status filter
  DrawStatusFilter();

  ImGui::Separator();

  // Calculate heights
  float detail_height = selected_proposal_ && !compact_mode_ ? 200.0f : 0.0f;
  float list_height =
      available_height > 0
          ? available_height - ImGui::GetCursorPosY() - detail_height
          : ImGui::GetContentRegionAvail().y - detail_height;

  // Proposal list
  {
    gui::StyledChild proposal_list("ProposalList", ImVec2(0, list_height),
                                   {.bg = theme.panel_bg_darker});
    DrawProposalList();
  }

  // Detail view (only in non-compact mode)
  if (selected_proposal_ && !compact_mode_) {
    ImGui::Separator();
    DrawProposalDetail();
  }

  // Confirmation dialog
  if (show_confirm_dialog_) {
    ImGui::OpenPopup("Confirm Action");
  }

  if (ImGui::BeginPopupModal("Confirm Action", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure you want to %s proposal %s?",
                confirm_action_.c_str(), confirm_proposal_id_.c_str());
    ImGui::Separator();

    if (ImGui::Button("Yes", ImVec2(80, 0))) {
      if (confirm_action_ == "accept") {
        AcceptProposal(confirm_proposal_id_);
      } else if (confirm_action_ == "reject") {
        RejectProposal(confirm_proposal_id_);
      } else if (confirm_action_ == "delete") {
        DeleteProposal(confirm_proposal_id_);
      }
      show_confirm_dialog_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("No", ImVec2(80, 0))) {
      show_confirm_dialog_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void AgentProposalsPanel::DrawStatusFilter() {
  if (!context_)
    return;

  auto& proposal_state = context_->proposal_state();
  const char* filter_labels[] = {"All", "Pending", "Accepted", "Rejected"};
  int current_filter = static_cast<int>(proposal_state.filter_mode);

  if (compact_mode_) {
    // Compact: just show pending count badge
    ImGui::Text("%s Proposals", ICON_MD_RULE);
    ImGui::SameLine();
    int pending = GetPendingCount();
    if (pending > 0) {
      AgentUI::StatusBadge(absl::StrFormat("%d", pending).c_str(),
                           AgentUI::ButtonColor::Warning);
    }
  } else {
    // Full: show filter buttons
    for (int i = 0; i < 4; ++i) {
      if (i > 0)
        ImGui::SameLine();
      bool selected = (current_filter == i);
      std::optional<gui::StyleColorGuard> filter_guard;
      if (selected) {
        filter_guard.emplace(ImGuiCol_Button,
                             ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
      }
      if (ImGui::SmallButton(filter_labels[i])) {
        proposal_state.filter_mode = static_cast<ProposalState::FilterMode>(i);
        needs_refresh_ = true;
      }
    }
  }
}

void AgentProposalsPanel::DrawProposalList() {
  if (proposals_.empty()) {
    ImGui::Spacing();
    ImGui::TextDisabled("No proposals found");
    return;
  }

  // Filter proposals based on current filter
  ProposalState::FilterMode filter_mode = ProposalState::FilterMode::kAll;
  if (context_) {
    filter_mode = context_->proposal_state().filter_mode;
  }

  for (const auto& proposal : proposals_) {
    // Apply filter
    if (filter_mode != ProposalState::FilterMode::kAll) {
      bool matches = false;
      switch (filter_mode) {
        case ProposalState::FilterMode::kPending:
          matches = (proposal.status ==
                     cli::ProposalRegistry::ProposalStatus::kPending);
          break;
        case ProposalState::FilterMode::kAccepted:
          matches = (proposal.status ==
                     cli::ProposalRegistry::ProposalStatus::kAccepted);
          break;
        case ProposalState::FilterMode::kRejected:
          matches = (proposal.status ==
                     cli::ProposalRegistry::ProposalStatus::kRejected);
          break;
        default:
          matches = true;
      }
      if (!matches)
        continue;
    }

    DrawProposalRow(proposal);
  }
}

void AgentProposalsPanel::DrawProposalRow(
    const cli::ProposalRegistry::ProposalMetadata& proposal) {
  const auto& theme = AgentUI::GetTheme();
  bool is_selected = (proposal.id == selected_proposal_id_);

  ImGui::PushID(proposal.id.c_str());

  // Selectable row
  if (ImGui::Selectable("##row", is_selected,
                        ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowOverlap,
                        ImVec2(0, compact_mode_ ? 24.0f : 40.0f))) {
    SelectProposal(proposal.id);
  }

  ImGui::SameLine();

  // Status icon
  ImVec4 status_color = GetStatusColor(proposal.status);
  ImGui::TextColored(status_color, "%s", GetStatusIcon(proposal.status));
  ImGui::SameLine();

  // Proposal ID and description
  if (compact_mode_) {
    ImGui::Text("%s", proposal.id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("(%d changes)", proposal.bytes_changed);
  } else {
    ImGui::BeginGroup();
    ImGui::Text("%s", proposal.id.c_str());
    ImGui::TextDisabled("%s", proposal.description.empty()
                                  ? "(no description)"
                                  : proposal.description.c_str());
    ImGui::EndGroup();

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150.0f);

    // Time and stats
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", FormatRelativeTime(proposal.created_at).c_str());
    ImGui::TextDisabled("%d bytes, %d cmds", proposal.bytes_changed,
                        proposal.commands_executed);
    ImGui::EndGroup();
  }

  // Quick actions (only for pending)
  if (proposal.status == cli::ProposalRegistry::ProposalStatus::kPending) {
    ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                    (compact_mode_ ? 60.0f : 100.0f));
    DrawQuickActions(proposal);
  }

  ImGui::PopID();
}

void AgentProposalsPanel::DrawQuickActions(
    const cli::ProposalRegistry::ProposalMetadata& proposal) {
  const auto& theme = AgentUI::GetTheme();

  gui::StyleColorGuard transparent_guard(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

  if (compact_mode_) {
    // Just accept/reject icons
    {
      gui::StyleColorGuard accept_guard(ImGuiCol_Text, theme.status_success);
      if (ImGui::SmallButton(ICON_MD_CHECK)) {
        confirm_action_ = "accept";
        confirm_proposal_id_ = proposal.id;
        show_confirm_dialog_ = true;
      }
    }

    ImGui::SameLine();

    {
      gui::StyleColorGuard reject_guard(ImGuiCol_Text, theme.status_error);
      if (ImGui::SmallButton(ICON_MD_CLOSE)) {
        confirm_action_ = "reject";
        confirm_proposal_id_ = proposal.id;
        show_confirm_dialog_ = true;
      }
    }
  } else {
    // Full buttons
    if (AgentUI::StyledButton(ICON_MD_CHECK " Accept", theme.status_success,
                              ImVec2(0, 0))) {
      confirm_action_ = "accept";
      confirm_proposal_id_ = proposal.id;
      show_confirm_dialog_ = true;
    }
    ImGui::SameLine();
    if (AgentUI::StyledButton(ICON_MD_CLOSE " Reject", theme.status_error,
                              ImVec2(0, 0))) {
      confirm_action_ = "reject";
      confirm_proposal_id_ = proposal.id;
      show_confirm_dialog_ = true;
    }
  }
}

void AgentProposalsPanel::DrawProposalDetail() {
  if (!selected_proposal_)
    return;

  const auto& theme = AgentUI::GetTheme();

  ImGui::BeginChild("ProposalDetail", ImVec2(0, 0), true);

  // Header
  ImGui::TextColored(theme.proposal_accent, "%s %s", ICON_MD_PREVIEW,
                     selected_proposal_->id.c_str());
  ImGui::SameLine();
  ImVec4 status_color = GetStatusColor(selected_proposal_->status);
  ImGui::TextColored(status_color, "(%s)",
                     GetStatusIcon(selected_proposal_->status));

  // Description
  if (!selected_proposal_->description.empty()) {
    ImGui::TextWrapped("%s", selected_proposal_->description.c_str());
  }

  ImGui::Separator();

  // Tabs for diff/log
  if (ImGui::BeginTabBar("DetailTabs")) {
    if (ImGui::BeginTabItem("Diff")) {
      DrawDiffView();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Log")) {
      DrawLogView();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  ImGui::EndChild();
}

void AgentProposalsPanel::DrawDiffView() {
  const auto& theme = AgentUI::GetTheme();

  if (diff_content_.empty() && selected_proposal_) {
    diff_content_ = ReadFileContents(selected_proposal_->diff_path, 200);
  }

  if (diff_content_.empty()) {
    ImGui::TextDisabled("No diff available");
    return;
  }

  gui::StyledChild diff_child("DiffContent", ImVec2(0, 0),
                              {.bg = theme.code_bg_color}, false,
                              ImGuiWindowFlags_HorizontalScrollbar);

  // Simple diff rendering with color highlighting
  std::istringstream stream(diff_content_);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) {
      ImGui::NewLine();
      continue;
    }
    ImVec4 line_color;
    if (line[0] == '+') {
      line_color = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
    } else if (line[0] == '-') {
      line_color = ImVec4(0.8f, 0.4f, 0.4f, 1.0f);
    } else if (line[0] == '@') {
      line_color = ImVec4(0.4f, 0.6f, 0.8f, 1.0f);
    } else {
      line_color = theme.text_secondary_color;
    }
    gui::ColoredText(line.c_str(), line_color);
  }
}

void AgentProposalsPanel::DrawLogView() {
  const auto& theme = AgentUI::GetTheme();

  if (log_content_.empty() && selected_proposal_) {
    log_content_ = ReadFileContents(selected_proposal_->log_path, 100);
  }

  if (log_content_.empty()) {
    ImGui::TextDisabled("No log available");
    return;
  }

  gui::StyledChild log_child("LogContent", ImVec2(0, 0),
                             {.bg = theme.code_bg_color}, false,
                             ImGuiWindowFlags_HorizontalScrollbar);
  if (log_child) {
    ImGui::TextUnformatted(log_content_.c_str());
  }
}

void AgentProposalsPanel::FocusProposal(const std::string& proposal_id) {
  SelectProposal(proposal_id);
  if (context_) {
    context_->proposal_state().focused_proposal_id = proposal_id;
  }
}

void AgentProposalsPanel::RefreshProposals() {
  needs_refresh_ = false;
  proposals_ = cli::ProposalRegistry::Instance().ListProposals();

  // Update context state
  if (context_) {
    auto& proposal_state = context_->proposal_state();
    proposal_state.total_proposals = static_cast<int>(proposals_.size());
    proposal_state.pending_proposals = 0;
    proposal_state.accepted_proposals = 0;
    proposal_state.rejected_proposals = 0;

    for (const auto& p : proposals_) {
      switch (p.status) {
        case cli::ProposalRegistry::ProposalStatus::kPending:
          ++proposal_state.pending_proposals;
          break;
        case cli::ProposalRegistry::ProposalStatus::kAccepted:
          ++proposal_state.accepted_proposals;
          break;
        case cli::ProposalRegistry::ProposalStatus::kRejected:
          ++proposal_state.rejected_proposals;
          break;
      }
    }
  }

  // Clear selected if it no longer exists
  if (!selected_proposal_id_.empty()) {
    bool found = false;
    for (const auto& p : proposals_) {
      if (p.id == selected_proposal_id_) {
        found = true;
        selected_proposal_ = &p;
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

int AgentProposalsPanel::GetPendingCount() const {
  int count = 0;
  for (const auto& p : proposals_) {
    if (p.status == cli::ProposalRegistry::ProposalStatus::kPending) {
      ++count;
    }
  }
  return count;
}

void AgentProposalsPanel::SelectProposal(const std::string& proposal_id) {
  if (proposal_id == selected_proposal_id_) {
    return;
  }

  selected_proposal_id_ = proposal_id;
  selected_proposal_ = nullptr;
  diff_content_.clear();
  log_content_.clear();

  for (const auto& p : proposals_) {
    if (p.id == proposal_id) {
      selected_proposal_ = &p;
      break;
    }
  }
}

const char* AgentProposalsPanel::GetStatusIcon(
    cli::ProposalRegistry::ProposalStatus status) const {
  switch (status) {
    case cli::ProposalRegistry::ProposalStatus::kPending:
      return ICON_MD_PENDING;
    case cli::ProposalRegistry::ProposalStatus::kAccepted:
      return ICON_MD_CHECK_CIRCLE;
    case cli::ProposalRegistry::ProposalStatus::kRejected:
      return ICON_MD_CANCEL;
    default:
      return ICON_MD_HELP;
  }
}

ImVec4 AgentProposalsPanel::GetStatusColor(
    cli::ProposalRegistry::ProposalStatus status) const {
  const auto& theme = AgentUI::GetTheme();
  switch (status) {
    case cli::ProposalRegistry::ProposalStatus::kPending:
      return theme.status_warning;
    case cli::ProposalRegistry::ProposalStatus::kAccepted:
      return theme.status_success;
    case cli::ProposalRegistry::ProposalStatus::kRejected:
      return theme.status_error;
    default:
      return theme.text_secondary_color;
  }
}

absl::Status AgentProposalsPanel::AcceptProposal(
    const std::string& proposal_id) {
  auto status = cli::ProposalRegistry::Instance().UpdateStatus(
      proposal_id, cli::ProposalRegistry::ProposalStatus::kAccepted);

  if (status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(absl::StrFormat("%s Proposal %s accepted",
                                           ICON_MD_CHECK_CIRCLE, proposal_id),
                           ToastType::kSuccess, 3.0f);
    }
    needs_refresh_ = true;

    // Notify via callback
    if (proposal_callbacks_.accept_proposal) {
      proposal_callbacks_.accept_proposal(proposal_id);
    }
  } else if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to accept proposal: %s", status.message()),
        ToastType::kError, 5.0f);
  }

  return status;
}

absl::Status AgentProposalsPanel::RejectProposal(
    const std::string& proposal_id) {
  auto status = cli::ProposalRegistry::Instance().UpdateStatus(
      proposal_id, cli::ProposalRegistry::ProposalStatus::kRejected);

  if (status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(absl::StrFormat("%s Proposal %s rejected",
                                           ICON_MD_CANCEL, proposal_id),
                           ToastType::kInfo, 3.0f);
    }
    needs_refresh_ = true;

    // Notify via callback
    if (proposal_callbacks_.reject_proposal) {
      proposal_callbacks_.reject_proposal(proposal_id);
    }
  } else if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to reject proposal: %s", status.message()),
        ToastType::kError, 5.0f);
  }

  return status;
}

absl::Status AgentProposalsPanel::DeleteProposal(
    const std::string& proposal_id) {
  auto status = cli::ProposalRegistry::Instance().RemoveProposal(proposal_id);

  if (status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(absl::StrFormat("%s Proposal %s deleted",
                                           ICON_MD_DELETE, proposal_id),
                           ToastType::kInfo, 3.0f);
    }
    needs_refresh_ = true;

    if (selected_proposal_id_ == proposal_id) {
      selected_proposal_id_.clear();
      selected_proposal_ = nullptr;
      diff_content_.clear();
      log_content_.clear();
    }
  } else if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Failed to delete proposal: %s", status.message()),
        ToastType::kError, 5.0f);
  }

  return status;
}

}  // namespace editor
}  // namespace yaze
