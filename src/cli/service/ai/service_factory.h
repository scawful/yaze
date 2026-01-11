#ifndef YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_
#define YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"

namespace yaze {
namespace cli {

struct AIServiceConfig {
  // "auto" (try gemini→anthropic→openai→mock), "gemini", "anthropic",
  // "openai", "ollama", or "mock".
  std::string provider = "auto";
  // Provider-specific model name.
  std::string model;
  // For Gemini.
  std::string gemini_api_key;
  // For OpenAI.
  std::string openai_api_key;
  // For Anthropic.
  std::string anthropic_api_key;
  // For Ollama.
  std::string ollama_host = "http://localhost:11434";
  // Enable debug logging.
  bool verbose = false;
};

// Create AI service using command-line flags
std::unique_ptr<AIService> CreateAIService();

// Create AI service with explicit configuration
std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config);
absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_
