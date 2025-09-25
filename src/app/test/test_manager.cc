#include "app/test/test_manager.h"

#include "app/gfx/arena.h"
#include "imgui/imgui.h"

#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_engine.h"
#endif

namespace yaze {
namespace test {

// Utility function implementations
const char* TestStatusToString(TestStatus status) {
  switch (status) {
    case TestStatus::kNotRun: return "Not Run";
    case TestStatus::kRunning: return "Running";
    case TestStatus::kPassed: return "Passed";
    case TestStatus::kFailed: return "Failed";
    case TestStatus::kSkipped: return "Skipped";
  }
  return "Unknown";
}

const char* TestCategoryToString(TestCategory category) {
  switch (category) {
    case TestCategory::kUnit: return "Unit";
    case TestCategory::kIntegration: return "Integration";
    case TestCategory::kUI: return "UI";
    case TestCategory::kPerformance: return "Performance";
    case TestCategory::kMemory: return "Memory";
  }
  return "Unknown";
}

ImVec4 GetTestStatusColor(TestStatus status) {
  switch (status) {
    case TestStatus::kNotRun: return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);     // Gray
    case TestStatus::kRunning: return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);    // Yellow
    case TestStatus::kPassed: return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);     // Green
    case TestStatus::kFailed: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);     // Red
    case TestStatus::kSkipped: return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);    // Orange
  }
  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

// TestManager implementation
TestManager& TestManager::Get() {
  static TestManager instance;
  return instance;
}

TestManager::TestManager() {
  // Initialize UI test engine
  InitializeUITesting();
}

TestManager::~TestManager() {
  ShutdownUITesting();
}

#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
void TestManager::InitializeUITesting() {
  if (!ui_test_engine_) {
    ui_test_engine_ = ImGuiTestEngine_CreateContext();
    if (ui_test_engine_) {
      ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(ui_test_engine_);
      test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
      test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
      test_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
      
      // Start the test engine
      ImGuiTestEngine_Start(ui_test_engine_, ImGui::GetCurrentContext());
    }
  }
}

void TestManager::StopUITesting() {
  if (ui_test_engine_ && ImGui::GetCurrentContext() != nullptr) {
    ImGuiTestEngine_Stop(ui_test_engine_);
  }
}

void TestManager::DestroyUITestingContext() {
  if (ui_test_engine_) {
    ImGuiTestEngine_DestroyContext(ui_test_engine_);
    ui_test_engine_ = nullptr;
  }
}

void TestManager::ShutdownUITesting() {
  // Complete shutdown - calls both phases
  StopUITesting();
  DestroyUITestingContext();
}
#endif

absl::Status TestManager::RunAllTests() {
  if (is_running_) {
    return absl::FailedPreconditionError("Tests are already running");
  }
  
  is_running_ = true;
  progress_ = 0.0f;
  last_results_.Clear();
  
  // Execute all test suites
  for (auto& suite : test_suites_) {
    if (suite->IsEnabled()) {
      current_test_name_ = suite->GetName();
      auto status = ExecuteTestSuite(suite.get());
      if (!status.ok()) {
        is_running_ = false;
        return status;
      }
      UpdateProgress();
    }
  }
  
  is_running_ = false;
  current_test_name_.clear();
  progress_ = 1.0f;
  
  return absl::OkStatus();
}

absl::Status TestManager::RunTestsByCategory(TestCategory category) {
  if (is_running_) {
    return absl::FailedPreconditionError("Tests are already running");
  }
  
  is_running_ = true;
  progress_ = 0.0f;
  last_results_.Clear();
  
  // Filter and execute test suites by category
  std::vector<TestSuite*> filtered_suites;
  for (auto& suite : test_suites_) {
    if (suite->IsEnabled() && suite->GetCategory() == category) {
      filtered_suites.push_back(suite.get());
    }
  }
  
  for (auto* suite : filtered_suites) {
    current_test_name_ = suite->GetName();
    auto status = ExecuteTestSuite(suite);
    if (!status.ok()) {
      is_running_ = false;
      return status;
    }
    UpdateProgress();
  }
  
  is_running_ = false;
  current_test_name_.clear();
  progress_ = 1.0f;
  
  return absl::OkStatus();
}

absl::Status TestManager::RunTestSuite(const std::string& suite_name) {
  if (is_running_) {
    return absl::FailedPreconditionError("Tests are already running");
  }
  
  auto it = suite_lookup_.find(suite_name);
  if (it == suite_lookup_.end()) {
    return absl::NotFoundError("Test suite not found: " + suite_name);
  }
  
  is_running_ = true;
  progress_ = 0.0f;
  last_results_.Clear();
  current_test_name_ = suite_name;
  
  auto status = ExecuteTestSuite(it->second);
  
  is_running_ = false;
  current_test_name_.clear();
  progress_ = 1.0f;
  
  return status;
}

void TestManager::RegisterTestSuite(std::unique_ptr<TestSuite> suite) {
  if (suite) {
    std::string name = suite->GetName();
    suite_lookup_[name] = suite.get();
    test_suites_.push_back(std::move(suite));
  }
}

std::vector<std::string> TestManager::GetTestSuiteNames() const {
  std::vector<std::string> names;
  names.reserve(test_suites_.size());
  for (const auto& suite : test_suites_) {
    names.push_back(suite->GetName());
  }
  return names;
}

TestSuite* TestManager::GetTestSuite(const std::string& name) {
  auto it = suite_lookup_.find(name);
  return it != suite_lookup_.end() ? it->second : nullptr;
}

void TestManager::UpdateResourceStats() {
  CollectResourceStats();
  TrimResourceHistory();
}

