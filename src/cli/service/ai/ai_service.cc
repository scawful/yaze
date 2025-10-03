#include "cli/service/ai/ai_service.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace cli {

absl::StatusOr<AgentResponse> MockAIService::GenerateResponse(
    const std::string& prompt) {
  AgentResponse response;
  if (prompt == "Place a tree") {
    response.text_response = "Sure, I can do that. Here is the command:";
    response.commands.push_back("overworld set-tile 0 10 20 0x02E");
    response.reasoning = "The user asked to place a tree, so I generated the appropriate `set-tile` command.";
  } else {
    response.text_response = "I'm sorry, I don't understand that prompt. Try 'Place a tree'.";
  }
  return response;
}

absl::StatusOr<AgentResponse> MockAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
  if (history.empty()) {
    return absl::InvalidArgumentError("History cannot be empty.");
  }
  return GenerateResponse(history.back().message);
}

}  // namespace cli
}  // namespace yaze

