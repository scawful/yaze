#ifndef YAZE_APP_TEST_TEST_MANAGER_H
#define YAZE_APP_TEST_TEST_MANAGER_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "util/log.h"

// Forward declarations
namespace yaze {
namespace editor {
class EditorManager;
}
}

#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
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
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
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
    util::logf("TestManager::SetCurrentRom called with ROM: %p", (void*)rom);
    if (rom) {
      util::logf("ROM title: '%s', loaded: %s", rom->title().c_str(), rom->is_loaded() ? "true" : "false");
    }
    current_rom_ = rom; 
  }
  Rom* GetCurrentRom() const { return current_rom_; }
  void RefreshCurrentRom(); // Refresh ROM pointer from editor manager
  // Remove EditorManager dependency to avoid circular includes
  
  // Enhanced ROM testing
  absl::Status LoadRomForTesting(const std::string& filename);
  void ShowRomComparisonResults(const Rom& before, const Rom& after);
  
  // Test ROM management
  absl::Status CreateTestRomCopy(Rom* source_rom, std::unique_ptr<Rom>& test_rom);
  std::string GenerateTestRomFilename(const std::string& base_name);
  void OfferTestSessionCreation(const std::string& test_rom_path);
  
 public:
  // ROM testing methods (work on copies, not originals)
  absl::Status TestRomSaveLoad(Rom* rom);
  absl::Status TestRomDataIntegrity(Rom* rom);
  absl::Status TestRomWithCopy(Rom* source_rom, std::function<absl::Status(Rom*)> test_function);
  
  // Test configuration management
  void DisableTest(const std::string& test_name) { disabled_tests_[test_name] = true; }
  void EnableTest(const std::string& test_name) { disabled_tests_[test_name] = false; }
  bool IsTestEnabled(const std::string& test_name) const { 
    auto it = disabled_tests_.find(test_name);
    return it == disabled_tests_.end() || !it->second;
  }
  // File dialog mode now uses global feature flags

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
};

// Utility functions for test result formatting
const char* TestStatusToString(TestStatus status);
const char* TestCategoryToString(TestCategory category);
ImVec4 GetTestStatusColor(TestStatus status);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_TEST_MANAGER_H
