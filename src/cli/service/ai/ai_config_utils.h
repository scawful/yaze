#ifndef YAZE_SRC_CLI_SERVICE_AI_AI_CONFIG_UTILS_H_
#define YAZE_SRC_CLI_SERVICE_AI_AI_CONFIG_UTILS_H_

#include <string>

#include "absl/strings/match.h"

namespace yaze {
namespace cli {

inline std::string NormalizeOpenAiBaseUrl(std::string base) {
  if (base.empty()) {
    return "https://api.openai.com";
  }
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (absl::EndsWith(base, "/v1")) {
    base.resize(base.size() - 3);
    while (!base.empty() && base.back() == '/') {
      base.pop_back();
    }
  }
  return base;
}

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_AI_CONFIG_UTILS_H_
