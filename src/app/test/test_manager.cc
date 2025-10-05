#include "app/test/test_manager.h"

#include <algorithm>
#include <filesystem>
#include <random>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/core/service/screenshot_utils.h"
#include "app/gui/widgets/widget_state_capture.h"
#include "app/core/features.h"
#include "util/file_util.h"
#include "app/gfx/arena.h"
#include "app/gui/icons.h"
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui.h"
#include "imgui_internal.h"
#else
#include "imgui.h"
#endif
#include "util/log.h"

// Forward declaration to avoid circular dependency
namespace yaze {
namespace editor {
class EditorManager;
}
}  // namespace yaze

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_engine.h"
#endif

namespace yaze {
namespace test {

namespace {

std::string GenerateFailureScreenshotPath(const std::string& test_id) {
  std::filesystem::path base_dir =
      std::filesystem::temp_directory_path() / "yaze" / "test-results" /
      test_id;
  std::error_code ec;
  std::filesystem::create_directories(base_dir, ec);

  const int64_t timestamp_ms = absl::ToUnixMillis(absl::Now());
  std::filesystem::path file_path =
      base_dir /
      std::filesystem::path(absl::StrFormat(
          "failure_%lld.bmp", static_cast<long long>(timestamp_ms)));
  return file_path.string();
}

}  // namespace

// Utility function implementations
const char* TestStatusToString(TestStatus status) {
  switch (status) {
    case TestStatus::kNotRun:
      return "Not Run";
    case TestStatus::kRunning:
      return "Running";
    case TestStatus::kPassed:
      return "Passed";
    case TestStatus::kFailed:
      return "Failed";
    case TestStatus::kSkipped:
      return "Skipped";
  }
  return "Unknown";
}

const char* TestCategoryToString(TestCategory category) {
  switch (category) {
    case TestCategory::kUnit:
      return "Unit";
    case TestCategory::kIntegration:
      return "Integration";
    case TestCategory::kUI:
      return "UI";
    case TestCategory::kPerformance:
      return "Performance";
    case TestCategory::kMemory:
      return "Memory";
  }
  return "Unknown";
}

ImVec4 GetTestStatusColor(TestStatus status) {
  switch (status) {
    case TestStatus::kNotRun:
      return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray
    case TestStatus::kRunning:
      return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
    case TestStatus::kPassed:
      return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
    case TestStatus::kFailed:
      return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
    case TestStatus::kSkipped:
      return ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange
  }
  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

// TestManager implementation
TestManager& TestManager::Get() {
  static TestManager instance;
  return instance;
}

TestManager::TestManager() {
  // Note: UI test engine initialization is deferred until ImGui context is ready
  // Call InitializeUITesting() explicitly after ImGui::CreateContext()
}

TestManager::~TestManager() {
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  ShutdownUITesting();
#endif
}

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
void TestManager::InitializeUITesting() {
  if (!ui_test_engine_) {
    // Check if ImGui context is ready
    if (ImGui::GetCurrentContext() == nullptr) {
      LOG_WARN("TestManager",
               "ImGui context not ready, deferring test engine initialization");
      return;
    }

    ui_test_engine_ = ImGuiTestEngine_CreateContext();
    if (ui_test_engine_) {
      ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(ui_test_engine_);
      test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
      test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
      test_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;

      // Start the test engine
      ImGuiTestEngine_Start(ui_test_engine_, ImGui::GetCurrentContext());
      LOG_INFO("TestManager", "ImGuiTestEngine initialized successfully");
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
  stats.memory_usage_mb =
      (stats.texture_count + stats.surface_count) / 1024;  // Rough estimate

  resource_history_.push_back(stats);
}

void TestManager::TrimResourceHistory() {
  if (resource_history_.size() > kMaxResourceHistorySize) {
    resource_history_.erase(
        resource_history_.begin(),
        resource_history_.begin() +
            (resource_history_.size() - kMaxResourceHistorySize));
  }
}

void TestManager::DrawTestDashboard(bool* show_dashboard) {
  bool* dashboard_flag = show_dashboard ? show_dashboard : &show_dashboard_;

  // Set a larger default window size
  ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Test Dashboard", dashboard_flag,
                    ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  // ROM status indicator with detailed information
  bool has_rom = current_rom_ && current_rom_->is_loaded();

  // Add real-time ROM status checking
  static int frame_counter = 0;
  frame_counter++;
  if (frame_counter % 60 == 0) {  // Check every 60 frames
    // Log ROM status periodically for debugging
    LOG_INFO("TestManager",
             "TestManager ROM status check - Frame %d: ROM %p, loaded: %s",
             frame_counter, (void*)current_rom_, has_rom ? "true" : "false");
  }

  if (ImGui::BeginTable("ROM_Status_Table", 2, ImGuiTableFlags_BordersInner)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("ROM Status:");
    ImGui::TableNextColumn();
    if (has_rom) {
      ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s Loaded",
                         ICON_MD_CHECK_CIRCLE);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("ROM Title:");
      ImGui::TableNextColumn();
      ImGui::Text("%s", current_rom_->title().c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("File Name:");
      ImGui::TableNextColumn();
      ImGui::Text("%s", current_rom_->filename().c_str());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Size:");
      ImGui::TableNextColumn();
      ImGui::Text("%.2f MB (%zu bytes)", current_rom_->size() / 1048576.0f,
                  current_rom_->size());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Modified:");
      ImGui::TableNextColumn();
      if (current_rom_->dirty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Yes",
                           ICON_MD_EDIT);
      } else {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s No",
                           ICON_MD_CHECK);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("ROM Pointer:");
      ImGui::TableNextColumn();
      ImGui::Text("%p", (void*)current_rom_);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Actions:");
      ImGui::TableNextColumn();
      if (ImGui::Button("Refresh ROM Reference")) {
        RefreshCurrentRom();
      }

    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s Not Loaded",
                         ICON_MD_WARNING);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("ROM Pointer:");
      ImGui::TableNextColumn();
      ImGui::Text("%p", (void*)current_rom_);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Status:");
      ImGui::TableNextColumn();
      ImGui::Text("ROM-dependent tests will be skipped");

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Actions:");
      ImGui::TableNextColumn();
      if (ImGui::Button("Refresh ROM Reference")) {
        RefreshCurrentRom();
      }
      ImGui::SameLine();
      if (ImGui::Button("Debug ROM State")) {
        LOG_INFO("TestManager", "=== ROM DEBUG INFO ===");
        LOG_INFO("TestManager", "current_rom_ pointer: %p", (void*)current_rom_);
        if (current_rom_) {
          LOG_INFO("TestManager", "ROM title: '%s'",
                   current_rom_->title().c_str());
          LOG_INFO("TestManager", "ROM size: %zu", current_rom_->size());
          LOG_INFO("TestManager", "ROM is_loaded(): %s",
                   current_rom_->is_loaded() ? "true" : "false");
          LOG_INFO("TestManager", "ROM data pointer: %p",
                   (void*)current_rom_->data());
        }
        LOG_INFO("TestManager", "======================");
      }
    }

    ImGui::EndTable();
  }
  ImGui::Separator();

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
        [[maybe_unused]] auto status =
            RunTestsByCategory(TestCategory::kIntegration);
      }
      if (ImGui::MenuItem("UI Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kUI);
      }
      if (ImGui::MenuItem("Performance Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status =
            RunTestsByCategory(TestCategory::kPerformance);
      }
      if (ImGui::MenuItem("Memory Tests", nullptr, false, !is_running_)) {
        [[maybe_unused]] auto status =
            RunTestsByCategory(TestCategory::kMemory);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Resource Monitor", nullptr, &show_resource_monitor_);
      ImGui::MenuItem("Google Tests", nullptr, &show_google_tests_);
      ImGui::MenuItem("ROM Test Results", nullptr, &show_rom_test_results_);
      ImGui::Separator();
      if (ImGui::MenuItem("Export Results", nullptr, false,
                          last_results_.total_tests > 0)) {
        // TODO: Implement result export
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("ROM")) {
      if (ImGui::MenuItem("Test Current ROM", nullptr, false,
                          current_rom_ && current_rom_->is_loaded())) {
        [[maybe_unused]] auto status =
            RunTestsByCategory(TestCategory::kIntegration);
      }
      if (ImGui::MenuItem("Load ROM for Testing...")) {
        show_rom_file_dialog_ = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Refresh ROM Reference")) {
        RefreshCurrentRom();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Configure")) {
      if (ImGui::MenuItem("Test Configuration")) {
        show_test_configuration_ = true;
      }
      ImGui::Separator();
      bool nfd_mode = core::FeatureFlags::get().kUseNativeFileDialog;
      if (ImGui::MenuItem("Use NFD File Dialog", nullptr, &nfd_mode)) {
        core::FeatureFlags::get().kUseNativeFileDialog = nfd_mode;
        LOG_INFO("TestManager", "Global file dialog mode changed to: %s",
                 nfd_mode ? "NFD" : "Bespoke");
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }

  // Show test configuration status
  int enabled_count = 0;
  int total_count = 0;
  static const std::vector<std::string> all_test_names = {
      "ROM_Header_Validation_Test",   "ROM_Data_Access_Test",
      "ROM_Graphics_Extraction_Test", "ROM_Overworld_Loading_Test",
      "Tile16_Editor_Test",           "Comprehensive_Save_Test",
      "ROM_Sprite_Data_Test",         "ROM_Music_Data_Test"};

  for (const auto& test_name : all_test_names) {
    total_count++;
    if (IsTestEnabled(test_name)) {
      enabled_count++;
    }
  }

  ImGui::Text("%s Test Status: %d/%d enabled", ICON_MD_CHECKLIST, enabled_count,
              total_count);
  if (enabled_count < total_count) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       "(Some tests disabled - check Configuration)");
  }

  // Enhanced test execution status
  if (is_running_) {
    ImGui::PushStyleColor(ImGuiCol_Text,
                          GetTestStatusColor(TestStatus::kRunning));
    ImGui::Text("%s Running: %s", ICON_MD_PLAY_CIRCLE_FILLED,
                current_test_name_.c_str());
    ImGui::PopStyleColor();
    ImGui::ProgressBar(progress_, ImVec2(-1, 0),
                       absl::StrFormat("%.0f%%", progress_ * 100.0f).c_str());
  } else {
    // Enhanced control buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button(
            absl::StrCat(ICON_MD_PLAY_ARROW, " Run All Tests").c_str(),
            ImVec2(140, 0))) {
      [[maybe_unused]] auto status = RunAllTests();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button(absl::StrCat(ICON_MD_SPEED, " Quick Test").c_str(),
                      ImVec2(100, 0))) {
      [[maybe_unused]] auto status = RunTestsByCategory(TestCategory::kMemory);
    }

    ImGui::SameLine();
    bool has_rom = current_rom_ && current_rom_->is_loaded();
    if (has_rom) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
    }
    if (ImGui::Button(absl::StrCat(ICON_MD_STORAGE, " ROM Tests").c_str(),
                      ImVec2(100, 0))) {
      if (has_rom) {
        [[maybe_unused]] auto status =
            RunTestsByCategory(TestCategory::kIntegration);
      }
    }
    if (has_rom) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      if (has_rom) {
        ImGui::SetTooltip("Run tests on current ROM: %s",
                          current_rom_->title().c_str());
      } else {
        ImGui::SetTooltip("Load a ROM to enable ROM-dependent tests");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(absl::StrCat(ICON_MD_CLEAR, " Clear").c_str(),
                      ImVec2(80, 0))) {
      ClearResults();
    }

    ImGui::SameLine();
    if (ImGui::Button(absl::StrCat(ICON_MD_SETTINGS, " Config").c_str(),
                      ImVec2(80, 0))) {
      show_test_configuration_ = true;
    }
  }

  ImGui::Separator();

  // Enhanced test results summary with better visuals
  if (last_results_.total_tests > 0) {
    // Test summary header
    ImGui::Text("%s Test Results Summary", ICON_MD_ASSESSMENT);

    // Progress bar showing pass rate
    float pass_rate = last_results_.GetPassRate();
    ImVec4 progress_color = pass_rate >= 0.9f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                            : pass_rate >= 0.7f
                                ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
                                : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progress_color);
    ImGui::ProgressBar(
        pass_rate, ImVec2(-1, 0),
        absl::StrFormat("Pass Rate: %.1f%%", pass_rate * 100.0f).c_str());
    ImGui::PopStyleColor();

    // Test counts with icons
    ImGui::Text("%s Total: %zu", ICON_MD_ANALYTICS, last_results_.total_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kPassed), "%s %zu",
                       ICON_MD_CHECK_CIRCLE, last_results_.passed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kFailed), "%s %zu",
                       ICON_MD_ERROR, last_results_.failed_tests);
    ImGui::SameLine();
    ImGui::TextColored(GetTestStatusColor(TestStatus::kSkipped), "%s %zu",
                       ICON_MD_SKIP_NEXT, last_results_.skipped_tests);

