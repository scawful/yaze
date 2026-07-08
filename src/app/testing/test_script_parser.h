#ifndef YAZE_APP_CORE_TESTING_TEST_SCRIPT_PARSER_H_
#define YAZE_APP_CORE_TESTING_TEST_SCRIPT_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace yaze {
namespace test {

struct TestScriptStep {
  std::string action;
  std::string target;
  std::string widget_key;
  std::string click_type;
  std::string text;
  bool clear_first = false;
  std::string condition;
  int timeout_ms = 0;
  std::string region;
  std::string format;
  bool expect_success = true;
  std::string expect_status;
  std::string expect_message;
  std::vector<std::string> expect_assertion_failures;
  std::map<std::string, int32_t> expect_metrics;
};

struct TestScript {
  int schema_version = 1;
  std::string recording_id;
  std::string name;
  std::string description;
  absl::Time created_at = absl::InfinitePast();
  absl::Duration duration = absl::ZeroDuration();
  std::vector<TestScriptStep> steps;
};

class TestScriptParser {
 public:
  static absl::Status WriteToFile(const TestScript& script,
                                  const std::string& path);

  static absl::StatusOr<TestScript> ParseFromFile(const std::string& path);
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_CORE_TESTING_TEST_SCRIPT_PARSER_H_
