#ifndef YAZE_CLI_AUTOMATION_TEST_GENERATION_API_H
#define YAZE_CLI_AUTOMATION_TEST_GENERATION_API_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/editor.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {
namespace automation {

/**
 * @brief API for automated test generation and execution
 *
 * Enables AI agents to generate, execute, and validate tests for YAZE components.
 * Supports recording user interactions, generating regression tests, and creating
 * comprehensive test suites.
 */
class TestGenerationAPI {
 public:
  // ============================================================================
  // Test Frameworks
  // ============================================================================

  enum class TestFramework {
    GTEST,      // Google Test
    CATCH2,     // Catch2
    DOCTEST,    // doctest
    IMGUI_TEST  // ImGui Test Engine
  };

  // ============================================================================
  // Recording Infrastructure
  // ============================================================================

  /**
   * @brief Recorded interaction for test generation
   */
  struct RecordedInteraction {
    std::chrono::steady_clock::time_point timestamp;
    std::string action_type;  // click, drag, keyboard, menu, etc.
    nlohmann::json parameters;
    nlohmann::json pre_state;   // State before action
    nlohmann::json post_state;  // State after action
  };

  /**
   * @brief Test recording session
   */
  struct RecordingSession {
    std::string name;
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point ended_at;
    std::vector<RecordedInteraction> interactions;
    nlohmann::json initial_state;
    nlohmann::json final_state;
    std::map<std::string, std::string> metadata;
  };

  /**
   * @brief Start recording user interactions
   * @param test_name Name for the test being recorded
   * @param capture_state If true, capture full editor state at each step
   * @return Status of recording start
   */
  absl::Status StartRecording(const std::string& test_name,
                              bool capture_state = true);

  /**
   * @brief Stop current recording session
   * @return Recorded session or error
   */
  absl::StatusOr<RecordingSession> StopRecording();

  /**
   * @brief Pause recording temporarily
   */
  void PauseRecording();

  /**
   * @brief Resume paused recording
   */
  void ResumeRecording();

  /**
   * @brief Add annotation to current recording
   * @param annotation Comment or description to add
   */
  void AnnotateRecording(const std::string& annotation);

  // ============================================================================
  // Test Code Generation
  // ============================================================================

  /**
   * @brief Options for test generation
   */
  struct GenerationOptions {
    TestFramework framework = TestFramework::GTEST;
    bool include_setup_teardown = true;
    bool generate_assertions = true;
    bool include_comments = true;
    bool use_fixtures = true;
    bool generate_mocks = false;
    std::string namespace_name = "yaze::test";
    std::string output_directory;
  };

  /**
   * @brief Generate test code from recording
   * @param session Recorded session to convert
   * @param options Generation options
   * @return Generated test code or error
   */
  absl::StatusOr<std::string> GenerateTestFromRecording(
      const RecordingSession& session,
      const GenerationOptions& options = {});

  /**
   * @brief Generate test from specification
   */
  struct TestSpecification {
    std::string class_under_test;
    std::vector<std::string> methods_to_test;
    bool include_edge_cases = true;
    bool include_error_cases = true;
    bool include_performance_tests = false;
    std::map<std::string, nlohmann::json> custom_cases;
  };

  /**
   * @brief Generate comprehensive test suite from specification
   * @param spec Test specification
   * @param options Generation options
   * @return Generated test code or error
   */
  absl::StatusOr<std::string> GenerateTestSuite(
      const TestSpecification& spec,
      const GenerationOptions& options = {});

  /**
   * @brief Generate regression test from bug report
   * @param bug_description Description of the bug
   * @param repro_steps Steps to reproduce
   * @param expected_behavior Expected correct behavior
   * @param options Generation options
   * @return Generated regression test or error
   */
  absl::StatusOr<std::string> GenerateRegressionTest(
      const std::string& bug_description,
      const std::vector<std::string>& repro_steps,
      const std::string& expected_behavior,
      const GenerationOptions& options = {});

  // ============================================================================
  // Test Fixtures and Mocks
  // ============================================================================

  /**
   * @brief Generate test fixture from current editor state
   * @param editor Editor to capture state from
   * @param fixture_name Name for the fixture class
   * @return Generated fixture code or error
   */
  absl::StatusOr<std::string> GenerateFixture(
      app::editor::Editor* editor,
      const std::string& fixture_name);

  /**
   * @brief Generate mock object for testing
   * @param interface_name Interface to mock
   * @param mock_name Name for mock class
   * @return Generated mock code or error
   */
  absl::StatusOr<std::string> GenerateMock(
      const std::string& interface_name,
      const std::string& mock_name);

