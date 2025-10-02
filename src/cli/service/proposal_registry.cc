#include "cli/service/proposal_registry.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

#include "util/macro.h"

namespace yaze {
namespace cli {

namespace {

std::filesystem::path DetermineDefaultRoot() {
  if (const char* env_root = std::getenv("YAZE_PROPOSAL_ROOT")) {
    return std::filesystem::path(env_root);
  }
  std::error_code ec;
  auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    return std::filesystem::current_path() / "yaze" / "proposals";
  }
  return temp_dir / "yaze" / "proposals";
}

}  // namespace

ProposalRegistry& ProposalRegistry::Instance() {
  static ProposalRegistry* instance = new ProposalRegistry();
  return *instance;
}

ProposalRegistry::ProposalRegistry()
    : root_directory_(DetermineDefaultRoot()) {}

void ProposalRegistry::SetRootDirectory(const std::filesystem::path& root) {
  std::lock_guard<std::mutex> lock(mutex_);
  root_directory_ = root;
  (void)EnsureRootExistsLocked();
}

const std::filesystem::path& ProposalRegistry::RootDirectory() const {
  return root_directory_;
}

absl::Status ProposalRegistry::EnsureRootExistsLocked() {
  std::error_code ec;
  if (!std::filesystem::exists(root_directory_, ec)) {
    if (!std::filesystem::create_directories(root_directory_, ec) && ec) {
      return absl::InternalError(absl::StrCat(
          "Failed to create proposal root at ", root_directory_.string(),
          ": ", ec.message()));
    }
  }
  return absl::OkStatus();
}

absl::Status ProposalRegistry::LoadProposalsFromDiskLocked() {
  std::error_code ec;
  
  // Check if root directory exists
  if (!std::filesystem::exists(root_directory_, ec)) {
    return absl::OkStatus();  // No proposals to load
  }

  // Iterate over all directories in the root
  for (const auto& entry : std::filesystem::directory_iterator(root_directory_, ec)) {
    if (ec) {
      continue;  // Skip entries that cause errors
    }
    
    if (!entry.is_directory()) {
      continue;  // Skip non-directories
    }

    std::string proposal_id = entry.path().filename().string();
    
    // Skip if already loaded (shouldn't happen, but be defensive)
    if (proposals_.find(proposal_id) != proposals_.end()) {
      continue;
    }

    // Reconstruct metadata from directory contents
    // Since we don't have a metadata.json file, we need to infer what we can
    std::filesystem::path log_path = entry.path() / "execution.log";
    std::filesystem::path diff_path = entry.path() / "diff.txt";
    
    // Check if log file exists to determine if this is a valid proposal
    if (!std::filesystem::exists(log_path, ec)) {
      continue;  // Not a valid proposal directory
    }

    // Extract timestamp from proposal ID (format: proposal-20251001T200215-1)
    absl::Time created_at = absl::Now();  // Default to now if parsing fails
    if (proposal_id.starts_with("proposal-")) {
      std::string time_str = proposal_id.substr(9, 15);  // Extract YYYYMMDDTHHmmSS
      std::string error;
      if (absl::ParseTime("%Y%m%dT%H%M%S", time_str, &created_at, &error)) {
        // Successfully parsed time
      }
    }

    // Get file modification time as a fallback
    auto ftime = std::filesystem::last_write_time(log_path, ec);
    if (!ec) {
      auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ftime - std::filesystem::file_time_type::clock::now() +
          std::chrono::system_clock::now());
      auto time_t_value = std::chrono::system_clock::to_time_t(sctp);
      created_at = absl::FromTimeT(time_t_value);
    }

    // Create minimal metadata for this proposal
    ProposalMetadata metadata{
        .id = proposal_id,
        .sandbox_id = "",  // Unknown - not stored in logs
        .description = "Loaded from disk",
        .prompt = "",  // Unknown - not stored in logs
        .status = ProposalStatus::kPending,
        .created_at = created_at,
        .reviewed_at = std::nullopt,
        .diff_path = diff_path,
        .log_path = log_path,
        .screenshots = {},
        .bytes_changed = 0,
        .commands_executed = 0,
    };

    // Count diff size if it exists
    if (std::filesystem::exists(diff_path, ec) && !ec) {
      metadata.bytes_changed = static_cast<int>(
          std::filesystem::file_size(diff_path, ec));
    }

    // Scan for screenshots
    for (const auto& file : std::filesystem::directory_iterator(entry.path(), ec)) {
      if (ec) continue;
      if (file.path().extension() == ".png" || 
          file.path().extension() == ".jpg" ||
          file.path().extension() == ".jpeg") {
        metadata.screenshots.push_back(file.path());
      }
    }

    proposals_[proposal_id] = metadata;
  }

  return absl::OkStatus();
}

std::string ProposalRegistry::GenerateProposalIdLocked() {
  absl::Time now = absl::Now();
  std::string time_component = absl::FormatTime("%Y%m%dT%H%M%S", now,
                                                absl::LocalTimeZone());
  ++sequence_;
  return absl::StrCat("proposal-", time_component, "-", sequence_);
}

