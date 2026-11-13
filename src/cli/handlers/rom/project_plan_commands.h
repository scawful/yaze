#ifndef YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_PLAN_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_PLAN_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Prints a yaze.plan/1 file's SHA-256, stable entry IDs and agent status, so
// an agent can key a "yaze.plan.agent_update/1" document to the bytes it read.
class PlanReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "plan-read"; }
  std::string GetUsage() const override {
    return "plan-read (--plan <plan.json> | --project <file.yaze>) "
           "[--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

// Checks, diffs and applies an agent update to the plan's "agent_status"
// section through the shared source-CAS publisher. Human plan fields are never
// written. A stale base_sha256 is refused before anything is written.
class PlanAgentApplyCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "plan-agent-apply"; }
  std::string GetUsage() const override {
    return "plan-agent-apply (--plan <plan.json> | --project <file.yaze>) "
           "--update <update.json> [--dry-run] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_PLAN_COMMANDS_H_
