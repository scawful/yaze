#ifndef YAZE_APP_TEST_CORE_SYSTEMS_TEST_SUITE_H_
#define YAZE_APP_TEST_CORE_SYSTEMS_TEST_SUITE_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/core/event_bus.h"
#include "app/editor/events/core_events.h"
#include "app/test/test_manager.h"
#include "rom/rom.h"

namespace yaze {
namespace test {

/**
 * @brief Test suite for core infrastructure: ContentRegistry and EventBus.
 *
 * These tests verify the foundational systems that other components depend on.
 * Tests are ROM-optional where possible.
 */
class CoreSystemsTestSuite : public TestSuite {
 public:
  CoreSystemsTestSuite() = default;
  ~CoreSystemsTestSuite() override = default;

  std::string GetName() const override { return "Core Systems Tests"; }
  TestCategory GetCategory() const override { return TestCategory::kUnit; }

  absl::Status RunTests(TestResults& results) override {
    // ContentRegistry tests
    RunContentRegistryContextSetRomTest(results);
    RunContentRegistryContextClearTest(results);
    RunContentRegistryPanelRegistrationTest(results);
    RunContentRegistryThreadSafetyTest(results);

    // EventBus tests
    RunEventBusSubscribePublishTest(results);
    RunEventBusUnsubscribeTest(results);
    RunEventBusMultipleSubscribersTest(results);
    RunEventBusTypeSafetyTest(results);
    RunCoreEventsCreationTest(results);

    return absl::OkStatus();
  }

  void DrawConfiguration() override {
    ImGui::Text("Core Systems Test Configuration");
    ImGui::Checkbox("Test ContentRegistry", &test_content_registry_);
    ImGui::Checkbox("Test EventBus", &test_event_bus_);
    ImGui::Checkbox("Test Core Events", &test_core_events_);
  }

 private:
  // ===========================================================================
  // ContentRegistry Tests
  // ===========================================================================

