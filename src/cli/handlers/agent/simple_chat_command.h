#ifndef YAZE_CLI_HANDLERS_AGENT_SIMPLE_CHAT_COMMAND_H_
#define YAZE_CLI_HANDLERS_AGENT_SIMPLE_CHAT_COMMAND_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class SimpleChatCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "simple-chat"; }
  std::string GetDescription() const {
    return "Simple text-based chat with the AI agent.";
  }
  std::string GetUsage() const override {
    return "simple-chat [--prompt <message>] [--file <path>] [--format "
           "<format>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_SIMPLE_CHAT_COMMAND_H_
