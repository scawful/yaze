#ifndef YAZE_SRC_CLI_HANDLERS_DIALOGUE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DIALOGUE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing dialogue messages
 */
class DialogueListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dialogue-list"; }
  std::string GetDescription() const {
    return "List dialogue messages with previews";
  }
  std::string GetUsage() const {
    return "dialogue-list [--limit <limit>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for reading dialogue messages
 */
class DialogueReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dialogue-read"; }
  std::string GetDescription() const {
    return "Read a specific dialogue message";
  }
  std::string GetUsage() const {
    return "dialogue-read --id <message_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for searching dialogue messages
 */
class DialogueSearchCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dialogue-search"; }
  std::string GetDescription() const {
    return "Search dialogue messages by text content";
  }
  std::string GetUsage() const {
    return "dialogue-search --query <query> [--limit <limit>] [--format "
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

#endif  // YAZE_SRC_CLI_HANDLERS_DIALOGUE_COMMANDS_H_
