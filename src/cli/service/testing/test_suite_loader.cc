#include "cli/service/testing/test_suite_loader.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/strings/string_view.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace {

using ::absl::string_view;

std::string Trim(string_view value) {
  return std::string(absl::StripAsciiWhitespace(value));
}

std::string StripComment(string_view line) {
  bool in_single_quote = false;
  bool in_double_quote = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\'') {
      if (!in_double_quote) {
        in_single_quote = !in_single_quote;
      }
    } else if (c == '"') {
      if (!in_single_quote) {
        in_double_quote = !in_double_quote;
      }
    } else if (c == '#' && !in_single_quote && !in_double_quote) {
      return std::string(line.substr(0, i));
    }
  }
  return std::string(line);
}

int CountIndent(string_view line) {
  int count = 0;
  for (char c : line) {
    if (c == ' ') {
      ++count;
    } else if (c == '\t') {
      count += 2;  // Treat tab as two spaces for simplicity
    } else {
      break;
    }
  }
  return count;
}

bool ParseKeyValue(string_view input, std::string* key, std::string* value) {
  size_t colon_pos = input.find(':');
  if (colon_pos == string_view::npos) {
    return false;
  }
  *key = Trim(input.substr(0, colon_pos));
  *value = Trim(input.substr(colon_pos + 1));
  return true;
}

std::string Unquote(string_view value) {
  std::string trimmed = Trim(value);
  if (trimmed.size() >= 2) {
    if ((trimmed.front() == '"' && trimmed.back() == '"') ||
        (trimmed.front() == '\'' && trimmed.back() == '\'')) {
      return std::string(trimmed.substr(1, trimmed.size() - 2));
    }
  }
  return trimmed;
}

std::vector<std::string> ParseInlineList(string_view value) {
  std::vector<std::string> items;
  std::string trimmed = Trim(value);
  if (trimmed.empty()) {
    return items;
  }
  if (trimmed.front() != '[' || trimmed.back() != ']') {
    items.push_back(Unquote(trimmed));
    return items;
  }
  string_view inner(trimmed.data() + 1, trimmed.size() - 2);
  if (inner.empty()) {
    return items;
  }
  for (string_view piece : absl::StrSplit(inner, ',', absl::SkipWhitespace())) {
    items.push_back(Unquote(piece));
  }
  return items;
}

absl::StatusOr<int> ParseInt(string_view value) {
  int result = 0;
  if (!absl::SimpleAtoi(value, &result)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected integer value, got '", value, "'"));
  }
  return result;
}

absl::StatusOr<int> ParseDurationSeconds(string_view value) {
  std::string lower = absl::AsciiStrToLower(std::string(value));
  if (lower.empty()) {
    return 0;
  }
  int multiplier = 1;
  if (absl::EndsWith(lower, "ms")) {
    lower = lower.substr(0, lower.size() - 2);
    multiplier = 0;  // Use 0 to indicate sub-second resolution
  } else if (absl::EndsWith(lower, "s")) {
    lower = lower.substr(0, lower.size() - 1);
  } else if (absl::EndsWith(lower, "m")) {
    lower = lower.substr(0, lower.size() - 1);
    multiplier = 60;
  }

  int numeric = 0;
  if (!absl::SimpleAtoi(lower, &numeric)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid duration value '", value, "'"));
  }

  if (multiplier == 0) {
    // Round milliseconds up to nearest whole second
    return (numeric + 999) / 1000;
  }
  return numeric * multiplier;
}

bool ParseBoolean(string_view value, bool* output) {
  std::string lower = absl::AsciiStrToLower(std::string(value));
  if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
    *output = true;
    return true;
  }
  if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
    *output = false;
    return true;
  }
  return false;
}

std::string DeriveTestName(const std::string& path) {
  std::filesystem::path fs_path(path);
  std::string stem = fs_path.stem().string();
  if (stem.empty()) {
    return path;
  }
  return stem;
}

