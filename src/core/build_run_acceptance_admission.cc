#include "core/build_run_acceptance_admission.h"

#include "zelda3/common.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze::core {
namespace fs = std::filesystem;

#if defined(YAZE_WITH_JSON)
absl::StatusOr<BuildRunAcceptanceConfig> ParseBuildRunAcceptanceConfig(
    const nlohmann::json& fixture) {
  if (!fixture.is_object()) {
    return absl::InvalidArgumentError(
        "build-run-native.json must be a JSON object");
  }
  BuildRunAcceptanceConfig config;
  const std::string workflow = fixture.value("workflow", "");
  if (workflow.empty() || workflow == "build_run") {
    config.workflow = BuildRunWorkflow::kLegacy;
  } else if (workflow == "proposal_edit") {
    config.workflow = BuildRunWorkflow::kProposalEdit;
  } else {
    return absl::InvalidArgumentError(
        "workflow must be omitted, build_run, or proposal_edit");
  }
  config.phase = fixture.value("phase", "");
  if (config.phase != "edit" && config.phase != "reopen") {
    return absl::InvalidArgumentError("phase must be edit or reopen");
  }
  config.map_id = fixture.value("map_id", -1);
  config.x = fixture.value("x", -1);
  config.y = fixture.value("y", -1);
  config.brush_id = fixture.value("brush_id", -1);
  config.original_tile = fixture.value("original_tile", -1);
  config.input_rom_sha256 = fixture.value("input_rom_sha256", "");
  config.expected_rom_sha256 = fixture.value("expected_rom_sha256", "");
  config.wait_for_window_observation =
      fixture.value("wait_for_window_observation", false);
  config.has_extra_cells = fixture.contains("extra_cells");

  if (config.map_id < 0 || config.map_id >= zelda3::kExpandedMapCount) {
    return absl::InvalidArgumentError("map_id out of range");
  }
  if (config.x < 0 || config.x >= 32 || config.y < 0 || config.y >= 32) {
    return absl::InvalidArgumentError("local cell out of range");
  }
  if (config.brush_id < 0 || config.brush_id > 0xFFF ||
      config.original_tile < 0 || config.original_tile > 0xFFF) {
    return absl::InvalidArgumentError("tile id out of range");
  }

  if (config.workflow == BuildRunWorkflow::kProposalEdit) {
    const std::string root = fixture.value("proposal_root", "");
    if (root.empty()) {
      return absl::InvalidArgumentError(
          "proposal_edit requires absolute proposal_root");
    }
    const fs::path proposal_root(root);
    if (!proposal_root.is_absolute()) {
      return absl::InvalidArgumentError(
          "proposal_edit requires absolute proposal_root");
    }
    config.proposal_root = proposal_root;
    if (config.has_extra_cells) {
      return absl::InvalidArgumentError("proposal_edit refuses extra_cells");
    }
    if (config.phase == "edit" && config.input_rom_sha256.empty()) {
      return absl::InvalidArgumentError(
          "proposal_edit edit requires input_rom_sha256");
    }
    if (config.phase == "reopen" && config.expected_rom_sha256.empty()) {
      return absl::InvalidArgumentError(
          "proposal_edit reopen requires expected_rom_sha256");
    }
  } else if (config.phase == "edit" && config.input_rom_sha256.empty()) {
    return absl::InvalidArgumentError("legacy edit requires input_rom_sha256");
  }
  return config;
}
#endif

absl::Status ValidateLegacyBuildRunProject(std::string_view project_name,
                                           const fs::path& rom_path,
                                           const fs::path& project_directory) {
  if (project_name.find("[Build Run acceptance]") == std::string_view::npos) {
    return absl::FailedPreconditionError(
        "Legacy build/run acceptance requires [Build Run acceptance] project");
  }
  std::error_code ec;
  const auto resolved_rom = fs::weakly_canonical(rom_path, ec);
  if (ec) {
    return absl::FailedPreconditionError("Cannot resolve legacy editing ROM");
  }
  const auto resolved_project_dir = fs::weakly_canonical(project_directory, ec);
  if (ec) {
    return absl::FailedPreconditionError(
        "Cannot resolve legacy project directory");
  }
  if (resolved_rom.parent_path() != resolved_project_dir) {
    return absl::FailedPreconditionError(
        "Legacy editing ROM must live in the project directory");
  }
  if (resolved_rom.filename() != "editing.sfc") {
    return absl::FailedPreconditionError(
        "Legacy build/run acceptance requires editing.sfc");
  }
  return absl::OkStatus();
}

absl::Status ValidateProposalEditAdmission(
    const BuildRunAcceptanceConfig& config, const ProposalWorkspace& workspace,
    const fs::path& loaded_project, const fs::path& loaded_rom,
    std::string_view loaded_rom_sha256) {
  if (config.workflow != BuildRunWorkflow::kProposalEdit) {
    return absl::InvalidArgumentError(
        "ValidateProposalEditAdmission requires proposal_edit workflow");
  }
  std::error_code ec;
  const auto project = fs::weakly_canonical(loaded_project, ec);
  if (ec) {
    return absl::FailedPreconditionError("Cannot resolve loaded project");
  }
  const auto rom = fs::weakly_canonical(loaded_rom, ec);
  if (ec) {
    return absl::FailedPreconditionError("Cannot resolve loaded ROM");
  }
  if (project != workspace.project_file) {
    return absl::FailedPreconditionError(
        "Loaded project is not the proposal draft project");
  }
  if (rom != workspace.rom_file) {
    return absl::FailedPreconditionError(
        "Loaded ROM is not the proposal draft ROM");
  }
  if (workspace.domain != "overworld") {
    return absl::FailedPreconditionError(
        "proposal_edit fixture requires overworld domain");
  }
  if (workspace.target != config.map_id) {
    return absl::FailedPreconditionError(
        "proposal_edit map_id must match packet target");
  }
  if (config.phase == "edit") {
    if (loaded_rom_sha256 != config.input_rom_sha256) {
      return absl::FailedPreconditionError(
          "Loaded ROM hash does not match input_rom_sha256");
    }
    if (loaded_rom_sha256 != workspace.baseline_rom_sha256) {
      return absl::FailedPreconditionError(
          "Loaded ROM hash does not match proposal baseline_rom_sha256");
    }
  } else if (loaded_rom_sha256 != config.expected_rom_sha256) {
    return absl::FailedPreconditionError(
        "Loaded ROM hash does not match expected_rom_sha256");
  }
  return absl::OkStatus();
}

}  // namespace yaze::core
