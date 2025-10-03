#ifndef YAZE_CLI_SERVICE_TEST_SUITE_WRITER_H_
#define YAZE_CLI_SERVICE_TEST_SUITE_WRITER_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/test_suite.h"

namespace yaze {
namespace cli {

// Serializes a TestSuiteDefinition into a YAML document that is accepted by
// ParseTestSuiteDefinition().
std::string BuildTestSuiteYaml(const TestSuiteDefinition& suite);

// Writes the suite definition to the supplied path, creating parent
// directories if necessary. When overwrite is false and the file already
// exists, an ALREADY_EXISTS error is returned.
absl::Status WriteTestSuiteToFile(const TestSuiteDefinition& suite,
                                  const std::string& path,
                                  bool overwrite = false);

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TEST_SUITE_WRITER_H_
