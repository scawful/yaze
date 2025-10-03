#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_

#include <string>
#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

class ToolDispatcher {
 public:
  ToolDispatcher() = default;

  // Execute a tool call and return the result as a string.
  absl::StatusOr<std::string> Dispatch(const ToolCall& tool_call);
   // Provide a ROM context for tool calls that require ROM access.
   void SetRomContext(Rom* rom) { rom_context_ = rom; }

 private:
   Rom* rom_context_ = nullptr;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
