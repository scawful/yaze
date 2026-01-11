#include "cli/service/ai/service_factory.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"

namespace yaze::cli {
namespace {

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
TEST(AIServiceFactoryTest, OpenAIWithKeyCreatesService) {
  AIServiceConfig config;
  config.provider = "openai";
  config.openai_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}
#endif

}  // namespace
}  // namespace yaze::cli
