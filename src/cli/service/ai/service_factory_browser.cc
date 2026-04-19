#include "cli/service/ai/service_factory.h"

#include <fstream>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "app/net/http_client.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/service/ai/provider_ids.h"
#include "rom/rom.h"
#include "util/platform_paths.h"

namespace {

constexpr char kDefaultOpenAiBaseUrl[] = "https://api.openai.com";
constexpr char kOraclePromptAsset[] = "agent/oracle_of_secrets_guide.txt";

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

std::string NormalizeBrowserProviderAlias(std::string provider) {
  provider = absl::AsciiStrToLower(std::move(provider));
  if (provider == yaze::cli::kProviderGoogle ||
      provider == yaze::cli::kProviderGoogleGemini) {
    return yaze::cli::kProviderGemini;
  }
  if (provider == yaze::cli::kProviderLmStudioDashed) {
    return yaze::cli::kProviderLmStudio;
  }
  if (provider == yaze::cli::kProviderChatGpt ||
      provider == yaze::cli::kProviderGpt ||
      provider == yaze::cli::kProviderCustomOpenAi ||
      provider == yaze::cli::kProviderOpenAiCompatible) {
    return yaze::cli::kProviderOpenAi;
  }
  return provider;
}

bool IsOpenAiCompatibleProvider(absl::string_view provider) {
  return provider == yaze::cli::kProviderOpenAi ||
         provider == yaze::cli::kProviderLmStudio ||
         provider == yaze::cli::kProviderHalext ||
         provider == yaze::cli::kProviderAfsBridge;
}

bool IsBrowserSupportedProvider(absl::string_view provider) {
  return provider == yaze::cli::kProviderMock ||
         provider == yaze::cli::kProviderGemini ||
         IsOpenAiCompatibleProvider(provider);
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

std::string ResolveOracleSystemInstruction(
    const yaze::cli::AIServiceConfig& config) {
  if (yaze::cli::DetectPromptProfile(config) !=
      yaze::cli::AgentPromptProfile::kOracleOfSecrets) {
    return "";
  }
  return ReadAssetFile(kOraclePromptAsset);
}

yaze::cli::AIServiceConfig NormalizeConfig(yaze::cli::AIServiceConfig config) {
  config.provider = NormalizeBrowserProviderAlias(std::move(config.provider));
  if (config.provider.empty()) {
    config.provider = yaze::cli::kProviderAuto;
  }
  config.openai_base_url =
      yaze::cli::NormalizeOpenAiBaseUrl(config.openai_base_url);
  return config;
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

#ifdef __EMSCRIPTEN__
#include "app/net/wasm/emscripten_http_client.h"
#endif

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

AIServiceConfig BuildAIServiceConfigFromFlags() {
  AIServiceConfig config;
  config.provider = ::absl::GetFlag(FLAGS_ai_provider);
  config.model = ::absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = ::absl::GetFlag(FLAGS_gemini_api_key);
  config.openai_base_url = ::absl::GetFlag(FLAGS_openai_base_url);
  config.rom_path_hint = ::absl::GetFlag(FLAGS_rom);
  return NormalizeConfig(std::move(config));
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
  AIServiceConfig config = NormalizeConfig(base_config);
  if (config.provider.empty() || config.provider == kProviderAuto) {
    config.provider = kProviderGemini;
  }
  if (!IsBrowserSupportedProvider(config.provider)) {
    return {};
  }
  return {config};
}

std::unique_ptr<AIService> CreateAIService() {
  AIServiceConfig config = BuildAIServiceConfigFromFlags();
  if (config.provider.empty() || config.provider == kProviderAuto) {
    config.provider = kProviderGemini;
  }
  if (config.model.empty()) {
    config.model = "gemini-2.5-flash";
  }
  return CreateAIService(config);
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig& config) {
  AIServiceConfig effective_config = NormalizeConfig(config);
  if (effective_config.provider.empty() ||
      effective_config.provider == kProviderAuto) {
    effective_config.provider = kProviderGemini;
  }

  auto service_or = CreateAIServiceStrict(effective_config);
  if (service_or.ok()) {
    return std::move(service_or.value());
  }

  return FinalizeService(std::make_unique<MockAIService>(), effective_config);
}

::absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig& config) {
  AIServiceConfig effective_config = NormalizeConfig(config);
  if (effective_config.provider.empty() ||
      effective_config.provider == kProviderAuto) {
    return absl::InvalidArgumentError(
        "CreateAIServiceStrict requires an explicit provider (not 'auto')");
  }

  if (effective_config.provider == kProviderMock) {
    return FinalizeService(std::make_unique<MockAIService>(), effective_config);
  }

  if (!IsBrowserSupportedProvider(effective_config.provider)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Provider '%s' is not supported in the browser build",
                        effective_config.provider));
  }

  BrowserAIConfig browser_config;
  browser_config.provider = effective_config.provider;
  browser_config.model = effective_config.model;
  browser_config.system_instruction =
      ResolveOracleSystemInstruction(effective_config);
  if (IsOpenAiCompatibleProvider(browser_config.provider)) {
    browser_config.api_key = effective_config.openai_api_key;
    browser_config.api_base =
        effective_config.openai_base_url.empty()
            ? kDefaultOpenAiBaseUrl
            : NormalizeOpenAIApiBase(effective_config.openai_base_url);
  } else {
    browser_config.api_key = effective_config.gemini_api_key;
  }
  browser_config.verbose = effective_config.verbose;

#ifdef __EMSCRIPTEN__
  auto http_client = std::make_unique<net::EmscriptenHttpClient>();
#else
  std::unique_ptr<net::IHttpClient> http_client = nullptr;
#endif

  return FinalizeService(std::make_unique<BrowserAIService>(
                             browser_config, std::move(http_client)),
                         effective_config);
}

}  // namespace cli
}  // namespace yaze
