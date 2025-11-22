#include "cli/service/ai/service_factory.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
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

namespace {

AIServiceConfig BuildConfigFromFlags() {
  AIServiceConfig config;
  config.provider = absl::AsciiStrToLower(absl::GetFlag(FLAGS_ai_provider));
  config.model = absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = absl::GetFlag(FLAGS_gemini_api_key);
  config.ollama_host = absl::GetFlag(FLAGS_ollama_host);

  if (config.gemini_api_key.empty()) {
    if (const char* env_key = std::getenv("GEMINI_API_KEY")) {
      config.gemini_api_key = env_key;
    }
  }
  if (config.model.empty()) {
    if (const char* env_model = std::getenv("OLLAMA_MODEL")) {
      config.model = env_model;
    }
  }
  return config;
}

}  // namespace

absl::StatusOr<AIServiceConfig> ResolveAIConfigFromFlags() {
  AIServiceConfig config = BuildConfigFromFlags();
  const bool explicit_provider =
      !config.provider.empty() && config.provider != "auto";

  if (config.provider.empty()) {
    config.provider = "auto";
  }

  if (config.provider == "auto") {
#ifdef YAZE_WITH_JSON
    if (!config.gemini_api_key.empty()) {
      config.provider = "gemini";
    } else
#endif
    {
      OllamaConfig test_config;
      test_config.base_url = config.ollama_host;
      if (!config.model.empty()) {
        test_config.model = config.model;
      }
      auto tester = std::make_unique<OllamaAIService>(test_config);
      if (tester->CheckAvailability().ok()) {
        config.provider = "ollama";
        if (config.model.empty()) {
          config.model = test_config.model;
        }
      } else {
        config.provider = "mock";
      }
    }
  }

  const std::string provider = absl::AsciiStrToLower(config.provider);

#ifdef YAZE_WITH_JSON
  if (provider == "gemini") {
    if (config.gemini_api_key.empty()) {
      return absl::FailedPreconditionError(
          "Gemini API key not provided. Set --gemini_api_key or "
          "GEMINI_API_KEY.");
    }
    return config;
  }
#else
  if (provider == "gemini") {
    return absl::FailedPreconditionError(
        "Gemini support not available: rebuild with YAZE_WITH_JSON=ON");
  }
#endif

  if (provider == "ollama") {
    OllamaConfig ollama_config;
    ollama_config.base_url = config.ollama_host;
    if (!config.model.empty()) {
      ollama_config.model = config.model;
    }
    auto tester = std::make_unique<OllamaAIService>(ollama_config);
    auto status = tester->CheckAvailability();
    if (!status.ok()) {
      return status;
    }
    return config;
  }

  if (provider == "mock") {
    return config;
  }

  if (explicit_provider) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown AI provider: %s", provider));
  }

  // Fallback for unexpected values when provider was not explicitly set
  AIServiceConfig fallback = config;
  fallback.provider = "mock";
  return fallback;
}

std::unique_ptr<AIService> CreateAIService() {
  auto config_or = ResolveAIConfigFromFlags();
  if (!config_or.ok()) {
    std::cerr << "⚠️  " << config_or.status().message() << std::endl;
    std::cerr << "   Falling back to MockAIService" << std::endl;
    return std::make_unique<MockAIService>();
  }
  return CreateAIService(*config_or);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  auto service_or = CreateAIServiceStrict(config);
  if (service_or.ok()) {
    if (config.provider != "mock") {
      std::cout << "🤖 AI Provider: " << config.provider << "\n";
    }
    return std::move(service_or.value());
  }

  std::cerr << "⚠️  " << service_or.status().message() << std::endl;
  std::cerr << "   Falling back to MockAIService" << std::endl;
  return std::make_unique<MockAIService>();
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceForCli() {
  ASSIGN_OR_RETURN(auto config, ResolveAIConfigFromFlags());
  return CreateAIServiceStrict(config);
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config) {
  std::string provider = absl::AsciiStrToLower(config.provider);
  if (provider.empty() || provider == "auto") {
    return absl::InvalidArgumentError(
        "CreateAIServiceStrict requires an explicit provider (not 'auto')");
  }

  if (provider == "mock") {
    return std::make_unique<MockAIService>();
  }

  if (provider == "ollama") {
    OllamaConfig ollama_config;
    ollama_config.base_url = config.ollama_host;
    if (!config.model.empty()) {
      ollama_config.model = config.model;
    } else if (const char* env_model = std::getenv("OLLAMA_MODEL")) {
      ollama_config.model = env_model;
    }

    auto service = std::make_unique<OllamaAIService>(ollama_config);
    auto status = service->CheckAvailability();
    if (!status.ok()) {
      return status;
    }
    return service;
  }

#ifdef YAZE_WITH_JSON
  if (provider == "gemini") {
    if (config.gemini_api_key.empty()) {
      return absl::FailedPreconditionError(
          "Gemini API key not provided. Set --gemini_api_key or "
          "GEMINI_API_KEY.");
    }
    GeminiConfig gemini_config(config.gemini_api_key);
    if (!config.model.empty()) {
      gemini_config.model = config.model;
    }
    gemini_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    gemini_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    gemini_config.verbose = config.verbose;
    return std::make_unique<GeminiAIService>(gemini_config);
  }
#else
  if (provider == "gemini") {
    return absl::FailedPreconditionError(
        "Gemini support not available: rebuild with YAZE_WITH_JSON=ON");
  }
#endif

  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown AI provider: %s", config.provider));
}

}  // namespace cli
}  // namespace yaze
