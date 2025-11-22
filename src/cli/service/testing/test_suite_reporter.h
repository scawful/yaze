#ifndef YAZE_CLI_SERVICE_TEST_SUITE_REPORTER_H_
#define YAZE_CLI_SERVICE_TEST_SUITE_REPORTER_H_

#include <string>

#include "absl/status/statusor.h"
#include "cli/service/testing/test_suite.h"

namespace yaze {
namespace cli {

std::string BuildTextSummary(const TestSuiteRunSummary& summary);
absl::StatusOr<std::string> BuildJUnitReport(
    const TestSuiteRunSummary& summary);
absl::Status WriteJUnitReport(const TestSuiteRunSummary& summary,
                              const std::string& output_path);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TEST_SUITE_REPORTER_H_
