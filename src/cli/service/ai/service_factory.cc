#include "cli/service/ai/service_factory.h"

#include <cstring>
#include <iostream>

#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/ollama_ai_service.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/gemini_ai_service.h"
#endif

namespace yaze {
namespace cli {

std::unique_ptr<AIService> CreateAIService() {
  // Priority: Ollama (local) > Gemini (remote) > Mock (testing)
  const char* provider_env = std::getenv("YAZE_AI_PROVIDER");
  const char* ollama_model = std::getenv("OLLAMA_MODEL");
  const std::string provider = provider_env ? provider_env : "";
  const bool gemini_requested = provider == "gemini";

#ifdef YAZE_WITH_JSON
  const char* gemini_key = std::getenv("GEMINI_API_KEY");
  const char* gemini_model = std::getenv("GEMINI_MODEL");
#endif

  // Explicit provider selection
  if (provider == "ollama") {
    OllamaConfig config;

    // Allow model override via env
    if (ollama_model && std::strlen(ollama_model) > 0) {
      config.model = ollama_model;
    }

    auto service = std::make_unique<OllamaAIService>(config);

    // Health check
    if (auto status = service->CheckAvailability(); !status.ok()) {
      std::cerr << "⚠️  Ollama unavailable: " << status.message() << std::endl;
      std::cerr << "   Falling back to MockAIService" << std::endl;
      return std::make_unique<MockAIService>();
    }

    std::cout << "🤖 Using Ollama AI with model: " << config.model << std::endl;
    return service;
  }

  // Gemini if API key provided
#ifdef YAZE_WITH_JSON
  if (gemini_key && std::strlen(gemini_key) > 0) {
    GeminiConfig config(gemini_key);

    // Allow model override via env
    if (gemini_model && std::strlen(gemini_model) > 0) {
      config.model = gemini_model;
    }

    auto service = std::make_unique<GeminiAIService>(config);

    // Health check
    if (auto status = service->CheckAvailability(); !status.ok()) {
      std::cerr << "⚠️  Gemini unavailable: " << status.message() << std::endl;
      std::cerr << "   Falling back to MockAIService" << std::endl;
      return std::make_unique<MockAIService>();
    }

    std::cout << "🤖 Using Gemini AI with model: " << config.model << std::endl;
    return service;
  }
#else
  if (gemini_requested || std::getenv("GEMINI_API_KEY")) {
    std::cerr << "⚠️  Gemini support not available: rebuild with YAZE_WITH_JSON=ON" << std::endl;
  }
#endif

  // Default: Mock service for testing
  std::cout << "🤖 Using MockAIService (no LLM configured)" << std::endl;
  std::cout
      << "   Tip: Set YAZE_AI_PROVIDER=ollama or GEMINI_API_KEY to enable LLM"
      << std::endl;
  return std::make_unique<MockAIService>();
}

}  // namespace cli
}  // namespace yaze