std::filesystem::path ProposalRegistry::ProposalDirectory(
    absl::string_view proposal_id) const {
  return root_directory_ / std::string(proposal_id);
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::CreateProposal(absl::string_view sandbox_id,
                                 absl::string_view prompt,
                                 absl::string_view description) {
  std::unique_lock<std::mutex> lock(mutex_);
  RETURN_IF_ERROR(EnsureRootExistsLocked());

  std::string id = GenerateProposalIdLocked();
  std::filesystem::path proposal_dir = ProposalDirectory(id);
  lock.unlock();

  std::error_code ec;
  if (!std::filesystem::create_directories(proposal_dir, ec) && ec) {
    return absl::InternalError(absl::StrCat(
        "Failed to create proposal directory at ", proposal_dir.string(),
        ": ", ec.message()));
  }

  lock.lock();
  proposals_[id] = ProposalMetadata{
      .id = id,
      .sandbox_id = std::string(sandbox_id),
      .description = std::string(description),
      .prompt = std::string(prompt),
      .status = ProposalStatus::kPending,
      .created_at = absl::Now(),
      .reviewed_at = std::nullopt,
      .diff_path = proposal_dir / "diff.txt",
      .log_path = proposal_dir / "execution.log",
      .screenshots = {},
      .bytes_changed = 0,
      .commands_executed = 0,
  };

  return proposals_.at(id);
}

absl::Status ProposalRegistry::RecordDiff(const std::string& proposal_id,
                                         absl::string_view diff_content) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::ofstream diff_file(it->second.diff_path, std::ios::out);
  if (!diff_file.is_open()) {
    return absl::InternalError(absl::StrCat(
        "Failed to open diff file: ", it->second.diff_path.string()));
  }

  diff_file << diff_content;
  diff_file.close();

  // Update bytes_changed metric (rough estimate based on diff size)
  it->second.bytes_changed = static_cast<int>(diff_content.size());

  return absl::OkStatus();
}

absl::Status ProposalRegistry::AppendLog(const std::string& proposal_id,
                                        absl::string_view log_entry) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::ofstream log_file(it->second.log_path,
                        std::ios::out | std::ios::app);
  if (!log_file.is_open()) {
    return absl::InternalError(absl::StrCat(
        "Failed to open log file: ", it->second.log_path.string()));
  }

  log_file << absl::FormatTime("[%Y-%m-%d %H:%M:%S] ", absl::Now(),
                               absl::LocalTimeZone())
           << log_entry << "\n";
  log_file.close();

  return absl::OkStatus();
}

absl::Status ProposalRegistry::AddScreenshot(
    const std::string& proposal_id,
    const std::filesystem::path& screenshot_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  // Verify screenshot exists
  std::error_code ec;
  if (!std::filesystem::exists(screenshot_path, ec)) {
    return absl::NotFoundError(
        absl::StrCat("Screenshot file not found: ", screenshot_path.string()));
  }

  it->second.screenshots.push_back(screenshot_path);
  return absl::OkStatus();
}

absl::Status ProposalRegistry::UpdateStatus(const std::string& proposal_id,
                                           ProposalStatus status) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  it->second.status = status;
  it->second.reviewed_at = absl::Now();

  return absl::OkStatus();
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::GetProposal(const std::string& proposal_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }
  return it->second;
}

std::vector<ProposalRegistry::ProposalMetadata>
ProposalRegistry::ListProposals(std::optional<ProposalStatus> filter_status) const {
  std::unique_lock<std::mutex> lock(mutex_);
  
  // Load proposals from disk if we haven't already
  if (proposals_.empty()) {
    // Cast away const for loading - this is a lazy initialization pattern
    auto* self = const_cast<ProposalRegistry*>(this);
    auto status = self->LoadProposalsFromDiskLocked();
    if (!status.ok()) {
      // Log error but continue - return empty list if loading fails
      std::cerr << "Warning: Failed to load proposals from disk: " 
                << status.message() << "\n";
    }
  }
  
  std::vector<ProposalMetadata> result;
  
  for (const auto& [id, metadata] : proposals_) {
    if (!filter_status.has_value() || metadata.status == *filter_status) {
      result.push_back(metadata);
    }
  }

  // Sort by creation time (newest first)
  std::sort(result.begin(), result.end(),
            [](const ProposalMetadata& a, const ProposalMetadata& b) {
              return a.created_at > b.created_at;
            });

  return result;
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::GetLatestPendingProposal() const {
  std::lock_guard<std::mutex> lock(mutex_);
  
  const ProposalMetadata* latest = nullptr;
  for (const auto& [id, metadata] : proposals_) {
    if (metadata.status == ProposalStatus::kPending) {
      if (!latest || metadata.created_at > latest->created_at) {
        latest = &metadata;
      }
    }
  }

  if (!latest) {
    return absl::NotFoundError("No pending proposals found");
  }

  return *latest;
}

absl::Status ProposalRegistry::RemoveProposal(const std::string& proposal_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::filesystem::path proposal_dir = ProposalDirectory(proposal_id);
  std::error_code ec;
  std::filesystem::remove_all(proposal_dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrCat(
        "Failed to remove proposal directory: ", ec.message()));
  }

  proposals_.erase(it);
  return absl::OkStatus();
}

absl::StatusOr<int> ProposalRegistry::CleanupOlderThan(absl::Duration max_age) {
  std::lock_guard<std::mutex> lock(mutex_);
  absl::Time cutoff = absl::Now() - max_age;
  int removed_count = 0;

  std::vector<std::string> to_remove;
  for (const auto& [id, metadata] : proposals_) {
    if (metadata.created_at < cutoff) {
      to_remove.push_back(id);
    }
  }

  for (const auto& id : to_remove) {
    std::filesystem::path proposal_dir = ProposalDirectory(id);
    std::error_code ec;
    std::filesystem::remove_all(proposal_dir, ec);
    // Continue even if removal fails
    proposals_.erase(id);
    ++removed_count;
  }

  return removed_count;
}

}  // namespace cli
}  // namespace yaze
