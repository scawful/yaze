#ifndef YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_
#define YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/common.h"

namespace yaze {
namespace cli {

class ModelRegistry {
 public:
  static ModelRegistry& GetInstance();

  // Register a service instance to be queried for models
  void RegisterService(std::shared_ptr<AIService> service);
  
  // Clear all registered services
  void ClearServices();

  // List models from all registered services
  absl::StatusOr<std::vector<ModelInfo>> ListAllModels();

 private:
  ModelRegistry() = default;
  ~ModelRegistry() = default;
  ModelRegistry(const ModelRegistry&) = delete;
  ModelRegistry& operator=(const ModelRegistry&) = delete;

  std::vector<std::shared_ptr<AIService>> services_;
  std::mutex mutex_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_

