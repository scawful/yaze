#ifndef YAZE_CORE_COMPILED_BUILD_RECEIPT_H_
#define YAZE_CORE_COMPILED_BUILD_RECEIPT_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json_fwd.hpp"

namespace yaze::project {
struct YazeProject;
}

namespace yaze::core {

// Outcome rule for Oracle's build receipt (Scripts/Build/build_receipt.py,
// schema 1): completed, exit 0 and no required gaps. A missing member throws
// nlohmann::json::exception like every other receipt field read.
bool IsSuccessfulBuildReceipt(const nlohmann::json& build);

struct CompiledBuildIdentity {
  std::string receipt_path;
  std::string receipt_sha256;
  std::string state;
  std::string base_rom_path;
  std::string base_rom_sha256;
  std::string output_rom_path;
  std::string output_rom_sha256;
  uint64_t output_rom_size = 0;
  // Empty when the receipt records no output manifest.
  std::string manifest_path;
  std::string manifest_sha256;
};

// Authenticates a receipt against an opened authoring project without writing
// anything. The receipt root must be the project code folder, its base ROM the
// live project ROM (which must still satisfy expected_hash), and its output the
// project build target; base, output and any recorded manifest are re-hashed.
// The source tree is not re-hashed, and the descriptor is bound only through
// its code folder and base ROM. Returns Unimplemented without JSON support.
absl::StatusOr<CompiledBuildIdentity> VerifyProjectBuildReceipt(
    const project::YazeProject& project,
    const std::filesystem::path& receipt_path);

// Requires loaded ROM bytes to be exactly the verified output.
absl::Status CheckCompiledBuildBytes(const CompiledBuildIdentity& build,
                                     const uint8_t* data, size_t size);

}  // namespace yaze::core

#endif  // YAZE_CORE_COMPILED_BUILD_RECEIPT_H_
