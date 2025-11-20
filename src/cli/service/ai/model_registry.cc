#include "cli/service/ai/model_registry.h"

#include <algorithm>

namespace yaze {
namespace cli {

ModelRegistry& ModelRegistry::GetInstance() {
  static ModelRegistry instance;
  return instance;
}

void ModelRegistry::RegisterService(std::shared_ptr<AIService> service) {
  std::lock_guard<std::mutex> lock(mutex_);
  services_.push_back(service);
}

void ModelRegistry::ClearServices() {
  std::lock_guard<std::mutex> lock(mutex_);
  services_.clear();
}

absl::StatusOr<std::vector<ModelInfo>> ModelRegistry::ListAllModels() {
  std::lock_guard<std::mutex> lock(mutex_);
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
  std::sort(all_models.begin(), all_models.end(),
            [](const ModelInfo& a, const ModelInfo& b) {
              return a.name < b.name;
            });
            
  return all_models;
}

}  // namespace cli
}  // namespace yaze