absl::Status ParseScalarConfig(const std::string& key, const std::string& value,
                               TestSuiteConfig* config) {
  if (key == "timeout_per_test") {
    ASSIGN_OR_RETURN(int seconds, ParseDurationSeconds(value));
    config->timeout_seconds = seconds;
    return absl::OkStatus();
  }
  if (key == "retry_on_failure") {
    ASSIGN_OR_RETURN(int retries, ParseInt(value));
    config->retry_on_failure = retries;
    return absl::OkStatus();
  }
  if (key == "parallel_execution") {
    bool enabled = false;
    if (!ParseBoolean(value, &enabled)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid boolean for parallel_execution: '", value,
                        "'"));
    }
    config->parallel_execution = enabled;
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown config key: '", key, "'"));
}

absl::Status ParseStringListBlock(const std::vector<std::string>& lines,
                                  size_t* index, int base_indent,
                                  std::vector<std::string>* output) {
  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent < base_indent) {
      break;
    }
    if (indent != base_indent) {
      return absl::InvalidArgumentError(
          "Invalid indentation in list block");
    }
    if (trimmed.empty() || trimmed.front() != '-') {
      return absl::InvalidArgumentError("Expected list entry starting with '-'");
    }
    std::string value = Trim(trimmed.substr(1));
    output->push_back(Unquote(value));
    ++(*index);
  }
  return absl::OkStatus();
}

absl::Status ParseParametersBlock(const std::vector<std::string>& lines,
                                  size_t* index, int base_indent,
                                  std::map<std::string, std::string>* params) {
  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent < base_indent) {
      break;
    }
    if (indent != base_indent) {
      return absl::InvalidArgumentError("Invalid indentation in parameters block");
    }
    std::string key;
    std::string value;
    if (!ParseKeyValue(trimmed, &key, &value)) {
      return absl::InvalidArgumentError(
          "Expected key/value pair inside parameters block");
    }
    (*params)[key] = Unquote(value);
    ++(*index);
  }
  return absl::OkStatus();
}

absl::Status ParseTestCaseEntry(const std::vector<std::string>& lines,
                                size_t* index, int base_indent,
                                TestGroupDefinition* group) {
  const std::string& raw_line = lines[*index];
  std::string stripped = StripComment(raw_line);
  int indent = CountIndent(stripped);
  if (indent != base_indent) {
    return absl::InvalidArgumentError("Invalid indentation for test case entry");
  }

  size_t dash_pos = stripped.find('-');
  if (dash_pos == std::string::npos) {
    return absl::InvalidArgumentError("Malformed list entry in tests block");
  }

  std::string content = Trim(stripped.substr(dash_pos + 1));
  TestCaseDefinition test;
  test.group_name = group->name;

  auto commit_test = [&]() {
    if (test.script_path.empty()) {
      return absl::InvalidArgumentError("Test case missing script_path");
    }
    if (test.name.empty()) {
      test.name = DeriveTestName(test.script_path);
    }
    if (test.id.empty()) {
      test.id = absl::StrCat(test.group_name, ":", test.name);
    }
    group->tests.push_back(std::move(test));
    return absl::OkStatus();
  };

  if (content.empty()) {
    ++(*index);
  } else if (content.find(':') == std::string::npos) {
    test.script_path = Unquote(content);
    ++(*index);
    return commit_test();
  } else {
    std::string key;
    std::string value;
    if (!ParseKeyValue(content, &key, &value)) {
      return absl::InvalidArgumentError("Malformed key/value in test entry");
    }
    if (key == "path" || key == "script" || key == "script_path") {
      test.script_path = Unquote(value);
    } else if (key == "name") {
      test.name = Unquote(value);
    } else if (key == "description") {
      test.description = Unquote(value);
    } else if (key == "id") {
      test.id = Unquote(value);
    } else if (key == "tags") {
      auto tags = ParseInlineList(value);
      test.tags.insert(test.tags.end(), tags.begin(), tags.end());
    } else {
      test.parameters[key] = Unquote(value);
    }
    ++(*index);
  }

  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent_next = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent_next <= base_indent) {
      break;
    }
    if (indent_next == base_indent + 2) {
      std::string key;
      std::string value;
      if (!ParseKeyValue(trimmed, &key, &value)) {
        return absl::InvalidArgumentError(
            "Expected key/value pair in test definition");
      }
      if (key == "path" || key == "script" || key == "script_path") {
        test.script_path = Unquote(value);
      } else if (key == "name") {
        test.name = Unquote(value);
      } else if (key == "description") {
        test.description = Unquote(value);
      } else if (key == "id") {
        test.id = Unquote(value);
      } else if (key == "tags") {
        auto tags = ParseInlineList(value);
        if (tags.empty() && value.empty()) {
          ++(*index);
          RETURN_IF_ERROR(ParseStringListBlock(lines, index, indent_next + 2,
                                              &test.tags));
          continue;
        }
        test.tags.insert(test.tags.end(), tags.begin(), tags.end());
      } else if (key == "parameters") {
        if (!value.empty()) {
          return absl::InvalidArgumentError(
              "parameters block must be indented on following lines");
        }
        ++(*index);
        RETURN_IF_ERROR(ParseParametersBlock(lines, index, indent_next + 2,
                                             &test.parameters));
        continue;
      } else {
        test.parameters[key] = Unquote(value);
      }
      ++(*index);
    } else {
      return absl::InvalidArgumentError(
          "Unexpected indentation inside test entry");
    }
  }

  return commit_test();
}

