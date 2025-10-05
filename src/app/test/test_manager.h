#ifndef YAZE_APP_TEST_TEST_MANAGER_H
#define YAZE_APP_TEST_TEST_MANAGER_H

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "app/rom.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

#include "util/log.h"

// Forward declarations
namespace yaze {
namespace editor {
class EditorManager;
}
}  // namespace yaze

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_engine.h"
#else
// Forward declaration when ImGui Test Engine is not available
struct ImGuiTestEngine;
#endif

namespace yaze {
namespace test {

// Test execution status
enum class TestStatus { kNotRun, kRunning, kPassed, kFailed, kSkipped };

// Test categories for organization
enum class TestCategory { kUnit, kIntegration, kUI, kPerformance, kMemory };

// Individual test result
struct TestResult {
  std::string name;
  std::string suite_name;
  TestCategory category;
  TestStatus status;
  std::string error_message;
  std::chrono::milliseconds duration;
  std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

// Overall test results summary
struct TestResults {
  std::vector<TestResult> individual_results;
  size_t total_tests = 0;
  size_t passed_tests = 0;
  size_t failed_tests = 0;
  size_t skipped_tests = 0;
  std::chrono::milliseconds total_duration{0};

  void AddResult(const TestResult& result) {
    individual_results.push_back(result);
    total_tests++;
    switch (result.status) {
      case TestStatus::kPassed:
        passed_tests++;
        break;
      case TestStatus::kFailed:
        failed_tests++;
        break;
      case TestStatus::kSkipped:
        skipped_tests++;
        break;
      default:
        break;
    }
    total_duration += result.duration;
  }

  void Clear() {
    individual_results.clear();
    total_tests = passed_tests = failed_tests = skipped_tests = 0;
    total_duration = std::chrono::milliseconds{0};
  }

  float GetPassRate() const {
    return total_tests > 0 ? static_cast<float>(passed_tests) / total_tests
                           : 0.0f;
  }
};

// Base class for test suites
class TestSuite {
 public:
  virtual ~TestSuite() = default;
  virtual std::string GetName() const = 0;
  virtual TestCategory GetCategory() const = 0;
  virtual absl::Status RunTests(TestResults& results) = 0;
  virtual void DrawConfiguration() {}
  virtual bool IsEnabled() const { return enabled_; }
  virtual void SetEnabled(bool enabled) { enabled_ = enabled; }

 protected:
  bool enabled_ = true;
};

// Resource monitoring for performance and memory tests
struct ResourceStats {
  size_t texture_count = 0;
  size_t surface_count = 0;
  size_t memory_usage_mb = 0;
  float frame_rate = 0.0f;
  std::chrono::time_point<std::chrono::steady_clock> timestamp;
};

// Test harness execution tracking for gRPC automation (IT-05)
#if defined(YAZE_WITH_GRPC)
enum class HarnessTestStatus {
  kUnspecified,
  kQueued,
  kRunning,
  kPassed,
  kFailed,
  kTimeout,
};

const char* HarnessStatusToString(HarnessTestStatus status);
HarnessTestStatus HarnessStatusFromString(absl::string_view status);

struct HarnessTestExecution {
  std::string test_id;
  std::string name;
  std::string category;
  HarnessTestStatus status = HarnessTestStatus::kUnspecified;
  absl::Time queued_at;
  absl::Time started_at;
  absl::Time completed_at;
  absl::Duration duration = absl::ZeroDuration();
  std::string error_message;
  std::vector<std::string> assertion_failures;
  std::vector<std::string> logs;
  std::map<std::string, int32_t> metrics;
  
  // IT-08b: Failure diagnostics
  std::string screenshot_path;
  int64_t screenshot_size_bytes = 0;
  std::string failure_context;
  std::string widget_state;  // IT-08c (future)
};

struct HarnessTestSummary {
  HarnessTestExecution latest_execution;
  int total_runs = 0;
  int pass_count = 0;
  int fail_count = 0;
  absl::Duration total_duration = absl::ZeroDuration();
};

class HarnessListener {
 public:
  virtual ~HarnessListener() = default;
  virtual void OnHarnessTestUpdated(const HarnessTestExecution& execution) = 0;
  virtual void OnHarnessPlanSummary(const std::string& summary) = 0;
};
#endif  // defined(YAZE_WITH_GRPC)

// Main test manager - singleton
class TestManager {
 public:
  static TestManager& Get();