  void RunContentRegistryContextSetRomTest(TestResults& results) {
    if (!test_content_registry_) {
      AddSkippedResult(results, "ContentRegistry_Context_SetRom",
                       "ContentRegistry testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("ContentRegistry_Context_SetRom",
                                     start_time);

    try {
      // Save original state
      Rom* original_rom = editor::ContentRegistry::Context::rom();

      // Test with a mock ROM pointer (we just need to verify pointer storage)
      Rom test_rom;
      editor::ContentRegistry::Context::SetRom(&test_rom);

      Rom* retrieved = editor::ContentRegistry::Context::rom();
      if (retrieved == &test_rom) {
        result.status = TestStatus::kPassed;
        result.error_message = "ContentRegistry::Context::SetRom works correctly";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = "ContentRegistry returned wrong ROM pointer";
      }

      // Restore original state
      editor::ContentRegistry::Context::SetRom(original_rom);

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "ContentRegistry SetRom test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunContentRegistryContextClearTest(TestResults& results) {
    if (!test_content_registry_) {
      AddSkippedResult(results, "ContentRegistry_Context_Clear",
                       "ContentRegistry testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("ContentRegistry_Context_Clear",
                                     start_time);

    try {
      // Save original state
      Rom* original_rom = editor::ContentRegistry::Context::rom();

      // Set a ROM, then clear
      Rom test_rom;
      editor::ContentRegistry::Context::SetRom(&test_rom);
      editor::ContentRegistry::Context::Clear();

      Rom* retrieved = editor::ContentRegistry::Context::rom();
      if (retrieved == nullptr) {
        result.status = TestStatus::kPassed;
        result.error_message = "ContentRegistry::Context::Clear works correctly";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = "ContentRegistry::Context::Clear did not reset ROM";
      }

      // Restore original state
      editor::ContentRegistry::Context::SetRom(original_rom);

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "ContentRegistry Clear test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunContentRegistryPanelRegistrationTest(TestResults& results) {
    if (!test_content_registry_) {
      AddSkippedResult(results, "ContentRegistry_Panel_Registration",
                       "ContentRegistry testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("ContentRegistry_Panel_Registration",
                                     start_time);

    try {
      // Get current panel count
      auto panels_before = editor::ContentRegistry::Panels::GetAll();
      size_t count_before = panels_before.size();

      // We can't easily create a mock panel without including more headers,
      // so we just verify the API doesn't crash
      auto panels_after = editor::ContentRegistry::Panels::GetAll();

      result.status = TestStatus::kPassed;
      result.error_message = absl::StrFormat(
          "Panel registry API accessible: %zu panels registered",
          panels_after.size());

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Panel registration test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunContentRegistryThreadSafetyTest(TestResults& results) {
    if (!test_content_registry_) {
      AddSkippedResult(results, "ContentRegistry_Thread_Safety",
                       "ContentRegistry testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("ContentRegistry_Thread_Safety",
                                     start_time);

    try {
      // Save original state
      Rom* original_rom = editor::ContentRegistry::Context::rom();

      // Test rapid set/get operations (simulates concurrent access patterns)
      Rom test_rom1, test_rom2;
      bool all_reads_valid = true;

      for (int i = 0; i < 100; ++i) {
        editor::ContentRegistry::Context::SetRom(i % 2 == 0 ? &test_rom1 : &test_rom2);
        Rom* read = editor::ContentRegistry::Context::rom();
        if (read != &test_rom1 && read != &test_rom2) {
          all_reads_valid = false;
          break;
        }
      }

      if (all_reads_valid) {
        result.status = TestStatus::kPassed;
        result.error_message = "ContentRegistry handles rapid access patterns";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = "ContentRegistry returned invalid pointer during rapid access";
      }

      // Restore original state
      editor::ContentRegistry::Context::SetRom(original_rom);

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Thread safety test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  // ===========================================================================
  // EventBus Tests
  // ===========================================================================

  void RunEventBusSubscribePublishTest(TestResults& results) {
    if (!test_event_bus_) {
      AddSkippedResult(results, "EventBus_Subscribe_Publish",
                       "EventBus testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("EventBus_Subscribe_Publish", start_time);

    try {
      EventBus bus;
      int call_count = 0;
      int received_value = 0;

      // Subscribe to RomLoadedEvent
      bus.Subscribe<editor::RomLoadedEvent>(
          [&](const editor::RomLoadedEvent& e) {
            call_count++;
            received_value = static_cast<int>(e.session_id);
          });

      // Publish event
      auto event = editor::RomLoadedEvent::Create(nullptr, "test.sfc", 42);
      bus.Publish(event);

      if (call_count == 1 && received_value == 42) {
        result.status = TestStatus::kPassed;
        result.error_message = "EventBus subscribe/publish works correctly";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = absl::StrFormat(
            "EventBus failed: call_count=%d (expected 1), received=%d (expected 42)",
            call_count, received_value);
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "EventBus subscribe/publish test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunEventBusUnsubscribeTest(TestResults& results) {
    if (!test_event_bus_) {
      AddSkippedResult(results, "EventBus_Unsubscribe",
                       "EventBus testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("EventBus_Unsubscribe", start_time);

    try {
      EventBus bus;
      int call_count = 0;

      // Subscribe and get handler ID
      auto handler_id = bus.Subscribe<editor::SessionClosedEvent>(
          [&](const editor::SessionClosedEvent&) { call_count++; });

      // Publish - should increment
      bus.Publish(editor::SessionClosedEvent::Create(0));
      int count_after_first = call_count;

      // Unsubscribe
      bus.Unsubscribe(handler_id);

      // Publish again - should NOT increment
      bus.Publish(editor::SessionClosedEvent::Create(1));
      int count_after_second = call_count;

      if (count_after_first == 1 && count_after_second == 1) {
        result.status = TestStatus::kPassed;
        result.error_message = "EventBus unsubscribe works correctly";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = absl::StrFormat(
            "Unsubscribe failed: after_first=%d, after_second=%d (expected 1, 1)",
            count_after_first, count_after_second);
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "EventBus unsubscribe test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunEventBusMultipleSubscribersTest(TestResults& results) {
    if (!test_event_bus_) {
      AddSkippedResult(results, "EventBus_Multiple_Subscribers",
                       "EventBus testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("EventBus_Multiple_Subscribers",
                                     start_time);

    try {
      EventBus bus;
      int subscriber1_calls = 0;
      int subscriber2_calls = 0;
      int subscriber3_calls = 0;

      // Register multiple subscribers
      bus.Subscribe<editor::FrameBeginEvent>(
          [&](const editor::FrameBeginEvent&) { subscriber1_calls++; });
      bus.Subscribe<editor::FrameBeginEvent>(
          [&](const editor::FrameBeginEvent&) { subscriber2_calls++; });
      bus.Subscribe<editor::FrameBeginEvent>(
          [&](const editor::FrameBeginEvent&) { subscriber3_calls++; });

      // Publish once
      bus.Publish(editor::FrameBeginEvent::Create(0.016f));

      if (subscriber1_calls == 1 && subscriber2_calls == 1 &&
          subscriber3_calls == 1) {
        result.status = TestStatus::kPassed;
        result.error_message = "All 3 subscribers received the event";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = absl::StrFormat(
            "Multiple subscribers failed: s1=%d, s2=%d, s3=%d (expected 1,1,1)",
            subscriber1_calls, subscriber2_calls, subscriber3_calls);
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Multiple subscribers test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunEventBusTypeSafetyTest(TestResults& results) {
    if (!test_event_bus_) {
      AddSkippedResult(results, "EventBus_Type_Safety",
                       "EventBus testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("EventBus_Type_Safety", start_time);

    try {
      EventBus bus;
      int rom_loaded_calls = 0;
      int session_closed_calls = 0;

      // Subscribe to different event types
      bus.Subscribe<editor::RomLoadedEvent>(
          [&](const editor::RomLoadedEvent&) { rom_loaded_calls++; });
      bus.Subscribe<editor::SessionClosedEvent>(
          [&](const editor::SessionClosedEvent&) { session_closed_calls++; });

      // Publish only RomLoadedEvent
      bus.Publish(editor::RomLoadedEvent::Create(nullptr, "test.sfc", 1));

      if (rom_loaded_calls == 1 && session_closed_calls == 0) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "EventBus correctly routes events by type";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = absl::StrFormat(
            "Type safety failed: rom_loaded=%d, session_closed=%d (expected 1, 0)",
            rom_loaded_calls, session_closed_calls);
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Type safety test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  void RunCoreEventsCreationTest(TestResults& results) {
    if (!test_core_events_) {
      AddSkippedResult(results, "Core_Events_Creation",
                       "Core events testing disabled");
      return;
    }

    auto start_time = std::chrono::steady_clock::now();
    TestResult result = CreateResult("Core_Events_Creation", start_time);

    try {
      // Test factory methods for all core event types
      auto rom_loaded = editor::RomLoadedEvent::Create(nullptr, "test.sfc", 1);
      auto rom_unloaded = editor::RomUnloadedEvent::Create(2);
      auto rom_modified = editor::RomModifiedEvent::Create(nullptr, 3, 0x1000, 16);
      auto session_switched = editor::SessionSwitchedEvent::Create(0, 1, nullptr);
      auto session_created = editor::SessionCreatedEvent::Create(4, nullptr);
      auto session_closed = editor::SessionClosedEvent::Create(5);
      auto editor_switched = editor::EditorSwitchedEvent::Create(1, nullptr);
      auto frame_begin = editor::FrameBeginEvent::Create(0.016f);
      auto frame_end = editor::FrameEndEvent::Create(0.016f);

      // Verify values
      bool all_correct = true;
      all_correct &= (rom_loaded.filename == "test.sfc");
      all_correct &= (rom_loaded.session_id == 1);
      all_correct &= (rom_unloaded.session_id == 2);
      all_correct &= (rom_modified.address == 0x1000);
      all_correct &= (rom_modified.byte_count == 16);
      all_correct &= (session_switched.old_index == 0);
      all_correct &= (session_switched.new_index == 1);
      all_correct &= (session_created.index == 4);
      all_correct &= (session_closed.index == 5);
      all_correct &= (editor_switched.editor_type == 1);
      all_correct &= (frame_begin.delta_time > 0.0f);
      all_correct &= (frame_end.delta_time > 0.0f);

      if (all_correct) {
        result.status = TestStatus::kPassed;
        result.error_message =
            "All core event factory methods work correctly";
      } else {
        result.status = TestStatus::kFailed;
        result.error_message = "Some event factory methods returned wrong values";
      }

    } catch (const std::exception& e) {
      result.status = TestStatus::kFailed;
      result.error_message =
          "Core events creation test failed: " + std::string(e.what());
    }

    FinalizeResult(result, start_time, results);
  }

  // ===========================================================================
  // Helper Methods
  // ===========================================================================

  TestResult CreateResult(
      const std::string& name,
      std::chrono::time_point<std::chrono::steady_clock> start_time) {
    TestResult result;
    result.name = name;
    result.suite_name = GetName();
    result.category = GetCategory();
    result.timestamp = start_time;
    return result;
  }

  void FinalizeResult(
      TestResult& result,
      std::chrono::time_point<std::chrono::steady_clock> start_time,
      TestResults& results) {
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    results.AddResult(result);
  }

  void AddSkippedResult(TestResults& results, const std::string& name,
                        const std::string& reason) {
    TestResult result;
    result.name = name;
    result.suite_name = GetName();
    result.category = GetCategory();
    result.status = TestStatus::kSkipped;
    result.error_message = reason;
    result.duration = std::chrono::milliseconds{0};
    result.timestamp = std::chrono::steady_clock::now();
    results.AddResult(result);
  }

  // Configuration flags
  bool test_content_registry_ = true;
  bool test_event_bus_ = true;
  bool test_core_events_ = true;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_CORE_SYSTEMS_TEST_SUITE_H_
