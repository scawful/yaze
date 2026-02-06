#include "app/test/agent_tools_test.h"
#include "app/test/test_manager.h"
#include "app/test/z3ed_test_suite.h"
#include "util/log.h"

#include "absl/flags/parse.h"

#include "imgui.h"

#if YAZE_ENABLE_EXPERIMENTAL_APP_TEST_SUITES
#include "app/test/dungeon_editor_test_suite.h"
#include "app/test/graphics_editor_test_suite.h"
#include "app/test/overworld_editor_test_suite.h"
#endif

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

#if YAZE_ENABLE_EXPERIMENTAL_APP_TEST_SUITES
  // Optional WIP suites. Off by default so they don't block baseline builds.
  yaze::test::TestManager::Get().RegisterTestSuite(
      std::make_unique<yaze::test::DungeonEditorTestSuite>());
  yaze::test::TestManager::Get().RegisterTestSuite(
      std::make_unique<yaze::test::OverworldEditorTestSuite>());
  yaze::test::TestManager::Get().RegisterTestSuite(
      std::make_unique<yaze::test::GraphicsEditorTestSuite>());
#endif

  LOG_INFO("Test", "Running all tests...");
  auto status = yaze::test::TestManager::Get().RunAllTests();

  // Cleanup
  ImGui::DestroyContext();

  if (status.ok()) {
    const auto& results = yaze::test::TestManager::Get().GetLastResults();
    LOG_INFO("Test", "Tests passed: %zu/%zu", results.passed_tests,
             results.total_tests);

    if (results.failed_tests > 0) {
      LOG_ERROR("Test", "--- FAILED TESTS ---");
      for (const auto& result : results.individual_results) {
        if (result.status == yaze::test::TestStatus::kFailed) {
          LOG_ERROR("Test", "[FAILED] %s::%s - %s",
                    result.suite_name.c_str(), result.name.c_str(),
                    result.error_message.c_str());
        }
      }
      LOG_ERROR("Test", "--------------------");
    }

    return (results.failed_tests == 0) ? 0 : 1;
  } else {
    LOG_ERROR("Test", "Test execution failed: %s", status.ToString().c_str());
    return 1;
  }
}
