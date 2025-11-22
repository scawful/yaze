#include "app/test/z3ed_test_suite.h"

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace test {

void RegisterZ3edTestSuites() {
#ifdef YAZE_WITH_GRPC
  LOG_INFO("Z3edTests", "Registering z3ed AI Agent test suites");

  // Register AI Agent test suite
  TestManager::Get().RegisterTestSuite(
      std::make_unique<Z3edAIAgentTestSuite>());

  // Register GUI Automation test suite
  TestManager::Get().RegisterTestSuite(
      std::make_unique<GUIAutomationTestSuite>());

  LOG_INFO("Z3edTests", "z3ed test suites registered successfully");
#else
  LOG_INFO("Z3edTests", "z3ed test suites not available (YAZE_WITH_GRPC=OFF)");
#endif  // YAZE_WITH_GRPC
}

}  // namespace test
}  // namespace yaze
