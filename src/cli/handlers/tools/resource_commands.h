#ifndef YAZE_SRC_CLI_HANDLERS_RESOURCE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_RESOURCE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing resource labels by type
 */
class ResourceListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "resource-list"; }
  std::string GetDescription() const {
    return "List resource labels for a specific type";
  }
  std::string GetUsage() const {
    return "resource-list --type <type> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"type"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for searching resource labels
 */
class ResourceSearchCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "resource-search"; }
  std::string GetDescription() const {
    return "Search resource labels across all categories";
  }
  std::string GetUsage() const {
    return "resource-search --query <query> [--type <type>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"query"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_RESOURCE_COMMANDS_H_
