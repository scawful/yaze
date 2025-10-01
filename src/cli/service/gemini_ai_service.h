#ifndef YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_
#define YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai_service.h"

namespace yaze {
namespace cli {

class GeminiAIService : public AIService {
 public:
  explicit GeminiAIService(const std::string& api_key);
  absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) override;

 private:
  std::string api_key_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_GEMINI_AI_SERVICE_H_