absl::Status ParseTestsBlock(const std::vector<std::string>& lines,
                             size_t* index, int base_indent,
                             TestGroupDefinition* group) {
  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent < base_indent) {
      break;
    }
    if (indent != base_indent) {
      return absl::InvalidArgumentError(
          "Invalid indentation inside tests block");
    }
    RETURN_IF_ERROR(ParseTestCaseEntry(lines, index, base_indent, group));
  }
  return absl::OkStatus();
}

absl::Status ParseGroupEntry(const std::vector<std::string>& lines,
                             size_t* index, TestSuiteDefinition* suite) {
  const std::string& raw_line = lines[*index];
  std::string stripped = StripComment(raw_line);
  int base_indent = CountIndent(stripped);

  size_t dash_pos = stripped.find('-');
  if (dash_pos == std::string::npos || dash_pos < base_indent) {
    return absl::InvalidArgumentError("Expected '-' to start group entry");
  }

  std::string content = Trim(stripped.substr(dash_pos + 1));
  TestGroupDefinition group;

  if (!content.empty()) {
    std::string key;
    std::string value;
    if (!ParseKeyValue(content, &key, &value)) {
      return absl::InvalidArgumentError("Malformed group entry");
    }
    if (key == "name") {
      group.name = Unquote(value);
    } else if (key == "description") {
      group.description = Unquote(value);
    } else if (key == "depends_on") {
      auto deps = ParseInlineList(value);
      group.depends_on.insert(group.depends_on.end(), deps.begin(), deps.end());
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown field in group entry: '", key, "'"));
    }
  }

  ++(*index);

  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent <= base_indent) {
      break;
    }
    if (indent == base_indent + 2) {
      std::string key;
      std::string value;
      if (!ParseKeyValue(trimmed, &key, &value)) {
        return absl::InvalidArgumentError(
            "Expected key/value pair inside group definition");
      }
      if (key == "name") {
        group.name = Unquote(value);
        ++(*index);
      } else if (key == "description") {
        group.description = Unquote(value);
        ++(*index);
      } else if (key == "depends_on") {
        if (!value.empty()) {
          auto deps = ParseInlineList(value);
          group.depends_on.insert(group.depends_on.end(), deps.begin(),
                                  deps.end());
          ++(*index);
        } else {
          ++(*index);
          RETURN_IF_ERROR(ParseStringListBlock(lines, index, base_indent + 4,
                                              &group.depends_on));
        }
      } else if (key == "tests") {
        if (!value.empty()) {
          return absl::InvalidArgumentError(
              "tests block must be defined as indented list");
        }
        ++(*index);
        RETURN_IF_ERROR(
            ParseTestsBlock(lines, index, base_indent + 4, &group));
      } else {
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown attribute in group definition: '", key,
                          "'"));
      }
    } else {
      return absl::InvalidArgumentError(
          "Unexpected indentation inside group definition");
    }
  }

  if (group.name.empty()) {
    return absl::InvalidArgumentError(
        "Each test group must define a name");
  }
  suite->groups.push_back(std::move(group));
  return absl::OkStatus();
}

