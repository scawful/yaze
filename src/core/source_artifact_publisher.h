#ifndef YAZE_CORE_SOURCE_ARTIFACT_PUBLISHER_H_
#define YAZE_CORE_SOURCE_ARTIFACT_PUBLISHER_H_

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::core {

// All source-artifact publishers share one persistent lock namespace so that
// different source domains cannot race while publishing into the same folder.
inline constexpr char kSourceArtifactPublicationLockBasename[] =
    ".yaze-message-source-sync.lock";

struct SourceArtifactUpdate {
  std::filesystem::path target;
  std::optional<std::string> before;
  std::string after;
};

// Labels only affect diagnostics. They let migrated callers retain their
// existing user-facing errors while the publication machinery stays generic.
struct SourceArtifactPublisherLabels {
  std::string subject = "source artifact";
  std::string published_file = "published source artifact";
};

using SourceArtifactReadbackValidator = std::function<absl::Status()>;

// Holds both the process-local semaphore and the durable per-directory locks
// for a set of absolute publication targets. Target paths are canonicalized
// and pinned while acquiring the lock. Callers should acquire this before
// reading their preflight bytes and keep it alive through publication.
class SourceArtifactPublicationLock {
 public:
  ~SourceArtifactPublicationLock();

  SourceArtifactPublicationLock(const SourceArtifactPublicationLock&) = delete;
  SourceArtifactPublicationLock& operator=(
      const SourceArtifactPublicationLock&) = delete;

 private:
  struct Impl;

  explicit SourceArtifactPublicationLock(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;

  friend absl::StatusOr<std::unique_ptr<SourceArtifactPublicationLock>>
  AcquireSourceArtifactPublicationLock(
      const std::vector<std::filesystem::path>& targets,
      const SourceArtifactPublisherLabels& labels);
  friend absl::Status PublishSourceArtifacts(
      const SourceArtifactPublicationLock& lock,
      std::vector<SourceArtifactUpdate> updates,
      std::string_view expected_primary_sha256,
      SourceArtifactReadbackValidator readback_validator);
};

// Validates the persistent lock paths without creating or acquiring them.
// Dry-run workflows use this to retain the same fail-closed path checks as a
// write without leaving lock files behind.
absl::Status ValidateSourceArtifactPublicationTargets(
    const std::vector<std::filesystem::path>& targets,
    const SourceArtifactPublisherLabels& labels = {});

absl::StatusOr<std::unique_ptr<SourceArtifactPublicationLock>>
AcquireSourceArtifactPublicationLock(
    const std::vector<std::filesystem::path>& targets,
    const SourceArtifactPublisherLabels& labels = {});

std::string ComputeSourceArtifactSha256(std::string_view content);

// Publishes all updates as one rollback-protected transaction using atomic
// replacement for each target. The first update is the primary CAS source and
// must contain its exact preflight bytes. Exact byte readback is always
// required; an optional domain validator runs before backups are removed so
// its failure can still roll the set back. This is not crash-atomic across the
// complete multi-file set: a process or system crash can interrupt the set
// between target replacements.
absl::Status PublishSourceArtifacts(
    const SourceArtifactPublicationLock& lock,
    std::vector<SourceArtifactUpdate> updates,
    std::string_view expected_primary_sha256,
    SourceArtifactReadbackValidator readback_validator = {});

}  // namespace yaze::core

#endif  // YAZE_CORE_SOURCE_ARTIFACT_PUBLISHER_H_
