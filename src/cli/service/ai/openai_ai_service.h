#ifndef YAZE_SRC_CLI_OPENAI_AI_SERVICE_H_
#define YAZE_SRC_CLI_OPENAI_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"

#ifdef YAZE_AI_RUNTIME_AVAILABLE
#include "cli/service/ai/prompt_builder.h"
#endif

namespace yaze {
namespace cli {

struct OpenAIConfig {
  std::string api_key;
  std::string base_url = "https://api.openai.com";  // LMStudio: http://localhost:1234
  std::string model = "gpt-4o-mini";  // Default to cost-effective model
  float temperature = 0.7f;
  int max_output_tokens = 2048;
  std::string system_instruction;
  bool use_function_calling = true;
  std::string prompt_version = "v3";
  bool verbose = false;

  OpenAIConfig() = default;
  explicit OpenAIConfig(const std::string& key) : api_key(key) {}
};

#ifdef YAZE_AI_RUNTIME_AVAILABLE

class OpenAIAIService : public AIService {
 public:
  explicit OpenAIAIService(const OpenAIConfig& config);
  void SetRomContext(Rom* rom) override;

  // Primary interface
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;

  // Health check
  absl::Status CheckAvailability();

  // List available models from OpenAI API
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override;

  std::string GetProviderName() const override { return "openai"; }

  // Function calling support
  void EnableFunctionCalling(bool enable = true);
  std::vector<std::string> GetAvailableTools() const;

 private:
  std::string BuildSystemInstruction();
  std::string BuildFunctionCallSchemas();
  absl::StatusOr<AgentResponse> ParseOpenAIResponse(
      const std::string& response_body);

  bool function_calling_enabled_ = true;

  OpenAIConfig config_;
  PromptBuilder prompt_builder_;
};

#else  // !YAZE_AI_RUNTIME_AVAILABLE

// Stub implementation when AI runtime is disabled
class OpenAIAIService : public AIService {
 public:
  explicit OpenAIAIService(const OpenAIConfig&) {}
  void SetRomContext(Rom*) override {}
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override {
    return absl::FailedPreconditionError(
        "OpenAI AI runtime is disabled (prompt: " + prompt + ")");
  }
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>&) override {
    return absl::FailedPreconditionError("OpenAI AI runtime is disabled");
  }
  absl::Status CheckAvailability() {
    return absl::FailedPreconditionError("OpenAI AI runtime is disabled");
  }
  void EnableFunctionCalling(bool) {}
  std::vector<std::string> GetAvailableTools() const { return {}; }
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override {
    return absl::FailedPreconditionError("OpenAI AI runtime is disabled");
  }
  std::string GetProviderName() const override { return "openai"; }
};

#endif  // YAZE_AI_RUNTIME_AVAILABLE

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_OPENAI_AI_SERVICE_H_
