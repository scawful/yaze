#include "cli/handlers/agent_command_registration.h"

#include <utility>

#include "cli/handlers/command_handlers_agent.h"
#include "cli/service/command_registry.h"

namespace yaze {
namespace cli {
namespace handlers {

void RegisterAgentCommandHandlers() {
  auto handlers = CreateAgentCommandHandlers();
  if (handlers.empty()) {
    return;
  }
  CommandRegistry::Instance().RegisterHandlers(std::move(handlers));
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
