#include "cli/service/ai/service_factory.h"

#include <cstring>
#include <iostream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/ollama_ai_service.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/gemini_ai_service.h"
#endif

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, prompt_version);
ABSL_DECLARE_FLAG(bool, use_function_calling);

namespace yaze {
namespace cli {

std::unique_ptr<AIService> CreateAIService() {
  // Read configuration from flags
  AIServiceConfig config;
  config.provider = absl::AsciiStrToLower(absl::GetFlag(FLAGS_ai_provider));
  config.model = absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = absl::GetFlag(FLAGS_gemini_api_key);
  config.ollama_host = absl::GetFlag(FLAGS_ollama_host);
  
  // Fall back to environment variables if flags not set
  if (config.gemini_api_key.empty()) {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    if (env_key) config.gemini_api_key = env_key;
  }
  if (config.model.empty()) {
    const char* env_model = std::getenv("OLLAMA_MODEL");
    if (env_model) config.model = env_model;
  }
  
  return CreateAIService(config);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  std::cout << "🤖 AI Provider: " << config.provider << "\n";
  
  // Ollama provider
  if (config.provider == "ollama") {
    OllamaConfig ollama_config;
    ollama_config.base_url = config.ollama_host;
    if (!config.model.empty()) {
      ollama_config.model = config.model;
    }

    auto service = std::make_unique<OllamaAIService>(ollama_config);

    // Health check
    if (auto status = service->CheckAvailability(); !status.ok()) {
      std::cerr << "⚠️  Ollama unavailable: " << status.message() << std::endl;
      std::cerr << "   Falling back to MockAIService" << std::endl;
      return std::make_unique<MockAIService>();
    }

    std::cout << "   Using model: " << ollama_config.model << std::endl;
    return service;
  }

  // Gemini provider
#ifdef YAZE_WITH_JSON
  if (config.provider == "gemini") {
    std::cerr << "🔧 Creating Gemini service..." << std::endl;
    
    if (config.gemini_api_key.empty()) {
      std::cerr << "⚠️  Gemini API key not provided" << std::endl;
      std::cerr << "   Use --gemini_api_key=<key> or set GEMINI_API_KEY environment variable" << std::endl;
      std::cerr << "   Falling back to MockAIService" << std::endl;
      return std::make_unique<MockAIService>();
    }
    
    std::cerr << "🔧 Building Gemini config..." << std::endl;
    GeminiConfig gemini_config(config.gemini_api_key);
    if (!config.model.empty()) {
      gemini_config.model = config.model;
    }
    gemini_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    gemini_config.use_function_calling = absl::GetFlag(FLAGS_use_function_calling);
    std::cerr << "🔧 Model: " << gemini_config.model << std::endl;
    std::cerr << "🔧 Prompt version: " << gemini_config.prompt_version << std::endl;

    std::cerr << "🔧 Creating Gemini service instance..." << std::endl;
    auto service = std::make_unique<GeminiAIService>(gemini_config);

    std::cerr << "🔧 Skipping availability check (causes segfault with SSL)" << std::endl;
    // Health check - DISABLED due to SSL issues
    // if (auto status = service->CheckAvailability(); !status.ok()) {
    //   std::cerr << "⚠️  Gemini unavailable: " << status.message() << std::endl;
    //   std::cerr << "   Falling back to MockAIService" << std::endl;
    //   return std::make_unique<MockAIService>();
    // }

    std::cout << "   Using model: " << gemini_config.model << std::endl;
    std::cerr << "🔧 Gemini service ready" << std::endl;
    return service;
  }
#else
  if (config.provider == "gemini") {
    std::cerr << "⚠️  Gemini support not available: rebuild with YAZE_WITH_JSON=ON" << std::endl;
    std::cerr << "   Falling back to MockAIService" << std::endl;
  }
#endif

  // Default: Mock service
  if (config.provider != "mock") {
    std::cout << "   No LLM configured, using MockAIService" << std::endl;
  }
  std::cout << "   Tip: Use --ai_provider=ollama or --ai_provider=gemini" << std::endl;
  return std::make_unique<MockAIService>();
}

}  // namespace cli
}  // namespace yaze
