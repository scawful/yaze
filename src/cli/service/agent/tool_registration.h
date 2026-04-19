#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_REGISTRATION_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_REGISTRATION_H_

namespace yaze {
namespace cli {
namespace agent {

class ToolRegistry;

void RegisterBuiltinAgentTools(ToolRegistry& registry);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_REGISTRATION_H_
