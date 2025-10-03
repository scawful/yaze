#ifndef YAZE_CLI_HANDLERS_AGENT_TEST_COMMON_H_
#define YAZE_CLI_HANDLERS_AGENT_TEST_COMMON_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {
namespace agent {

// Common helper functions for test command handlers

std::string TrimWhitespace(absl::string_view value);

bool IsInteractiveInput();

std::string PromptWithDefault(const std::string& prompt,
                               const std::string& default_value,
                               bool allow_empty = true);

std::string PromptRequired(const std::string& prompt,
                           const std::string& default_value = std::string());

int PromptInt(const std::string& prompt, int default_value, int min_value);

bool PromptYesNo(const std::string& prompt, bool default_value);

std::vector<std::string> ParseCommaSeparated(absl::string_view input);

bool ParseKeyValueEntry(const std::string& input, std::string* key,
                        std::string* value);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_TEST_COMMON_H_

