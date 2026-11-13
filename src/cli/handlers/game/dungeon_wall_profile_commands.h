#ifndef YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PROFILE_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PROFILE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Read-only selection/validation. Optional output is a fresh preview artifact,
// never a ROM patch, project mutation or native family dispatcher.
class DungeonWallProfileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-wall-profile"; }
  std::string GetUsage() const override {
    return "dungeon-wall-profile --source=<profiles.json> --room=<id> "
           "[--world=<0|1>] [--entrance=<id>] [--legacy-source=<patterns.asm>] "
           "[--output=<fresh-preview.asm>]";
  }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& context) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Dungeon Wall Profile"; }
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PROFILE_COMMANDS_H_
