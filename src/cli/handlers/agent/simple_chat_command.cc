#include "cli/handlers/agent/simple_chat_command.h"
#include "cli/service/agent/simple_chat_session.h"

namespace yaze {
namespace cli {
namespace handlers {

absl::Status SimpleChatCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  
  agent::SimpleChatSession session;
  session.SetRomContext(rom);
  
  // Configure session from parser
  agent::AgentConfig config;
  if (parser.HasFlag("verbose")) {
    config.verbose = true;
  }
  if (auto format = parser.GetString("format")) {
      // Simplified format handling
  }
  session.SetConfig(config);

  if (auto file = parser.GetString("file")) {
    return session.RunBatch(*file);
  } else if (auto prompt = parser.GetString("prompt")) {
    std::string response;
    return session.SendAndWaitForResponse(*prompt, &response);
  } else {
    return session.RunInteractive();
  }
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
