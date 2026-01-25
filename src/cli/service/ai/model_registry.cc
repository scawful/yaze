#include "cli/service/ai/model_registry.h"

#include <algorithm>
#include <chrono>

namespace {

constexpr std::chrono::seconds kModelCacheTtl(30);

}  // namespace

namespace yaze {
namespace cli {

ModelRegistry& ModelRegistry::GetInstance() {
  static ModelRegistry instance;
  return instance;
}

void ModelRegistry::RegisterService(std::shared_ptr<AIService> service) {
  std::lock_guard<std::mutex> lock(mutex_);
  services_.push_back(service);
  InvalidateCacheLocked();
}

void ModelRegistry::ClearServices() {
  std::lock_guard<std::mutex> lock(mutex_);
  services_.clear();
  InvalidateCacheLocked();
}

void ModelRegistry::InvalidateCacheLocked() {
  cached_models_.clear();
  cache_valid_ = false;
}

absl::StatusOr<std::vector<ModelInfo>> ModelRegistry::ListAllModels(
    bool force_refresh) {
  std::lock_guard<std::mutex> lock(mutex_);
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

  // Sort by name
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
