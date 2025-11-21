#ifndef YAZE_CLI_SERVICE_TEST_SUITE_H_
#define YAZE_CLI_SERVICE_TEST_SUITE_H_

#include <map>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "cli/service/gui/gui_automation_client.h"

namespace yaze {
namespace cli {

struct TestSuiteConfig {
  int timeout_seconds = 0;
  int retry_on_failure = 0;
  bool parallel_execution = false;
};

struct TestCaseDefinition {
  std::string id;
  std::string name;
  std::string script_path;
  std::string description;
  std::string group_name;
  std::vector<std::string> tags;
  std::map<std::string, std::string> parameters;
};

struct TestGroupDefinition {
  std::string name;
  std::string description;
  std::vector<std::string> depends_on;
  std::vector<TestCaseDefinition> tests;
};

struct TestSuiteDefinition {
  std::string name;
  std::string description;
  std::string version;
  TestSuiteConfig config;
  std::vector<TestGroupDefinition> groups;

  const TestGroupDefinition* FindGroup(absl::string_view group_name) const;
};

inline const TestGroupDefinition* TestSuiteDefinition::FindGroup(
    absl::string_view group_name) const {
  for (const auto& group : groups) {
    if (group.name == group_name) {
      return &group;
    }
  }
  return nullptr;
}

enum class TestCaseOutcome { kPassed, kFailed, kError, kSkipped };

struct TestCaseRunResult {
  const TestCaseDefinition* test = nullptr;
  const TestGroupDefinition* group = nullptr;
  TestCaseOutcome outcome = TestCaseOutcome::kError;
  absl::Time start_time = absl::InfinitePast();
  absl::Duration duration = absl::ZeroDuration();
  int attempts = 0;
  int retries = 0;
  std::string message;
  std::string replay_session_id;
  std::vector<AssertionOutcome> assertions;
  std::vector<std::string> logs;
};

struct TestSuiteRunSummary {
  const TestSuiteDefinition* suite = nullptr;
  std::vector<TestCaseRunResult> results;
  absl::Time started_at = absl::Now();
  absl::Duration total_duration = absl::ZeroDuration();
  int passed = 0;
  int failed = 0;
  int errors = 0;
  int skipped = 0;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TEST_SUITE_H_
