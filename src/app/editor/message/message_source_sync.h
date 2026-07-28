#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_SOURCE_SYNC_H_
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_SOURCE_SYNC_H_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "core/project.h"

namespace yaze::editor {

struct MessageSourceSyncOptions {
  // Source synchronization is a dry-run unless this is explicitly enabled.
  bool write = false;

  // Required in write mode. This is compared with the SHA-256 of the exact
  // canonical bundle bytes immediately before publication.
  std::string expected_source_sha256;
};

struct MessageSourceSyncResult {
  std::filesystem::path canonical_bundle_path;
  std::filesystem::path generated_asm_include_path;
  std::vector<std::filesystem::path> backup_paths;

  int first_expanded_id = 0;
  int last_expanded_id = 0;
  int expanded_count = 0;
  int incoming_updates = 0;
  size_t encoded_size = 0;
  size_t capacity = 0;

  std::string source_sha256_before;
  std::string proposed_source_sha256;
  bool changed = false;
  bool wrote = false;
};

// Compute the lowercase SHA-256 used by canonical source/include provenance.
std::string ComputeMessageSourceSha256(std::string_view content);

// Merge a validated expanded-message subset into the complete canonical
// project bundle and render its deterministic, no-org Asar include.
//
// Both publication targets come from messages.source in the project's loaded
// hack manifest. The paths must remain within the project root after symlink
// resolution. No ROM is opened or mutated by this service.
absl::StatusOr<MessageSourceSyncResult> SyncMessageSource(
    const project::YazeProject& project,
    const std::filesystem::path& incoming_bundle_path,
    const MessageSourceSyncOptions& options = {});

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_SOURCE_SYNC_H_
