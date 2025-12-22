#ifndef YAZE_APP_EDITOR_AGENT_AGENT_PROPOSALS_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_PROPOSALS_PANEL_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/agent/agent_state.h"
#include "cli/service/planning/proposal_registry.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;

/**
 * @class AgentProposalsPanel
 * @brief Compact proposal management panel for agent sidebar
 *
 * This component displays a list of agent-generated proposals with:
 * - Compact list view with status indicators
 * - Quick accept/reject actions
 * - Expandable detail view for selected proposal
 * - Status filtering (All/Pending/Accepted/Rejected)
 *
 * Designed to work in both sidebar mode (compact) and standalone (full detail).
 * Uses AgentUIContext for shared state with other agent components.
 */
class AgentProposalsPanel {
 public:
  AgentProposalsPanel();
  ~AgentProposalsPanel() = default;

  /**
   * @brief Set the shared UI context
   */
  void SetContext(AgentUIContext* context);

  /**
   * @brief Set toast manager for notifications
   */
  void SetToastManager(ToastManager* toast_manager);

  /**
   * @brief Set ROM reference for proposal merging
   */
  void SetRom(Rom* rom);

  /**
   * @brief Set proposal callbacks
   */
  void SetProposalCallbacks(const ProposalCallbacks& callbacks);

  /**
   * @brief Draw the complete proposals panel
   * @param available_height Height available (0 = auto)
   */
  void Draw(float available_height = 0.0f);

  /**
   * @brief Draw just the proposal list (for custom layouts)
   */
  void DrawProposalList();

  /**
   * @brief Draw the detail view for selected proposal
   */
  void DrawProposalDetail();

  /**
   * @brief Set compact mode for sidebar usage
   */
  void SetCompactMode(bool compact) { compact_mode_ = compact; }
  bool IsCompactMode() const { return compact_mode_; }

  /**
   * @brief Focus a specific proposal by ID
   */
  void FocusProposal(const std::string& proposal_id);

  /**
   * @brief Refresh the proposal list from registry
   */
  void RefreshProposals();

  /**
   * @brief Get count of pending proposals
   */
  int GetPendingCount() const;

  /**
   * @brief Get total proposal count
   */
  int GetTotalCount() const { return static_cast<int>(proposals_.size()); }

 private:
  void DrawStatusFilter();
  void DrawProposalRow(const cli::ProposalRegistry::ProposalMetadata& proposal);
  void DrawQuickActions(const cli::ProposalRegistry::ProposalMetadata& proposal);
  void DrawDiffView();
  void DrawLogView();

  absl::Status AcceptProposal(const std::string& proposal_id);
  absl::Status RejectProposal(const std::string& proposal_id);
  absl::Status DeleteProposal(const std::string& proposal_id);

  void SelectProposal(const std::string& proposal_id);
  const char* GetStatusIcon(cli::ProposalRegistry::ProposalStatus status) const;
  ImVec4 GetStatusColor(cli::ProposalRegistry::ProposalStatus status) const;

  AgentUIContext* context_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  Rom* rom_ = nullptr;
  ProposalCallbacks proposal_callbacks_;

  bool compact_mode_ = false;
  bool needs_refresh_ = true;

  // Proposal data
  std::vector<cli::ProposalRegistry::ProposalMetadata> proposals_;
  std::string selected_proposal_id_;
  const cli::ProposalRegistry::ProposalMetadata* selected_proposal_ = nullptr;

  // Detail view data
  std::string diff_content_;
  std::string log_content_;

  // Confirmation state
  bool show_confirm_dialog_ = false;
  std::string confirm_action_;
  std::string confirm_proposal_id_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_PROPOSALS_PANEL_H_