  // Core test execution
  absl::Status RunAllTests();
  absl::Status RunTestsByCategory(TestCategory category);
  absl::Status RunTestSuite(const std::string& suite_name);

  // Test suite management
  void RegisterTestSuite(std::unique_ptr<TestSuite> suite);
  std::vector<std::string> GetTestSuiteNames() const;
  TestSuite* GetTestSuite(const std::string& name);

  // Results access
  const TestResults& GetLastResults() const { return last_results_; }
  void ClearResults() { last_results_.Clear(); }

  // Configuration
  void SetMaxConcurrentTests(size_t max_concurrent) {
    max_concurrent_tests_ = max_concurrent;
  }
  void SetTestTimeout(std::chrono::seconds timeout) { test_timeout_ = timeout; }

  // Resource monitoring
  void UpdateResourceStats();
  const std::vector<ResourceStats>& GetResourceHistory() const {
    return resource_history_;
  }

  // UI Testing (ImGui Test Engine integration)
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* GetUITestEngine() { return ui_test_engine_; }
  void InitializeUITesting();
  void StopUITesting();  // Stop test engine while ImGui context is valid
  void DestroyUITestingContext();  // Destroy test engine after ImGui context is
                                   // destroyed
  void ShutdownUITesting();  // Complete shutdown (calls both Stop and Destroy)
#else
  void* GetUITestEngine() { return nullptr; }
  void InitializeUITesting() {}
  void StopUITesting() {}
  void DestroyUITestingContext() {}
  void ShutdownUITesting() {}
#endif

  // Status queries
  bool IsTestRunning() const { return is_running_; }
  const std::string& GetCurrentTestName() const { return current_test_name_; }
  float GetProgress() const { return progress_; }

  // UI Interface
  void DrawTestDashboard(bool* show_dashboard = nullptr);

  // ROM-dependent testing
  void SetCurrentRom(Rom* rom) {
    LOG_INFO("TestManager", "SetCurrentRom called with ROM: %p", (void*)rom);
    if (rom) {
      LOG_INFO("TestManager", "ROM title: '%s', loaded: %s",
               rom->title().c_str(), rom->is_loaded() ? "true" : "false");
    }
    current_rom_ = rom;
  }
  Rom* GetCurrentRom() const { return current_rom_; }
  void RefreshCurrentRom();  // Refresh ROM pointer from editor manager
  // Remove EditorManager dependency to avoid circular includes

  // Enhanced ROM testing
  absl::Status LoadRomForTesting(const std::string& filename);
  void ShowRomComparisonResults(const Rom& before, const Rom& after);

  // Test ROM management
  absl::Status CreateTestRomCopy(Rom* source_rom,
                                 std::unique_ptr<Rom>& test_rom);
  std::string GenerateTestRomFilename(const std::string& base_name);
  void OfferTestSessionCreation(const std::string& test_rom_path);

 public:
  // ROM testing methods (work on copies, not originals)
  absl::Status TestRomSaveLoad(Rom* rom);
  absl::Status TestRomDataIntegrity(Rom* rom);
  absl::Status TestRomWithCopy(Rom* source_rom,
                               std::function<absl::Status(Rom*)> test_function);

  // Test configuration management
  void DisableTest(const std::string& test_name) {
    disabled_tests_[test_name] = true;
  }
  void EnableTest(const std::string& test_name) {
    disabled_tests_[test_name] = false;
  }
  bool IsTestEnabled(const std::string& test_name) const {
    auto it = disabled_tests_.find(test_name);
    return it == disabled_tests_.end() || !it->second;
  }
  // File dialog mode now uses global feature flags

