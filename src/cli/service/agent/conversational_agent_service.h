#ifndef YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/service_factory.h"
#include "cli/service/agent/proposal_executor.h"
#include "cli/service/agent/tool_dispatcher.h"
// Advanced features (only available when Z3ED_AI=ON)
#ifdef Z3ED_AI
#include "cli/service/agent/learned_knowledge_service.h"
#include "cli/service/agent/todo_manager.h"
#include "cli/service/agent/advanced_routing.h"
#include "cli/service/agent/agent_pretraining.h"
#endif

#ifdef SendMessage
#undef SendMessage
#endif

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
  std::vector<std::string> warnings;
  struct ModelMetadata {
    std::string provider;
    std::string model;
    double latency_seconds = 0.0;
    int tool_iterations = 0;
    std::vector<std::string> tool_names;
    std::map<std::string, std::string> parameters;
  };
  std::optional<ModelMetadata> model_metadata;
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
  absl::Status ConfigureProvider(const AIServiceConfig& config);
  const AIServiceConfig& provider_config() const { return provider_config_; }
  void SetToolPreferences(const ToolDispatcher::ToolPreferences& prefs);

  ChatMessage::SessionMetrics GetMetrics() const;

  void ReplaceHistory(std::vector<ChatMessage> history);
  
#ifdef Z3ED_AI
  // Advanced Features Access (only when Z3ED_AI=ON)
  LearnedKnowledgeService& learned_knowledge() { return learned_knowledge_; }
  TodoManager& todo_manager() { return todo_manager_; }
  
  // Inject learned context into next message
  void EnableContextInjection(bool enable) { inject_learned_context_ = enable; }
  void EnablePretraining(bool enable) { inject_pretraining_ = enable; }
#endif

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
  
#ifdef Z3ED_AI
  // Context enhancement (only when Z3ED_AI=ON)
  std::string BuildEnhancedPrompt(const std::string& user_message);
  std::string InjectLearnedContext(const std::string& message);
  std::string InjectPretraining();
  
  // Response enhancement
  ChatMessage EnhanceResponse(const ChatMessage& response, const std::string& user_message);
#endif

  std::vector<ChatMessage> history_;
  std::unique_ptr<AIService> ai_service_;
  ToolDispatcher tool_dispatcher_;
  ToolDispatcher::ToolPreferences tool_preferences_;
  AIServiceConfig provider_config_;
  Rom* rom_context_ = nullptr;
  AgentConfig config_;
  InternalMetrics metrics_;
  
#ifdef Z3ED_AI
  // Advanced features (only when Z3ED_AI=ON)
  LearnedKnowledgeService learned_knowledge_;
  TodoManager todo_manager_;
  bool inject_learned_context_ = true;
  bool inject_pretraining_ = false;  // One-time injection on first message
  bool pretraining_injected_ = false;
#endif
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
