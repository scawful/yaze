#include "cli/handlers/agent/test_common.h"

#include <cctype>
#include <iostream>
#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace yaze {
namespace cli {
namespace agent {

std::string TrimWhitespace(absl::string_view value) {
  return std::string(absl::StripAsciiWhitespace(value));
}

bool IsInteractiveInput() {
#if defined(_WIN32)
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

std::string PromptWithDefault(const std::string& prompt,
                              const std::string& default_value,
                              bool allow_empty) {
  while (true) {
    std::cout << prompt;
    if (!default_value.empty()) {
      std::cout << " [" << default_value << "]";
    }
    std::cout << ": ";
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line)) {
      return default_value;
    }
    std::string trimmed = TrimWhitespace(line);
    if (!trimmed.empty()) {
      return trimmed;
    }
    if (!default_value.empty()) {
      return default_value;
    }
    if (allow_empty) {
      return std::string();
    }
    std::cout << "  Value is required." << std::endl;
  }
}

std::string PromptRequired(const std::string& prompt,
                           const std::string& default_value) {
  return PromptWithDefault(prompt, default_value, /*allow_empty=*/false);
}

int PromptInt(const std::string& prompt, int default_value, int min_value) {
  while (true) {
    std::string default_str = absl::StrCat(default_value);
    std::string input = PromptWithDefault(prompt, default_str);
    if (input.empty()) {
      return default_value;
    }
    int value = 0;
    if (absl::SimpleAtoi(input, &value) && value >= min_value) {
      return value;
    }
    std::cout << "  Enter an integer >= " << min_value << "." << std::endl;
  }
}

bool PromptYesNo(const std::string& prompt, bool default_value) {
  while (true) {
    std::cout << prompt << " [" << (default_value ? "Y/n" : "y/N") << "]: ";
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {
      return default_value;
    }
    std::string trimmed = TrimWhitespace(line);
    if (trimmed.empty()) {
      return default_value;
    }
    char c =
        static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
    if (c == 'y') {
      return true;
    }
    if (c == 'n') {
      return false;
    }
    std::cout << "  Please respond with 'y' or 'n'." << std::endl;
  }
}

std::vector<std::string> ParseCommaSeparated(absl::string_view input) {
  std::vector<std::string> values;
  for (absl::string_view token : absl::StrSplit(input, ',')) {
    std::string trimmed = TrimWhitespace(token);
    if (!trimmed.empty()) {
      values.push_back(trimmed);
    }
  }
  return values;
}

bool ParseKeyValueEntry(const std::string& input, std::string* key,
                        std::string* value) {
  size_t equals = input.find('=');
  if (equals == std::string::npos) {
    return false;
  }
  *key = TrimWhitespace(absl::string_view(input.data(), equals));
  *value = TrimWhitespace(
      absl::string_view(input.data() + equals + 1, input.size() - equals - 1));
  return !key->empty();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
