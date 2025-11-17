#ifndef YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
#define YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "cli/service/ai/ai_service.h"

#ifdef YAZE_AI_RUNTIME_AVAILABLE
#include "cli/service/ai/prompt_builder.h"
#endif

namespace yaze {
namespace cli {

// Ollama configuration for local LLM inference
struct OllamaConfig {
  std::string base_url = "http://localhost:11434";  // Default Ollama endpoint
  std::string model = "qwen2.5-coder:0.5b";         // Lightweight default with tool-calling
  float temperature = 0.1;                          // Low temp for deterministic commands
  int max_tokens = 2048;                            // Sufficient for command lists
  std::string system_prompt;                        // Injected from resource catalogue
  bool use_enhanced_prompting = true;               // Enable few-shot examples
  float top_p = 0.92f;
  int top_k = 40;
  int num_ctx = 4096;
  bool stream = false;
  bool use_chat_completions = true;
  std::vector<std::string> favorite_models;
};

#ifdef YAZE_AI_RUNTIME_AVAILABLE

class OllamaAIService : public AIService {
 public:
  explicit OllamaAIService(const OllamaConfig& config);

  struct ModelInfo {
    std::string name;
    std::string digest;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
    uint64_t size_bytes = 0;
    absl::Time modified_at = absl::InfinitePast();
  };

  void SetRomContext(Rom* rom) override;
  
  // Generate z3ed commands from natural language prompt
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;
  
  // Health check: verify Ollama server is running and model is available
  absl::Status CheckAvailability();
  
  // List available models on Ollama server
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels();

 private:
  OllamaConfig config_;
  PromptBuilder prompt_builder_;
  
  // Build system prompt from resource catalogue
  std::string BuildSystemPrompt();
  
  // Parse JSON response from Ollama API
  absl::StatusOr<std::string> ParseOllamaResponse(const std::string& json_response);
};

#else  // !YAZE_AI_RUNTIME_AVAILABLE

class OllamaAIService : public AIService {
 public:
  struct ModelInfo {
    std::string name;
    std::string digest;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
    uint64_t size_bytes = 0;
    absl::Time modified_at = absl::InfinitePast();
  };

  explicit OllamaAIService(const OllamaConfig&) {}
  void SetRomContext(Rom*) override {}
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string&) override {
    return absl::FailedPreconditionError(
        "Ollama AI runtime is disabled");
  }
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>&) override {
    return absl::FailedPreconditionError(
        "Ollama AI runtime is disabled");
  }
  absl::Status CheckAvailability() {
    return absl::FailedPreconditionError(
        "Ollama AI runtime is disabled");
  }
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() {
    return absl::FailedPreconditionError(
        "Ollama AI runtime is disabled");
  }
};

#endif  // YAZE_AI_RUNTIME_AVAILABLE

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
