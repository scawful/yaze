#ifndef YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"

namespace yaze {
namespace cli {
namespace agent {

struct ChatMessage {
  enum class Sender { kUser, kAgent };
  Sender sender;
  std::string message;
  absl::Time timestamp;
};

class ConversationalAgentService {
 public:
  ConversationalAgentService();

  // Send a message from the user and get the agent's response.
  absl::StatusOr<ChatMessage> SendMessage(const std::string& message);

  // Get the full chat history.
  const std::vector<ChatMessage>& GetHistory() const;

 private:
  std::vector<ChatMessage> history_;
  std::unique_ptr<AIService> ai_service_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_CONVERSATIONAL_AGENT_SERVICE_H_
