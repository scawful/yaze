#ifndef YAZE_CLI_HANDLERS_GAME_DUNGEON_SPATIAL_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_DUNGEON_SPATIAL_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::zelda3 {
class Room;
}

namespace yaze::cli::handlers {

absl::Status WriteRoomSpatialReport(Rom* rom, const zelda3::Room& room,
                                    const resources::ArgumentParser& parser,
                                    resources::OutputFormatter& formatter);

class DungeonTemplatePlanCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-template-plan"; }
  std::string GetUsage() const override {
    return "dungeon-template-plan --room <hex> --template <file.json> "
           "--x <tile8> --y <tile8> [--clearance "
           "<x,y,width,height[,upper|lower|both]>] "
           "[--allow-existing <indices>] [--allow-pairs "
           "<template:existing,...>] "
           "[--cells] [--project-context <project>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_CLI_HANDLERS_GAME_DUNGEON_SPATIAL_COMMANDS_H_