    ImGui::Text("%s Duration: %lld ms", ICON_MD_TIMER,
                last_results_.total_duration.count());

    // Test suite breakdown
    if (ImGui::CollapsingHeader("Test Suite Breakdown")) {
      std::unordered_map<std::string, std::pair<size_t, size_t>>
          suite_stats;  // passed, total
      for (const auto& result : last_results_.individual_results) {
        suite_stats[result.suite_name].second++;  // total
        if (result.status == TestStatus::kPassed) {
          suite_stats[result.suite_name].first++;  // passed
        }
      }

      for (const auto& [suite_name, stats] : suite_stats) {
        float suite_pass_rate =
            stats.second > 0 ? static_cast<float>(stats.first) / stats.second
                             : 0.0f;
        ImGui::Text("%s: %zu/%zu (%.0f%%)", suite_name.c_str(), stats.first,
                    stats.second, suite_pass_rate * 100.0f);
      }
    }
  }

  ImGui::Separator();

  // Enhanced test filter with category selection
  ImGui::Text("%s Filter & View Options", ICON_MD_FILTER_LIST);

  // Category filter
  const char* categories[] = {"All", "Unit",        "Integration",
                              "UI",  "Performance", "Memory"};
  static int selected_category = 0;
  if (ImGui::Combo("Category", &selected_category, categories,
                   IM_ARRAYSIZE(categories))) {
    switch (selected_category) {
      case 0:
        category_filter_ = TestCategory::kUnit;
        break;  // All - use Unit as default
      case 1:
        category_filter_ = TestCategory::kUnit;
        break;
      case 2:
        category_filter_ = TestCategory::kIntegration;
        break;
      case 3:
        category_filter_ = TestCategory::kUI;
        break;
      case 4:
        category_filter_ = TestCategory::kPerformance;
        break;
      case 5:
        category_filter_ = TestCategory::kMemory;
        break;
    }
  }

  // Text filter
  static char filter_buffer[256] = "";
  ImGui::SetNextItemWidth(-80);
  if (ImGui::InputTextWithHint("##filter", "Search tests...", filter_buffer,
                               sizeof(filter_buffer))) {
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
      ImGui::TextColored(
          ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "No test results to display. Run some tests to see results here.");
    } else {
      for (const auto& result : last_results_.individual_results) {
        // Apply filters
        bool category_match =
            (selected_category == 0) || (result.category == category_filter_);
        bool text_match =
            test_filter_.empty() ||
            result.name.find(test_filter_) != std::string::npos ||
            result.suite_name.find(test_filter_) != std::string::npos;

        if (!category_match || !text_match) {
          continue;
        }

        ImGui::PushID(&result);

        // Status icon and test name
        const char* status_icon = ICON_MD_HELP;
        switch (result.status) {
          case TestStatus::kPassed:
            status_icon = ICON_MD_CHECK_CIRCLE;
            break;
          case TestStatus::kFailed:
            status_icon = ICON_MD_ERROR;
            break;
          case TestStatus::kSkipped:
            status_icon = ICON_MD_SKIP_NEXT;
            break;
          case TestStatus::kRunning:
            status_icon = ICON_MD_PLAY_CIRCLE_FILLED;
            break;
          default:
            break;
        }

        ImGui::TextColored(GetTestStatusColor(result.status), "%s %s::%s",
                           status_icon, result.suite_name.c_str(),
                           result.name.c_str());

        // Show duration and timestamp on same line if space allows
        if (ImGui::GetContentRegionAvail().x > 200) {
          ImGui::SameLine();
          ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%lld ms)",
                             result.duration.count());
        }

        // Show detailed information for failed tests
        if (result.status == TestStatus::kFailed &&
            !result.error_message.empty()) {
          ImGui::Indent();
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));
          ImGui::TextWrapped("%s %s", ICON_MD_ERROR_OUTLINE,
                             result.error_message.c_str());
          ImGui::PopStyleColor();
          ImGui::Unindent();
        }

        // Show additional info for passed tests if they have messages
        if (result.status == TestStatus::kPassed &&
            !result.error_message.empty()) {
          ImGui::Indent();
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.8f, 1.0f));
          ImGui::TextWrapped("%s %s", ICON_MD_INFO,
                             result.error_message.c_str());
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
    ImGui::Begin(absl::StrCat(ICON_MD_MONITOR, " Resource Monitor").c_str(),
                 &show_resource_monitor_);

    if (!resource_history_.empty()) {
      const auto& latest = resource_history_.back();
      ImGui::Text("%s Textures: %zu", ICON_MD_TEXTURE, latest.texture_count);
      ImGui::Text("%s Surfaces: %zu", ICON_MD_LAYERS, latest.surface_count);
      ImGui::Text("%s Memory: %zu MB", ICON_MD_MEMORY, latest.memory_usage_mb);
      ImGui::Text("%s FPS: %.1f", ICON_MD_SPEED, latest.frame_rate);

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

  // Google Tests window
  if (show_google_tests_) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Google Tests", &show_google_tests_)) {
      ImGui::Text("%s Google Test Integration", ICON_MD_SCIENCE);
      ImGui::Separator();

#ifdef YAZE_ENABLE_GTEST
      ImGui::Text("Google Test framework is available");

      if (ImGui::Button("Run All Google Tests")) {
        // Run Google tests - this would integrate with gtest
        LOG_INFO("TestManager", "Running Google Tests...");
      }

      ImGui::SameLine();
      if (ImGui::Button("Run Specific Test Suite")) {
        // Show test suite selector
      }

      ImGui::Separator();
      ImGui::Text("Available Test Suites:");
      ImGui::BulletText("Unit Tests");
      ImGui::BulletText("Integration Tests");
      ImGui::BulletText("Performance Tests");
#else
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                         "%s Google Test framework not available",
                         ICON_MD_WARNING);
      ImGui::Text("Enable YAZE_ENABLE_GTEST to use Google Test integration");
