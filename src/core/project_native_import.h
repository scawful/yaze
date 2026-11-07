#ifndef YAZE_CORE_PROJECT_NATIVE_IMPORT_H_
#define YAZE_CORE_PROJECT_NATIVE_IMPORT_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"

namespace yaze::project {

// Adoption does not compile an engine or grant runtime qualification. The
// importer has already produced a complete image and its hash-bound receipt.
// A caller refreshing bundled code supplies a separately staged source tree;
// this operation never merges or replaces that tree.
struct NativeOverworldImportBuildSettings {
  std::string code_folder;
  std::string build_script;
  std::string build_target;
  std::string asm_entry_point;
  std::string hack_manifest_file;
};

struct NativeOverworldProjectImportPlan {
  YazeProject project;
  std::string expected_descriptor;
  std::vector<core::SourceArtifactUpdate> updates;
  std::string image_sha256;

  // Uses the normal descriptor/source transaction, including CAS, readback
  // and rollback. Existing native Save evidence becomes stale; only a real
  // subsequent editor Save may establish the new Build identity.
  absl::StatusOr<std::string> Publish(
      core::SourceArtifactReadbackValidator validator = {});
};

// Read-only preparation. The exact saved descriptor hash is required even for
// a dry run. Producer/proof references, write policy and unrelated settings
// remain intact. The legacy terrain file remains on disk as recovery data.
absl::StatusOr<NativeOverworldProjectImportPlan>
PrepareNativeOverworldProjectImport(
    const YazeProject& project, const std::string& prepared_rom,
    const std::string& import_receipt,
    const std::string& expected_descriptor_sha256,
    const NativeOverworldImportBuildSettings& build = {});

}  // namespace yaze::project

#endif  // YAZE_CORE_PROJECT_NATIVE_IMPORT_H_
