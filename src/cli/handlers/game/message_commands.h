#ifndef YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing messages
 */
class MessageListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-list"; }
  std::string GetDescription() const { return "List available messages"; }
  std::string GetUsage() const {
    return "message-list [--limit <limit>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for reading messages
 */
class MessageReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-read"; }
  std::string GetDescription() const { return "Read a specific message"; }
  std::string GetUsage() const {
    return "message-read --id <message_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for searching messages
 */
class MessageSearchCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "message-search"; }
  std::string GetDescription() const {
    return "Search messages by text content";
  }
  std::string GetUsage() const {
    return "message-search --query <query> [--limit <limit>] [--format "
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

#endif  // YAZE_SRC_CLI_HANDLERS_MESSAGE_COMMANDS_H_
