#ifndef YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {
struct ChatMessage;
}
// Abstract interface for AI services
class AIService {
 public:
  virtual ~AIService() = default;

  // Provide the AI service with the active ROM so prompts can include
  // project-specific context.
  virtual void SetRomContext(Rom* rom) { (void)rom; }

  // Generate a response from a single prompt.
  virtual absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) = 0;

  // Generate a response from a conversation history.
  virtual absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) = 0;

  // List available models for this service
  virtual absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() {
    return std::vector<ModelInfo>{};
  }

  // Get the provider name
  virtual std::string GetProviderName() const = 0;
};

// Mock implementation for testing
class MockAIService : public AIService {
 public:
  void SetRomContext(Rom* rom) override { (void)rom; }
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;

  std::string GetProviderName() const override { return "mock"; }

  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override {
    std::vector<ModelInfo> models;
    models.push_back({.name = "mock-model",
                      .display_name = "Mock Model",
                      .provider = "mock",
                      .description = "A mock model for testing",
                      .is_local = true});
    return models;
  }
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_AI_SERVICE_H_
