#ifndef YAZE_SRC_CLI_HANDLERS_SPRITE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_SPRITE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing sprites
 */
class SpriteListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "sprite-list"; }
  std::string GetDescription() const {
    return "List available sprites";
  }
  std::string GetUsage() const {
    return "sprite-list [--type <type>] [--limit <limit>] [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting sprite properties
 */
class SpritePropertiesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "sprite-properties"; }
  std::string GetDescription() const {
    return "Get properties of a specific sprite";
  }
  std::string GetUsage() const {
    return "sprite-properties --id <sprite_id> [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting sprite palette information
 */
class SpritePaletteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "sprite-palette"; }
  std::string GetDescription() const {
    return "Get palette information for a sprite";
  }
  std::string GetUsage() const {
    return "sprite-palette --id <sprite_id> [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_SPRITE_COMMANDS_H_