#endif
    }
    ImGui::End();
  }

  // ROM Test Results window
  if (show_rom_test_results_) {
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ROM Test Results", &show_rom_test_results_)) {
      ImGui::Text("%s ROM Analysis Results", ICON_MD_ANALYTICS);

      if (current_rom_ && current_rom_->is_loaded()) {
        ImGui::Text("Testing ROM: %s", current_rom_->title().c_str());
        ImGui::Separator();

        // Show ROM-specific test results
        if (ImGui::CollapsingHeader("ROM Data Integrity",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Text("ROM Size: %.2f MB", current_rom_->size() / 1048576.0f);
          ImGui::Text("Modified: %s", current_rom_->dirty() ? "Yes" : "No");

          if (ImGui::Button("Run Data Integrity Check")) {
            [[maybe_unused]] auto status = TestRomDataIntegrity(current_rom_);
            [[maybe_unused]] auto suite_status =
                RunTestsByCategory(TestCategory::kIntegration);
          }
        }

        if (ImGui::CollapsingHeader("Save/Load Testing")) {
          ImGui::Text("Test ROM save and load operations");

          if (ImGui::Button("Test Save Operations")) {
            [[maybe_unused]] auto status = TestRomSaveLoad(current_rom_);
          }

          ImGui::SameLine();
          if (ImGui::Button("Test Load Operations")) {
            [[maybe_unused]] auto status = TestRomSaveLoad(current_rom_);
          }
        }

        if (ImGui::CollapsingHeader("Editor Integration")) {
          ImGui::Text("Test editor components with current ROM");

          if (ImGui::Button("Test Overworld Editor")) {
            // Test overworld editor with current ROM
          }

          ImGui::SameLine();
          if (ImGui::Button("Test Tile16 Editor")) {
            // Test tile16 editor with current ROM
          }
        }

      } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                           "%s No ROM loaded for analysis", ICON_MD_WARNING);
      }
    }
    ImGui::End();
  }

  // ROM File Dialog
  if (show_rom_file_dialog_) {
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Appearing);
    if (ImGui::Begin("Load ROM for Testing", &show_rom_file_dialog_,
                     ImGuiWindowFlags_NoResize)) {
      ImGui::Text("%s Load ROM for Testing", ICON_MD_FOLDER_OPEN);
      ImGui::Separator();

      ImGui::Text("Select a ROM file to run tests on:");

      if (ImGui::Button("Browse ROM File...", ImVec2(-1, 0))) {
        // TODO: Implement file dialog to load ROM specifically for testing
        // This would be separate from the main editor ROM
        show_rom_file_dialog_ = false;
      }

      ImGui::Separator();
      if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
        show_rom_file_dialog_ = false;
      }
    }
    ImGui::End();
  }

  // Test Configuration Window
  if (show_test_configuration_) {
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Test Configuration", &show_test_configuration_)) {
      ImGui::Text("%s Test Configuration", ICON_MD_SETTINGS);
      ImGui::Separator();

      // File Dialog Configuration
      if (ImGui::CollapsingHeader("File Dialog Settings",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("File Dialog Implementation:");

        bool nfd_mode = core::FeatureFlags::get().kUseNativeFileDialog;
        if (ImGui::RadioButton("NFD (Native File Dialog)", nfd_mode)) {
          core::FeatureFlags::get().kUseNativeFileDialog = true;
          LOG_INFO("TestManager", "Global file dialog mode set to: NFD");
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Use NFD library for native OS file dialogs (global setting)");
        }

        if (ImGui::RadioButton("Bespoke Implementation", !nfd_mode)) {
          core::FeatureFlags::get().kUseNativeFileDialog = false;
          LOG_INFO("TestManager", "Global file dialog mode set to: Bespoke");
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Use custom file dialog implementation (global setting)");
        }

        ImGui::Separator();
        ImGui::Text(
            "Current Mode: %s",
            core::FeatureFlags::get().kUseNativeFileDialog ? "NFD" : "Bespoke");
        ImGui::TextColored(
            ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Note: This setting affects ALL file dialogs in the application");

        if (ImGui::Button("Test Current File Dialog")) {
          // Test the current file dialog implementation
          LOG_INFO("TestManager", "Testing global file dialog mode: %s",
                   core::FeatureFlags::get().kUseNativeFileDialog
                       ? "NFD"
                       : "Bespoke");

          // Actually test the file dialog
          auto result = util::FileDialogWrapper::ShowOpenFileDialog();
          if (!result.empty()) {
            LOG_INFO("TestManager", "File dialog test successful: %s",
                     result.c_str());
          } else {
            LOG_INFO("TestManager",
                     "File dialog test: No file selected or dialog canceled");
          }
        }

        ImGui::SameLine();
        if (ImGui::Button("Test NFD Directly")) {
          auto result = util::FileDialogWrapper::ShowOpenFileDialogNFD();
          if (!result.empty()) {
            LOG_INFO("TestManager", "NFD test successful: %s", result.c_str());
          } else {
            LOG_INFO(
                "TestManager",
                "NFD test: No file selected, canceled, or error occurred");
          }
        }

        ImGui::SameLine();
        if (ImGui::Button("Test Bespoke Directly")) {
          auto result = util::FileDialogWrapper::ShowOpenFileDialogBespoke();
          if (!result.empty()) {
            LOG_INFO("TestManager", "Bespoke test successful: %s",
                     result.c_str());
          } else {
            LOG_INFO("TestManager",
                     "Bespoke test: No file selected or not implemented");
          }
        }
      }

      // Test Selection Configuration
      if (ImGui::CollapsingHeader("Test Selection",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Enable/Disable Individual Tests:");
        ImGui::Separator();

        // List of known tests with their risk levels
        static const std::vector<std::pair<std::string, std::string>>
            known_tests = {
                {"ROM_Header_Validation_Test",
                 "Safe - Read-only ROM header validation"},
                {"ROM_Data_Access_Test",
                 "Safe - Basic ROM data access testing"},
                {"ROM_Graphics_Extraction_Test",
                 "Safe - Graphics data extraction testing"},
                {"ROM_Overworld_Loading_Test",
                 "Safe - Overworld data loading testing"},
                {"Tile16_Editor_Test",
                 "Moderate - Tile16 editor initialization"},
                {"Comprehensive_Save_Test",
                 "DANGEROUS - Known to crash, uses ROM copies"},
                {"ROM_Sprite_Data_Test", "Safe - Sprite data validation"},
                {"ROM_Music_Data_Test", "Safe - Music data validation"}};

        // Initialize problematic tests as disabled by default
        static bool initialized_defaults = false;
        if (!initialized_defaults) {
          DisableTest(
              "Comprehensive_Save_Test");  // Disable crash-prone test by default
          initialized_defaults = true;
        }

        if (ImGui::BeginTable(
                "TestSelection", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Test Name", ImGuiTableColumnFlags_WidthFixed,
                                  200);
          ImGui::TableSetupColumn("Risk Level",
                                  ImGuiTableColumnFlags_WidthStretch);
          ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                                  80);
          ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
                                  100);
          ImGui::TableHeadersRow();

          for (const auto& [test_name, description] : known_tests) {
            bool enabled = IsTestEnabled(test_name);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", test_name.c_str());

            ImGui::TableNextColumn();
            // Color-code the risk level
            if (description.find("DANGEROUS") != std::string::npos) {
              ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                                 description.c_str());
            } else if (description.find("Moderate") != std::string::npos) {
              ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s",
                                 description.c_str());
            } else {
              ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%s",
                                 description.c_str());
            }

            ImGui::TableNextColumn();
            if (enabled) {
              ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s ON",
                                 ICON_MD_CHECK);
            } else {
              ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s OFF",
                                 ICON_MD_BLOCK);
            }

            ImGui::TableNextColumn();
            ImGui::PushID(test_name.c_str());
            if (enabled) {
              if (ImGui::Button("Disable")) {
                DisableTest(test_name);
                LOG_INFO("TestManager", "Disabled test: %s", test_name.c_str());
              }
            } else {
              if (ImGui::Button("Enable")) {
                EnableTest(test_name);
                LOG_INFO("TestManager", "Enabled test: %s", test_name.c_str());
              }
            }
            ImGui::PopID();
          }

          ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text("Quick Actions:");

        if (ImGui::Button("Enable Safe Tests Only")) {
          for (const auto& [test_name, description] : known_tests) {
            if (description.find("Safe") != std::string::npos) {
              EnableTest(test_name);
            } else {
              DisableTest(test_name);
            }
          }
          LOG_INFO("TestManager", "Enabled only safe tests");
        }
        ImGui::SameLine();

        if (ImGui::Button("Enable All Tests")) {
          for (const auto& [test_name, description] : known_tests) {
            EnableTest(test_name);
          }
          LOG_INFO("TestManager", "Enabled all tests (including dangerous ones)");
        }
        ImGui::SameLine();

        if (ImGui::Button("Disable All Tests")) {
          for (const auto& [test_name, description] : known_tests) {
            DisableTest(test_name);
          }
          LOG_INFO("TestManager", "Disabled all tests");
        }

        ImGui::Separator();
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
            "⚠️  Recommendation: Use 'Enable Safe Tests Only' to avoid crashes");
      }

      // Platform-specific settings
      if (ImGui::CollapsingHeader("Platform Settings")) {
        ImGui::Text("macOS Tahoe Compatibility:");
        ImGui::BulletText("NFD may have issues on macOS Sequoia+");
        ImGui::BulletText("Bespoke dialog provides fallback option");
        ImGui::BulletText(
            "Global setting affects File → Open, Project dialogs, etc.");

        ImGui::Separator();
        ImGui::Text("Test Both Implementations:");

        if (ImGui::Button("Quick Test NFD")) {
          auto result = util::FileDialogWrapper::ShowOpenFileDialogNFD();
          LOG_INFO("TestManager", "NFD test result: %s",
                   result.empty() ? "Failed/Canceled" : result.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Quick Test Bespoke")) {
          auto result = util::FileDialogWrapper::ShowOpenFileDialogBespoke();
          LOG_INFO("TestManager", "Bespoke test result: %s",
                   result.empty() ? "Failed/Not Implemented" : result.c_str());
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Note: These tests don't change the global setting");
      }
    }
    ImGui::End();
  }

  // Test Session Creation Dialog
  if (show_test_session_dialog_) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Appearing);

    if (ImGui::Begin("Test ROM Session", &show_test_session_dialog_,
                     ImGuiWindowFlags_NoResize)) {
      ImGui::Text("%s Test ROM Created Successfully", ICON_MD_CHECK_CIRCLE);
      ImGui::Separator();

      ImGui::Text("A test ROM has been created with your modifications:");
      ImGui::Text("File: %s", test_rom_path_for_session_.c_str());

      // Extract just the filename for display
      std::string display_filename = test_rom_path_for_session_;
      auto last_slash = display_filename.find_last_of("/\\");
      if (last_slash != std::string::npos) {
        display_filename = display_filename.substr(last_slash + 1);
      }

      ImGui::Separator();
      ImGui::Text("Would you like to open this test ROM in a new session?");

      if (ImGui::Button(
              absl::StrFormat("%s Open in New Session", ICON_MD_TAB).c_str(),
              ImVec2(200, 0))) {
        // TODO: This would need access to EditorManager to create a new session
        // For now, just show a message
        LOG_INFO("TestManager",
                 "User requested to open test ROM in new session: %s",
                 test_rom_path_for_session_.c_str());
        show_test_session_dialog_ = false;
      }

      ImGui::SameLine();
      if (ImGui::Button(
              absl::StrFormat("%s Keep Current Session", ICON_MD_CLOSE).c_str(),
              ImVec2(200, 0))) {
        show_test_session_dialog_ = false;
      }

      ImGui::Separator();
      ImGui::TextColored(
          ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
          "Note: Test ROM contains your modifications and can be");
      ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                         "opened later using File → Open");
    }
    ImGui::End();
  }
}

