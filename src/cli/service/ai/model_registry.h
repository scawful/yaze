#ifndef YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_
#define YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/common.h"
#include "cli/service/ai/service_factory.h"

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
  absl::StatusOr<std::vector<ModelInfo>> ListAllModels(
      bool force_refresh = false);

  // List models for a specific provider config, cached for the session per
  // provider/endpoint/override unless force_refresh is true.
  absl::StatusOr<std::vector<ModelInfo>> ListModelsForProvider(
      const AIServiceConfig& config, bool force_refresh = false);

 private:
  ModelRegistry() = default;
  ~ModelRegistry() = default;
  ModelRegistry(const ModelRegistry&) = delete;
  ModelRegistry& operator=(const ModelRegistry&) = delete;

  void EnsureDiscoveredServicesLocked();
  void InvalidateCacheLocked();

  std::vector<std::shared_ptr<AIService>> services_;
  std::vector<ModelInfo> cached_models_;
  std::map<std::string, std::vector<ModelInfo>> provider_session_cache_;
  std::chrono::steady_clock::time_point cache_timestamp_{};
  bool cache_valid_ = false;
  bool auto_discovery_attempted_ = false;
  std::mutex mutex_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_MODEL_REGISTRY_H_
