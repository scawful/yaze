#ifndef YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_
#define YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"

namespace yaze {
class Rom;

namespace cli {

enum class AgentPromptProfile {
  kStandard,
  kOracleOfSecrets,
};

struct AIServiceConfig {
  // "auto" (auto-detect configured provider), "gemini",
  // "anthropic", "openai", "ollama", or "mock".
  // Aliases: "claude" => anthropic, "chatgpt"/"lmstudio" => openai.
  std::string provider = "auto";
  // Provider-specific model name.
  std::string model;
  // For Gemini.
  std::string gemini_api_key;
  // For OpenAI (and LMStudio/compatible APIs).
  std::string openai_api_key;
  std::string openai_base_url = "https://api.openai.com";
  // For Anthropic.
  std::string anthropic_api_key;
  // For Ollama.
  std::string ollama_host = "http://localhost:11434";
  // Hint for ROM-aware prompt selection when a loaded ROM object is not yet
  // available.
  std::string rom_path_hint;
  // Optional loaded ROM for prompt-context and Oracle guide auto-detection.
  Rom* rom_context = nullptr;
  // Enable debug logging.
  bool verbose = false;
};

// Build service configuration from flags + environment fallbacks.
AIServiceConfig BuildAIServiceConfigFromFlags();

// Detect which prompt profile should be used for the current ROM context.
AgentPromptProfile DetectPromptProfile(const AIServiceConfig& config);

// Discover provider configs suitable for model-listing APIs. Returned configs
// always use canonical provider names and exclude unsupported aliases.
std::vector<AIServiceConfig> DiscoverModelRegistryConfigs(
    const AIServiceConfig& base_config);

// Create AI service using command-line flags
std::unique_ptr<AIService> CreateAIService();

// Create AI service with explicit configuration
std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config);
absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_SERVICE_FACTORY_H_
