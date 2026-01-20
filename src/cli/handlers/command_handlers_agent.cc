#include "cli/handlers/command_handlers_agent.h"

#include <memory>
#include <vector>

#ifndef __EMSCRIPTEN__
#include "cli/handlers/agent/simple_chat_command.h"
#endif

namespace yaze {
namespace cli {
namespace handlers {

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAgentCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

#ifndef __EMSCRIPTEN__
  handlers.push_back(std::make_unique<SimpleChatCommandHandler>());
#endif

  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
