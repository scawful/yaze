#ifndef YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/agent/proposal_executor.h"
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
  struct ProposalSummary {
    std::string id;
    int change_count = 0;
    int executed_commands = 0;
    std::filesystem::path sandbox_rom_path;
    std::filesystem::path proposal_json_path;
  };
  Sender sender;
  std::string message;
  absl::Time timestamp;
  std::optional<std::string> json_pretty;
  std::optional<TableData> table_data;
  bool is_internal = false;  // True for tool results and other messages not meant for user display
  struct SessionMetrics {
    int turn_index = 0;
    int total_user_messages = 0;
    int total_agent_messages = 0;
    int total_tool_calls = 0;
    int total_commands = 0;
    int total_proposals = 0;
    double total_elapsed_seconds = 0.0;
    double average_latency_seconds = 0.0;
  };
  std::optional<SessionMetrics> metrics;
  std::optional<ProposalSummary> proposal;
};

enum class AgentOutputFormat {
  kFriendly,
  kCompact,
  kMarkdown,
  kJson
};

struct AgentConfig {
  int max_tool_iterations = 4;  // Maximum number of tool calling iterations
  int max_retry_attempts = 3;   // Maximum retries on errors
  bool verbose = false;          // Enable verbose diagnostic output
  bool show_reasoning = true;    // Show LLM reasoning in output
  size_t max_history_messages = 50;  // Maximum stored history messages per session
  bool trim_history = true;          // Whether to trim history beyond the limit
  bool enable_vim_mode = false;      // Enable vim-style line editing in simple-chat
  AgentOutputFormat output_format = AgentOutputFormat::kFriendly;
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

  ChatMessage::SessionMetrics GetMetrics() const;

  void ReplaceHistory(std::vector<ChatMessage> history);

 private:
  struct InternalMetrics {
    int user_messages = 0;
    int agent_messages = 0;
    int tool_calls = 0;
    int commands_generated = 0;
    int proposals_created = 0;
    int turns_completed = 0;
    absl::Duration total_latency = absl::ZeroDuration();
  };

  void TrimHistoryIfNeeded();
  ChatMessage::SessionMetrics BuildMetricsSnapshot() const;
  void RebuildMetricsFromHistory();

  std::vector<ChatMessage> history_;
  std::unique_ptr<AIService> ai_service_;
  ToolDispatcher tool_dispatcher_;
  Rom* rom_context_ = nullptr;
  AgentConfig config_;
  InternalMetrics metrics_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
