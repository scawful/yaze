#ifndef YAZE_CLI_SERVICE_TEST_SUITE_LOADER_H_
#define YAZE_CLI_SERVICE_TEST_SUITE_LOADER_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "cli/service/test_suite.h"

namespace yaze {
namespace cli {

absl::StatusOr<TestSuiteDefinition> ParseTestSuiteDefinition(absl::string_view content);
absl::StatusOr<TestSuiteDefinition> LoadTestSuiteFromFile(const std::string& path);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TEST_SUITE_LOADER_H_