  // ============================================================================
  // Test Execution
  // ============================================================================

  /**
   * @brief Test execution result
   */
  struct TestResult {
    bool passed;
    std::string test_name;
    double execution_time_ms;
    std::string output;
    std::vector<std::string> failures;
    std::vector<std::string> warnings;
    nlohmann::json coverage_data;
  };

  /**
   * @brief Execute generated test code
   * @param test_code Generated test code
   * @param compile_only If true, only compile without running
   * @return Test results or error
   */
  absl::StatusOr<TestResult> ExecuteGeneratedTest(
      const std::string& test_code,
      bool compile_only = false);

  /**
   * @brief Run existing test file
   * @param test_file Path to test file
   * @param filter Optional test filter pattern
   * @return Test results or error
   */
  absl::StatusOr<std::vector<TestResult>> RunTestFile(
      const std::string& test_file,
      const std::string& filter = "");

  // ============================================================================
  // Coverage Analysis
  // ============================================================================

  /**
   * @brief Coverage report for tested code
   */
  struct CoverageReport {
    double line_coverage_percent;
    double branch_coverage_percent;
    double function_coverage_percent;
    std::map<std::string, double> file_coverage;
    std::vector<std::string> uncovered_lines;
    std::vector<std::string> uncovered_branches;
  };

  /**
   * @brief Generate coverage report for tests
   * @param test_results Results from test execution
   * @return Coverage report or error
   */
  absl::StatusOr<CoverageReport> GenerateCoverageReport(
      const std::vector<TestResult>& test_results);

  // ============================================================================
  // Test Validation
  // ============================================================================

  /**
   * @brief Validate generated test code
   * @param test_code Code to validate
   * @return List of validation issues (empty if valid)
   */
  std::vector<std::string> ValidateTestCode(const std::string& test_code);

  /**
   * @brief Check if test covers specified requirements
   * @param test_code Test code to analyze
   * @param requirements List of requirements to check
   * @return Coverage mapping of requirements to test cases
   */
  std::map<std::string, std::vector<std::string>> CheckRequirementsCoverage(
      const std::string& test_code,
      const std::vector<std::string>& requirements);

  // ============================================================================
  // AI-Assisted Test Generation
  // ============================================================================

  /**
   * @brief Use AI model to suggest test cases
   * @param code_under_test Code to generate tests for
   * @param model_name AI model to use (e.g., "gemini", "ollama")
   * @return Suggested test cases as JSON
   */
  absl::StatusOr<nlohmann::json> SuggestTestCases(
      const std::string& code_under_test,
      const std::string& model_name = "gemini");

  /**
   * @brief Use AI to improve existing test
   * @param test_code Existing test code
   * @param improvement_goals What to improve (coverage, performance, etc.)
   * @return Improved test code or error
   */
  absl::StatusOr<std::string> ImproveTest(
      const std::string& test_code,
      const std::vector<std::string>& improvement_goals);

  // ============================================================================
  // Test Organization
  // ============================================================================

  /**
   * @brief Organize tests into suites
   * @param test_files List of test files
   * @return Organization structure as JSON
   */
  nlohmann::json OrganizeTestSuites(
      const std::vector<std::string>& test_files);

  /**
   * @brief Generate test documentation
   * @param test_file Test file to document
   * @param format Documentation format (markdown, html, etc.)
   * @return Generated documentation or error
   */
  absl::StatusOr<std::string> GenerateTestDocumentation(
      const std::string& test_file,
      const std::string& format = "markdown");

  // ============================================================================
  // Callbacks and Events
  // ============================================================================

  using RecordingCallback = std::function<void(const RecordedInteraction&)>;
  using GenerationCallback = std::function<void(const std::string& progress)>;

  /**
   * @brief Set callback for recording events
   */
  void SetRecordingCallback(RecordingCallback callback);

  /**
   * @brief Set callback for generation progress
   */
  void SetGenerationCallback(GenerationCallback callback);

 private:
  std::unique_ptr<RecordingSession> current_recording_;
  bool is_recording_ = false;
  bool is_paused_ = false;
  RecordingCallback recording_callback_;
  GenerationCallback generation_callback_;

  // Helper methods
  std::string GenerateTestHeader(const GenerationOptions& options) const;
  std::string GenerateTestBody(const RecordingSession& session,
                               const GenerationOptions& options) const;
  std::string GenerateAssertion(const RecordedInteraction& interaction,
                                TestFramework framework) const;
  absl::Status CompileTest(const std::string& test_code) const;
};

}  // namespace automation
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_AUTOMATION_TEST_GENERATION_API_H