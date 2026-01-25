#include "cli/service/ai/service_factory.h"

#include <cstdlib>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);

namespace yaze::cli {
namespace {

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
        openai_base_url_(absl::GetFlag(FLAGS_openai_base_url)) {}

  ~ScopedFlagState() {
    absl::SetFlag(&FLAGS_ai_provider, ai_provider_);
    absl::SetFlag(&FLAGS_ai_model, ai_model_);
    absl::SetFlag(&FLAGS_gemini_api_key, gemini_api_key_);
    absl::SetFlag(&FLAGS_anthropic_api_key, anthropic_api_key_);
    absl::SetFlag(&FLAGS_ollama_host, ollama_host_);
    absl::SetFlag(&FLAGS_openai_base_url, openai_base_url_);
  }

 private:
  std::string ai_provider_;
  std::string ai_model_;
  std::string gemini_api_key_;
  std::string anthropic_api_key_;
  std::string ollama_host_;
  std::string openai_base_url_;
};

// These tests require AI runtime to be enabled (service_factory.cc vs stub)
#ifdef YAZE_AI_RUNTIME_AVAILABLE

TEST(AIServiceFactoryTest, OpenAIMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "openai";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
#ifdef YAZE_WITH_JSON
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("OpenAI API key"));
#else
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kInvalidArgument);
#endif
}

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, GeminiMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "gemini";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("Gemini API key"));
}
#else
TEST(AIServiceFactoryTest, GeminiUnavailableWithoutJsonSupport) {
  AIServiceConfig config;
  config.provider = "gemini";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("YAZE_WITH_JSON"));
}
#endif

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, AnthropicMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "anthropic";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("Anthropic API key"));
}
#else
TEST(AIServiceFactoryTest, AnthropicUnavailableWithoutJsonSupport) {
  AIServiceConfig config;
  config.provider = "anthropic";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("YAZE_WITH_JSON"));
}
#endif

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, OpenAIWithKeyCreatesService) {
  AIServiceConfig config;
  config.provider = "openai";
  config.openai_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, OpenAILocalBaseAllowsEmptyKey) {
  AIServiceConfig config;
  config.provider = "openai";
  config.openai_base_url = "http://localhost:1234";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsLocalOpenAIBase) {
  AIServiceConfig config;
  config.provider = "auto";
  config.openai_base_url = "http://localhost:1234";

  auto service = CreateAIService(config);

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsOpenAIBaseFromEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);
  ScopedEnvVar openai_base("OPENAI_BASE_URL", "http://localhost:1234");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsOpenAIBaseFromApiBaseEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_base("OPENAI_BASE_URL", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);
  ScopedEnvVar openai_api_base("OPENAI_API_BASE",
                               "http://localhost:1234/v1");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}
#endif  // YAZE_WITH_JSON

TEST(AIServiceFactoryTest, AutoDetectsOllamaFromEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_base("OPENAI_BASE_URL", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar set_ollama_host("OLLAMA_HOST", "http://localhost:11434");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "ollama");
}

#endif  // YAZE_AI_RUNTIME_AVAILABLE

// Test stub behavior when AI runtime is disabled
#ifndef YAZE_AI_RUNTIME_AVAILABLE

TEST(AIServiceFactoryTest, StubReturnsDisabledError) {
  AIServiceConfig config;
  config.provider = "openai";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("AI runtime features are disabled"));
}

#endif  // !YAZE_AI_RUNTIME_AVAILABLE

}  // namespace
}  // namespace yaze::cli
