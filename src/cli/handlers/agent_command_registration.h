#ifndef YAZE_CLI_HANDLERS_AGENT_COMMAND_REGISTRATION_H_
#define YAZE_CLI_HANDLERS_AGENT_COMMAND_REGISTRATION_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace cli {
namespace handlers {

void RegisterAgentCommandHandlers();

// Runs `z3ed agent <subcommand> ...`; `args` starts at the subcommand.
absl::Status HandleAgentCommand(const std::vector<std::string>& args);

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_COMMAND_REGISTRATION_H_
