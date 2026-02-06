#ifndef YAZE_SRC_CLI_HANDLERS_DUNGEON_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DUNGEON_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing sprites in a dungeon room
 */
class DungeonListSpritesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-list-sprites"; }
  std::string GetDescription() const {
    return "List all sprites in a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-list-sprites --room <room_id> [--sprite-registry <path>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for describing a dungeon room
 */
class DungeonDescribeRoomCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-describe-room"; }
  std::string GetDescription() const {
    return "Get detailed description of a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-describe-room --room <room_id> [--sprite-registry <path>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing chest items in dungeon rooms
 */
class DungeonListChestsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-list-chests"; }
  std::string GetDescription() const {
    return "List chest contents for dungeon rooms";
  }
  std::string GetUsage() const {
    return "dungeon-list-chests [--room <room_id>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for inspecting a dungeon entrance
 */
class DungeonGetEntranceCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-get-entrance"; }
  std::string GetDescription() const {
    return "Get entrance metadata for a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-get-entrance --entrance <entrance_id> [--spawn] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for exporting room data
 */
class DungeonExportRoomCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-export-room"; }
  std::string GetDescription() const {
    return "Export room data to JSON format";
  }
  std::string GetUsage() const {
    return "dungeon-export-room --room <room_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing objects in a room
 */
class DungeonListObjectsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-list-objects"; }
  std::string GetDescription() const {
    return "List all objects in a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-list-objects --room <room_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting room tiles
 */
class DungeonGetRoomTilesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-get-room-tiles"; }
  std::string GetDescription() const {
    return "Get tile data for a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-get-room-tiles --room <room_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for setting room properties
 */
class DungeonSetRoomPropertyCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-set-room-property"; }
  std::string GetDescription() const {
    return "Set a property on a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-set-room-property --room <room_id> --property <property> "
           "--value <value> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room", "property", "value"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for reading raw room header bytes (diagnostic)
 */
class DungeonRoomHeaderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-room-header"; }
  std::string GetDescription() const {
    return "Read raw room header bytes for debugging ROM address issues";
  }
  std::string GetUsage() const {
    return "dungeon-room-header --room <room_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Generate track collision data from rail objects in a room.
 *
 * Reads Object 0x31 (rail) positions, classifies tiles by neighbor
 * connectivity, and optionally writes the collision map to ROM.
 * Dry-run by default (preview only). Use --write to commit.
 */
class DungeonGenerateTrackCollisionCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const {
    return "dungeon-generate-track-collision";
  }
  std::string GetDescription() const {
    return "Generate collision overlay from rail objects for minecart rooms";
  }
  std::string GetUsage() const {
    return "dungeon-generate-track-collision --room <room_id> "
           "[--write] [--visualize] [--promote-switch X,Y] "
           "[--format <json|text>]";
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

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_COMMANDS_H_
