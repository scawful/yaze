#ifndef YAZE_APP_NET_COLLABORATION_SERVICE_H_
#define YAZE_APP_NET_COLLABORATION_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/net/rom_version_manager.h"
#include "app/net/websocket_client.h"
#include "app/rom.h"

namespace yaze {

namespace net {

/**
 * @class CollaborationService
 * @brief High-level service integrating version management with networking
 * 
 * Bridges the gap between:
 * - Local ROM version management
 * - Remote collaboration via WebSocket
 * - Proposal approval workflow
 * 
 * Features:
 * - Automatic ROM sync on changes
 * - Network-aware proposal approval
 * - Conflict resolution
 * - Auto-backup before network operations
 */
class CollaborationService {
 public:
  struct Config {
    bool auto_sync_enabled = true;
    int sync_interval_ms = 5000;  // 5 seconds
    bool require_approval_for_sync = true;
    bool create_snapshot_before_sync = true;
  };

  explicit CollaborationService(Rom* rom);
  ~CollaborationService();

  /**
   * Initialize the service
   */
  absl::Status Initialize(const Config& config, RomVersionManager* version_mgr,
                          ProposalApprovalManager* approval_mgr);

  /**
   * Connect to collaboration server
   */
  absl::Status Connect(const std::string& host, int port = 8765);

  /**
   * Disconnect from server
   */
  void Disconnect();

  /**
   * Host a new session
   */
  absl::Status HostSession(const std::string& session_name,
                           const std::string& username, bool ai_enabled = true);

  /**
   * Join existing session
   */
  absl::Status JoinSession(const std::string& session_code,
                           const std::string& username);

  /**
   * Leave current session
   */
  absl::Status LeaveSession();

  /**
   * Submit local changes as proposal
   */
  absl::Status SubmitChangesAsProposal(const std::string& description,
                                       const std::string& username);

  /**
   * Apply received ROM sync
   */
  absl::Status ApplyRomSync(const std::string& diff_data,
                            const std::string& rom_hash,
                            const std::string& sender);

  /**
   * Handle incoming proposal
   */
  absl::Status HandleIncomingProposal(const std::string& proposal_id,
                                      const nlohmann::json& proposal_data,
                                      const std::string& sender);

  /**
   * Vote on proposal
   */
  absl::Status VoteOnProposal(const std::string& proposal_id, bool approved,
                              const std::string& username);

  /**
   * Apply approved proposal
   */
  absl::Status ApplyApprovedProposal(const std::string& proposal_id);

  /**
   * Get connection status
   */
  bool IsConnected() const;

  /**
   * Get session info
   */
  absl::StatusOr<SessionInfo> GetSessionInfo() const;

  /**
   * Get WebSocket client (for advanced usage)
   */
  WebSocketClient* GetClient() { return client_.get(); }

  /**
   * Enable/disable auto-sync
   */
  void SetAutoSync(bool enabled);

 private:
  Rom* rom_;
  RomVersionManager* version_mgr_;
  ProposalApprovalManager* approval_mgr_;
  std::unique_ptr<WebSocketClient> client_;
  Config config_;

  // Sync state
  std::string last_sync_hash_;
  bool sync_in_progress_;

  // Callbacks for network events
  void OnRomSyncReceived(const nlohmann::json& payload);
  void OnProposalReceived(const nlohmann::json& payload);
  void OnProposalUpdated(const nlohmann::json& payload);
  void OnParticipantJoined(const nlohmann::json& payload);
  void OnParticipantLeft(const nlohmann::json& payload);

  // Helper functions
  std::string GenerateDiff(const std::string& from_hash,
                           const std::string& to_hash);
  absl::Status ApplyDiff(const std::string& diff_data);
  bool ShouldAutoSync();
};

}  // namespace net

}  // namespace yaze

#endif  // YAZE_APP_NET_COLLABORATION_SERVICE_H_
