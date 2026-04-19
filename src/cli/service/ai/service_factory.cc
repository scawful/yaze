#include "cli/service/ai/service_factory.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/local_gemini_cli_service.h"
#include "cli/service/ai/ollama_ai_service.h"
#include "rom/rom.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/anthropic_ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/openai_ai_service.h"
#endif

namespace {

constexpr char kDefaultOpenAiBaseUrl[] = "https://api.openai.com";
constexpr char kDefaultOllamaHost[] = "http://localhost:11434";
constexpr char kOraclePromptAsset[] = "agent/oracle_of_secrets_guide.txt";

std::string NormalizeProviderAlias(std::string provider) {
  provider = absl::AsciiStrToLower(provider);
  if (provider == "claude" || provider == "anthropic-claude" ||
      provider == "sonnet" || provider == "opus") {
    return "anthropic";
  }
  if (provider == "chatgpt" || provider == "gpt" || provider == "lmstudio" ||
      provider == "lm-studio" || provider == "custom-openai" ||
      provider == "openai-compatible") {
    return "openai";
  }
  if (provider == "google" || provider == "google-gemini") {
    return "gemini";
  }
  return provider;
}

bool IsLikelyOracleRomPath(absl::string_view rom_path) {
  if (rom_path.empty()) {
    return false;
  }
  const std::string lowered = absl::AsciiStrToLower(std::string(rom_path));
  return absl::StrContains(lowered, "oracle") ||
         absl::StrContains(lowered, "oos");
}

std::string ReadAssetFile(absl::string_view relative_path) {
  auto asset_path =
      yaze::util::PlatformPaths::FindAsset(std::string(relative_path));
  if (!asset_path.ok()) {
    return "";
  }
  std::ifstream file(asset_path->string());
  if (!file.good()) {
    return "";
  }
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

bool HasOllamaHint(const yaze::cli::AIServiceConfig& config) {
  if (config.ollama_host != kDefaultOllamaHost) {
    return true;
  }
  const char* env_ollama_host = std::getenv("OLLAMA_HOST");
  if (env_ollama_host && *env_ollama_host) {
    return true;
  }
  const char* env_ollama_model = std::getenv("OLLAMA_MODEL");
  return env_ollama_model && *env_ollama_model;
}

bool HasOpenAiEndpointHint(const yaze::cli::AIServiceConfig& config) {
  return !config.openai_base_url.empty() &&
         config.openai_base_url != kDefaultOpenAiBaseUrl;
}

void ApplyEnvironmentFallbacks(yaze::cli::AIServiceConfig& config) {
  if (config.gemini_api_key.empty()) {
    if (const char* env_key = std::getenv("GEMINI_API_KEY")) {
      config.gemini_api_key = env_key;
    }
  }
  if (config.anthropic_api_key.empty()) {
    if (const char* env_key = std::getenv("ANTHROPIC_API_KEY")) {
      config.anthropic_api_key = env_key;
    }
  }
  if (config.openai_api_key.empty()) {
    if (const char* openai_key = std::getenv("OPENAI_API_KEY")) {
      config.openai_api_key = openai_key;
    }
  }
  if (config.openai_base_url.empty() ||
      config.openai_base_url == kDefaultOpenAiBaseUrl) {
    const char* env_openai_base = std::getenv("OPENAI_BASE_URL");
    if (!env_openai_base || !*env_openai_base) {
      env_openai_base = std::getenv("OPENAI_API_BASE");
    }
    if (env_openai_base && *env_openai_base) {
      config.openai_base_url = env_openai_base;
    }
  }
  if (config.ollama_host.empty() || config.ollama_host == kDefaultOllamaHost) {
    if (const char* env_ollama_host = std::getenv("OLLAMA_HOST");
        env_ollama_host && *env_ollama_host) {
      config.ollama_host = env_ollama_host;
    }
  }
  if (config.model.empty()) {
    if (const char* env_model = std::getenv("OLLAMA_MODEL")) {
      config.model = env_model;
    }
  }
}

yaze::cli::AIServiceConfig NormalizeConfig(yaze::cli::AIServiceConfig config) {
  config.provider = NormalizeProviderAlias(std::move(config.provider));
  if (config.provider.empty()) {
    config.provider = "auto";
  }
  config.openai_base_url =
      yaze::cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
  return config;
}

std::string ResolveOracleSystemInstruction(
    const yaze::cli::AIServiceConfig& config) {
  if (yaze::cli::DetectPromptProfile(config) !=
      yaze::cli::AgentPromptProfile::kOracleOfSecrets) {
    return "";
  }
  return ReadAssetFile(kOraclePromptAsset);
}

std::unique_ptr<yaze::cli::AIService> FinalizeService(
    std::unique_ptr<yaze::cli::AIService> service,
    const yaze::cli::AIServiceConfig& config) {
  if (service != nullptr && config.rom_context != nullptr) {
    service->SetRomContext(config.rom_context);
  }
  return service;
}

}  // namespace

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, prompt_version);
ABSL_DECLARE_FLAG(bool, use_function_calling);
ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

