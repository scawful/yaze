#ifndef YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for getting palette colors
 */
class PaletteGetColorsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "palette-get-colors"; }
  std::string GetDescription() const {
    return "Get colors from a palette";
  }
  std::string GetUsage() const {
    return "palette-get-colors --palette <palette_id> [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"palette"});
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for setting palette colors
 */
class PaletteSetColorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "palette-set-color"; }
  std::string GetDescription() const {
    return "Set a color in a palette";
  }
  std::string GetUsage() const {
    return "palette-set-color --palette <palette_id> --index <index> --color <color> [--format <json|text>]";
  }
  
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"palette", "index", "color"});
  }
  
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for analyzing palettes
 */
class PaletteAnalyzeCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "palette-analyze"; }
  std::string GetDescription() const {
    return "Analyze palette colors and properties";
  }
  std::string GetUsage() const {
    return "palette-analyze [--palette <palette_id>] [--format <json|text>]";
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

#endif  // YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_
