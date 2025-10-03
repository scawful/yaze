#ifndef YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
#define YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/prompt_builder.h"

namespace yaze {
namespace cli {

// Ollama configuration for local LLM inference
struct OllamaConfig {
  std::string base_url = "http://localhost:11434";  // Default Ollama endpoint
  std::string model = "qwen2.5-coder:7b";           // Recommended for code generation
  float temperature = 0.1;                          // Low temp for deterministic commands
  int max_tokens = 2048;                            // Sufficient for command lists
  std::string system_prompt;                        // Injected from resource catalogue
  bool use_enhanced_prompting = true;               // Enable few-shot examples
};

class OllamaAIService : public AIService {
 public:
  explicit OllamaAIService(const OllamaConfig& config);
  
  // Generate z3ed commands from natural language prompt
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;
  
  // Health check: verify Ollama server is running and model is available
  absl::Status CheckAvailability();
  
  // List available models on Ollama server
  absl::StatusOr<std::vector<std::string>> ListAvailableModels();

 private:
  OllamaConfig config_;
  PromptBuilder prompt_builder_;
  
  // Build system prompt from resource catalogue
  std::string BuildSystemPrompt();
  
  // Parse JSON response from Ollama API
  absl::StatusOr<std::string> ParseOllamaResponse(const std::string& json_response);
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
