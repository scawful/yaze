#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_EDIT_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_EDIT_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Place a sprite in a dungeon room and optionally write to ROM.
 *
 * Dry-run by default (preview only). Use --write to commit + save ROM.
 */
class DungeonPlaceSpriteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-place-sprite"; }
  std::string GetDescription() const {
    return "Place a sprite in a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-place-sprite --room <hex> --id <hex> --x <int> --y <int> "
           "[--subtype <int>] [--layer <0|1>] "
           "[--write] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room", "id", "x", "y"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Remove a sprite from a dungeon room by index or position.
 *
 * Dry-run by default. Use --write to commit + save ROM.
 */
class DungeonRemoveSpriteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-remove-sprite"; }
  std::string GetDescription() const {
    return "Remove a sprite from a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-remove-sprite --room <hex> "
           "[--index <int> | --x <int> --y <int>] "
           "[--write] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Place a dungeon object (track segment, door, etc.) in a room.
 *
 * Dry-run by default. Use --write to commit + save ROM.
 */
class DungeonPlaceObjectCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-place-object"; }
  std::string GetDescription() const {
    return "Place a dungeon object in a room";
  }
  std::string GetUsage() const override {
    return "dungeon-place-object --room <hex> --id <hex> --x <int> --y <int> "
           "[--size <int>] [--layer <0|1|2>] "
           "[--write] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room", "id", "x", "y"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Set individual custom collision tiles in a dungeon room.
 *
 * Supports setting one or more tiles. Dry-run by default.
 * Use --write to commit + save ROM.
 */
class DungeonSetCollisionTileCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const override {
    return "dungeon-set-collision-tile";
  }
  std::string GetDescription() const {
    return "Set custom collision tiles in a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-set-collision-tile --room <hex> "
           "--tiles <x,y,tile>[;<x,y,tile>...] "
           "[--write] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room", "tiles"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_EDIT_COMMANDS_H_
