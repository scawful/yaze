#include "cli/handlers/command_handlers.h"

#include <memory>
#include <vector>

#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/graphics/palette_commands.h"
#include "cli/handlers/agent/todo_commands.h"

namespace yaze {
namespace cli {
namespace handlers {

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateCliCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Graphics commands (supported in browser)
  handlers.push_back(std::make_unique<HexReadCommandHandler>());
  handlers.push_back(std::make_unique<HexWriteCommandHandler>());
  handlers.push_back(std::make_unique<HexSearchCommandHandler>());

  // Palette commands (supported in browser)
  handlers.push_back(std::make_unique<PaletteGetColorsCommandHandler>());
  handlers.push_back(std::make_unique<PaletteSetColorCommandHandler>());
  handlers.push_back(std::make_unique<PaletteAnalyzeCommandHandler>());

  return handlers;
}

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAgentCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Todo commands
  handlers.push_back(std::make_unique<TodoAddCommandHandler>());
  handlers.push_back(std::make_unique<TodoListCommandHandler>());
  handlers.push_back(std::make_unique<TodoUpdateCommandHandler>());
  handlers.push_back(std::make_unique<TodoCompleteCommandHandler>());
  
  // Note: Chat and other heavy agent commands are currently disabled in browser build
  // to avoid complex dependency chains (curl, threads, etc).

  return handlers;
}

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAllCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Add CLI handlers
  auto cli_handlers = CreateCliCommandHandlers();
  for (auto& handler : cli_handlers) {
    handlers.push_back(std::move(handler));
  }

  // Add agent handlers
  auto agent_handlers = CreateAgentCommandHandlers();
  for (auto& handler : agent_handlers) {
    handlers.push_back(std::move(handler));
  }

  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