absl::Status ParseGroupBlock(const std::vector<std::string>& lines,
                             size_t* index, TestSuiteDefinition* suite) {
  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent < 2) {
      break;
    }
    if (indent != 2) {
      return absl::InvalidArgumentError(
          "Invalid indentation inside test_groups block");
    }
    RETURN_IF_ERROR(ParseGroupEntry(lines, index, suite));
  }
  return absl::OkStatus();
}

absl::Status ParseConfigBlock(const std::vector<std::string>& lines,
                              size_t* index, TestSuiteConfig* config) {
  while (*index < lines.size()) {
    std::string raw = StripComment(lines[*index]);
    std::string trimmed = Trim(raw);
    int indent = CountIndent(raw);
    if (trimmed.empty()) {
      ++(*index);
      continue;
    }
    if (indent < 2) {
      break;
    }
    if (indent != 2) {
      return absl::InvalidArgumentError(
          "Invalid indentation inside config block");
    }
    std::string key;
    std::string value;
    if (!ParseKeyValue(trimmed, &key, &value)) {
      return absl::InvalidArgumentError(
          "Expected key/value pair inside config block");
    }
    RETURN_IF_ERROR(ParseScalarConfig(key, value, config));
    ++(*index);
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<TestSuiteDefinition> ParseTestSuiteDefinition(
    absl::string_view content) {
  std::vector<std::string> lines = absl::StrSplit(content, '\n');
  TestSuiteDefinition suite;
  size_t index = 0;

  while (index < lines.size()) {
    std::string raw = StripComment(lines[index]);
    std::string trimmed = Trim(raw);
    if (trimmed.empty()) {
      ++index;
      continue;
    }

    int indent = CountIndent(raw);
    if (indent != 0) {
      return absl::InvalidArgumentError(
          "Top-level entries must not be indented in suite definition");
    }

    std::string key;
    std::string value;
    if (!ParseKeyValue(trimmed, &key, &value)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Malformed top-level entry: '", trimmed, "'"));
    }

    if (key == "name") {
      suite.name = Unquote(value);
      ++index;
    } else if (key == "description") {
      suite.description = Unquote(value);
      ++index;
    } else if (key == "version") {
      suite.version = Unquote(value);
      ++index;
    } else if (key == "config") {
      if (!value.empty()) {
        return absl::InvalidArgumentError(
            "config block must not specify inline value");
      }
      ++index;
      RETURN_IF_ERROR(ParseConfigBlock(lines, &index, &suite.config));
    } else if (key == "test_groups") {
      if (!value.empty()) {
        return absl::InvalidArgumentError(
            "test_groups must be defined as an indented list");
      }
      ++index;
      RETURN_IF_ERROR(ParseGroupBlock(lines, &index, &suite));
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown top-level key: '", key, "'"));
    }
  }

  if (suite.name.empty()) {
    suite.name = "Unnamed Suite";
  }
  if (suite.version.empty()) {
    suite.version = "1.0";
  }
  return suite;
}

absl::StatusOr<TestSuiteDefinition> LoadTestSuiteFromFile(
    const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Failed to open test suite file '", path, "'"));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return ParseTestSuiteDefinition(buffer.str());
}

}  // namespace cli
}  // namespace yaze
