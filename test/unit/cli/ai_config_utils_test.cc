#include "cli/service/ai/ai_config_utils.h"

#include <gtest/gtest.h>

namespace yaze::cli {
namespace {

TEST(AIConfigUtilsTest, NormalizeOpenAiBaseUrlDefaultsWhenEmpty) {
  EXPECT_EQ(NormalizeOpenAiBaseUrl(""), "https://api.openai.com");
}

TEST(AIConfigUtilsTest, NormalizeOpenAiBaseUrlStripsTrailingSlashes) {
  EXPECT_EQ(NormalizeOpenAiBaseUrl("https://api.openai.com/"),
            "https://api.openai.com");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://localhost:1234////"),
            "http://localhost:1234");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://localhost:1234/custom/"),
            "http://localhost:1234/custom");
}

TEST(AIConfigUtilsTest, NormalizeOpenAiBaseUrlStripsV1Suffix) {
  EXPECT_EQ(NormalizeOpenAiBaseUrl("https://api.openai.com/v1"),
            "https://api.openai.com");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("https://api.openai.com/v1/"),
            "https://api.openai.com");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://localhost:1234/v1"),
            "http://localhost:1234");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://localhost:1234/v1////"),
            "http://localhost:1234");
}

TEST(AIConfigUtilsTest, NormalizeOpenAiBaseUrlKeepsNonV1Paths) {
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://example.com/v1/custom"),
            "http://example.com/v1/custom");
  EXPECT_EQ(NormalizeOpenAiBaseUrl("http://example.com/v1beta"),
            "http://example.com/v1beta");
}

}  // namespace
}  // namespace yaze::cli
