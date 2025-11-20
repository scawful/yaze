#ifndef YAZE_SRC_CLI_SERVICE_AGENT_SIMPLE_CHAT_SESSION_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_SIMPLE_CHAT_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/agent/vim_mode.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

/**
 * @class SimpleChatSession
 * @brief Simple text-based chat session for AI agent interaction
 * 
 * Provides a basic REPL-style interface without FTXUI dependencies,
 * suitable for automated testing and AI agent interactions.
 * 
 * Supports multiple input modes:
 * - Interactive REPL (default when stdin is a TTY)
 * - Piped input (reads lines from stdin)
 * - Batch file (reads lines from file)
 * - Single message (programmatic use)
 */
class SimpleChatSession {
 public:
  SimpleChatSession();

  // Set ROM context for tool execution
  void SetRomContext(Rom* rom);

  // Set agent configuration
  void SetConfig(const AgentConfig& config) {
    config_ = config;
    agent_service_.SetConfig(config);
  }

  // Send a single message and get response (blocking)
  absl::Status SendAndWaitForResponse(const std::string& message,
                                      std::string* response_out = nullptr);

  // Run interactive REPL mode (reads from stdin)
  // If stdin is piped, runs in quiet mode
  absl::Status RunInteractive();

  // Run batch mode from file (one message per line)
  absl::Status RunBatch(const std::string& input_file);

  // Get full conversation history
  const std::vector<ChatMessage>& GetHistory() const {
    return agent_service_.GetHistory();
  }

  // Clear conversation history
  void Reset() { agent_service_.ResetConversation(); }

 private:
  void PrintMessage(const ChatMessage& msg, bool show_timestamp = false);
  void PrintTable(const ChatMessage::TableData& table);
  std::string ReadLineWithVim();
  std::vector<std::string> GetAutocompleteOptions(const std::string& partial);

  ConversationalAgentService agent_service_;
  AgentConfig config_;
  std::unique_ptr<VimMode> vim_mode_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_SIMPLE_CHAT_SESSION_H_
