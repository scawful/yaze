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

struct AgentConfig {
  int max_tool_iterations = 4;  // Maximum number of tool calling iterations
  int max_retry_attempts = 3;   // Maximum retries on errors
  bool verbose = false;          // Enable verbose diagnostic output
  bool show_reasoning = true;    // Show LLM reasoning in output
};

class ConversationalAgentService {
 public:
  ConversationalAgentService();
  explicit ConversationalAgentService(const AgentConfig& config);

  // Send a message from the user and get the agent's response.
  absl::StatusOr<ChatMessage> SendMessage(const std::string& message);

  // Get the full chat history.
  const std::vector<ChatMessage>& GetHistory() const;

  // Provide the service with a ROM context for tool execution.
  void SetRomContext(Rom* rom);

  // Clear the current conversation history, preserving ROM/tool context.
  void ResetConversation();

  // Configuration
  void SetConfig(const AgentConfig& config) { config_ = config; }
  const AgentConfig& GetConfig() const { return config_; }

 private:
  std::vector<ChatMessage> history_;
  std::unique_ptr<AIService> ai_service_;
  ToolDispatcher tool_dispatcher_;
  Rom* rom_context_ = nullptr;
  AgentConfig config_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
