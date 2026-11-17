#include "cli/service/ai/model_registry.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/cli_harness_service.h"
#include "cli/service/ai/provider_ids.h"

#ifdef YAZE_WITH_JSON
#include "cli/service/ai/anthropic_ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/openai_ai_service.h"
#endif

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

#ifdef YAZE_WITH_JSON
TEST(ModelRegistryTest, ParsesRecordedAnthropicOpenAiAndGeminiModelResponses) {
  const std::string anthropic_json = R"({
    "data": [
      {"id": "claude-opus-5-5", "display_name": "Claude Opus 5.5"},
      {"id": "claude-sonnet-5-5", "display_name": "Claude Sonnet 5.5"},
      {"id": "claude-3-5-sonnet-20241022", "display_name": "Legacy 3.5"}
    ]
  })";
  auto anthropic_models = ParseAnthropicModelsJson(anthropic_json);
  ASSERT_EQ(anthropic_models.size(), 2u);
  EXPECT_EQ(anthropic_models[0].name, "claude-opus-5-5");
  EXPECT_EQ(anthropic_models[1].name, "claude-sonnet-5-5");

  const std::string openai_cloud_json = R"({
    "data": [
      {"id": "gpt-5.2"},
      {"id": "gpt-5-mini"},
      {"id": "gpt-4.1"},
      {"id": "o3"},
      {"id": "gpt-3.5-turbo"},
      {"id": "whisper-1"},
      {"id": "gpt-realtime"},
      {"id": "text-embedding-3-large"}
    ]
  })";
  auto cloud_models = ParseOpenAIModelsJson(openai_cloud_json, false);
  std::vector<std::string> cloud_names;
  for (const auto& m : cloud_models) {
    cloud_names.push_back(m.name);
  }
  EXPECT_THAT(cloud_names,
              testing::ElementsAre("gpt-5.2", "gpt-5-mini", "gpt-4.1", "o3"));

  const std::string lmstudio_json = R"({
    "data": [
      {"id": "scawfulbot-qwen3-8b-v1p1-repair-dpo"},
      {"id": "oracle-tools-7b"}
    ]
  })";
  auto local_models = ParseOpenAIModelsJson(lmstudio_json, true);
  ASSERT_EQ(local_models.size(), 2u);
  EXPECT_TRUE(local_models[0].is_local);
  EXPECT_EQ(local_models[1].name, "oracle-tools-7b");

  const std::string gemini_json = R"({
    "models": [
      {"name": "models/gemini-3.5-flash", "displayName": "Gemini 3.5 Flash", "supportedGenerationMethods": ["generateContent"]},
      {"name": "models/gemini-2.5-pro", "displayName": "Gemini 2.5 Pro", "supportedGenerationMethods": ["generateContent"]},
      {"name": "models/gemini-1.5-flash", "displayName": "Legacy 1.5", "supportedGenerationMethods": ["generateContent"]},
      {"name": "models/gemini-embedding-001", "displayName": "Embed", "supportedGenerationMethods": ["embedContent"]}
    ]
  })";
  auto gemini_models = ParseGeminiModelsJson(gemini_json);
  ASSERT_EQ(gemini_models.size(), 2u);
  EXPECT_EQ(gemini_models[0].name, "gemini-3.5-flash");
  EXPECT_EQ(gemini_models[1].name, "gemini-2.5-pro");
}

TEST(ModelRegistryTest, FallbackModelListsContainNoLegacy2024Ids) {
  std::vector<ModelInfo> all_fallbacks;
  auto append_all = [&](std::vector<ModelInfo> items) {
    all_fallbacks.insert(all_fallbacks.end(), items.begin(), items.end());
  };
  append_all(GetFallbackAnthropicModels());
  append_all(GetFallbackOpenAIModels());
  append_all(GetFallbackGeminiModels());
  for (const char* cli_id :
       {kProviderClaudeCode, kProviderCodex, kProviderCursorAgent, kProviderAgy,
        kProviderGeminiCli}) {
    append_all(GetFallbackCliModels(cli_id));
  }

  EXPECT_FALSE(all_fallbacks.empty());
  for (const auto& model : all_fallbacks) {
    EXPECT_FALSE(absl::StartsWith(model.name, "claude-3")) << model.name;
    EXPECT_FALSE(absl::StartsWith(model.name, "gpt-3.5")) << model.name;
    EXPECT_FALSE(absl::StartsWith(model.name, "gemini-1.5")) << model.name;
  }
}

TEST(ModelRegistryTest, SessionCachesPerProviderUntilForceRefresh) {
  ModelRegistryGuard guard;
  AIServiceConfig cfg;
  cfg.provider = kProviderAnthropic;
  cfg.anthropic_api_key = "";

  auto first = ModelRegistry::GetInstance().ListModelsForProvider(cfg, false);
  ASSERT_TRUE(first.ok());
  EXPECT_FALSE(first->empty());

  auto second = ModelRegistry::GetInstance().ListModelsForProvider(cfg, false);
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(second->size(), first->size());

  auto refreshed =
      ModelRegistry::GetInstance().ListModelsForProvider(cfg, true);
  ASSERT_TRUE(refreshed.ok());
  EXPECT_FALSE(refreshed->empty());
}
#endif  // YAZE_WITH_JSON

}  // namespace
}  // namespace yaze::cli
