#include "cli/service/testing/test_suite_reporter.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace {

std::string OutcomeToString(TestCaseOutcome outcome) {
  switch (outcome) {
    case TestCaseOutcome::kPassed:
      return "PASS";
    case TestCaseOutcome::kFailed:
      return "FAIL";
    case TestCaseOutcome::kError:
      return "ERROR";
    case TestCaseOutcome::kSkipped:
      return "SKIP";
  }
  return "UNKNOWN";
}

std::string EscapeXml(absl::string_view input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (char c : input) {
    switch (c) {
      case '&':
        escaped.append("&amp;");
        break;
      case '<':
        escaped.append("&lt;");
        break;
      case '>':
        escaped.append("&gt;");
        break;
      case '\"':
        escaped.append("&quot;");
        break;
      case '\'':
        escaped.append("&apos;");
        break;
      default:
        escaped.push_back(c);
        break;
    }
  }
  return escaped;
}

std::string JoinLogs(const std::vector<std::string>& logs) {
  if (logs.empty()) {
    return {};
  }
  return absl::StrJoin(logs, "\n");
}

void ComputeSummaryCounters(const TestSuiteRunSummary& summary, int* total,
                            int* passed, int* failed, int* errors,
                            int* skipped) {
  *total = static_cast<int>(summary.results.size());
  *passed = summary.passed;
  *failed = summary.failed;
  *errors = summary.errors;
  *skipped = summary.skipped;

  if (*passed + *failed + *errors + *skipped != *total) {
    *passed = *failed = *errors = *skipped = 0;
    for (const auto& result : summary.results) {
      switch (result.outcome) {
        case TestCaseOutcome::kPassed:
          ++(*passed);
          break;
        case TestCaseOutcome::kFailed:
          ++(*failed);
          break;
        case TestCaseOutcome::kError:
          ++(*errors);
          break;
        case TestCaseOutcome::kSkipped:
          ++(*skipped);
          break;
      }
    }
  }
}

absl::Duration TotalDuration(const TestSuiteRunSummary& summary) {
  if (summary.total_duration > absl::ZeroDuration()) {
    return summary.total_duration;
  }
  absl::Duration total = absl::ZeroDuration();
  for (const auto& result : summary.results) {
    total += result.duration;
  }
  return total;
}

}  // namespace

std::string BuildTextSummary(const TestSuiteRunSummary& summary) {
  std::ostringstream oss;
  int total = 0;
  int passed = 0;
  int failed = 0;
  int errors = 0;
  int skipped = 0;
  ComputeSummaryCounters(summary, &total, &passed, &failed, &errors, &skipped);

  absl::Time timestamp = summary.started_at;
  if (timestamp == absl::InfinitePast()) {
    timestamp = absl::Now();
  }

  oss << "Suite: "
      << (summary.suite && !summary.suite->name.empty() ? summary.suite->name
                                                        : "Unnamed Suite")
      << "\n";
  oss << "Started: "
      << absl::FormatTime("%Y-%m-%d %H:%M:%S", timestamp, absl::UTCTimeZone())
      << " UTC\n";
  oss << "Totals: " << total << " (" << passed << " passed, " << failed
      << " failed, " << errors << " errors, " << skipped << " skipped)";

  absl::Duration duration = TotalDuration(summary);
  if (duration > absl::ZeroDuration()) {
    oss << " in " << absl::StrFormat("%.2fs", absl::ToDoubleSeconds(duration));
  }
  oss << "\n\n";

  for (const auto& result : summary.results) {
    std::string group_name;
    if (result.group) {
      group_name = result.group->name;
    } else if (result.test) {
      group_name = result.test->group_name;
    }
    std::string test_name = result.test ? result.test->name : "<unnamed>";
    oss << "  [" << OutcomeToString(result.outcome) << "] ";
    if (!group_name.empty()) {
      oss << group_name << " :: ";
    }
    oss << test_name;
    if (result.duration > absl::ZeroDuration()) {
      oss << " ("
          << absl::StrFormat("%.2fs", absl::ToDoubleSeconds(result.duration))
          << ")";
    }
    oss << "\n";
    if (!result.message.empty() && result.outcome != TestCaseOutcome::kPassed) {
      oss << "    " << result.message << "\n";
    }
    if (!result.assertions.empty() &&
        result.outcome != TestCaseOutcome::kPassed) {
      for (const auto& assertion : result.assertions) {
        oss << "    - " << assertion.description << " : "
            << (assertion.passed ? "PASS" : "FAIL");
        if (!assertion.error_message.empty()) {
          oss << " (" << assertion.error_message << ")";
        }
        oss << "\n";
      }
    }
  }

  return oss.str();
}