absl::Status TestManager::ExecuteTestSuite(TestSuite* suite) {
  if (!suite) {
    return absl::InvalidArgumentError("Test suite is null");
  }
  
  // Collect resource stats before test
  CollectResourceStats();
  
  // Execute the test suite
  auto status = suite->RunTests(last_results_);
  
  // Collect resource stats after test
  CollectResourceStats();
  
  return status;
}

void TestManager::UpdateProgress() {
  if (test_suites_.empty()) {
    progress_ = 1.0f;
    return;
  }
  
  size_t completed = 0;
  for (const auto& suite : test_suites_) {
    if (suite->IsEnabled()) {
      completed++;
    }
  }
  
  progress_ = static_cast<float>(completed) / test_suites_.size();
}

void TestManager::CollectResourceStats() {
  ResourceStats stats;
  stats.timestamp = std::chrono::steady_clock::now();
  
  // Get Arena statistics
  auto& arena = gfx::Arena::Get();
  stats.texture_count = arena.GetTextureCount();
  stats.surface_count = arena.GetSurfaceCount();
  
  // Get frame rate from ImGui
  stats.frame_rate = ImGui::GetIO().Framerate;
  
  // Estimate memory usage (simplified)
  stats.memory_usage_mb = (stats.texture_count + stats.surface_count) / 1024; // Rough estimate
  
  resource_history_.push_back(stats);
}

void TestManager::TrimResourceHistory() {
  if (resource_history_.size() > kMaxResourceHistorySize) {
    resource_history_.erase(
        resource_history_.begin(),
        resource_history_.begin() + (resource_history_.size() - kMaxResourceHistorySize));
  }
}

void TestManager::DrawTestDashboard() {
  show_dashboard_ = true; // Enable dashboard visibility
  
  ImGui::Begin("Test Dashboard", &show_dashboard_, ImGuiWindowFlags_MenuBar);
  
  // Menu bar
    if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Run")) {
      if (ImGui::MenuItem("All Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunAllTests();
      }
      if (ImGui::MenuItem("Unit Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kUnit);
      }
      if (ImGui::MenuItem("Integration Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kIntegration);
      }
      if (ImGui::MenuItem("UI Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kUI);
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Resource Monitor", nullptr, &show_resource_monitor_);
      ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
  }
  
  // Test execution status
  if (is_running_) {
    ImGui::Text("Running: %s", current_test_name_.c_str());
    ImGui::ProgressBar(progress_, ImVec2(-1, 0), "");
  } else {
    if (ImGui::Button("Run All Tests", ImVec2(120, 0))) {
      [[maybe_unused]] auto status = RunAllTests();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Results", ImVec2(120, 0))) {
      ClearResults();
    }
  }
  
  ImGui::Separator();
  
  // Test results summary
  if (last_results_.total_tests > 0) {
    ImGui::Text("Total Tests: %zu", last_results_.total_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kPassed), 
                      "Passed: %zu", last_results_.passed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kFailed), 
                      "Failed: %zu", last_results_.failed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kSkipped), 
                      "Skipped: %zu", last_results_.skipped_tests);
    
    ImGui::Text("Pass Rate: %.1f%%", last_results_.GetPassRate() * 100.0f);
    ImGui::Text("Total Duration: %lld ms", last_results_.total_duration.count());
  }
  
  ImGui::Separator();
  
  // Test filter
  static char filter_buffer[256] = "";
  if (ImGui::InputText("Filter", filter_buffer, sizeof(filter_buffer))) {
    test_filter_ = std::string(filter_buffer);
  }
  
  // Test results list
  if (ImGui::BeginChild("TestResults", ImVec2(0, 0), true)) {
    for (const auto& result : last_results_.individual_results) {
      if (!test_filter_.empty() && 
          result.name.find(test_filter_) == std::string::npos) {
        continue;
      }
      
      ImGui::PushID(&result);
      ImGui::TextColored(GetTestStatusColor(result.status), 
                        "[%s] %s::%s", 
                        TestStatusToString(result.status),
                        result.suite_name.c_str(),
                        result.name.c_str());
      
      if (result.status == TestStatus::kFailed && !result.error_message.empty()) {
        ImGui::Indent();
        ImGui::TextWrapped("Error: %s", result.error_message.c_str());
        ImGui::Unindent();
      }
      
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  
  ImGui::End();
  
  // Resource monitor window
  if (show_resource_monitor_) {
    ImGui::Begin("Resource Monitor", &show_resource_monitor_);
    
    if (!resource_history_.empty()) {
      const auto& latest = resource_history_.back();
      ImGui::Text("Textures: %zu", latest.texture_count);
      ImGui::Text("Surfaces: %zu", latest.surface_count);
      ImGui::Text("Memory: %zu MB", latest.memory_usage_mb);
      ImGui::Text("FPS: %.1f", latest.frame_rate);
      
      // Simple plot of resource usage over time
      if (resource_history_.size() > 1) {
        std::vector<float> texture_counts;
        std::vector<float> surface_counts;
        texture_counts.reserve(resource_history_.size());
        surface_counts.reserve(resource_history_.size());
        
        for (const auto& stats : resource_history_) {
          texture_counts.push_back(static_cast<float>(stats.texture_count));
          surface_counts.push_back(static_cast<float>(stats.surface_count));
        }
        
        ImGui::PlotLines("Textures", texture_counts.data(), 
                        static_cast<int>(texture_counts.size()), 0, nullptr, 
                        0.0f, FLT_MAX, ImVec2(0, 80));
        ImGui::PlotLines("Surfaces", surface_counts.data(), 
                        static_cast<int>(surface_counts.size()), 0, nullptr, 
                        0.0f, FLT_MAX, ImVec2(0, 80));
      }
    }
    
    ImGui::End();
  }
}

}  // namespace test
}  // namespace yaze
