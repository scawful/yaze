#ifndef YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for finding tiles in overworld
 */
class OverworldFindTileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-find-tile"; }
  std::string GetDescription() const {
    return "Find tiles by ID in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-find-tile --tile <tile_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"tile"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for describing overworld maps
 */
class OverworldDescribeMapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-describe-map"; }
  std::string GetDescription() const {
    return "Get detailed description of an overworld map";
  }
  std::string GetUsage() const {
    return "overworld-describe-map --screen <screen_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"screen"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing warps in overworld
 */
class OverworldListWarpsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-warps"; }
  std::string GetDescription() const {
    return "List all warps in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-warps [--screen <screen_id>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing sprites in overworld
 */
class OverworldListSpritesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-sprites"; }
  std::string GetDescription() const {
    return "List all sprites in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-sprites [--screen <screen_id>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing items in overworld
 */
class OverworldListItemsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-items"; }
  std::string GetDescription() const {
    return "List all hidden item placements in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-items [--screen <screen_id>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting entrance information
 */
class OverworldGetEntranceCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-get-entrance"; }
  std::string GetDescription() const {
    return "Get entrance information from overworld";
  }
  std::string GetUsage() const {
    return "overworld-get-entrance --entrance <entrance_id> [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting tile statistics
 */
class OverworldTileStatsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-tile-stats"; }
  std::string GetDescription() const {
    return "Get tile usage statistics for overworld";
  }
  std::string GetUsage() const {
    return "overworld-tile-stats [--screen <screen_id>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_