absl::StatusOr<std::string> BuildJUnitReport(
    const TestSuiteRunSummary& summary) {
  std::ostringstream oss;
  int total = 0;
  int passed = 0;
  int failed = 0;
  int errors = 0;
  int skipped = 0;
  ComputeSummaryCounters(summary, &total, &passed, &failed, &errors, &skipped);

  absl::Time timestamp = summary.started_at;
  if (timestamp == absl::InfinitePast()) {
    timestamp = absl::Now();
  }
  absl::Duration duration = TotalDuration(summary);

  std::string suite_name =
      summary.suite ? summary.suite->name : "YAZE GUI Test Suite";

  oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  oss << "<testsuite name=\"" << EscapeXml(suite_name) << "\" tests=\"" << total
      << "\" failures=\"" << failed << "\" errors=\"" << errors
      << "\" skipped=\"" << skipped << "\" time=\""
      << absl::StrFormat("%.3f", absl::ToDoubleSeconds(duration))
      << "\" timestamp=\""
      << EscapeXml(absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", timestamp,
                                    absl::UTCTimeZone()))
      << "\">\n";

  for (const auto& result : summary.results) {
    std::string classname;
    if (result.group) {
      classname = result.group->name;
    } else if (result.test) {
      classname = result.test->group_name;
    }
    std::string test_name = result.test ? result.test->name : "Test";
    oss << "  <testcase classname=\"" << EscapeXml(classname) << "\" name=\""
        << EscapeXml(test_name) << "\" time=\""
        << absl::StrFormat("%.3f", absl::ToDoubleSeconds(result.duration))
        << "\">";

    if (result.outcome == TestCaseOutcome::kFailed) {
      std::string body = result.message;
      if (!result.assertions.empty()) {
        std::vector<std::string> assertion_lines;
        for (const auto& assertion : result.assertions) {
          assertion_lines.push_back(
              absl::StrCat(assertion.description, " => ",
                           assertion.passed ? "PASS" : "FAIL"));
        }
        body = absl::StrCat(body, "\n", absl::StrJoin(assertion_lines, "\n"));
      }
      oss << "\n    <failure message=\"" << EscapeXml(result.message) << "\">"
          << EscapeXml(body) << "</failure>";
      if (!result.logs.empty()) {
        oss << "\n    <system-out>" << EscapeXml(JoinLogs(result.logs))
            << "</system-out>";
      }
      oss << "\n  </testcase>\n";
      continue;
    }

    if (result.outcome == TestCaseOutcome::kError) {
      std::string detail = result.message;
      if (!result.logs.empty()) {
        detail = absl::StrCat(detail, "\n", JoinLogs(result.logs));
      }
      oss << "\n    <error message=\"" << EscapeXml(result.message) << "\">"
          << EscapeXml(detail) << "</error>";
      oss << "\n  </testcase>\n";
      continue;
    }

    if (!result.logs.empty()) {
      oss << "\n    <system-out>" << EscapeXml(JoinLogs(result.logs))
          << "</system-out>";
    }
    oss << "\n  </testcase>\n";
  }

  oss << "</testsuite>\n";
  return oss.str();
}

absl::Status WriteJUnitReport(const TestSuiteRunSummary& summary,
                              const std::string& output_path) {
  ASSIGN_OR_RETURN(std::string xml, BuildJUnitReport(summary));
  std::filesystem::path path(output_path);
  if (path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return absl::InternalError(absl::StrCat(
          "Failed to create directory for JUnit report: ", ec.message()));
    }
  }
  std::ofstream out(path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrCat("Unable to open JUnit output file '", output_path, "'"));
  }
  out << xml;
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
