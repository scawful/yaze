#include "cli/service/agent/conversational_agent_service.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "cli/service/ai/service_factory.h"

namespace yaze {
namespace cli {
namespace agent {

ConversationalAgentService::ConversationalAgentService() {
  ai_service_ = CreateAIService();
}

absl::StatusOr<ChatMessage> ConversationalAgentService::SendMessage(
    const std::string& message) {
  // 1. Add user message to history.
  history_.push_back({ChatMessage::Sender::kUser, message, absl::Now()});

  // 2. Get response from the AI service using the full history.
  auto response_or = ai_service_->GenerateResponse(history_);
  if (!response_or.ok()) {
    return absl::InternalError(absl::StrCat("Failed to get AI response: ",
                                           response_or.status().message()));
  }

  const auto& agent_response = response_or.value();

  // For now, combine text and commands for display.
  // In the future, the TUI/GUI will handle these differently.
  std::string response_text = agent_response.text_response;
  if (!agent_response.commands.empty()) {
    response_text += "\n\nCommands:\n" + absl::StrJoin(agent_response.commands, "\n");
  }

  // If the agent requested a tool call, dispatch it.
  if (!agent_response.tool_calls.empty()) {
    for (const auto& tool_call : agent_response.tool_calls) {
      auto tool_result_or = tool_dispatcher_.Dispatch(tool_call);
      if (tool_result_or.ok()) {
        // Add the tool result to the history and send back to the AI.
        history_.push_back({ChatMessage::Sender::kAgent, tool_result_or.value(), absl::Now()});
        return SendMessage(""); // Re-prompt the AI with the new context.
      } else {
        // Handle tool execution error.
        return absl::InternalError(absl::StrCat("Tool execution failed: ", tool_result_or.status().message()));
      }
    }
  }

  ChatMessage chat_response = {ChatMessage::Sender::kAgent, response_text,
                               absl::Now()};

  // 3. Add agent response to history.
  history_.push_back(chat_response);

  return chat_response;
}

const std::vector<ChatMessage>& ConversationalAgentService::GetHistory() const {
  return history_;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
