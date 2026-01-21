#include "cli/service/ai/service_factory.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/local_gemini_cli_service.h"
#include "cli/service/ai/ollama_ai_service.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/anthropic_ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/openai_ai_service.h"
#endif

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
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
  config.anthropic_api_key = absl::GetFlag(FLAGS_anthropic_api_key);
  config.ollama_host = absl::GetFlag(FLAGS_ollama_host);
  config.openai_base_url = absl::GetFlag(FLAGS_openai_base_url);

  // Fall back to environment variables if flags not set
  if (config.gemini_api_key.empty()) {
    const char* env_key = std::getenv("GEMINI_API_KEY");
    if (env_key)
      config.gemini_api_key = env_key;
  }
  if (config.anthropic_api_key.empty()) {
    const char* env_key = std::getenv("ANTHROPIC_API_KEY");
    if (env_key)
      config.anthropic_api_key = env_key;
  }
  if (config.openai_api_key.empty()) {
    const char* openai_key = std::getenv("OPENAI_API_KEY");
    if (openai_key) {
      config.openai_api_key = openai_key;
    }
  }
  if (config.model.empty()) {
    const char* env_model = std::getenv("OLLAMA_MODEL");
    if (env_model)
      config.model = env_model;
  }

  return CreateAIService(config);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  AIServiceConfig effective_config = config;
  if (effective_config.provider.empty()) {
    effective_config.provider = "auto";
  }

  if (effective_config.provider == "auto") {
#ifdef YAZE_WITH_JSON
    if (!effective_config.gemini_api_key.empty()) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found Gemini API key, using Gemini\n";
      effective_config.provider = "gemini";
    } else if (!effective_config.anthropic_api_key.empty()) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found Anthropic API key, using Anthropic\n";
      effective_config.provider = "anthropic";
    } else if (!effective_config.openai_api_key.empty()) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found OpenAI API key, using OpenAI\n";
      effective_config.provider = "openai";
      if (effective_config.model.empty()) {
        effective_config.model = "gpt-4o-mini";
      }
    } else
#endif
    {
      std::cout << "🤖 No AI provider configured, using MockAIService\n";
      std::cout << "   Tip: Set GEMINI_API_KEY, ANTHROPIC_API_KEY, or "
                   "OPENAI_API_KEY\n";
      effective_config.provider = "mock";
    }
  }

  if (effective_config.provider != "mock") {
    std::cout << "🤖 AI Provider: " << effective_config.provider << "\n";
  }

  auto service_or = CreateAIServiceStrict(effective_config);
  if (service_or.ok()) {
    return std::move(service_or.value());
  }

  std::cerr << "⚠️  " << service_or.status().message() << std::endl;
  std::cerr << "   Falling back to MockAIService" << std::endl;
  return std::make_unique<MockAIService>();
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

    return std::make_unique<OllamaAIService>(ollama_config);
  }

  if (provider == "gemini-cli" || provider == "local-gemini") {
    return std::make_unique<LocalGeminiCliService>(
        config.model.empty() ? "gemini-2.5-flash" : config.model);
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
  if (provider == "anthropic") {
    if (config.anthropic_api_key.empty()) {
      return absl::FailedPreconditionError(
          "Anthropic API key not provided. Set --anthropic_api_key or "
          "ANTHROPIC_API_KEY.");
    }
    AnthropicConfig anthropic_config(config.anthropic_api_key);
    if (!config.model.empty()) {
      anthropic_config.model = config.model;
    }
    anthropic_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    anthropic_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    anthropic_config.verbose = config.verbose;
    return std::make_unique<AnthropicAIService>(anthropic_config);
  }
  if (provider == "openai") {
    // LMStudio doesn't require an API key - allow empty key for local servers
    bool is_local_server = config.openai_base_url != "https://api.openai.com";
    if (config.openai_api_key.empty() && !is_local_server) {
      return absl::FailedPreconditionError(
          "OpenAI API key not provided. Set OPENAI_API_KEY.\n"
          "For LMStudio, use --openai_base_url=http://localhost:1234");
    }
    OpenAIConfig openai_config(config.openai_api_key);
    openai_config.base_url = config.openai_base_url;
    if (!config.model.empty()) {
      openai_config.model = config.model;
    }
    openai_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    openai_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    openai_config.verbose = config.verbose;
    return std::make_unique<OpenAIAIService>(openai_config);
  }
#else
  if (provider == "gemini" || provider == "anthropic") {
    return absl::FailedPreconditionError(
        "AI support not available: rebuild with YAZE_WITH_JSON=ON");
  }
#endif

  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown AI provider: %s", config.provider));
}

}  // namespace cli
}  // namespace yaze
