#ifndef YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/agent/tool_dispatcher.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

struct ChatMessage {
  enum class Sender { kUser, kAgent };
  struct TableData {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
  };
  Sender sender;
  std::string message;
  absl::Time timestamp;
  std::optional<std::string> json_pretty;
  std::optional<TableData> table_data;
};

class ConversationalAgentService {
 public:
  ConversationalAgentService();

  // Send a message from the user and get the agent's response.
  absl::StatusOr<ChatMessage> SendMessage(const std::string& message);

  // Get the full chat history.
  const std::vector<ChatMessage>& GetHistory() const;

  // Provide the service with a ROM context for tool execution.
  void SetRomContext(Rom* rom);

 private:
  std::vector<ChatMessage> history_;
  std::unique_ptr<AIService> ai_service_;
  ToolDispatcher tool_dispatcher_;
  Rom* rom_context_ = nullptr;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