AIServiceConfig BuildAIServiceConfigFromFlags() {
  AIServiceConfig config;
  config.provider = absl::GetFlag(FLAGS_ai_provider);
  config.model = absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = absl::GetFlag(FLAGS_gemini_api_key);
  config.anthropic_api_key = absl::GetFlag(FLAGS_anthropic_api_key);
  config.ollama_host = absl::GetFlag(FLAGS_ollama_host);
  config.openai_base_url = absl::GetFlag(FLAGS_openai_base_url);
  config.rom_path_hint = absl::GetFlag(FLAGS_rom);
  ApplyEnvironmentFallbacks(config);
  return NormalizeConfig(std::move(config));
}

AgentPromptProfile DetectPromptProfile(const AIServiceConfig& config) {
  if (config.rom_context != nullptr &&
      IsLikelyOracleRomPath(config.rom_context->filename())) {
    return AgentPromptProfile::kOracleOfSecrets;
  }
  if (IsLikelyOracleRomPath(config.rom_path_hint)) {
    return AgentPromptProfile::kOracleOfSecrets;
  }
  return AgentPromptProfile::kStandard;
}

std::vector<AIServiceConfig> DiscoverModelRegistryConfigs(
    const AIServiceConfig& base_config) {
  const AIServiceConfig effective_config = NormalizeConfig(base_config);
  std::vector<AIServiceConfig> configs;
  std::set<std::string> seen_providers;

  auto append_provider = [&](absl::string_view provider_name) {
    const std::string canonical =
        NormalizeProviderAlias(std::string(provider_name));
    if (canonical.empty() || !seen_providers.insert(canonical).second) {
      return;
    }
    AIServiceConfig provider_config = effective_config;
    provider_config.provider = canonical;
    configs.push_back(std::move(provider_config));
  };

  if (effective_config.provider != "auto") {
    append_provider(effective_config.provider);
    return configs;
  }

  if (!effective_config.gemini_api_key.empty()) {
    append_provider("gemini");
  }
  if (HasOpenAiEndpointHint(effective_config) ||
      !effective_config.openai_api_key.empty()) {
    append_provider("openai");
  }
  if (!effective_config.anthropic_api_key.empty()) {
    append_provider("anthropic");
  }
  if (HasOllamaHint(effective_config)) {
    append_provider("ollama");
  }
  return configs;
}

