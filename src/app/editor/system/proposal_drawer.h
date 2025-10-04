#ifndef YAZE_APP_EDITOR_SYSTEM_PROPOSAL_DRAWER_H
#define YAZE_APP_EDITOR_SYSTEM_PROPOSAL_DRAWER_H

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/planning/proposal_registry.h"

namespace yaze {
class Rom;
}

namespace yaze {
namespace editor {

/**
 * @class ProposalDrawer
 * @brief ImGui drawer for displaying and managing agent proposals
 * 
 * Provides a UI for reviewing agent-generated ROM modification proposals,
 * including:
 * - List of all proposals with status indicators
 * - Detailed view of selected proposal (metadata, diff, logs)
 * - Accept/Reject controls
 * - Filtering by status (Pending/Accepted/Rejected)
 * 
 * Integrates with the CLI ProposalRegistry service to enable
 * human-in-the-loop review of agentic modifications.
 */
class ProposalDrawer {
 public:
  ProposalDrawer();
  ~ProposalDrawer() = default;

  // Set the ROM instance to merge proposals into
  void SetRom(Rom* rom) { rom_ = rom; }

  // Render the proposal drawer UI
  void Draw();

  // Show/hide the drawer
  void Show() { visible_ = true; }
  void Hide() { visible_ = false; }
  void Toggle() { visible_ = !visible_; }
  bool IsVisible() const { return visible_; }
  void FocusProposal(const std::string& proposal_id);

 private:
  void DrawProposalList();
  void DrawProposalDetail();
  void DrawPolicyStatus();  // NEW: Display policy evaluation results
  void DrawStatusFilter();
  void DrawActionButtons();
  
  absl::Status AcceptProposal(const std::string& proposal_id);
  absl::Status RejectProposal(const std::string& proposal_id);
  absl::Status DeleteProposal(const std::string& proposal_id);
  
  void RefreshProposals();
  void SelectProposal(const std::string& proposal_id);
  
  bool visible_ = false;
  bool needs_refresh_ = true;
  
  // Filter state
  enum class StatusFilter {
    kAll,
    kPending,
    kAccepted,
    kRejected
  };
  StatusFilter status_filter_ = StatusFilter::kAll;
  
  // Proposal state
  std::vector<cli::ProposalRegistry::ProposalMetadata> proposals_;
  std::string selected_proposal_id_;
  cli::ProposalRegistry::ProposalMetadata* selected_proposal_ = nullptr;
  
  // Diff display state
  std::string diff_content_;
  std::string log_content_;
  int log_display_lines_ = 50;
  
  // UI state
  float drawer_width_ = 400.0f;
  bool show_confirm_dialog_ = false;
  bool show_override_dialog_ = false;  // NEW: Policy override confirmation
  std::string confirm_action_;
  std::string confirm_proposal_id_;
  
  // ROM reference for merging
  Rom* rom_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PROPOSAL_DRAWER_H
