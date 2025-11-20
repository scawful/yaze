#include "cli/service/agent/prompt_manager.h"
#include <fstream>
#include <sstream>
#include "util/file_util.h"

namespace yaze {
namespace cli {
namespace agent {

std::string PromptManager::LoadPrompt(PromptMode mode) {
  std::string path = GetPromptPath(mode);
  std::ifstream file(path);
  if (!file)
    return "";

  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

std::string PromptManager::GetPromptPath(PromptMode mode) {
  switch (mode) {
    case PromptMode::kStandard:
      return "assets/agent/system_prompt_v3.txt";
    case PromptMode::kOracleOfSecrets:
      return "assets/agent/oracle_of_secrets_guide.txt";
    case PromptMode::kCustom:
      return "assets/agent/custom_prompt.txt";
  }
  return "";
}

std::vector<PromptMode> PromptManager::GetAvailableModes() {
  return {PromptMode::kStandard, PromptMode::kOracleOfSecrets};
}

const char* PromptManager::ModeToString(PromptMode mode) {
  switch (mode) {
    case PromptMode::kStandard:
      return "ALTTP Standard";
    case PromptMode::kOracleOfSecrets:
      return "Oracle of Secrets";
    case PromptMode::kCustom:
      return "Custom";
  }
  return "Unknown";
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
