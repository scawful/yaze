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
  if (message.empty() && history_.empty()) {
    return absl::InvalidArgumentError(
        "Conversation must start with a non-empty message.");
  }

  if (!message.empty()) {
    history_.push_back({ChatMessage::Sender::kUser, message, absl::Now()});
  }

  constexpr int kMaxToolIterations = 4;
  for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
    auto response_or = ai_service_->GenerateResponse(history_);
    if (!response_or.ok()) {
      return absl::InternalError(absl::StrCat(
          "Failed to get AI response: ", response_or.status().message()));
    }

    const auto& agent_response = response_or.value();

    if (!agent_response.tool_calls.empty()) {
      bool executed_tool = false;
      for (const auto& tool_call : agent_response.tool_calls) {
        auto tool_result_or = tool_dispatcher_.Dispatch(tool_call);
        if (!tool_result_or.ok()) {
          return absl::InternalError(absl::StrCat(
              "Tool execution failed: ", tool_result_or.status().message()));
        }

        const std::string& tool_output = tool_result_or.value();
        if (!tool_output.empty()) {
          history_.push_back(
              {ChatMessage::Sender::kAgent, tool_output, absl::Now()});
        }
        executed_tool = true;
      }

      if (executed_tool) {
        // Re-query the AI with updated context.
        continue;
      }
    }

    std::string response_text = agent_response.text_response;
    if (!agent_response.reasoning.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Reasoning: ");
      response_text.append(agent_response.reasoning);
    }
    if (!agent_response.commands.empty()) {
      if (!response_text.empty()) {
        response_text.append("\n\n");
      }
      response_text.append("Commands:\n");
      response_text.append(absl::StrJoin(agent_response.commands, "\n"));
    }

    ChatMessage chat_response = {ChatMessage::Sender::kAgent, response_text,
                                 absl::Now()};
    history_.push_back(chat_response);
    return chat_response;
  }

  return absl::InternalError(
      "Agent did not produce a response after executing tools.");
}

const std::vector<ChatMessage>& ConversationalAgentService::GetHistory() const {
  return history_;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
