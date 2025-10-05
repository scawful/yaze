#ifndef YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_
#define YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/prompt_builder.h"

namespace yaze {
namespace cli {

struct GeminiConfig {
  std::string api_key;
  std::string model = "gemini-2.5-flash";  // Default to flash model
  float temperature = 0.7f;
  int max_output_tokens = 2048;
  mutable std::string system_instruction;  // Mutable to allow lazy initialization
  bool use_enhanced_prompting = true;  // Enable few-shot examples
  bool use_function_calling = true;  // Use native Gemini function calling (enabled by default for 2.0+)
  std::string prompt_version = "v3";  // Which prompt file to use (default, v2, v3, etc.)
  bool verbose = false;  // Enable debug logging
  
  GeminiConfig() = default;
  explicit GeminiConfig(const std::string& key) : api_key(key) {}
};

class GeminiAIService : public AIService {
 public:
  explicit GeminiAIService(const GeminiConfig& config);
  void SetRomContext(Rom* rom) override;
  
  // Primary interface
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;
  
  // Health check
  absl::Status CheckAvailability();
  
  // Function calling support
  void EnableFunctionCalling(bool enable = true);
  std::vector<std::string> GetAvailableTools() const;

  // Multimodal support (vision + text)
  absl::StatusOr<AgentResponse> GenerateMultimodalResponse(
      const std::string& image_path, const std::string& prompt);

 private:
  std::string BuildSystemInstruction();
  std::string BuildFunctionCallSchemas();
  absl::StatusOr<AgentResponse> ParseGeminiResponse(
      const std::string& response_body);
  
  // Helper for encoding images as base64
  absl::StatusOr<std::string> EncodeImageToBase64(
      const std::string& image_path) const;
  
  bool function_calling_enabled_ = true;
  
  GeminiConfig config_;
  PromptBuilder prompt_builder_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_