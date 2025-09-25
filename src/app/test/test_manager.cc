#include "app/test/test_manager.h"

#include "absl/strings/str_format.h"
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
      if (ImGui::MenuItem("All Tests", "Ctrl+T", false, !is_running_)) {
        [[maybe_unused]] auto status = RunAllTests();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Unit Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kUnit);
      }
      if (ImGui::MenuItem("Integration Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kIntegration);
      }
      if (ImGui::MenuItem("UI Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kUI);
      }
      if (ImGui::MenuItem("Performance Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kPerformance);
      }
      if (ImGui::MenuItem("Memory Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kMemory);
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Resource Monitor", nullptr, &show_resource_monitor_);
      ImGui::Separator();
      if (ImGui::MenuItem("Export Results", nullptr, false, last_results_.total_tests > 0)) {
        // TODO: Implement result export
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Configure")) {
      if (ImGui::MenuItem("Test Settings")) {
        // Show configuration for all test suites
      }
      ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
  }
  
  // Enhanced test execution status
  if (is_running_) {
    ImGui::PushStyleColor(ImGuiCol_Text, GetTestStatusColor(TestStatus::kRunning));
    ImGui::Text("⚡ Running: %s", current_test_name_.c_str());
    ImGui::PopStyleColor();
    ImGui::ProgressBar(progress_, ImVec2(-1, 0), 
                      absl::StrFormat("%.0f%%", progress_ * 100.0f).c_str());
  } else {
    // Enhanced control buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("🚀 Run All Tests", ImVec2(140, 0))) {
      [[maybe_unused]] auto status = RunAllTests();
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (ImGui::Button("🧪 Quick Test", ImVec2(100, 0))) {
      [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kMemory);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("🗑️ Clear", ImVec2(80, 0))) {
      ClearResults();
    }
  }
  
  ImGui::Separator();
  
  // Enhanced test results summary with better visuals
  if (last_results_.total_tests > 0) {
    // Test summary header
    ImGui::Text("📊 Test Results Summary");
    
    // Progress bar showing pass rate
    float pass_rate = last_results_.GetPassRate();
    ImVec4 progress_color = pass_rate >= 0.9f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                           pass_rate >= 0.7f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :
                                              ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progress_color);
    ImGui::ProgressBar(pass_rate, ImVec2(-1, 0), 
                      absl::StrFormat("Pass Rate: %.1f%%", pass_rate * 100.0f).c_str());
    ImGui::PopStyleColor();
    
    // Test counts with icons
    ImGui::Text("📈 Total: %zu", last_results_.total_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kPassed), 
                      "✅ %zu", last_results_.passed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kFailed), 
                      "❌ %zu", last_results_.failed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kSkipped), 
                      "⏭️ %zu", last_results_.skipped_tests);
    
    ImGui::Text("⏱️ Duration: %lld ms", last_results_.total_duration.count());
    
    // Test suite breakdown
    if (ImGui::CollapsingHeader("Test Suite Breakdown")) {
      std::unordered_map<std::string, std::pair<size_t, size_t>> suite_stats; // passed, total
      for (const auto& result : last_results_.individual_results) {
        suite_stats[result.suite_name].second++; // total
        if (result.status == TestStatus::kPassed) {
          suite_stats[result.suite_name].first++; // passed
        }
      }
      
      for (const auto& [suite_name, stats] : suite_stats) {
        float suite_pass_rate = stats.second > 0 ? 
                                static_cast<float>(stats.first) / stats.second : 0.0f;
        ImGui::Text("%s: %zu/%zu (%.0f%%)", 
                   suite_name.c_str(), stats.first, stats.second, 
                   suite_pass_rate * 100.0f);
      }
    }
  }
  
  ImGui::Separator();
  
  // Enhanced test filter with category selection
  ImGui::Text("🔍 Filter & View Options");
  
  // Category filter
  const char* categories[] = {"All", "Unit", "Integration", "UI", "Performance", "Memory"};
  static int selected_category = 0;
  if (ImGui::Combo("Category", &selected_category, categories, IM_ARRAYSIZE(categories))) {
    switch (selected_category) {
      case 0: category_filter_ = TestCategory::kUnit; break; // All - use Unit as default
      case 1: category_filter_ = TestCategory::kUnit; break;
      case 2: category_filter_ = TestCategory::kIntegration; break;
      case 3: category_filter_ = TestCategory::kUI; break;
      case 4: category_filter_ = TestCategory::kPerformance; break;
      case 5: category_filter_ = TestCategory::kMemory; break;
    }
  }
  
  // Text filter
  static char filter_buffer[256] = "";
  ImGui::SetNextItemWidth(-80);
  if (ImGui::InputTextWithHint("##filter", "Search tests...", filter_buffer, sizeof(filter_buffer))) {
    test_filter_ = std::string(filter_buffer);
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    filter_buffer[0] = '\0';
    test_filter_.clear();
  }
  
  ImGui::Separator();
  
  // Enhanced test results list with better formatting
  if (ImGui::BeginChild("TestResults", ImVec2(0, 0), true)) {
    if (last_results_.individual_results.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                        "No test results to display. Run some tests to see results here.");
    } else {
      for (const auto& result : last_results_.individual_results) {
        // Apply filters
        bool category_match = (selected_category == 0) || (result.category == category_filter_);
        bool text_match = test_filter_.empty() || 
                         result.name.find(test_filter_) != std::string::npos ||
                         result.suite_name.find(test_filter_) != std::string::npos;
        
        if (!category_match || !text_match) {
          continue;
        }
        
        ImGui::PushID(&result);
        
        // Status icon and test name
        const char* status_icon = "❓";
        switch (result.status) {
          case TestStatus::kPassed: status_icon = "✅"; break;
          case TestStatus::kFailed: status_icon = "❌"; break;
          case TestStatus::kSkipped: status_icon = "⏭️"; break;
          case TestStatus::kRunning: status_icon = "⚡"; break;
          default: break;
        }
        
        ImGui::TextColored(GetTestStatusColor(result.status), 
                          "%s %s::%s", 
                          status_icon,
                          result.suite_name.c_str(),
                          result.name.c_str());
        
        // Show duration and timestamp on same line if space allows
        if (ImGui::GetContentRegionAvail().x > 200) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                            "(%lld ms)", result.duration.count());
        }
        
        // Show detailed information for failed tests
        if (result.status == TestStatus::kFailed && !result.error_message.empty()) {
          ImGui::Indent();
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
          ImGui::TextWrapped("💥 %s", result.error_message.c_str());
          ImGui::PopStyleColor();
          ImGui::Unindent();
        }
        
        // Show additional info for passed tests if they have messages
        if (result.status == TestStatus::kPassed && !result.error_message.empty()) {
          ImGui::Indent();
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.8f, 1.0f));
          ImGui::TextWrapped("ℹ️ %s", result.error_message.c_str());
          ImGui::PopStyleColor();
          ImGui::Unindent();
        }
        
        ImGui::PopID();
      }
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
