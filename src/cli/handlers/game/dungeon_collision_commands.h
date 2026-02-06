#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_COLLISION_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_COLLISION_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class DungeonListCustomCollisionCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const override {
    return "dungeon-list-custom-collision";
  }
  std::string GetDescription() const {
    return "List ZScream-style custom collision tiles for a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-list-custom-collision --room <room_id> "
           "[--tiles <hex,hex,...>] [--nonzero] [--all] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_COLLISION_COMMANDS_H_
