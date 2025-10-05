#ifndef YAZE_CLI_HANDLERS_AGENT_TODO_COMMANDS_H_
#define YAZE_CLI_HANDLERS_AGENT_TODO_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Handle z3ed agent todo commands
 * 
 * Commands:
 *   agent todo create <description> [--category=<cat>] [--priority=<n>]
 *   agent todo list [--status=<status>] [--category=<cat>]
 *   agent todo update <id> --status=<status>
 *   agent todo show <id>
 *   agent todo delete <id>
 *   agent todo clear-completed
 *   agent todo next
 *   agent todo plan
 */
absl::Status HandleTodoCommand(const std::vector<std::string>& args);

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_TODO_COMMANDS_H_
