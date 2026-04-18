#include "app/editor/oracle/oracle_hack_workflow_backend.h"

#include <vector>

#include "app/emu/mesen/mesen_socket_client.h"
#include "cli/handlers/game/oracle_menu_commands.h"
#include "cli/handlers/game/oracle_smoke_check_commands.h"
#include "core/hack_manifest.h"
#include "core/oracle_progression_loader.h"
#include "core/project.h"

namespace yaze::editor {
namespace {

absl::StatusOr<core::OracleProgressionState> ReadOracleProgressionState(
    emu::mesen::MesenSocketClient& client) {
  constexpr uint32_t kBaseAddress = 0x7EF000;
  constexpr uint16_t kStartOffset = core::OracleProgressionState::kPendantOffset;
  constexpr uint16_t kEndOffset = core::OracleProgressionState::kSideQuestOffset;
  constexpr size_t kReadLength = kEndOffset - kStartOffset + 1;
  constexpr uint32_t kReadAddress = kBaseAddress + kStartOffset;

  auto bytes_or = client.ReadBlock(kReadAddress, kReadLength);
  if (!bytes_or.ok()) {
    return bytes_or.status();
  }
  if (bytes_or->size() < kReadLength) {
    return absl::DataLossError("SRAM read returned truncated data");
  }

  core::OracleProgressionState state;
  const auto read_byte = [&](uint16_t offset) -> uint8_t {
    return (*bytes_or)[offset - kStartOffset];
  };

  state.pendants = read_byte(core::OracleProgressionState::kPendantOffset);
  state.crystal_bitfield =
      read_byte(core::OracleProgressionState::kCrystalOffset);
  state.game_state = read_byte(core::OracleProgressionState::kGameStateOffset);
  state.oosprog2 = read_byte(core::OracleProgressionState::kOosProg2Offset);
  state.oosprog = read_byte(core::OracleProgressionState::kOosProgOffset);
  state.side_quest = read_byte(core::OracleProgressionState::kSideQuestOffset);
  return state;
}

}  // namespace

std::string OracleHackWorkflowBackend::GetBackendId() const {
  return "oracle";
}

oracle_validation::OracleRunResult OracleHackWorkflowBackend::RunValidation(
    oracle_validation::RunMode mode,
    const oracle_validation::SmokeOptions& smoke_options,
    const oracle_validation::PreflightOptions& preflight_options,
    Rom* rom_context) const {
  oracle_validation::OracleRunResult result;
  result.mode = mode;
  result.timestamp = oracle_validation::CurrentTimestamp();

  if (mode == oracle_validation::RunMode::kPreflight) {
    auto args = oracle_validation::BuildPreflightArgs(preflight_options);
    result.cli_command =
        oracle_validation::BuildCliCommand("dungeon-oracle-preflight", args);

    cli::handlers::DungeonOraclePreflightCommandHandler handler;
    auto status = handler.Run(args, rom_context, &result.raw_output);
    result.command_ok =
        status.ok() || status.code() == absl::StatusCode::kFailedPrecondition;
    result.status_code = status.code();
    if (!status.ok() && status.code() != absl::StatusCode::kFailedPrecondition) {
      result.error_message = std::string(status.message());
      return result;
    }

    auto parsed = oracle_validation::ParsePreflightOutput(result.raw_output);
    if (parsed.ok()) {
      result.preflight = *parsed;
    } else {
      result.json_parse_failed = true;
    }
    return result;
  }

  auto args = oracle_validation::BuildSmokeArgs(smoke_options);
  result.cli_command =
      oracle_validation::BuildCliCommand("oracle-smoke-check", args);

  cli::handlers::OracleSmokeCheckCommandHandler handler;
  auto status = handler.Run(args, rom_context, &result.raw_output);
  result.command_ok =
      status.ok() || status.code() == absl::StatusCode::kFailedPrecondition;
  result.status_code = status.code();
  if (!status.ok() && status.code() != absl::StatusCode::kFailedPrecondition) {
    result.error_message = std::string(status.message());
    return result;
  }

  auto parsed = oracle_validation::ParseSmokeCheckOutput(result.raw_output);
  if (parsed.ok()) {
    result.smoke = *parsed;
  } else {
    result.json_parse_failed = true;
  }
  return result;
}

core::HackManifest* OracleHackWorkflowBackend::ResolveManifest(
    project::YazeProject* project) const {
  if (project && project->hack_manifest.loaded()) {
    return &project->hack_manifest;
  }
  return nullptr;
}

std::optional<core::OracleProgressionState>
OracleHackWorkflowBackend::GetProgressionState(
    const core::HackManifest& manifest) const {
  return manifest.oracle_progression_state();
}

void OracleHackWorkflowBackend::SetProgressionState(
    core::HackManifest& manifest, const core::OracleProgressionState& state) const {
  manifest.SetOracleProgressionState(state);
}

void OracleHackWorkflowBackend::ClearProgressionState(
    core::HackManifest& manifest) const {
  manifest.ClearOracleProgressionState();
}

absl::StatusOr<core::OracleProgressionState>
OracleHackWorkflowBackend::LoadProgressionStateFromFile(
    const std::string& filepath) const {
  return core::LoadOracleProgressionFromSrmFile(filepath);
}

absl::StatusOr<core::OracleProgressionState>
OracleHackWorkflowBackend::ReadProgressionStateFromLiveSram(
    emu::mesen::MesenSocketClient& client) const {
  return ReadOracleProgressionState(client);
}

const core::StoryEventGraph* OracleHackWorkflowBackend::GetStoryGraph(
    const core::HackManifest& manifest) const {
  if (!manifest.HasProjectRegistry()) {
    return nullptr;
  }
  return &manifest.project_registry().story_events;
}

}  // namespace yaze::editor
