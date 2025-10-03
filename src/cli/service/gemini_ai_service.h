#ifndef YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_
#define YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai_service.h"
#include "cli/service/prompt_builder.h"

namespace yaze {
namespace cli {

struct GeminiConfig {
  std::string api_key;
  std::string model = "gemini-2.5-flash";  // Default to flash model
  float temperature = 0.7f;
  int max_output_tokens = 2048;
  std::string system_instruction;
  bool use_enhanced_prompting = true;  // Enable few-shot examples
  
  GeminiConfig() = default;
  explicit GeminiConfig(const std::string& key) : api_key(key) {}
};

class GeminiAIService : public AIService {
 public:
  explicit GeminiAIService(const GeminiConfig& config);
  
  // Primary interface
  absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) override;
  
  // Health check
  absl::Status CheckAvailability();

 private:
  std::string BuildSystemInstruction();
  absl::StatusOr<std::vector<std::string>> ParseGeminiResponse(
      const std::string& response_body);
  
  GeminiConfig config_;
  PromptBuilder prompt_builder_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_