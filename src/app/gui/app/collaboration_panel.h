#ifndef YAZE_APP_GUI_WIDGETS_COLLABORATION_PANEL_H_
#define YAZE_APP_GUI_WIDGETS_COLLABORATION_PANEL_H_

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"
#include "app/rom.h"
#include "imgui/imgui.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {

namespace gui {

/**
 * @struct RomSyncEntry
 * @brief Represents a ROM synchronization event
 */
struct RomSyncEntry {
  std::string sync_id;
  std::string sender;
  std::string rom_hash;
  int64_t timestamp;
  size_t diff_size;
  bool applied;
  std::string error_message;
};

/**
 * @struct SnapshotEntry
 * @brief Represents a shared snapshot (image, map state, etc.)
 */
struct SnapshotEntry {
  std::string snapshot_id;
  std::string sender;
  std::string snapshot_type;  // "screenshot", "map_state", "tile_data", etc.
  int64_t timestamp;
  size_t data_size;
  std::vector<uint8_t> data;  // Base64-decoded image or JSON data
  bool is_image;
  
  // For images: decoded texture data
  void* texture_id = nullptr;
  int width = 0;
  int height = 0;
};

/**
 * @struct ProposalEntry  
 * @brief Represents an AI-generated proposal
 */
struct ProposalEntry {
  std::string proposal_id;
  std::string sender;
  std::string description;
  std::string proposal_data;  // JSON or formatted text
  std::string status;  // "pending", "approved", "rejected", "applied"
  int64_t timestamp;
  
#ifdef YAZE_WITH_JSON
  nlohmann::json metadata;
#endif
};

/**
 * @class CollaborationPanel
 * @brief ImGui panel for collaboration features
 * 
 * Displays:
 * - ROM sync history and status
 * - Shared snapshots gallery
 * - Proposal management and voting
 */
class CollaborationPanel {
 public:
  CollaborationPanel();
  ~CollaborationPanel();
  
  /**
   * Initialize with ROM and version manager
   */
  void Initialize(Rom* rom, net::RomVersionManager* version_mgr,
                  net::ProposalApprovalManager* approval_mgr);
  
  /**
   * Render the collaboration panel
   */
  void Render(bool* p_open = nullptr);
  
  /**
   * Add a ROM sync event
   */
  void AddRomSync(const RomSyncEntry& entry);
  
  /**
   * Add a snapshot
   */
  void AddSnapshot(const SnapshotEntry& entry);
  
  /**
   * Add a proposal
   */
  void AddProposal(const ProposalEntry& entry);
  
  /**
   * Update proposal status
   */
  void UpdateProposalStatus(const std::string& proposal_id, const std::string& status);
  
  /**
   * Clear all collaboration data
   */
  void Clear();
  
  /**
   * Get proposal by ID
   */
  ProposalEntry* GetProposal(const std::string& proposal_id);
  
 private:
  void RenderRomSyncTab();
  void RenderSnapshotsTab();
  void RenderProposalsTab();
  void RenderVersionHistoryTab();
  void RenderApprovalTab();
  
  void RenderRomSyncEntry(const RomSyncEntry& entry, int index);
  void RenderSnapshotEntry(const SnapshotEntry& entry, int index);
  void RenderProposalEntry(const ProposalEntry& entry, int index);
  void RenderVersionSnapshot(const net::RomSnapshot& snapshot, int index);
  void RenderApprovalProposal(const net::ProposalApprovalManager::ApprovalStatus& status, int index);
  
  // Integration components
  Rom* rom_;
  net::RomVersionManager* version_mgr_;
  net::ProposalApprovalManager* approval_mgr_;
  
  // Tab selection
  int selected_tab_;
  
  // Data
  std::vector<RomSyncEntry> rom_syncs_;
  std::vector<SnapshotEntry> snapshots_;
  std::vector<ProposalEntry> proposals_;
  
  // UI state
  int selected_rom_sync_;
  int selected_snapshot_;
  int selected_proposal_;
  bool show_sync_details_;
  bool show_snapshot_preview_;
  bool auto_scroll_;
  
  // Filters
  char search_filter_[256];
  bool filter_pending_only_;
  
  // Colors
  struct {
    ImVec4 sync_applied;
    ImVec4 sync_pending;
    ImVec4 sync_error;
    ImVec4 proposal_pending;
    ImVec4 proposal_approved;
    ImVec4 proposal_rejected;
    ImVec4 proposal_applied;
  } colors_;
  
  // Helper functions
  std::string FormatTimestamp(int64_t timestamp);
  std::string FormatFileSize(size_t bytes);
  const char* GetProposalStatusIcon(const std::string& status);
  ImVec4 GetProposalStatusColor(const std::string& status);
};

}  // namespace gui

}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_COLLABORATION_PANEL_H_
