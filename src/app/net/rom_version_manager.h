#ifndef YAZE_APP_NET_ROM_VERSION_MANAGER_H_
#define YAZE_APP_NET_ROM_VERSION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {

namespace net {

/**
 * @struct RomSnapshot
 * @brief Represents a versioned snapshot of ROM state
 */
struct RomSnapshot {
  std::string snapshot_id;
  std::string description;
  int64_t timestamp;
  std::string rom_hash;
  std::vector<uint8_t> rom_data;
  size_t compressed_size;

  // Metadata
  std::string creator;
  bool is_checkpoint;  // Manual checkpoint vs auto-backup
  bool is_safe_point;  // Marked as "known good" by host

#ifdef YAZE_WITH_JSON
  nlohmann::json metadata;  // Custom metadata (proposals applied, etc.)
#endif
};

/**
 * @struct VersionDiff
 * @brief Represents differences between two ROM versions
 */
struct VersionDiff {
  std::string from_snapshot_id;
  std::string to_snapshot_id;
  std::vector<std::pair<size_t, std::vector<uint8_t>>> changes;  // offset, data
  size_t total_bytes_changed;
  std::vector<std::string> proposals_applied;  // IDs of proposals in this diff
};

/**
 * @class RomVersionManager
 * @brief Manages ROM versioning, snapshots, and rollback capabilities
 *
 * Provides:
 * - Automatic periodic snapshots
 * - Manual checkpoints
 * - Rollback to any previous version
 * - Diff generation between versions
 * - Corruption detection and recovery
 */
class RomVersionManager {
 public:
  struct Config {
    bool enable_auto_backup = true;
    int auto_backup_interval_seconds = 300;  // 5 minutes
    size_t max_snapshots = 50;
    size_t max_storage_mb = 500;  // 500MB max for all snapshots
    bool compress_snapshots = true;
    bool enable_corruption_detection = true;
  };

  explicit RomVersionManager(Rom* rom);
  ~RomVersionManager();

  /**
   * Initialize version management
   */
  absl::Status Initialize(const Config& config);

  /**
   * Create a snapshot of current ROM state
   */
  absl::StatusOr<std::string> CreateSnapshot(const std::string& description,
                                             const std::string& creator,
                                             bool is_checkpoint = false);

  /**
   * Restore ROM to a previous snapshot
   */
  absl::Status RestoreSnapshot(const std::string& snapshot_id);

  /**
   * Mark a snapshot as a safe point (host-verified)
   */
  absl::Status MarkAsSafePoint(const std::string& snapshot_id);

  /**
   * Get all snapshots, sorted by timestamp
   */
  std::vector<RomSnapshot> GetSnapshots(bool safe_points_only = false) const;

  /**
   * Get a specific snapshot
   */
  absl::StatusOr<RomSnapshot> GetSnapshot(const std::string& snapshot_id) const;

  /**
   * Delete a snapshot
   */
  absl::Status DeleteSnapshot(const std::string& snapshot_id);

  /**
   * Generate diff between two snapshots
   */
  absl::StatusOr<VersionDiff> GenerateDiff(const std::string& from_id,
                                           const std::string& to_id) const;

  /**
   * Check for ROM corruption
   */
  absl::StatusOr<bool> DetectCorruption();

  /**
   * Auto-recover from corruption using nearest safe point
   */
  absl::Status AutoRecover();

  /**
   * Export snapshot to file
   */
  absl::Status ExportSnapshot(const std::string& snapshot_id,
                              const std::string& filepath);

  /**
   * Import snapshot from file
   */
  absl::Status ImportSnapshot(const std::string& filepath);

  /**
   * Get current ROM hash
   */
  std::string GetCurrentHash() const;

  /**
   * Cleanup old snapshots based on policy
   */
  absl::Status CleanupOldSnapshots();

  /**
   * Get statistics
   */
  struct Stats {
    size_t total_snapshots;
    size_t safe_points;
    size_t auto_backups;
    size_t manual_checkpoints;
    size_t total_storage_bytes;
    int64_t oldest_snapshot_timestamp;
    int64_t newest_snapshot_timestamp;
  };
  Stats GetStats() const;

 private:
  Rom* rom_;
  Config config_;
  std::map<std::string, RomSnapshot> snapshots_;
  std::string last_known_hash_;
  int64_t last_backup_time_;

  // Helper functions
  std::string ComputeRomHash() const;
  std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data) const;
  std::vector<uint8_t> DecompressData(
      const std::vector<uint8_t>& compressed) const;
  absl::Status ValidateRomIntegrity() const;
  size_t GetTotalStorageUsed() const;
  void PruneOldSnapshots();
};

/**
 * @class ProposalApprovalManager
 * @brief Manages proposal approval workflow for collaborative sessions
 *
 * Features:
 * - Host approval required for all changes
 * - Participant voting system
 * - Automatic rollback on rejection
 * - Change tracking and audit log
 */
class ProposalApprovalManager {
 public:
  enum class ApprovalMode {
    kHostOnly,      // Only host can approve
    kMajorityVote,  // Majority of participants must approve
    kUnanimous,     // All participants must approve
    kAutoApprove    // No approval needed (dangerous!)
  };

  struct ApprovalStatus {
    std::string proposal_id;
    std::string status;  // "pending", "approved", "rejected", "applied"
    std::map<std::string, bool> votes;  // username -> approved
    int64_t created_at;
    int64_t decided_at;
    std::string snapshot_before;  // Snapshot ID before applying
    std::string snapshot_after;   // Snapshot ID after applying
  };

  explicit ProposalApprovalManager(RomVersionManager* version_mgr);

  /**
   * Set approval mode for the session
   */
  void SetApprovalMode(ApprovalMode mode);

  /**
   * Set host username
   */
  void SetHost(const std::string& host_username);

  /**
   * Submit a proposal for approval
   */
  absl::Status SubmitProposal(const std::string& proposal_id,
                              const std::string& sender,
                              const std::string& description,
                              const nlohmann::json& proposal_data);

  /**
   * Vote on a proposal
   */
  absl::Status VoteOnProposal(const std::string& proposal_id,
                              const std::string& username, bool approved);

  /**
   * Apply an approved proposal
   */
  absl::Status ApplyProposal(const std::string& proposal_id, Rom* rom);

  /**
   * Reject and rollback a proposal
   */
  absl::Status RejectProposal(const std::string& proposal_id);

  /**
   * Get proposal status
   */
  absl::StatusOr<ApprovalStatus> GetProposalStatus(
      const std::string& proposal_id) const;

  /**
   * Get all pending proposals
   */
  std::vector<ApprovalStatus> GetPendingProposals() const;

  /**
   * Check if proposal is approved
   */
  bool IsProposalApproved(const std::string& proposal_id) const;

  /**
   * Get audit log
   */
  std::vector<ApprovalStatus> GetAuditLog() const;

 private:
  RomVersionManager* version_mgr_;
  ApprovalMode mode_;
  std::string host_username_;
  std::map<std::string, ApprovalStatus> proposals_;
  std::vector<std::string> participants_;

  bool CheckApprovalThreshold(const ApprovalStatus& status) const;
};

}  // namespace net

}  // namespace yaze

#endif  // YAZE_APP_NET_ROM_VERSION_MANAGER_H_
