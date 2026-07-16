#ifndef YAZE_CLI_HANDLERS_GAME_DUNGEON_STREAM_PLAN_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_DUNGEON_STREAM_PLAN_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Read-only inventory, alias/overlap, and free-space diagnostics for
// manifest-owned dungeon streams. Despite its historical name, this command
// neither accepts replacements nor emits an immutable move/write plan; that
// replacement-aware output is pending.
class DungeonStreamPlanCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-stream-plan"; }
  std::string GetUsage() const override {
    return "dungeon-stream-plan --kind <objects|sprites|pot_items> "
           "--manifest <path> [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_CLI_HANDLERS_GAME_DUNGEON_STREAM_PLAN_COMMANDS_H_
