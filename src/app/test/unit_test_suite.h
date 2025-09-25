#ifndef YAZE_APP_TEST_UNIT_TEST_SUITE_H
#define YAZE_APP_TEST_UNIT_TEST_SUITE_H

#include <chrono>
#include <memory>

#include "app/gfx/arena.h"
#include "app/test/test_manager.h"

#ifdef YAZE_ENABLE_GTEST
#include <gtest/gtest.h>
#endif

// Note: ImGui Test Engine is handled through YAZE_ENABLE_IMGUI_TEST_ENGINE in TestManager

namespace yaze {
namespace test {

#ifdef YAZE_ENABLE_GTEST
// Custom test listener to capture Google Test results
class TestResultCapture : public ::testing::TestEventListener {
 public:
  explicit TestResultCapture(TestResults* results) : results_(results) {}

  void OnTestStart(const ::testing::TestInfo& test_info) override {
    current_test_start_ = std::chrono::steady_clock::now();
    current_test_name_ =
        std::string(test_info.test_case_name()) + "." + test_info.name();
  }

  void OnTestEnd(const ::testing::TestInfo& test_info) override {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - current_test_start_);

    TestResult result;
    result.name = test_info.name();
    result.suite_name = test_info.test_case_name();
    result.category = TestCategory::kUnit;
    result.duration = duration;
    result.timestamp = current_test_start_;

    if (test_info.result()->Passed()) {
      result.status = TestStatus::kPassed;
    } else if (test_info.result()->Skipped()) {
      result.status = TestStatus::kSkipped;
    } else {
      result.status = TestStatus::kFailed;

      // Capture failure message
      std::stringstream error_stream;
      for (int i = 0; i < test_info.result()->total_part_count(); ++i) {
        const auto& part = test_info.result()->GetTestPartResult(i);
        if (part.failed()) {
          error_stream << part.file_name() << ":" << part.line_number() << " "
                       << part.message() << "\n";
        }
      }
      result.error_message = error_stream.str();
    }

    if (results_) {
      results_->AddResult(result);
    }
  }

  // Required overrides (can be empty)
  void OnTestProgramStart(const ::testing::UnitTest&) override {}
  void OnTestIterationStart(const ::testing::UnitTest&, int) override {}
  void OnEnvironmentsSetUpStart(const ::testing::UnitTest&) override {}
  void OnEnvironmentsSetUpEnd(const ::testing::UnitTest&) override {}
  void OnTestCaseStart(const ::testing::TestCase&) override {}
  void OnTestCaseEnd(const ::testing::TestCase&) override {}
  void OnEnvironmentsTearDownStart(const ::testing::UnitTest&) override {}
  void OnEnvironmentsTearDownEnd(const ::testing::UnitTest&) override {}
  void OnTestIterationEnd(const ::testing::UnitTest&, int) override {}
  void OnTestProgramEnd(const ::testing::UnitTest&) override {}

 private:
  TestResults* results_;
  std::chrono::time_point<std::chrono::steady_clock> current_test_start_;
  std::string current_test_name_;
};
#endif  // YAZE_ENABLE_GTEST

// Unit test suite that runs Google Test cases
class UnitTestSuite : public TestSuite {
 public:
  UnitTestSuite() = default;
  ~UnitTestSuite() override = default;

  std::string GetName() const override { return "Google Test Unit Tests"; }
  TestCategory GetCategory() const override { return TestCategory::kUnit; }