  // Harness test introspection (IT-05)
#if defined(YAZE_WITH_GRPC)
  std::string RegisterHarnessTest(const std::string& name,
                                  const std::string& category)
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  void MarkHarnessTestRunning(const std::string& test_id)
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  void MarkHarnessTestCompleted(
      const std::string& test_id, HarnessTestStatus status,
      const std::string& error_message = "",
      const std::vector<std::string>& assertion_failures = {},
      const std::vector<std::string>& logs = {},
      const std::map<std::string, int32_t>& metrics = {})
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  void AppendHarnessTestLog(const std::string& test_id,
                            const std::string& log_entry)
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  absl::StatusOr<HarnessTestExecution> GetHarnessTestExecution(
      const std::string& test_id) const
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  std::vector<HarnessTestSummary> ListHarnessTestSummaries(
      const std::string& category_filter = "") const
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);
  
  // IT-08b: Capture failure diagnostics
  void CaptureFailureContext(const std::string& test_id)
      ABSL_LOCKS_EXCLUDED(harness_history_mutex_);

  void SetHarnessListener(HarnessListener* listener);

  absl::Status ReplayLastPlan();
#endif
  absl::Status ShowHarnessDashboard();
  absl::Status ShowHarnessActiveTests();
  void RecordPlanSummary(const std::string& summary);

 private:
  TestManager();
  ~TestManager();

  // Test execution helpers
  absl::Status ExecuteTestSuite(TestSuite* suite);
  void UpdateProgress();

  // Resource monitoring helpers
  void CollectResourceStats();
  void TrimResourceHistory();

  // Member variables
  std::vector<std::unique_ptr<TestSuite>> test_suites_;
  std::unordered_map<std::string, TestSuite*> suite_lookup_;

  TestResults last_results_;
  bool is_running_ = false;
  std::string current_test_name_;
  float progress_ = 0.0f;

  // Configuration
  size_t max_concurrent_tests_ = 1;
  std::chrono::seconds test_timeout_{30};

  // Resource monitoring
  std::vector<ResourceStats> resource_history_;
  static constexpr size_t kMaxResourceHistorySize = 1000;

  // UI Testing
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  ImGuiTestEngine* ui_test_engine_ = nullptr;
#endif

  // UI State
  bool show_dashboard_ = false;
  bool show_resource_monitor_ = false;
  std::string test_filter_;
  TestCategory category_filter_ = TestCategory::kUnit;

  // ROM-dependent testing
  Rom* current_rom_ = nullptr;
  // Removed editor_manager_ to avoid circular dependency

  // UI state
  bool show_google_tests_ = false;
  bool show_rom_test_results_ = false;
  bool show_rom_file_dialog_ = false;
  bool show_test_session_dialog_ = false;
  bool show_test_configuration_ = false;
  std::string test_rom_path_for_session_;

  // Test selection and configuration
  std::unordered_map<std::string, bool> disabled_tests_;

  // Harness test tracking
#if defined(YAZE_WITH_GRPC)
  struct HarnessAggregate {
  int total_runs = 0;
  int pass_count = 0;
  int fail_count = 0;
  absl::Duration total_duration = absl::ZeroDuration();
    std::string category;
    absl::Time last_run;
    HarnessTestExecution latest_execution;
  };

  std::unordered_map<std::string, HarnessTestExecution> harness_history_
    ABSL_GUARDED_BY(harness_history_mutex_);
  std::unordered_map<std::string, HarnessAggregate> harness_aggregates_
    ABSL_GUARDED_BY(harness_history_mutex_);
  std::deque<std::string> harness_history_order_;
  size_t harness_history_limit_ = 200;
  mutable absl::Mutex harness_history_mutex_;
#if defined(YAZE_WITH_GRPC)
  HarnessListener* harness_listener_ ABSL_GUARDED_BY(mutex_) = nullptr;
#endif
#endif  // defined(YAZE_WITH_GRPC)

  std::string GenerateHarnessTestIdLocked(absl::string_view prefix)
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(harness_history_mutex_);
  void TrimHarnessHistoryLocked()
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(harness_history_mutex_);

    absl::Mutex mutex_;
};

// Utility functions for test result formatting
const char* TestStatusToString(TestStatus status);
const char* TestCategoryToString(TestCategory category);
ImVec4 GetTestStatusColor(TestStatus status);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_TEST_MANAGER_H
