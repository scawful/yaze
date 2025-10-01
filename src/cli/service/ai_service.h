#ifndef YAZE_SRC_CLI_AI_SERVICE_H_
#define YAZE_SRC_CLI_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze {
namespace cli {

class AIService {
 public:
  virtual ~AIService() = default;
  virtual absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) = 0;
};

class MockAIService : public AIService {
 public:
  absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_AI_SERVICE_H_
