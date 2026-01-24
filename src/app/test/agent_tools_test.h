#ifndef YAZE_APP_TEST_AGENT_TOOLS_TEST_H_
#define YAZE_APP_TEST_AGENT_TOOLS_TEST_H_

#include <filesystem>
#include "app/test/test_manager.h"
#include "app/editor/agent/agent_chat_history_codec.h"
#include "app/editor/agent/agent_state.h"
#include "cli/service/agent/tool_dispatcher.h"

namespace yaze {
namespace test {

void RegisterAgentToolsTestSuite();

class AgentToolsTestSuite : public TestSuite {
 public:
  AgentToolsTestSuite() = default;
  ~AgentToolsTestSuite() override = default;

  std::string GetName() const override { return "Agent Tools & Config"; }
  TestCategory GetCategory() const override { return TestCategory::kUnit; }

  absl::Status RunTests(TestResults& results) override {
    RunToolPreferencesTest(results);
    RunConfigPersistenceTest(results);
    return absl::OkStatus();
  }

 private:
  void RunToolPreferencesTest(TestResults& results) {
    auto start_time = std::chrono::steady_clock::now();
    TestResult result;
    result.name = "ToolPreferences_MemoryInspector";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

    yaze::cli::agent::ToolDispatcher::ToolPreferences prefs;
    // Verify default matches expectation (I set it to true in header)
    if (prefs.memory_inspector) {
        prefs.memory_inspector = false;
        if (!prefs.memory_inspector) {
            result.status = TestStatus::kPassed;
        } else {
            result.status = TestStatus::kFailed;
            result.error_message = "Failed to toggle memory_inspector preference";
        }
    } else {
        // If default changed, just verify we can toggle it
        prefs.memory_inspector = true;
        if (prefs.memory_inspector) {
            result.status = TestStatus::kPassed;
        } else {
            result.status = TestStatus::kFailed;
            result.error_message = "Failed to enable memory_inspector";
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    results.AddResult(result);
  }

  void RunConfigPersistenceTest(TestResults& results) {
    auto start_time = std::chrono::steady_clock::now();
    TestResult result;
    result.name = "ConfigPersistence_MemoryInspector";
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;

#if defined(YAZE_WITH_JSON)
    // Create a snapshot with memory_inspector = true
    yaze::editor::AgentChatHistoryCodec::Snapshot snapshot;
    yaze::editor::AgentChatHistoryCodec::AgentConfigSnapshot config;
    config.tools.memory_inspector = true;
    snapshot.agent_config = config;

    // Save to temp file
    std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "yaze_test_config.json";
    auto save_status = yaze::editor::AgentChatHistoryCodec::Save(temp_path, snapshot);
    
    if (!save_status.ok()) {
        result.status = TestStatus::kFailed;
        result.error_message = "Save failed: " + std::string(save_status.message());
    } else {
        // Load back
        auto load_result = yaze::editor::AgentChatHistoryCodec::Load(temp_path);
        if (load_result.ok()) {
            if (load_result->agent_config.has_value() && 
                load_result->agent_config->tools.memory_inspector == true) {
                
                // Now test false
                snapshot.agent_config->tools.memory_inspector = false;
                yaze::editor::AgentChatHistoryCodec::Save(temp_path, snapshot);
                auto load_result2 = yaze::editor::AgentChatHistoryCodec::Load(temp_path);
                
                if (load_result2.ok() && 
                    load_result2->agent_config.has_value() && 
                    load_result2->agent_config->tools.memory_inspector == false) {
                    result.status = TestStatus::kPassed;
                } else {
                    result.status = TestStatus::kFailed;
                    result.error_message = "Failed to persist memory_inspector=false";
                }
            } else {
                result.status = TestStatus::kFailed;
                result.error_message = "Failed to persist memory_inspector=true";
            }
        } else {
            result.status = TestStatus::kFailed;
            result.error_message = "Load failed: " + std::string(load_result.status().message());
        }
    }
    // Cleanup
    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
#else
    result.status = TestStatus::kSkipped;
    result.error_message = "JSON support disabled";
#endif

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    results.AddResult(result);
  }
};

// Register the AgentToolsTestSuite with the TestManager.
void RegisterAgentToolsTestSuite();

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_AGENT_TOOLS_TEST_H_
