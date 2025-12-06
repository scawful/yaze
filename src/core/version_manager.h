#ifndef YAZE_CORE_VERSION_MANAGER_H_
#define YAZE_CORE_VERSION_MANAGER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/project.h"

namespace yaze {
namespace core {

struct SnapshotResult {
  bool success;
  std::string commit_hash;
  std::string rom_backup_path;
  std::string message;
};

/**
 * @class VersionManager
 * @brief Manages project versioning (Git) and ROM artifact snapshots.
 *
 * Handles the complexity of separating code (Git) from artifacts (ROMs),
 * ensuring that every "Snapshot" captures both the state of the code
 * and the resulting build artifact.
 */
class VersionManager {
 public:
  explicit VersionManager(project::YazeProject* project);

  // Core Actions
  absl::Status InitializeGit();
  absl::StatusOr<SnapshotResult> CreateSnapshot(const std::string& message);
  
  // Queries
  bool IsGitInitialized() const;
  std::string GetCurrentHash() const;
  std::vector<std::string> GetHistory(int limit = 10) const;

 private:
  // Git Helpers
  absl::Status GitInit();
  absl::Status GitAddAll();
  absl::Status GitCommit(const std::string& message);
  std::string GitRevParseHead() const;
  
  // ROM Helpers
  absl::Status BackupRomArtifact(const std::string& timestamp_str);
  
  // System Helpers
  absl::Status RunCommand(const std::string& cmd);
  absl::StatusOr<std::string> RunCommandOutput(const std::string& cmd) const;

  project::YazeProject* project_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_VERSION_MANAGER_H_
