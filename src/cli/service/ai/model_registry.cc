#include "cli/service/ai/model_registry.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "cli/service/ai/local_gemini_cli_service.h"
#include "cli/service/ai/ollama_ai_service.h"
#include "cli/service/ai/service_factory.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/anthropic_ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/openai_ai_service.h"
#endif

namespace {

constexpr std::chrono::seconds kModelCacheTtl(30);

absl::StatusOr<std::shared_ptr<yaze::cli::AIService>> CreateDiscoveryService(
    const yaze::cli::AIServiceConfig& config) {
  const std::string provider = config.provider;

  if (provider == "mock") {
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::MockAIService>());
  }

  if (provider == "ollama") {
    yaze::cli::OllamaConfig ollama_config;
    ollama_config.base_url = config.ollama_host;
    if (!config.model.empty()) {
      ollama_config.model = config.model;
    }
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::OllamaAIService>(ollama_config));
  }

  if (provider == "gemini-cli" || provider == "local-gemini") {
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::LocalGeminiCliService>(
            config.model.empty() ? "gemini-2.5-flash" : config.model));
  }

#ifdef YAZE_WITH_JSON
  if (provider == "gemini") {
    yaze::cli::GeminiConfig gemini_config(config.gemini_api_key);
    if (!config.model.empty()) {
      gemini_config.model = config.model;
    }
    gemini_config.verbose = config.verbose;
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::GeminiAIService>(gemini_config));
  }
  if (provider == "anthropic") {
    yaze::cli::AnthropicConfig anthropic_config(config.anthropic_api_key);
    if (!config.model.empty()) {
      anthropic_config.model = config.model;
    }
    anthropic_config.verbose = config.verbose;
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::AnthropicAIService>(anthropic_config));
  }
  if (provider == "openai") {
    yaze::cli::OpenAIConfig openai_config(config.openai_api_key);
    openai_config.base_url = config.openai_base_url;
    if (!config.model.empty()) {
      openai_config.model = config.model;
    }
    openai_config.verbose = config.verbose;
    return std::static_pointer_cast<yaze::cli::AIService>(
        std::make_shared<yaze::cli::OpenAIAIService>(openai_config));
  }
#endif

  return absl::InvalidArgumentError(
      absl::StrCat("Unsupported model discovery provider: ", provider));
}

}  // namespace

namespace yaze {
namespace cli {

ModelRegistry& ModelRegistry::GetInstance() {
  static ModelRegistry instance;
  return instance;
}

void ModelRegistry::RegisterService(std::shared_ptr<AIService> service) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto_discovery_attempted_ = true;
  services_.push_back(std::move(service));
  InvalidateCacheLocked();
}

void ModelRegistry::ClearServices() {
  std::lock_guard<std::mutex> lock(mutex_);
  services_.clear();
  auto_discovery_attempted_ = false;
  InvalidateCacheLocked();
}

void ModelRegistry::EnsureDiscoveredServicesLocked() {
  if (auto_discovery_attempted_ || !services_.empty()) {
    return;
  }

  auto_discovery_attempted_ = true;
  for (const auto& config :
       DiscoverModelRegistryConfigs(BuildAIServiceConfigFromFlags())) {
    auto service_or = CreateDiscoveryService(config);
    if (!service_or.ok()) {
      continue;
    }
    auto service = std::move(service_or.value());
    if (config.rom_context != nullptr) {
      service->SetRomContext(config.rom_context);
    }
    services_.push_back(std::move(service));
  }
  InvalidateCacheLocked();
}

void ModelRegistry::InvalidateCacheLocked() {
  cached_models_.clear();
  cache_valid_ = false;
}

absl::StatusOr<std::vector<ModelInfo>> ModelRegistry::ListAllModels(
    bool force_refresh) {
  std::lock_guard<std::mutex> lock(mutex_);
  EnsureDiscoveredServicesLocked();

  const auto now = std::chrono::steady_clock::now();
  if (!force_refresh && cache_valid_ &&
      (now - cache_timestamp_) < kModelCacheTtl) {
    return cached_models_;
  }
  std::vector<ModelInfo> all_models;

  for (const auto& service : services_) {
    auto models_or = service->ListAvailableModels();
    if (models_or.ok()) {
      auto& models = *models_or;
      all_models.insert(all_models.end(),
                        std::make_move_iterator(models.begin()),
                        std::make_move_iterator(models.end()));
    }
  }

  std::sort(
      all_models.begin(), all_models.end(),
      [](const ModelInfo& a, const ModelInfo& b) { return a.name < b.name; });

  for (auto& model : all_models) {
    if (model.display_name.empty()) {
      model.display_name = model.name;
    }
  }

  cached_models_ = all_models;
  cache_timestamp_ = now;
  cache_valid_ = true;

  return all_models;
}

}  // namespace cli
}  // namespace yaze
