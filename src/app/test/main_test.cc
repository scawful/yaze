#include "app/test/test_manager.h"
#include "app/test/z3ed_test_suite.h"
#include "app/test/agent_tools_test.h"
#include "util/log.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#include "imgui.h"

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  
  // Initialize ImGui context for tests
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Configure minimal logging
  yaze::util::LogManager::instance().configure(
      yaze::util::LogLevel::INFO, "", {"Test"});

  LOG_INFO("Test", "Registering test suites...");
  yaze::test::RegisterZ3edTestSuites();
  
  // Register our new agent tools test suite
  yaze::test::RegisterAgentToolsTestSuite();

  LOG_INFO("Test", "Running all tests...");
  auto status = yaze::test::TestManager::Get().RunAllTests();

  // Cleanup
  ImGui::DestroyContext();

  if (status.ok()) {
    const auto& results = yaze::test::TestManager::Get().GetLastResults();
    LOG_INFO("Test", "Tests passed: %zu/%zu", results.passed_tests, results.total_tests);
    return (results.failed_tests == 0) ? 0 : 1;
  } else {
    LOG_ERROR("Test", "Test execution failed: %s", status.ToString().c_str());
    return 1;
  }
}
