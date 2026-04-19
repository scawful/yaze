#include "cli/service/ai/model_registry.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "cli/service/ai/ai_service.h"

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze::cli {
namespace {

class ModelRegistryGuard {
 public:
  ModelRegistryGuard() { ModelRegistry::GetInstance().ClearServices(); }
  ~ModelRegistryGuard() { ModelRegistry::GetInstance().ClearServices(); }
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const char* value) : name_(name) {
    const char* prev = std::getenv(name);
    if (prev) {
      had_prev_ = true;
      prev_value_ = prev;
    }
    Set(value);
  }

  ~ScopedEnvVar() {
    if (had_prev_) {
      Set(prev_value_.c_str());
    } else {
      Unset();
    }
  }

 private:
  void Set(const char* value) {
#ifdef _WIN32
    _putenv_s(name_.c_str(), value ? value : "");
#else
    if (value) {
      setenv(name_.c_str(), value, 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

  void Unset() {
#ifdef _WIN32
    _putenv_s(name_.c_str(), "");
#else
    unsetenv(name_.c_str());
#endif
  }

  std::string name_;
  bool had_prev_ = false;
  std::string prev_value_;
};

class ScopedFlagState {
 public:
  ScopedFlagState()
      : ai_provider_(absl::GetFlag(FLAGS_ai_provider)),
        ai_model_(absl::GetFlag(FLAGS_ai_model)),
        gemini_api_key_(absl::GetFlag(FLAGS_gemini_api_key)),
        anthropic_api_key_(absl::GetFlag(FLAGS_anthropic_api_key)),
        ollama_host_(absl::GetFlag(FLAGS_ollama_host)),
        openai_base_url_(absl::GetFlag(FLAGS_openai_base_url)),
        rom_(absl::GetFlag(FLAGS_rom)) {}

  ~ScopedFlagState() {
    absl::SetFlag(&FLAGS_ai_provider, ai_provider_);
    absl::SetFlag(&FLAGS_ai_model, ai_model_);
    absl::SetFlag(&FLAGS_gemini_api_key, gemini_api_key_);
    absl::SetFlag(&FLAGS_anthropic_api_key, anthropic_api_key_);
    absl::SetFlag(&FLAGS_ollama_host, ollama_host_);
    absl::SetFlag(&FLAGS_openai_base_url, openai_base_url_);
    absl::SetFlag(&FLAGS_rom, rom_);
  }

 private:
  std::string ai_provider_;
  std::string ai_model_;
  std::string gemini_api_key_;
  std::string anthropic_api_key_;
  std::string ollama_host_;
  std::string openai_base_url_;
  std::string rom_;
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

TEST(ModelRegistryTest, AutoDiscoversExplicitProviderFromFlags) {
  ModelRegistryGuard guard;
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "gemini");
  absl::SetFlag(&FLAGS_gemini_api_key, "");

  auto models_or = ModelRegistry::GetInstance().ListAllModels();

  ASSERT_TRUE(models_or.ok());
  ASSERT_FALSE(models_or->empty());
  EXPECT_EQ(models_or->front().provider, "gemini");
}

TEST(ModelRegistryTest, AutoDiscoversConfiguredAnthropicProviderFromEnv) {
  ModelRegistryGuard guard;
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");

  ScopedEnvVar set_anthropic("ANTHROPIC_API_KEY", "test-anthropic-key");
  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_openai("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_base("OPENAI_BASE_URL", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);

  auto models_or = ModelRegistry::GetInstance().ListAllModels();

  ASSERT_TRUE(models_or.ok());
  ASSERT_FALSE(models_or->empty());
  EXPECT_EQ(models_or->front().provider, "anthropic");
}

}  // namespace
}  // namespace yaze::cli
