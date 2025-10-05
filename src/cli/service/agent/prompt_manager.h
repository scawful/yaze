#ifndef YAZE_CLI_SERVICE_AGENT_PROMPT_MANAGER_H_
#define YAZE_CLI_SERVICE_AGENT_PROMPT_MANAGER_H_

#include <string>
#include <vector>

namespace yaze {
namespace cli {
namespace agent {

enum class PromptMode {
  kStandard,        // Standard ALTTP
  kOracleOfSecrets, // Oracle of Secrets hack
  kCustom           // User-defined
};

class PromptManager {
 public:
  static std::string LoadPrompt(PromptMode mode);
  static std::string GetPromptPath(PromptMode mode);
  static std::vector<PromptMode> GetAvailableModes();
  static const char* ModeToString(PromptMode mode);
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_PROMPT_MANAGER_H_
