#ifndef YAZE_SRC_CLI_HANDLERS_GUI_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GUI_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for placing tiles via GUI automation
 */
class GuiPlaceTileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "gui-place-tile"; }
  std::string GetDescription() const {
    return "Place a tile at specific coordinates using GUI automation";
  }
  std::string GetUsage() const {
    return "gui-place-tile --tile <tile_id> --x <x> --y <y> [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"tile", "x", "y"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for clicking GUI elements
 */
class GuiClickCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "gui-click"; }
  std::string GetDescription() const {
    return "Click on a GUI element using automation";
  }
  std::string GetUsage() const {
    return "gui-click --target <target> [--click-type <left|right|middle>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"target"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for discovering GUI tools
 */
class GuiDiscoverToolCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "gui-discover-tool"; }
  std::string GetDescription() const {
    return "Discover available GUI tools and widgets";
  }
  std::string GetUsage() const {
    return "gui-discover-tool [--window <window>] [--type <type>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for taking screenshots
 */
class GuiScreenshotCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "gui-screenshot"; }
  std::string GetDescription() const { return "Take a screenshot of the GUI"; }
  std::string GetUsage() const {
    return "gui-screenshot [--region <region>] [--format <format>] [--format "
           "<json|text>]";
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

#endif  // YAZE_SRC_CLI_HANDLERS_GUI_COMMANDS_H_
