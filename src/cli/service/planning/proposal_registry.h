#ifndef YAZE_SRC_CLI_SERVICE_PROPOSAL_REGISTRY_H_
#define YAZE_SRC_CLI_SERVICE_PROPOSAL_REGISTRY_H_

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace yaze {
namespace cli {

// ProposalRegistry tracks agent-generated ROM modification proposals,
// storing metadata, diffs, execution logs, and optional screenshots for
// human review. Each proposal is associated with a sandbox ROM copy.
//
// Proposals follow a lifecycle:
// 1. Created - agent generates a proposal during `agent run`
// 2. Reviewed - user inspects via `agent diff` or TUI
// 3. Accepted - changes committed to main ROM via `agent commit`
// 4. Rejected - sandbox cleaned up via `agent revert`
class ProposalRegistry {
 public:
  enum class ProposalStatus {
    kPending,    // Created but not yet reviewed
    kAccepted,   // User accepted the changes
    kRejected,   // User rejected the changes
  };

  struct ProposalMetadata {
    std::string id;
    std::string sandbox_id;
    std::filesystem::path sandbox_directory;
    std::filesystem::path sandbox_rom_path;
    std::string description;
    std::string prompt;  // Original agent prompt that created this proposal
    ProposalStatus status;
    absl::Time created_at;
    std::optional<absl::Time> reviewed_at;
    
    // File paths relative to proposal directory
    std::filesystem::path diff_path;
    std::filesystem::path log_path;
    std::vector<std::filesystem::path> screenshots;
    
    // Statistics
    int bytes_changed;
    int commands_executed;
  };

  static ProposalRegistry& Instance();

  // Set the root directory for storing proposal data. If not set, uses
  // YAZE_PROPOSAL_ROOT env var or defaults to system temp directory.
  void SetRootDirectory(const std::filesystem::path& root);

  const std::filesystem::path& RootDirectory() const;

  // Creates a new proposal linked to the given sandbox. The proposal directory
  // is created under the root, and metadata is initialized.
  absl::StatusOr<ProposalMetadata> CreateProposal(
      absl::string_view sandbox_id,
      const std::filesystem::path& sandbox_rom_path,
      absl::string_view prompt,
      absl::string_view description);

  // Records a diff between original and modified ROM for the proposal.
  // The diff content is written to a file within the proposal directory.
  absl::Status RecordDiff(const std::string& proposal_id,
                         absl::string_view diff_content);

  // Appends log output from command execution to the proposal's log file.
  absl::Status AppendLog(const std::string& proposal_id,
                        absl::string_view log_entry);

  // Adds a screenshot path to the proposal metadata. Screenshots should
  // be copied into the proposal directory before calling this.
  absl::Status AddScreenshot(const std::string& proposal_id,
                             const std::filesystem::path& screenshot_path);

  // Updates the number of commands executed for a proposal. Used to track
  // how many CLI commands ran when generating the proposal.
  absl::Status UpdateCommandStats(const std::string& proposal_id,
                                  int commands_executed);

  // Updates the proposal status (pending -> accepted/rejected) and sets
  // the review timestamp.
  absl::Status UpdateStatus(const std::string& proposal_id,
                           ProposalStatus status);

  // Returns the metadata for a specific proposal.
  absl::StatusOr<ProposalMetadata> GetProposal(
      const std::string& proposal_id) const;

  // Lists all proposals, optionally filtered by status.
  std::vector<ProposalMetadata> ListProposals(
      std::optional<ProposalStatus> filter_status = std::nullopt) const;

  // Returns the most recently created proposal that is still pending.
  absl::StatusOr<ProposalMetadata> GetLatestPendingProposal() const;

  // Removes a proposal and its associated files from disk and memory.
  absl::Status RemoveProposal(const std::string& proposal_id);

  // Deletes all proposals older than the specified age.
  absl::StatusOr<int> CleanupOlderThan(absl::Duration max_age);

 private:
  ProposalRegistry();

  absl::Status EnsureRootExistsLocked();
  absl::Status LoadProposalsFromDiskLocked();
  std::string GenerateProposalIdLocked();
  std::filesystem::path ProposalDirectory(absl::string_view proposal_id) const;
  absl::Status WriteMetadataLocked(const ProposalMetadata& metadata) const;

  std::filesystem::path root_directory_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ProposalMetadata> proposals_;
  int sequence_ = 0;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_PROPOSAL_REGISTRY_H_