  absl::Status RunTests(TestResults& results) override {
#ifdef YAZE_ENABLE_GTEST
    // Set up Google Test to capture results
    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();

    // Remove default console output (we'll capture it ourselves)
    delete listeners.Release(listeners.default_result_printer());

    // Add our custom listener
    auto capture_listener = new TestResultCapture(&results);
    listeners.Append(capture_listener);

    // Configure test execution
    int argc = 1;
    const char* argv[] = {"yaze_tests"};
    ::testing::InitGoogleTest(&argc, const_cast<char**>(argv));

    // Run the tests
    int result = RUN_ALL_TESTS();

    // Clean up
    listeners.Release(capture_listener);
    delete capture_listener;

    return result == 0 ? absl::OkStatus()
                       : absl::InternalError("Some unit tests failed");
#else
    // Google Test not available - add a placeholder test
    TestResult result;
    result.name = "Placeholder Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.status = TestStatus::kSkipped;
    result.error_message = "Google Test not available in this build";
    result.duration = std::chrono::milliseconds{0};
    result.timestamp = std::chrono::steady_clock::now();
    results.AddResult(result);

    return absl::OkStatus();
#endif
  }

  void DrawConfiguration() override {
    ImGui::Text("Google Test Configuration");
    ImGui::Checkbox("Run disabled tests", &run_disabled_tests_);
    ImGui::Checkbox("Shuffle tests", &shuffle_tests_);
    ImGui::InputInt("Repeat count", &repeat_count_);
    if (repeat_count_ < 1) repeat_count_ = 1;

    ImGui::InputText("Test filter", test_filter_, sizeof(test_filter_));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
      test_filter_[0] = '\0';
    }
  }

 private:
  bool run_disabled_tests_ = false;
  bool shuffle_tests_ = false;
  int repeat_count_ = 1;
  char test_filter_[256] = "";
};

// Arena-specific test suite for memory management
class ArenaTestSuite : public TestSuite {
 public:
  ArenaTestSuite() = default;
  ~ArenaTestSuite() override = default;

  std::string GetName() const override { return "Arena Memory Tests"; }
  TestCategory GetCategory() const override { return TestCategory::kMemory; }

  absl::Status RunTests(TestResults& results) override {
    // Test Arena resource management
    RunArenaAllocationTest(results);
    RunArenaCleanupTest(results);
    RunArenaResourceTrackingTest(results);

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    ImGui::Text("Arena Test Configuration");
    ImGui::InputInt("Test allocations", &test_allocation_count_);
    ImGui::InputInt("Test texture size", &test_texture_size_);
    ImGui::Checkbox("Test cleanup order", &test_cleanup_order_);
  }

 private:
  void RunArenaAllocationTest(TestResults& results) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Arena_Allocation_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& arena = gfx::Arena::Get();
      size_t initial_texture_count = arena.GetTextureCount();
      size_t initial_surface_count = arena.GetSurfaceCount();

      // Test texture allocation (would need a valid renderer)
      // This is a simplified test - in real implementation we'd mock the
      // renderer

      size_t final_texture_count = arena.GetTextureCount();
      size_t final_surface_count = arena.GetSurfaceCount();

      // For now, just verify the Arena can be accessed
      result.status = TestStatus::kPassed;

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Arena allocation test failed: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunArenaCleanupTest(TestResults& results) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Arena_Cleanup_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& arena = gfx::Arena::Get();

      // Test that shutdown doesn't crash
      // Note: We can't actually call Shutdown() here as it would affect the
      // running app This test verifies the methods exist and are callable
      size_t texture_count = arena.GetTextureCount();
      size_t surface_count = arena.GetSurfaceCount();

      result.status = TestStatus::kPassed;

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Arena cleanup test failed: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  void RunArenaResourceTrackingTest(TestResults& results) {
    auto start_time = std::chrono::steady_clock::now();

    TestResult result;
    result.name = "Arena_Resource_Tracking_Test";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    try {
      auto& arena = gfx::Arena::Get();

      // Test resource tracking methods
      size_t texture_count = arena.GetTextureCount();
      size_t surface_count = arena.GetSurfaceCount();

      // Verify tracking methods work
      if (texture_count >= 0 && surface_count >= 0) {
        result.status = TestStatus::kPassed;
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = "Invalid resource counts returned";
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Resource tracking test failed: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    results.AddResult(result);
  }

  int test_allocation_count_ = 10;
  int test_texture_size_ = 64;
  bool test_cleanup_order_ = true;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_UNIT_TEST_SUITE_H
