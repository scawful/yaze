#ifndef YAZE_AI_RUNTIME_AVAILABLE

#include "cli/service/ai/service_factory.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::cli {

std::unique_ptr<AIService> CreateAIService() {
  return std::make_unique<MockAIService>();
}

std::unique_ptr<AIService> CreateAIService(const AIServiceConfig&) {
  return std::make_unique<MockAIService>();
}

absl::StatusOr<std::unique_ptr<AIService>> CreateAIServiceStrict(
    const AIServiceConfig&) {
  return absl::FailedPreconditionError(
      "AI runtime features are disabled in this build");
}

}  // namespace yaze::cli

#endif  // !YAZE_AI_RUNTIME_AVAILABLE