void TestManager::RefreshCurrentRom() {
  LOG_INFO("TestManager", "=== TestManager ROM Refresh ===");

  // Log current TestManager ROM state for debugging
  if (current_rom_) {
    LOG_INFO("TestManager", "TestManager ROM pointer: %p", (void*)current_rom_);
    LOG_INFO("TestManager", "ROM is_loaded(): %s",
             current_rom_->is_loaded() ? "true" : "false");
    if (current_rom_->is_loaded()) {
      LOG_INFO("TestManager", "ROM title: '%s'",
               current_rom_->title().c_str());
      LOG_INFO("TestManager", "ROM size: %.2f MB",
               current_rom_->size() / 1048576.0f);
      LOG_INFO("TestManager", "ROM dirty: %s",
               current_rom_->dirty() ? "true" : "false");
    }
  } else {
    LOG_INFO("TestManager", "TestManager ROM pointer is null");
    LOG_INFO(
        "TestManager",
        "Note: ROM should be set by EditorManager when ROM is loaded");
  }
  LOG_INFO("TestManager", "===============================");
}

absl::Status TestManager::CreateTestRomCopy(
    Rom* source_rom, std::unique_ptr<Rom>& test_rom) {
  if (!source_rom || !source_rom->is_loaded()) {
    return absl::FailedPreconditionError("Source ROM not loaded");
  }

  LOG_INFO("TestManager", "Creating test ROM copy from: %s",
           source_rom->title().c_str());

  // Create a new ROM instance
  test_rom = std::make_unique<Rom>();

  // Copy the ROM data
  auto rom_data = source_rom->vector();
  auto load_status = test_rom->LoadFromData(rom_data, true);
  if (!load_status.ok()) {
    return load_status;
  }

  LOG_INFO("TestManager", "Test ROM copy created successfully (size: %.2f MB)",
           test_rom->size() / 1048576.0f);
  return absl::OkStatus();
}

