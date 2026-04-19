#ifndef YAZE_AI_RUNTIME_AVAILABLE

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "cli/service/ai/provider_ids.h"
#include "cli/service/ai/service_factory.h"
#include "rom/rom.h"

namespace yaze::cli {

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, rom);

namespace {

bool IsLikelyOracleRomPath(absl::string_view rom_path) {
  if (rom_path.empty()) {
    return false;
  }
  const std::string lowered = absl::AsciiStrToLower(std::string(rom_path));
  return absl::StrContains(lowered, "oracle") ||
         absl::StrContains(lowered, "oos");
}

}  // namespace

AIServiceConfig BuildAIServiceConfigFromFlags() {
  AIServiceConfig config;
  config.provider = absl::GetFlag(FLAGS_ai_provider);
  config.model = absl::GetFlag(FLAGS_ai_model);
  config.gemini_api_key = absl::GetFlag(FLAGS_gemini_api_key);
  config.anthropic_api_key = absl::GetFlag(FLAGS_anthropic_api_key);
  config.ollama_host = absl::GetFlag(FLAGS_ollama_host);
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
  if (base_config.provider.empty() || base_config.provider == kProviderAuto) {
    return {};
  }
  return {base_config};
}

std::unique_ptr<AIService> CreateAIService() {
  return std::make_unique<MockAIService>();
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig&) {
  return std::make_unique<MockAIService>();
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig&) {
  return absl::FailedPreconditionError(
      "AI runtime features are disabled in this build");
}

}  // namespace yaze::cli

#endif  // !YAZE_AI_RUNTIME_AVAILABLE
