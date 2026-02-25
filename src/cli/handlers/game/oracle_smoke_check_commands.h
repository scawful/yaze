#ifndef YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_SMOKE_CHECK_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_SMOKE_CHECK_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Consolidated Oracle ROM smoke check covering three subsystems:
//   D4 Zora Temple  — structural water-fill preflight + room readiness
//   D6 Goron Mines  — minecart audit on 4 known rooms (0xA8,0xB8,0xD8,0xDA)
//   D3 Kalyxo Castle — prison room 0x32 collision readiness
//
// Default mode: fail only on structural issues (missing water-fill region,
//   corrupted table, etc.).
// Strict mode (--strict-readiness): also fail when D4/D3 rooms lack authored
//   collision data.
// D6 gate (--min-d6-track-rooms N): fail when fewer than N D6 rooms have
//   track rail objects; treated as a structural failure.
//
// Output: one JSON object with ok/status/checks fields.
// See docs/internal/agents/rom-safety-guardrails.md for pass/fail semantics.
class OracleSmokeCheckCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "oracle-smoke-check"; }
  std::string GetUsage() const override {
    return "oracle-smoke-check [--strict-readiness] "
           "[--min-d6-track-rooms <N>] [--report <path>] "
           "[--format <json|text>]";
  }

  Descriptor Describe() const override;

  // Probes --report path writability before the formatter starts — zero-stdout
  // semantics on failure (same contract as dungeon-oracle-preflight).
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_ORACLE_SMOKE_CHECK_COMMANDS_H_
