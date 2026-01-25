#include "cli/service/ai/model_registry.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "cli/service/ai/ai_service.h"

namespace yaze::cli {
namespace {

class ModelRegistryGuard {
 public:
  ModelRegistryGuard() { ModelRegistry::GetInstance().ClearServices(); }
  ~ModelRegistryGuard() { ModelRegistry::GetInstance().ClearServices(); }
};

class CountingModelService : public MockAIService {
 public:
  CountingModelService(int* counter, std::vector<ModelInfo> models)
      : counter_(counter), models_(std::move(models)) {}

  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override {
    if (counter_) {
      ++(*counter_);
    }
    return models_;
  }

  std::string GetProviderName() const override { return "counting"; }

 private:
  int* counter_ = nullptr;
  std::vector<ModelInfo> models_;
};

TEST(ModelRegistryTest, CachesModelsWithinTtl) {
  ModelRegistryGuard guard;
  int calls = 0;
  auto service = std::make_shared<CountingModelService>(
      &calls, std::vector<ModelInfo>{{.name = "alpha"}});
  auto& registry = ModelRegistry::GetInstance();
  registry.RegisterService(service);

  auto first = registry.ListAllModels();
  ASSERT_TRUE(first.ok());
  auto second = registry.ListAllModels();
  ASSERT_TRUE(second.ok());

  EXPECT_EQ(calls, 1);
  EXPECT_EQ(first->size(), 1u);
  EXPECT_EQ(first->at(0).display_name, "alpha");
  EXPECT_EQ(second->size(), 1u);
}

TEST(ModelRegistryTest, RegisterServiceInvalidatesCache) {
  ModelRegistryGuard guard;
  int calls_one = 0;
  int calls_two = 0;
  auto& registry = ModelRegistry::GetInstance();

  auto service_one = std::make_shared<CountingModelService>(
      &calls_one, std::vector<ModelInfo>{{.name = "alpha"}});
  registry.RegisterService(service_one);

  auto first = registry.ListAllModels();
  ASSERT_TRUE(first.ok());
  EXPECT_EQ(calls_one, 1);

  auto service_two = std::make_shared<CountingModelService>(
      &calls_two, std::vector<ModelInfo>{{.name = "beta"}});
  registry.RegisterService(service_two);

  auto second = registry.ListAllModels();
  ASSERT_TRUE(second.ok());

  EXPECT_EQ(calls_one, 2);
  EXPECT_EQ(calls_two, 1);
}

TEST(ModelRegistryTest, ForceRefreshBypassesCache) {
  ModelRegistryGuard guard;
  int calls = 0;
  auto service = std::make_shared<CountingModelService>(
      &calls, std::vector<ModelInfo>{{.name = "alpha"}});
  auto& registry = ModelRegistry::GetInstance();
  registry.RegisterService(service);

  auto first = registry.ListAllModels();
  ASSERT_TRUE(first.ok());
  auto cached = registry.ListAllModels();
  ASSERT_TRUE(cached.ok());
  auto refreshed = registry.ListAllModels(true);
  ASSERT_TRUE(refreshed.ok());

  EXPECT_EQ(calls, 2);
}

}  // namespace
}  // namespace yaze::cli
