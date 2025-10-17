#include "app/net/rom_version_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"

// For compression (placeholder - would use zlib or similar)
#include <vector>

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {

namespace net {

namespace {

// Simple hash function (in production, use SHA256)
std::string ComputeHash(const std::vector<uint8_t>& data) {
  uint32_t hash = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    hash = hash * 31 + data[i];
  }
  return absl::StrFormat("%08x", hash);
}

// Generate unique ID
std::string GenerateId() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  return absl::StrFormat("snap_%lld", ms);
}

int64_t GetCurrentTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace

// ============================================================================
// RomVersionManager Implementation
// ============================================================================

RomVersionManager::RomVersionManager(Rom* rom)
    : rom_(rom),
      last_backup_time_(0) {
}

RomVersionManager::~RomVersionManager() {
  // Cleanup if needed
}

absl::Status RomVersionManager::Initialize(const Config& config) {
  config_ = config;
  
  // Create initial snapshot
  auto initial_result = CreateSnapshot(
      "Initial state",
      "system",
      true);
  
  if (!initial_result.ok()) {
    return initial_result.status();
  }
  
  // Mark as safe point
  return MarkAsSafePoint(*initial_result);
}

absl::StatusOr<std::string> RomVersionManager::CreateSnapshot(
    const std::string& description,
    const std::string& creator,
    bool is_checkpoint) {
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Get ROM data
  std::vector<uint8_t> rom_data(rom_->size());
  std::memcpy(rom_data.data(), rom_->data(), rom_->size());
  
  // Create snapshot
  RomSnapshot snapshot;
  snapshot.snapshot_id = GenerateId();
  snapshot.description = description;
  snapshot.timestamp = GetCurrentTimestamp();
  snapshot.rom_hash = ComputeHash(rom_data);
  snapshot.creator = creator;
  snapshot.is_checkpoint = is_checkpoint;
  snapshot.is_safe_point = false;
  
  // Compress if enabled
  if (config_.compress_snapshots) {
    snapshot.rom_data = CompressData(rom_data);
    snapshot.compressed_size = snapshot.rom_data.size();
  } else {
    snapshot.rom_data = std::move(rom_data);
    snapshot.compressed_size = snapshot.rom_data.size();
  }
  
#ifdef YAZE_WITH_JSON
  snapshot.metadata = nlohmann::json::object();
  snapshot.metadata["size"] = rom_->size();
  snapshot.metadata["auto_backup"] = !is_checkpoint;
#endif
  
  // Store snapshot
  snapshots_[snapshot.snapshot_id] = std::move(snapshot);
  last_known_hash_ = snapshots_[snapshot.snapshot_id].rom_hash;
  
  // Cleanup if needed
  if (snapshots_.size() > config_.max_snapshots) {
    CleanupOldSnapshots();
  }
  
  return snapshots_[snapshot.snapshot_id].snapshot_id;
}

absl::Status RomVersionManager::RestoreSnapshot(const std::string& snapshot_id) {
  auto it = snapshots_.find(snapshot_id);
  if (it == snapshots_.end()) {
    return absl::NotFoundError(absl::StrCat("Snapshot not found: ", snapshot_id));
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  const RomSnapshot& snapshot = it->second;
  
  // Decompress if needed
  std::vector<uint8_t> rom_data;
  if (config_.compress_snapshots) {
    rom_data = DecompressData(snapshot.rom_data);
  } else {
    rom_data = snapshot.rom_data;
  }
  
  // Verify size matches
  if (rom_data.size() != rom_->size()) {
    return absl::DataLossError("Snapshot size mismatch");
  }
  
  // Create backup before restore
  auto backup_result = CreateSnapshot(
      "Pre-restore backup",
      "system",
      false);
  
  if (!backup_result.ok()) {
    return absl::InternalError("Failed to create pre-restore backup");
  }
  
  // Restore ROM data
  std::memcpy(rom_->mutable_data(), rom_data.data(), rom_data.size());
  
  last_known_hash_ = snapshot.rom_hash;
  
  return absl::OkStatus();
}

absl::Status RomVersionManager::MarkAsSafePoint(const std::string& snapshot_id) {
  auto it = snapshots_.find(snapshot_id);
  if (it == snapshots_.end()) {
    return absl::NotFoundError("Snapshot not found");
  }
  
  it->second.is_safe_point = true;
  return absl::OkStatus();
}

std::vector<RomSnapshot> RomVersionManager::GetSnapshots(bool safe_points_only) const {
  std::vector<RomSnapshot> result;
  
  for (const auto& [id, snapshot] : snapshots_) {
    if (!safe_points_only || snapshot.is_safe_point) {
      result.push_back(snapshot);
    }
  }
  
  // Sort by timestamp (newest first)
  std::sort(result.begin(), result.end(),
            [](const RomSnapshot& a, const RomSnapshot& b) {
              return a.timestamp > b.timestamp;
            });
  
  return result;
}

absl::StatusOr<RomSnapshot> RomVersionManager::GetSnapshot(
    const std::string& snapshot_id) const {
  auto it = snapshots_.find(snapshot_id);
  if (it == snapshots_.end()) {
    return absl::NotFoundError("Snapshot not found");
  }
  return it->second;
}

absl::Status RomVersionManager::DeleteSnapshot(const std::string& snapshot_id) {
  auto it = snapshots_.find(snapshot_id);
  if (it == snapshots_.end()) {
    return absl::NotFoundError("Snapshot not found");
  }
  
  // Don't allow deleting safe points
  if (it->second.is_safe_point) {
    return absl::FailedPreconditionError("Cannot delete safe point");
  }
  
  snapshots_.erase(it);
  return absl::OkStatus();
}

absl::StatusOr<bool> RomVersionManager::DetectCorruption() {
  if (!config_.enable_corruption_detection) {
    return false;
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Compute current hash
  std::vector<uint8_t> current_data(rom_->size());
  std::memcpy(current_data.data(), rom_->data(), rom_->size());
  std::string current_hash = ComputeHash(current_data);
  
  // Basic integrity checks
  auto integrity_status = ValidateRomIntegrity();
  if (!integrity_status.ok()) {
    return true;  // Corruption detected
  }
  
  // Check against last known good hash (if modified unexpectedly)
  if (!last_known_hash_.empty() && current_hash != last_known_hash_) {
    // ROM changed without going through version manager
    // This might be intentional, so just flag it
    return false;
  }
  
  return false;
}

absl::Status RomVersionManager::AutoRecover() {
  // Find most recent safe point
  auto snapshots = GetSnapshots(true);
  if (snapshots.empty()) {
    return absl::NotFoundError("No safe points available for recovery");
  }
  
  // Restore from most recent safe point
  return RestoreSnapshot(snapshots[0].snapshot_id);
}

std::string RomVersionManager::GetCurrentHash() const {
  if (!rom_ || !rom_->is_loaded()) {
    return "";
  }
  
  std::vector<uint8_t> data(rom_->size());
  std::memcpy(data.data(), rom_->data(), rom_->size());
  return ComputeHash(data);
}

absl::Status RomVersionManager::CleanupOldSnapshots() {
  // Keep safe points and checkpoints
  // Remove oldest auto-backups first
  
  std::vector<std::pair<int64_t, std::string>> auto_backups;
  for (const auto& [id, snapshot] : snapshots_) {
    if (!snapshot.is_safe_point && !snapshot.is_checkpoint) {
      auto_backups.push_back({snapshot.timestamp, id});
    }
  }
  
  // Sort by timestamp (oldest first)
  std::sort(auto_backups.begin(), auto_backups.end());
  
  // Delete oldest until within limits
  while (snapshots_.size() > config_.max_snapshots && !auto_backups.empty()) {
    snapshots_.erase(auto_backups.front().second);
    auto_backups.erase(auto_backups.begin());
  }
  
  // Check storage limit
  while (GetTotalStorageUsed() > config_.max_storage_mb * 1024 * 1024 &&
         !auto_backups.empty()) {
    snapshots_.erase(auto_backups.front().second);
    auto_backups.erase(auto_backups.begin());
  }
  
  return absl::OkStatus();
}

RomVersionManager::Stats RomVersionManager::GetStats() const {
  Stats stats = {};
  stats.total_snapshots = snapshots_.size();
  
  for (const auto& [id, snapshot] : snapshots_) {
    if (snapshot.is_safe_point) stats.safe_points++;
    if (snapshot.is_checkpoint) stats.manual_checkpoints++;
    if (!snapshot.is_checkpoint) stats.auto_backups++;
    stats.total_storage_bytes += snapshot.compressed_size;
    
    if (stats.oldest_snapshot_timestamp == 0 ||
        snapshot.timestamp < stats.oldest_snapshot_timestamp) {
      stats.oldest_snapshot_timestamp = snapshot.timestamp;
    }
    
    if (snapshot.timestamp > stats.newest_snapshot_timestamp) {
      stats.newest_snapshot_timestamp = snapshot.timestamp;
    }
  }
  
  return stats;
}

// Private helper methods

std::string RomVersionManager::ComputeRomHash() const {
  if (!rom_ || !rom_->is_loaded()) {
    return "";
  }
  
  std::vector<uint8_t> data(rom_->size());
  std::memcpy(data.data(), rom_->data(), rom_->size());
  return ComputeHash(data);
}

std::vector<uint8_t> RomVersionManager::CompressData(
    const std::vector<uint8_t>& data) const {
  // Placeholder: In production, use zlib or similar
  // For now, just return the data as-is
  return data;
}

std::vector<uint8_t> RomVersionManager::DecompressData(
    const std::vector<uint8_t>& compressed) const {
  // Placeholder: In production, use zlib or similar
  return compressed;
}

absl::Status RomVersionManager::ValidateRomIntegrity() const {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Basic checks
  if (rom_->size() == 0) {
    return absl::DataLossError("ROM size is zero");
  }
  
  // Check for valid SNES header
  // (This is a simplified check - real validation would be more thorough)
  if (rom_->size() < 0x8000) {
    return absl::DataLossError("ROM too small to be valid");
  }
  
  return absl::OkStatus();
}

size_t RomVersionManager::GetTotalStorageUsed() const {
  size_t total = 0;
  for (const auto& [id, snapshot] : snapshots_) {
    total += snapshot.compressed_size;
  }
  return total;
}

// ============================================================================
// ProposalApprovalManager Implementation
// ============================================================================

ProposalApprovalManager::ProposalApprovalManager(RomVersionManager* version_mgr)
    : version_mgr_(version_mgr),
      mode_(ApprovalMode::kHostOnly) {
}

void ProposalApprovalManager::SetApprovalMode(ApprovalMode mode) {
  mode_ = mode;
}

void ProposalApprovalManager::SetHost(const std::string& host_username) {
  host_username_ = host_username;
}

absl::Status ProposalApprovalManager::SubmitProposal(
    const std::string& proposal_id,
    const std::string& sender,
    const std::string& description,
    const nlohmann::json& proposal_data) {
  
  ApprovalStatus status;
  status.proposal_id = proposal_id;
  status.status = "pending";
  status.created_at = GetCurrentTimestamp();
  status.decided_at = 0;
  
  // Create snapshot before potential application
  auto snapshot_result = version_mgr_->CreateSnapshot(
      absl::StrCat("Before proposal: ", description),
      sender,
      false);
  
  if (!snapshot_result.ok()) {
    return snapshot_result.status();
  }
  
  status.snapshot_before = *snapshot_result;
  
  proposals_[proposal_id] = status;
  
  return absl::OkStatus();
}

absl::Status ProposalApprovalManager::VoteOnProposal(
    const std::string& proposal_id,
    const std::string& username,
    bool approved) {
  
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError("Proposal not found");
  }
  
  ApprovalStatus& status = it->second;
  
  if (status.status != "pending") {
    return absl::FailedPreconditionError("Proposal already decided");
  }
  
  // Record vote
  status.votes[username] = approved;
  
  // Check if decision can be made
  if (CheckApprovalThreshold(status)) {
    status.status = "approved";
    status.decided_at = GetCurrentTimestamp();
  } else {
    // Check if rejection threshold reached
    size_t rejection_count = 0;
    for (const auto& [user, vote] : status.votes) {
      if (!vote) rejection_count++;
    }
    
    // If host rejected (in host-only mode), reject immediately
    if (mode_ == ApprovalMode::kHostOnly && 
        username == host_username_ && !approved) {
      status.status = "rejected";
      status.decided_at = GetCurrentTimestamp();
    }
  }
  
  return absl::OkStatus();
}

bool ProposalApprovalManager::CheckApprovalThreshold(
    const ApprovalStatus& status) const {
  
  switch (mode_) {
    case ApprovalMode::kHostOnly:
      // Only host vote matters
      if (status.votes.find(host_username_) != status.votes.end()) {
        return status.votes.at(host_username_);
      }
      return false;
    
    case ApprovalMode::kMajorityVote: {
      size_t approval_count = 0;
      for (const auto& [user, approved] : status.votes) {
        if (approved) approval_count++;
      }
      return approval_count > participants_.size() / 2;
    }
    
    case ApprovalMode::kUnanimous: {
      if (status.votes.size() < participants_.size()) {
        return false;  // Not everyone voted yet
      }
      for (const auto& [user, approved] : status.votes) {
        if (!approved) return false;
      }
      return true;
    }
    
    case ApprovalMode::kAutoApprove:
      return true;
  }
  
  return false;
}

bool ProposalApprovalManager::IsProposalApproved(
    const std::string& proposal_id) const {
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return false;
  }
  return it->second.status == "approved";
}

std::vector<ProposalApprovalManager::ApprovalStatus> 
ProposalApprovalManager::GetPendingProposals() const {
  std::vector<ApprovalStatus> pending;
  for (const auto& [id, status] : proposals_) {
    if (status.status == "pending") {
      pending.push_back(status);
    }
  }
  return pending;
}

absl::StatusOr<ProposalApprovalManager::ApprovalStatus> 
ProposalApprovalManager::GetProposalStatus(
    const std::string& proposal_id) const {
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError("Proposal not found");
  }
  return it->second;
}

}  // namespace net

}  // namespace yaze
