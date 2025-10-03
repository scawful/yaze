#ifndef YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"

namespace yaze {
namespace cli {
namespace agent {
struct ChatMessage;
}
// Abstract interface for AI services
class AIService {
 public:
  virtual ~AIService() = default;

  // Generate a response from a single prompt.
  virtual absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) = 0;

  // Generate a response from a conversation history.
  virtual absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) = 0;
};

// Mock implementation for testing
class MockAIService : public AIService {
 public:
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_
