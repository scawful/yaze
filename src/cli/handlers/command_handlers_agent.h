#ifndef YAZE_SRC_CLI_HANDLERS_COMMAND_HANDLERS_AGENT_H_
#define YAZE_SRC_CLI_HANDLERS_COMMAND_HANDLERS_AGENT_H_

#include <memory>
#include <vector>

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAgentCommandHandlers();

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_COMMAND_HANDLERS_AGENT_H_