std::unique_ptr<AIService> CreateAIService() {
  return CreateAIService(BuildAIServiceConfigFromFlags());
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  AIServiceConfig effective_config = NormalizeConfig(config);

  if (effective_config.provider == "auto") {
    if (!effective_config.gemini_api_key.empty()) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found Gemini API key, using Gemini\n";
      effective_config.provider = "gemini";
    } else if (HasOpenAiEndpointHint(effective_config)) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found OpenAI-compatible base URL, using OpenAI\n";
      if (effective_config.model.empty()) {
        std::cout << "   Tip: Set --ai_model for local servers\n";
      }
      effective_config.provider = "openai";
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
    } else if (HasOllamaHint(effective_config)) {
      std::cout << "🤖 Auto-detecting AI provider...\n";
      std::cout << "   Found Ollama configuration, using Ollama\n";
      effective_config.provider = "ollama";
    } else {
      std::cout << "🤖 No AI provider configured, using MockAIService\n";
      std::cout
          << "   Tip: Set GEMINI_API_KEY, ANTHROPIC_API_KEY, OPENAI_API_KEY,"
             " OPENAI_BASE_URL, or OLLAMA_HOST/OLLAMA_MODEL\n";
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
  return FinalizeService(std::make_unique<MockAIService>(), effective_config);
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config) {
  const AIServiceConfig effective_config = NormalizeConfig(config);
  const std::string provider = effective_config.provider;
  if (provider.empty() || provider == "auto") {
    return absl::InvalidArgumentError(
        "CreateAIServiceStrict requires an explicit provider (not 'auto')");
  }

  const std::string oracle_system_instruction =
      ResolveOracleSystemInstruction(effective_config);

  if (provider == "mock") {
    return FinalizeService(std::make_unique<MockAIService>(), effective_config);
  }

  if (provider == "ollama") {
    OllamaConfig ollama_config;
    ollama_config.base_url = effective_config.ollama_host;
    if (!effective_config.model.empty()) {
      ollama_config.model = effective_config.model;
    } else if (const char* env_model = std::getenv("OLLAMA_MODEL")) {
      ollama_config.model = env_model;
    }
    if (!oracle_system_instruction.empty()) {
      ollama_config.system_prompt = oracle_system_instruction;
    }
    return FinalizeService(std::make_unique<OllamaAIService>(ollama_config),
                           effective_config);
  }

  if (provider == "gemini-cli" || provider == "local-gemini") {
    return FinalizeService(
        std::make_unique<LocalGeminiCliService>(effective_config.model.empty()
                                                    ? "gemini-2.5-flash"
                                                    : effective_config.model),
        effective_config);
  }

#ifdef YAZE_WITH_JSON
  if (provider == "gemini") {
    if (effective_config.gemini_api_key.empty()) {
      return absl::FailedPreconditionError(
          "Gemini API key not provided. Set --gemini_api_key or "
          "GEMINI_API_KEY.");
    }
    GeminiConfig gemini_config(effective_config.gemini_api_key);
    if (!effective_config.model.empty()) {
      gemini_config.model = effective_config.model;
    }
    if (!oracle_system_instruction.empty()) {
      gemini_config.system_instruction = oracle_system_instruction;
    }
    gemini_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    gemini_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    gemini_config.verbose = effective_config.verbose;
    return FinalizeService(std::make_unique<GeminiAIService>(gemini_config),
                           effective_config);
  }
  if (provider == "anthropic") {
    if (effective_config.anthropic_api_key.empty()) {
      return absl::FailedPreconditionError(
          "Anthropic API key not provided. Set --anthropic_api_key or "
          "ANTHROPIC_API_KEY.");
    }
    AnthropicConfig anthropic_config(effective_config.anthropic_api_key);
    if (!effective_config.model.empty()) {
      anthropic_config.model = effective_config.model;
    }
    if (!oracle_system_instruction.empty()) {
      anthropic_config.system_instruction = oracle_system_instruction;
    }
    anthropic_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    anthropic_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    anthropic_config.verbose = effective_config.verbose;
    return FinalizeService(
        std::make_unique<AnthropicAIService>(anthropic_config),
        effective_config);
  }
  if (provider == "openai") {
    const bool is_local_server =
        effective_config.openai_base_url != kDefaultOpenAiBaseUrl;
    if (effective_config.openai_api_key.empty() && !is_local_server) {
      return absl::FailedPreconditionError(
          "OpenAI API key not provided. Set OPENAI_API_KEY.\n"
          "For LMStudio, use --openai_base_url=http://localhost:1234");
    }
    OpenAIConfig openai_config(effective_config.openai_api_key);
    openai_config.base_url = effective_config.openai_base_url;
    if (!effective_config.model.empty()) {
      openai_config.model = effective_config.model;
    }
    if (!oracle_system_instruction.empty()) {
      openai_config.system_instruction = oracle_system_instruction;
    }
    openai_config.prompt_version = absl::GetFlag(FLAGS_prompt_version);
    openai_config.use_function_calling =
        absl::GetFlag(FLAGS_use_function_calling);
    openai_config.verbose = effective_config.verbose;
    return FinalizeService(std::make_unique<OpenAIAIService>(openai_config),
                           effective_config);
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
