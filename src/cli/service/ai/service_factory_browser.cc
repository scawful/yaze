#include "cli/service/ai/service_factory.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "app/net/http_client.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/browser_ai_service.h"
#include "rom/rom.h"

namespace {

std::string NormalizeOpenAIApiBase(std::string base) {
  if (base.empty()) {
    return base;
  }
  base = yaze::cli::NormalizeOpenAiBaseUrl(base);
  if (!absl::EndsWith(base, "/v1")) {
    base += "/v1";
  }
  return base;
}

bool IsLikelyOracleRomPath(absl::string_view rom_path) {
  if (rom_path.empty()) {
    return false;
  }
  const std::string lowered = absl::AsciiStrToLower(std::string(rom_path));
  return absl::StrContains(lowered, "oracle") ||
         absl::StrContains(lowered, "oos");
}

}  // namespace

#ifdef __EMSCRIPTEN__
#include "app/net/wasm/emscripten_http_client.h"
#endif

namespace yaze {
namespace cli {

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, rom);

AIServiceConfig BuildAIServiceConfigFromFlags() {
  AIServiceConfig config;
  config.provider = absl::GetFlag(FLAGS_ai_provider);
  config.model = absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = absl::GetFlag(FLAGS_gemini_api_key);
  config.openai_base_url = absl::GetFlag(FLAGS_openai_base_url);
  config.rom_path_hint = absl::GetFlag(FLAGS_rom);
  return config;
}

AgentPromptProfile DetectPromptProfile(const AIServiceConfig& config) {
  if (config.rom_context != nullptr &&
      IsLikelyOracleRomPath(config.rom_context->filename())) {
    return AgentPromptProfile::kOracleOfSecrets;
  }
  return IsLikelyOracleRomPath(config.rom_path_hint)
             ? AgentPromptProfile::kOracleOfSecrets
             : AgentPromptProfile::kStandard;
}

std::vector<AIServiceConfig> DiscoverModelRegistryConfigs(
    const AIServiceConfig& base_config) {
  AIServiceConfig config = base_config;
  if (config.provider.empty() || config.provider == "auto") {
    config.provider = "gemini";
  }
  return {config};
}

std::unique_ptr<AIService> CreateAIService() {
  AIServiceConfig config = BuildAIServiceConfigFromFlags();
  if (config.provider.empty() || config.provider == "auto") {
    config.provider = "gemini";
  }
  if (config.model.empty()) {
    config.model = "gemini-1.5-flash";
  }
  return CreateAIService(config);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  // For browser, we always use BrowserAIService wrapping Gemini
  // The browser client handles API keys via config/JS

  BrowserAIConfig browser_config;
  browser_config.provider =
      config.provider.empty() ? "gemini" : config.provider;
  browser_config.model = config.model;
  if (browser_config.model.empty()) {
    browser_config.model = (browser_config.provider == "openai")
                               ? "gpt-4o-mini"
                               : "gemini-1.5-flash";
  }
  if (browser_config.provider == "openai") {
    browser_config.api_key = config.openai_api_key;
    browser_config.api_base = NormalizeOpenAIApiBase(config.openai_base_url);
  } else {
    browser_config.api_key = config.gemini_api_key;
  }
  browser_config.verbose = config.verbose;

#ifdef __EMSCRIPTEN__
  auto http_client = std::make_unique<net::EmscriptenHttpClient>();
#else
  // Fallback for non-wasm builds if this file is included
  std::unique_ptr<net::IHttpClient> http_client = nullptr;
#endif

  return std::make_unique<BrowserAIService>(browser_config,
                                            std::move(http_client));
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config) {
  return CreateAIService(config);
}

}  // namespace cli
}  // namespace yaze
