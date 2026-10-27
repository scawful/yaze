#ifndef YAZE_CORE_NATIVE_SAVE_RECEIPT_H_
#define YAZE_CORE_NATIVE_SAVE_RECEIPT_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::core {

struct NativeSaveArtifact {
  std::string role;
  std::filesystem::path path;
  std::string expected_sha256;
};

struct NativeSaveContext {
  std::filesystem::path path;
  std::string editing_source_path;
  std::string project_path;
};

// One pointer per canonical editing-source origin, keyed by its UTF-8 SHA-256.
// The app-data root is queried on each call so isolated callers stay isolated.
// These APIs return Unimplemented when JSON support is disabled.
absl::StatusOr<NativeSaveContext> GetNativeSaveContext(
    const std::string& editing_source_path, const std::string& project_path);

// Invalidates a previously committed transfer after a successful project-only
// relocation or session activation. A missing pointer remains absent. This
// records rebound state without advancing a native generation. The internal
// session_changed reason also serializes as rebound.
absl::Status RetireNativeSaveContext(const NativeSaveContext& context,
                                     const std::string& state);

struct NativeSaveReceiptRequest {
  std::filesystem::path receipt_path;
  std::string operation_id;
  std::string operation;
  std::vector<NativeSaveArtifact> artifacts;
  std::vector<std::string> binding_errors;
  std::optional<NativeSaveContext> context;
};

struct NativeSaveReceipt {
  uint64_t generation;
  std::string json;
};

// Records an already completed native save. All paths must be absolute, roles
// unique, and expected hashes lowercase SHA-256. Distinct roles may share an
// exact canonical path only with the same expected hash. The rom and producer
// roles are required. A new operation advances the persisted generation, even
// when artifact bytes are unchanged. Repeating its current operation_id is
// idempotent only when every binding and the operation are identical.
// Explicit binding_errors record incomplete optional project qualification;
// included artifacts still require exact hashes and required roles stay valid.
//
// Inputs are verified under the existing source-publication lock and again on
// readback. The optional process context is published with the receipt. Only
// receipt/context outputs are written or rolled back; a failure cannot undo
// the save that the caller has already completed. Browser builds retain the
// publisher's unsupported durable-lock behavior.
absl::StatusOr<NativeSaveReceipt> PublishNativeSaveReceipt(
    const NativeSaveReceiptRequest& request);

}  // namespace yaze::core

#endif  // YAZE_CORE_NATIVE_SAVE_RECEIPT_H_
