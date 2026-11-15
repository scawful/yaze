#ifndef YAZE_CORE_BUILD_RUN_ACCEPTANCE_ADMISSION_H_
#define YAZE_CORE_BUILD_RUN_ACCEPTANCE_ADMISSION_H_

#include <filesystem>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/proposal_workspace.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json_fwd.hpp"
#endif

namespace yaze::core {

enum class BuildRunWorkflow {
  kLegacy = 0,
  kProposalEdit = 1,
};

struct BuildRunAcceptanceConfig {
  BuildRunWorkflow workflow = BuildRunWorkflow::kLegacy;
  std::string phase;
  std::filesystem::path proposal_root;
  int map_id = -1;
  int x = -1;
  int y = -1;
  int brush_id = -1;
  int original_tile = -1;
  std::string input_rom_sha256;
  std::string expected_rom_sha256;
  bool wait_for_window_observation = false;
  bool has_extra_cells = false;
};

#if defined(YAZE_WITH_JSON)
absl::StatusOr<BuildRunAcceptanceConfig> ParseBuildRunAcceptanceConfig(
    const nlohmann::json& fixture);
#endif

// Legacy build/run acceptance keeps the name + editing.sfc ROM guards.
absl::Status ValidateLegacyBuildRunProject(
    std::string_view project_name, const std::filesystem::path& rom_path,
    const std::filesystem::path& project_directory);

// Proposal edit mode requires a validated packet bound to the loaded draft.
absl::Status ValidateProposalEditAdmission(
    const BuildRunAcceptanceConfig& config, const ProposalWorkspace& workspace,
    const std::filesystem::path& loaded_project,
    const std::filesystem::path& loaded_rom,
    std::string_view loaded_rom_sha256);

}  // namespace yaze::core

#endif  // YAZE_CORE_BUILD_RUN_ACCEPTANCE_ADMISSION_H_