std::string TestManager::GenerateTestRomFilename(
    const std::string& base_name) {
  // Generate filename with timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto local_time = *std::localtime(&time_t);

  std::string timestamp =
      absl::StrFormat("%04d%02d%02d_%02d%02d%02d", local_time.tm_year + 1900,
                      local_time.tm_mon + 1, local_time.tm_mday,
                      local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

  std::string base_filename = base_name;
  // Remove any path and extension
  auto last_slash = base_filename.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    base_filename = base_filename.substr(last_slash + 1);
  }
  auto last_dot = base_filename.find_last_of('.');
  if (last_dot != std::string::npos) {
    base_filename = base_filename.substr(0, last_dot);
  }

  return absl::StrFormat("%s_test_%s.sfc", base_filename.c_str(),
                         timestamp.c_str());
}

void TestManager::OfferTestSessionCreation(const std::string& test_rom_path) {
  // Store the test ROM path for the dialog
  test_rom_path_for_session_ = test_rom_path;
  show_test_session_dialog_ = true;
}

absl::Status TestManager::TestRomWithCopy(
    Rom* source_rom, std::function<absl::Status(Rom*)> test_function) {
  if (!source_rom || !source_rom->is_loaded()) {
    return absl::FailedPreconditionError("Source ROM not loaded");
  }

  // Create a copy of the ROM for testing
  std::unique_ptr<Rom> test_rom;
  RETURN_IF_ERROR(CreateTestRomCopy(source_rom, test_rom));

  LOG_INFO("TestManager", "Executing test function on ROM copy");

  // Run the test function on the copy
  auto test_result = test_function(test_rom.get());

  LOG_INFO("TestManager", "Test function completed with status: %s",
           test_result.ToString().c_str());

  return test_result;
}

