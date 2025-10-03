#include "cli/service/test_suite_writer.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"

namespace yaze {
namespace cli {
namespace {

std::string Indent(int count) { return std::string(count, ' '); }

std::string QuoteYaml(absl::string_view value) {
  std::string escaped(value);
  absl::StrReplaceAll({{"\\", "\\\\"}, {"\"", "\\\""}}, &escaped);
  return absl::StrCat("\"", escaped, "\"");
}

void AppendLine(std::string* out, int indent, absl::string_view line) {
  out->append(Indent(indent));
  out->append(line.data(), line.size());
  out->append("\n");
}

void AppendScalar(std::string* out, int indent, absl::string_view key,
                  absl::string_view value, bool quote) {
  out->append(Indent(indent));
  out->append(key.data(), key.size());
  out->append(":");
  if (!value.empty()) {
    out->append(" ");
    if (quote) {
      out->append(QuoteYaml(value));
    } else {
      out->append(value.data(), value.size());
    }
  }
  out->append("\n");
}

std::string FormatDuration(int seconds) {
  if (seconds <= 0) {
    return "0s";
  }
  if (seconds % 60 == 0) {
    return absl::StrCat(seconds / 60, "m");
  }
  return absl::StrCat(seconds, "s");
}

std::string FormatBool(bool value) { return value ? "true" : "false"; }

std::string JoinQuotedList(const std::vector<std::string>& values) {
  if (values.empty()) {
    return "[]";
  }
  std::vector<std::string> quoted;
  quoted.reserve(values.size());
  for (const auto& v : values) {
    quoted.push_back(QuoteYaml(v));
  }
  return absl::StrCat("[", absl::StrJoin(quoted, ", "), "]");
}

}  // namespace

std::string BuildTestSuiteYaml(const TestSuiteDefinition& suite) {
  std::string output;

  if (!suite.name.empty()) {
    AppendScalar(&output, 0, "name", suite.name, /*quote=*/true);
  } else {
    AppendScalar(&output, 0, "name", "Unnamed Suite", /*quote=*/true);
  }
  if (!suite.description.empty()) {
    AppendScalar(&output, 0, "description", suite.description,
                 /*quote=*/true);
  }
  if (!suite.version.empty()) {
    AppendScalar(&output, 0, "version", suite.version, /*quote=*/true);
  }

  AppendLine(&output, 0, "config:");
  AppendScalar(&output, 2, "timeout_per_test",
               FormatDuration(suite.config.timeout_seconds),
               /*quote=*/false);
  AppendScalar(&output, 2, "retry_on_failure",
               absl::StrCat(suite.config.retry_on_failure),
               /*quote=*/false);
  AppendScalar(&output, 2, "parallel_execution",
               FormatBool(suite.config.parallel_execution),
               /*quote=*/false);

  AppendLine(&output, 0, "test_groups:");
  for (size_t i = 0; i < suite.groups.size(); ++i) {
    const TestGroupDefinition& group = suite.groups[i];
    AppendLine(&output, 2, "- name: " + QuoteYaml(group.name));
    if (!group.description.empty()) {
      AppendScalar(&output, 4, "description", group.description,
                   /*quote=*/true);
    }
    if (!group.depends_on.empty()) {
      AppendScalar(&output, 4, "depends_on",
                   JoinQuotedList(group.depends_on), /*quote=*/false);
    }

    AppendLine(&output, 4, "tests:");
    for (const TestCaseDefinition& test : group.tests) {
      AppendLine(&output, 6, "- path: " + QuoteYaml(test.script_path));
      if (!test.name.empty() && test.name != test.script_path) {
        AppendScalar(&output, 8, "name", test.name, /*quote=*/true);
      }
      if (!test.description.empty()) {
        AppendScalar(&output, 8, "description", test.description,
                     /*quote=*/true);
      }
      if (!test.tags.empty()) {
        AppendScalar(&output, 8, "tags", JoinQuotedList(test.tags),
                     /*quote=*/false);
      }
      if (!test.parameters.empty()) {
        AppendLine(&output, 8, "parameters:");
        for (const auto& [key, value] : test.parameters) {
          AppendScalar(&output, 10, key, value, /*quote=*/true);
        }
      }
    }

    if (!group.tests.empty() && i + 1 < suite.groups.size()) {
      output.append("\n");
    }
  }

  return output;
}

absl::Status WriteTestSuiteToFile(const TestSuiteDefinition& suite,
                                  const std::string& path, bool overwrite) {
  std::filesystem::path output_path(path);
  std::error_code ec;
  if (!overwrite && std::filesystem::exists(output_path, ec)) {
    if (!ec) {
      return absl::AlreadyExistsError(
          absl::StrCat("Test suite file already exists: ", path));
    }
  }

  std::filesystem::path parent = output_path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return absl::InternalError(absl::StrCat(
          "Failed to create directories for ", path, ": ", ec.message()));
    }
  }

  std::ofstream stream(output_path, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to open file for writing: ", path));
  }

  std::string yaml = BuildTestSuiteYaml(suite);
  stream << yaml;
  stream.close();

  if (!stream) {
    return absl::InternalError(
        absl::StrCat("Failed to write test suite to ", path));
  }
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
