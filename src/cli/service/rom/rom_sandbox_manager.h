#ifndef YAZE_SRC_CLI_SERVICE_ROM_SANDBOX_MANAGER_H_
#define YAZE_SRC_CLI_SERVICE_ROM_SANDBOX_MANAGER_H_

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "app/rom.h"

namespace yaze {
namespace cli {

// RomSandboxManager coordinates creation and lifecycle management of sandboxed
// ROM copies. Agent workflows operate on sandboxes so that proposals can be
// reviewed before landing in the primary project ROM. The manager currently
// tracks sandboxes in-memory for the running process and persists files to a
// configurable root directory on disk.
class RomSandboxManager {
 public:
  struct SandboxMetadata {
    std::string id;
    std::filesystem::path directory;
    std::filesystem::path rom_path;
    std::string source_rom;
    std::string description;
    absl::Time created_at;
  };

  static RomSandboxManager& Instance();

  // Set the root directory used for new sandboxes. Must be called before any
  // sandboxes are created. If not set, a default rooted at the system temporary
  // directory is used (or the value of the YAZE_SANDBOX_ROOT environment
  // variable when present).
  void SetRootDirectory(const std::filesystem::path& root);

  const std::filesystem::path& RootDirectory() const;

  // Creates a new sandbox by copying the provided ROM into a unique directory
  // under the root. The new sandbox becomes the active sandbox for the current
  // process. Metadata is returned describing the sandbox on success.
  absl::StatusOr<SandboxMetadata> CreateSandbox(Rom& rom,
                                                absl::string_view description);

  // Returns the metadata for the active sandbox if one exists.
  absl::StatusOr<SandboxMetadata> ActiveSandbox() const;

  // Returns the absolute path to the active sandbox ROM copy. Equivalent to
  // ActiveSandbox()->rom_path but with more descriptive errors when unset.
  absl::StatusOr<std::filesystem::path> ActiveSandboxRomPath() const;

  // List all sandboxes tracked during the current process lifetime. This will
  // include the active sandbox (if any) and previously created sandboxes that
  // have not been cleaned up.
  std::vector<SandboxMetadata> ListSandboxes() const;

  // Removes the sandbox identified by |id| from the index and deletes its on
  // disk directory. If the sandbox is currently active the active sandbox is
  // cleared. Missing sandboxes result in a NotFound status.
  absl::Status RemoveSandbox(const std::string& id);

  // Deletes any sandboxes that are older than |max_age|, returning the number
  // of sandboxes removed. This is currently best-effort; individual removals
  // may produce errors which are aggregated into the returned status.
  absl::StatusOr<int> CleanupOlderThan(absl::Duration max_age);

 private:
  RomSandboxManager();

  absl::Status EnsureRootExistsLocked();
  std::string GenerateSandboxIdLocked();

  std::filesystem::path root_directory_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, SandboxMetadata> sandboxes_;
  std::optional<std::string> active_sandbox_id_;
  int sequence_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_ROM_SANDBOX_MANAGER_H_
