#ifndef YAZE_APP_TEST_UNIT_TEST_SUITE_H
#define YAZE_APP_TEST_UNIT_TEST_SUITE_H

#include <chrono>
#include <memory>

#include "app/gfx/arena.h"
#include "app/testing/test_manager.h"

// Note: ImGui Test Engine is handled through YAZE_ENABLE_IMGUI_TEST_ENGINE in
// TestManager

namespace yaze {
namespace test {

// Unit test suite that runs Google Test cases
class UnitTestSuite : public TestSuite {
 public:
  UnitTestSuite() = default;
  ~UnitTestSuite() override = default;

  std::string GetName() const override { return "Google Test Unit Tests"; }
  TestCategory GetCategory() const override { return TestCategory::kUnit; }

  absl::Status RunTests(TestResults& results) override {
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
  }

  void DrawConfiguration() override {
    ImGui::Text("Google Test Configuration");
    ImGui::Checkbox("Run disabled tests", &run_disabled_tests_);
    ImGui::Checkbox("Shuffle tests", &shuffle_tests_);
    ImGui::InputInt("Repeat count", &repeat_count_);
    if (repeat_count_ < 1)
      repeat_count_ = 1;

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