absl::Status TestManager::LoadRomForTesting(const std::string& filename) {
  // This would load a ROM specifically for testing purposes
  // For now, just log the request
  LOG_INFO("TestManager", "Request to load ROM for testing: %s",
           filename.c_str());
  return absl::UnimplementedError(
      "ROM loading for testing not yet implemented");
}

void TestManager::ShowRomComparisonResults(const Rom& before,
                                           const Rom& after) {
  if (ImGui::Begin("ROM Comparison Results")) {
    ImGui::Text("%s ROM Before/After Comparison", ICON_MD_COMPARE);
    ImGui::Separator();

    if (ImGui::BeginTable("RomComparison", 3, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Property");
      ImGui::TableSetupColumn("Before");
      ImGui::TableSetupColumn("After");
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Size");
      ImGui::TableNextColumn();
      ImGui::Text("%.2f MB", before.size() / 1048576.0f);
      ImGui::TableNextColumn();
      ImGui::Text("%.2f MB", after.size() / 1048576.0f);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Modified");
      ImGui::TableNextColumn();
      ImGui::Text("%s", before.dirty() ? "Yes" : "No");
      ImGui::TableNextColumn();
      ImGui::Text("%s", after.dirty() ? "Yes" : "No");

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

absl::Status TestManager::TestRomSaveLoad(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("No ROM loaded for testing");
  }

  // Use TestRomWithCopy to avoid affecting the original ROM
  return TestRomWithCopy(rom, [this](Rom* test_rom) -> absl::Status {
    LOG_INFO("TestManager", "Testing ROM save/load operations on copy: %s",
             test_rom->title().c_str());

    // Perform test modifications on the copy
    // Test save operations
    Rom::SaveSettings settings;
    settings.backup = false;
    settings.save_new = true;
    settings.filename = GenerateTestRomFilename(test_rom->title());

    auto save_status = test_rom->SaveToFile(settings);
    if (!save_status.ok()) {
      return save_status;
    }

    LOG_INFO("TestManager", "Test ROM saved successfully to: %s",
             settings.filename.c_str());

    // Offer to open test ROM in new session
    OfferTestSessionCreation(settings.filename);

    return absl::OkStatus();
  });
}

absl::Status TestManager::TestRomDataIntegrity(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("No ROM loaded for testing");
  }

  // Use TestRomWithCopy for integrity testing (read-only but uses copy for safety)
  return TestRomWithCopy(rom, [](Rom* test_rom) -> absl::Status {
    LOG_INFO("TestManager", "Testing ROM data integrity on copy: %s",
             test_rom->title().c_str());

    // Perform data integrity checks on the copy
    // This validates ROM structure, checksums, etc. without affecting original

    // Basic ROM structure validation
    if (test_rom->size() < 0x100000) {  // 1MB minimum for ALTTP
      return absl::FailedPreconditionError(
          "ROM file too small for A Link to the Past");
    }

    // Check ROM header
    auto header_status = test_rom->ReadByteVector(0x7FC0, 32);
    if (!header_status.ok()) {
      return header_status.status();
    }

    LOG_INFO("TestManager", "ROM integrity check passed for: %s",
             test_rom->title().c_str());
    return absl::OkStatus();
  });
}

std::string TestManager::RegisterHarnessTest(const std::string& name,
                                             const std::string& category) {
  absl::MutexLock lock(&harness_history_mutex_);

  const std::string sanitized_category = category.empty() ? "grpc" : category;
  std::string test_id = GenerateHarnessTestIdLocked(sanitized_category);

  HarnessTestExecution execution;
  execution.test_id = test_id;
  execution.name = name;
  execution.category = sanitized_category;
  execution.status = HarnessTestStatus::kQueued;
  execution.queued_at = absl::Now();
  execution.started_at = absl::InfinitePast();
  execution.completed_at = absl::InfinitePast();

  harness_history_[test_id] = execution;
  harness_history_order_.push_back(test_id);
  TrimHarnessHistoryLocked();

  HarnessAggregate& aggregate = harness_aggregates_[name];
  if (aggregate.category.empty()) {
    aggregate.category = sanitized_category;
  }
  aggregate.last_run = execution.queued_at;
  aggregate.latest_execution = execution;

  return test_id;
}

void TestManager::MarkHarnessTestRunning(const std::string& test_id) {
  absl::MutexLock lock(&harness_history_mutex_);

  auto it = harness_history_.find(test_id);
  if (it == harness_history_.end()) {
    return;
  }

  HarnessTestExecution& execution = it->second;
  execution.status = HarnessTestStatus::kRunning;
  execution.started_at = absl::Now();

  HarnessAggregate& aggregate = harness_aggregates_[execution.name];
  if (aggregate.category.empty()) {
    aggregate.category = execution.category;
  }
  aggregate.latest_execution = execution;
}

void TestManager::MarkHarnessTestCompleted(
    const std::string& test_id, HarnessTestStatus status,
    const std::string& error_message,
    const std::vector<std::string>& assertion_failures,
    const std::vector<std::string>& logs,
    const std::map<std::string, int32_t>& metrics) {
  bool capture_failure_context =
      status == HarnessTestStatus::kFailed ||
      status == HarnessTestStatus::kTimeout;

  {
    absl::MutexLock lock(&harness_history_mutex_);

    auto it = harness_history_.find(test_id);
    if (it == harness_history_.end()) {
      return;
    }

    HarnessTestExecution& execution = it->second;
    execution.status = status;
    if (execution.started_at == absl::InfinitePast()) {
      execution.started_at = execution.queued_at;
    }
    execution.completed_at = absl::Now();
    execution.duration = execution.completed_at - execution.started_at;
    execution.error_message = error_message;
    if (!assertion_failures.empty()) {
      execution.assertion_failures = assertion_failures;
    }
    if (!logs.empty()) {
      execution.logs.insert(execution.logs.end(), logs.begin(), logs.end());
    }
    if (!metrics.empty()) {
      execution.metrics.insert(metrics.begin(), metrics.end());
    }

    HarnessAggregate& aggregate = harness_aggregates_[execution.name];
    if (aggregate.category.empty()) {
      aggregate.category = execution.category;
    }
    aggregate.total_runs += 1;
    if (status == HarnessTestStatus::kPassed) {
      aggregate.pass_count += 1;
    } else if (status == HarnessTestStatus::kFailed ||
               status == HarnessTestStatus::kTimeout) {
      aggregate.fail_count += 1;
    }
    aggregate.total_duration += execution.duration;
    aggregate.last_run = execution.completed_at;
    aggregate.latest_execution = execution;
  }

  if (capture_failure_context) {
    CaptureFailureContext(test_id);
  }
}

void TestManager::AppendHarnessTestLog(const std::string& test_id,
                                       const std::string& log_entry) {
  absl::MutexLock lock(&harness_history_mutex_);

  auto it = harness_history_.find(test_id);
  if (it == harness_history_.end()) {
    return;
  }

  HarnessTestExecution& execution = it->second;
  execution.logs.push_back(log_entry);

  HarnessAggregate& aggregate = harness_aggregates_[execution.name];
  aggregate.latest_execution.logs = execution.logs;
}

absl::StatusOr<HarnessTestExecution> TestManager::GetHarnessTestExecution(
    const std::string& test_id) const {
  absl::MutexLock lock(&harness_history_mutex_);

  auto it = harness_history_.find(test_id);
  if (it == harness_history_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("Test ID '%s' not found", test_id));
  }

  return it->second;
}

std::vector<HarnessTestSummary> TestManager::ListHarnessTestSummaries(
    const std::string& category_filter) const {
  absl::MutexLock lock(&harness_history_mutex_);
  std::vector<HarnessTestSummary> summaries;
  summaries.reserve(harness_aggregates_.size());

  for (const auto& [name, aggregate] : harness_aggregates_) {
    if (!category_filter.empty() && aggregate.category != category_filter) {
      continue;
    }

    HarnessTestSummary summary;
    summary.latest_execution = aggregate.latest_execution;
    summary.total_runs = aggregate.total_runs;
    summary.pass_count = aggregate.pass_count;
    summary.fail_count = aggregate.fail_count;
    summary.total_duration = aggregate.total_duration;
    summaries.push_back(summary);
  }

  std::sort(summaries.begin(), summaries.end(),
            [](const HarnessTestSummary& a, const HarnessTestSummary& b) {
              absl::Time time_a = a.latest_execution.completed_at;
              if (time_a == absl::InfinitePast()) {
                time_a = a.latest_execution.queued_at;
              }
              absl::Time time_b = b.latest_execution.completed_at;
              if (time_b == absl::InfinitePast()) {
                time_b = b.latest_execution.queued_at;
              }
              return time_a > time_b;
            });

  return summaries;
}

std::string TestManager::GenerateHarnessTestIdLocked(absl::string_view prefix) {
  static std::mt19937 rng(std::random_device{}());
  static std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFF);

  std::string sanitized =
      absl::StrReplaceAll(std::string(prefix), {{" ", "_"}, {":", "_"}});
  if (sanitized.empty()) {
    sanitized = "test";
  }

  for (int attempt = 0; attempt < 8; ++attempt) {
    std::string candidate = absl::StrFormat("%s_%08x", sanitized, dist(rng));
    if (harness_history_.find(candidate) == harness_history_.end()) {
      return candidate;
    }
  }

  return absl::StrFormat(
      "%s_%lld", sanitized,
      static_cast<long long>(absl::ToUnixMillis(absl::Now())));
}

