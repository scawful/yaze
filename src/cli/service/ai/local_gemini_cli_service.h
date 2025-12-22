#ifndef YAZE_SRC_CLI_SERVICE_AI_LOCAL_GEMINI_CLI_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AI_LOCAL_GEMINI_CLI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_service.h"

#ifdef YAZE_AI_RUNTIME_AVAILABLE
#include "cli/service/ai/prompt_builder.h"
#endif

namespace yaze {
namespace cli {

class LocalGeminiCliService : public AIService {
 public:
  explicit LocalGeminiCliService(const std::string& model = "gemini-2.5-flash");

  void SetRomContext(Rom* rom) override;
  absl::StatusOr<AgentResponse> GenerateResponse(const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(const std::vector<agent::ChatMessage>& history) override;
  std::string GetProviderName() const override { return "gemini-cli"; }

 private:
  std::string EscapeShellArg(const std::string& arg);
  absl::StatusOr<AgentResponse> ExecuteGeminiCli(const std::string& prompt);

  std::string model_;
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  PromptBuilder prompt_builder_;
#endif
};

} // namespace cli
} // namespace yaze

#endif // YAZE_SRC_CLI_SERVICE_AI_LOCAL_GEMINI_CLI_SERVICE_H_