void TestManager::TrimHarnessHistoryLocked() {
  while (harness_history_order_.size() > harness_history_limit_) {
    const std::string& oldest_id = harness_history_order_.front();
    auto it = harness_history_.find(oldest_id);
    if (it != harness_history_.end()) {
      harness_history_.erase(it);
    }
    harness_history_order_.pop_front();
  }
}

void TestManager::CaptureFailureContext(const std::string& test_id) {
  // IT-08b: Capture failure diagnostics
  // Note: This method is called with the harness_history_mutex_ unlocked
  // to avoid deadlock when Screenshot helper touches SDL state.

  // 1. Capture execution context metadata from ImGui.
  std::string failure_context;
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx != nullptr) {
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
    ImGuiWindow* current_window = ctx->CurrentWindow;
    ImGuiWindow* nav_window = ctx->NavWindow;
    ImGuiWindow* hovered_window = ctx->HoveredWindow;

    const char* current_name =
        (current_window && current_window->Name) ? current_window->Name : "none";
    const char* nav_name =
        (nav_window && nav_window->Name) ? nav_window->Name : "none";
    const char* hovered_name = (hovered_window && hovered_window->Name)
                                    ? hovered_window->Name
                                    : "none";

    ImGuiID active_id = ImGui::GetActiveID();
    ImGuiID hovered_id = ImGui::GetHoveredID();
    failure_context = absl::StrFormat(
        "frame=%d current_window=%s nav_window=%s hovered_window=%s "
        "active_id=0x%08X hovered_id=0x%08X",
        ImGui::GetFrameCount(), current_name, nav_name, hovered_name,
        active_id, hovered_id);
#else
    failure_context =
        absl::StrFormat("frame=%d", ImGui::GetFrameCount());
#endif
  } else {
    failure_context = "ImGui context not available";
  }

  std::string artifact_path;
  {
    absl::MutexLock lock(&harness_history_mutex_);
    auto it = harness_history_.find(test_id);
    if (it == harness_history_.end()) {
      return;
    }

    HarnessTestExecution& execution = it->second;
    execution.failure_context = failure_context;
    if (execution.screenshot_path.empty()) {
      execution.screenshot_path = GenerateFailureScreenshotPath(test_id);
    }
    artifact_path = execution.screenshot_path;
  }

  // 2. Capture widget state snapshot (IT-08c) and failure screenshot.
  std::string widget_state = core::CaptureWidgetState();
#if defined(YAZE_WITH_GRPC)
  absl::StatusOr<ScreenshotArtifact> screenshot_artifact =
      CaptureHarnessScreenshot(artifact_path);
#endif

  {
    absl::MutexLock lock(&harness_history_mutex_);
    auto it = harness_history_.find(test_id);
    if (it == harness_history_.end()) {
      return;
    }

    HarnessTestExecution& execution = it->second;
    execution.failure_context = failure_context;
    execution.widget_state = widget_state;

#if defined(YAZE_WITH_GRPC)
    if (screenshot_artifact.ok()) {
      execution.screenshot_path = screenshot_artifact->file_path;
      execution.screenshot_size_bytes = screenshot_artifact->file_size_bytes;
      execution.logs.push_back(absl::StrFormat(
          "[auto-capture] Failure screenshot saved to %s (%lld bytes)",
          execution.screenshot_path,
          static_cast<long long>(execution.screenshot_size_bytes)));
    } else {
      execution.logs.push_back(absl::StrFormat(
          "[auto-capture] Screenshot capture failed: %s",
          screenshot_artifact.status().message()));
    }
#else
    execution.logs.push_back(
        "[auto-capture] Screenshot capture unavailable (YAZE_WITH_GRPC=OFF)");
#endif

    // Keep aggregate cache in sync with the latest execution snapshot.
    auto aggregate_it = harness_aggregates_.find(execution.name);
    if (aggregate_it != harness_aggregates_.end()) {
      aggregate_it->second.latest_execution = execution;
    }
  }

#if defined(YAZE_WITH_GRPC)
  if (screenshot_artifact.ok()) {
    LOG_INFO("TestManager",
             "Captured failure context for test %s: %s", test_id.c_str(),
             failure_context.c_str());
    LOG_INFO("TestManager",
             "Failure screenshot stored at %s (%lld bytes)",
             screenshot_artifact->file_path.c_str(),
             static_cast<long long>(screenshot_artifact->file_size_bytes));
  } else {
    LOG_WARN("TestManager",
             "Failed to capture screenshot for test %s: %s", test_id.c_str(),
             screenshot_artifact.status().ToString().c_str());
  }
#else
  LOG_INFO(
      "TestManager",
      "Screenshot capture unavailable (YAZE_WITH_GRPC=OFF) for test %s",
      test_id.c_str());
#endif
  LOG_INFO("TestManager", "Widget state: %s", widget_state.c_str());
}

}  // namespace test
}  // namespace yaze
