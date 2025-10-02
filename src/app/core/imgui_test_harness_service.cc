#include "app/core/imgui_test_harness_service.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include <iostream>
#include <thread>

#include "absl/strings/str_format.h"
#include "app/core/proto/imgui_test_harness.grpc.pb.h"
#include "app/core/proto/imgui_test_harness.pb.h"
#include "app/test/test_manager.h"
#include "yaze.h"  // For YAZE_VERSION_STRING

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

// Helper to register and run a test dynamically
namespace {
struct DynamicTestData {
  std::function<void(ImGuiTestContext*)> test_func;
};

void RunDynamicTest(ImGuiTestContext* ctx) {
  auto* data = (DynamicTestData*)ctx->Test->UserData;
  if (data && data->test_func) {
    data->test_func(ctx);
  }
}

// Helper to check if a test has completed (not queued or running)
bool IsTestCompleted(ImGuiTest* test) {
  return test->Output.Status != ImGuiTestStatus_Queued &&
         test->Output.Status != ImGuiTestStatus_Running;
}

// Thread-safe state for Wait RPC communication
struct WaitState {
  std::atomic<bool> condition_met{false};
  std::mutex message_mutex;
  std::string message;
  
  void SetMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(message_mutex);
    message = msg;
  }
  
  std::string GetMessage() {
    std::lock_guard<std::mutex> lock(message_mutex);
    return message;
  }
};

// Thread-safe state for Assert RPC communication
struct AssertState {
  std::atomic<bool> assertion_passed{false};
  std::mutex data_mutex;
  std::string message;
  std::string actual_value;
  std::string expected_value;
  
  void SetResult(bool passed, const std::string& msg, 
                 const std::string& actual, const std::string& expected) {
    std::lock_guard<std::mutex> lock(data_mutex);
    assertion_passed.store(passed);
    message = msg;
    actual_value = actual;
    expected_value = expected;
  }
  
  void GetResult(bool& passed, std::string& msg, 
                 std::string& actual, std::string& expected) {
    std::lock_guard<std::mutex> lock(data_mutex);
    passed = assertion_passed.load();
    msg = message;
    actual = actual_value;
    expected = expected_value;
  }
};

}  // namespace
#endif

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

namespace yaze {
namespace test {

// gRPC service wrapper that forwards to our implementation
class ImGuiTestHarnessServiceGrpc final : public ImGuiTestHarness::Service {
 public:
  explicit ImGuiTestHarnessServiceGrpc(ImGuiTestHarnessServiceImpl* impl)
      : impl_(impl) {}

  grpc::Status Ping(grpc::ServerContext* context, const PingRequest* request,
                    PingResponse* response) override {
    auto status = impl_->Ping(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Click(grpc::ServerContext* context, const ClickRequest* request,
                     ClickResponse* response) override {
    auto status = impl_->Click(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Type(grpc::ServerContext* context, const TypeRequest* request,
                    TypeResponse* response) override {
    auto status = impl_->Type(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Wait(grpc::ServerContext* context, const WaitRequest* request,
                    WaitResponse* response) override {
    auto status = impl_->Wait(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Assert(grpc::ServerContext* context,
                      const AssertRequest* request,
                      AssertResponse* response) override {
    auto status = impl_->Assert(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

  grpc::Status Screenshot(grpc::ServerContext* context,
                          const ScreenshotRequest* request,
                          ScreenshotResponse* response) override {
    auto status = impl_->Screenshot(request, response);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }
    return grpc::Status::OK;
  }

 private:
  ImGuiTestHarnessServiceImpl* impl_;
};

// ============================================================================
// ImGuiTestHarnessServiceImpl - RPC Handlers
// ============================================================================

absl::Status ImGuiTestHarnessServiceImpl::Ping(const PingRequest* request,
                                                PingResponse* response) {
  // Echo back the message with "Pong: " prefix
  response->set_message(absl::StrFormat("Pong: %s", request->message()));

  // Add current timestamp
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  response->set_timestamp_ms(ms.count());

  // Add YAZE version
  response->set_yaze_version(YAZE_VERSION_STRING);

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Click(const ClickRequest* request,
                                                 ClickResponse* response) {
  auto start = std::chrono::steady_clock::now();

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Validate test manager
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  // Get ImGuiTestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse target: "button:Open ROM" -> type=button, label="Open ROM"
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label' (e.g. 'button:Open ROM')");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);

  // Convert click type
  ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;
  switch (request->type()) {
    case ClickRequest::LEFT:
      mouse_button = ImGuiMouseButton_Left;
      break;
    case ClickRequest::RIGHT:
      mouse_button = ImGuiMouseButton_Right;
      break;
    case ClickRequest::MIDDLE:
      mouse_button = ImGuiMouseButton_Middle;
      break;
    case ClickRequest::DOUBLE:
      // Double click handled below
      break;
    default:
      break;
  }

  // Create a dynamic test to perform the click
  bool success = false;
  std::string message;
  
  auto test_data = std::make_shared<DynamicTestData>();
  test_data->test_func = [=, &success, &message](ImGuiTestContext* ctx) {
    try {
      if (request->type() == ClickRequest::DOUBLE) {
        ctx->ItemDoubleClick(widget_label.c_str());
      } else {
        ctx->ItemClick(widget_label.c_str(), mouse_button);
      }
      success = true;
      message = absl::StrFormat("Clicked %s '%s'", widget_type, widget_label);
    } catch (const std::exception& e) {
      success = false;
      message = absl::StrFormat("Click failed: %s", e.what());
    }
  };

  // Register the test
  std::string test_name = absl::StrFormat("grpc_click_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  
  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();
  
  // Queue test for async execution
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  
  // Poll for test completion (with timeout)
  auto timeout = std::chrono::seconds(5);
  auto wait_start = std::chrono::steady_clock::now();
  while (!IsTestCompleted(test)) {
    if (std::chrono::steady_clock::now() - wait_start > timeout) {
      success = false;
      message = "Test timeout - widget not found or unresponsive";
      break;
    }
    // Yield to allow ImGui event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Check final test status
  if (IsTestCompleted(test)) {
    if (test->Output.Status == ImGuiTestStatus_Success) {
      success = true;
    } else {
      success = false;
      if (message.empty()) {
        message = absl::StrFormat("Test failed with status: %d", 
                                  test->Output.Status);
      }
    }
  }
  
  // Note: Test cleanup will be handled by ImGuiTestEngine's FinishTests()
  // Do NOT call ImGuiTestEngine_UnregisterTest() here - it causes assertion failure

#else
  // ImGuiTestEngine not available - stub implementation
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  bool success = true;
  std::string message = absl::StrFormat("[STUB] Clicked %s '%s' (ImGuiTestEngine not available)", 
                                       widget_type, widget_label);
#endif

  // Calculate execution time
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Type(const TypeRequest* request,
                                                TypeResponse* response) {
  auto start = std::chrono::steady_clock::now();

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Validate test manager
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  // Get ImGuiTestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse target: "input:Filename" -> type=input, label="Filename"
  std::string target = request->target();
  size_t colon_pos = target.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label' (e.g. 'input:Filename')");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  std::string text = request->text();
  bool clear_first = request->clear_first();

  // Create a dynamic test to perform the typing
  bool success = false;
  std::string message;
  
  auto test_data = std::make_shared<DynamicTestData>();
  test_data->test_func = [=, &success, &message](ImGuiTestContext* ctx) {
    try {
      // Find the input field
      ImGuiTestItemInfo item = ctx->ItemInfo(widget_label.c_str());
      if (item.ID == 0) {
        success = false;
        message = absl::StrFormat("Input field '%s' not found", widget_label);
        return;
      }

      // Click to focus the input field first
      ctx->ItemClick(widget_label.c_str());
      
      // Clear existing text if requested
      if (clear_first) {
        // Select all (Ctrl+A or Cmd+A depending on platform)
        ctx->KeyPress(ImGuiMod_Shortcut | ImGuiKey_A);
        // Delete selected text
        ctx->KeyPress(ImGuiKey_Delete);
      }

      // Type the new text
      ctx->ItemInputValue(widget_label.c_str(), text.c_str());
      
      success = true;
      message = absl::StrFormat("Typed '%s' into %s '%s'%s", 
                               text, widget_type, widget_label,
                               clear_first ? " (cleared first)" : "");
    } catch (const std::exception& e) {
      success = false;
      message = absl::StrFormat("Type failed: %s", e.what());
    }
  };

  // Register the test
  std::string test_name = absl::StrFormat("grpc_type_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  
  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();
  
  // Queue test for async execution
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  
  // Poll for test completion (with timeout)
  auto timeout = std::chrono::seconds(5);
  auto wait_start = std::chrono::steady_clock::now();
  while (!IsTestCompleted(test)) {
    if (std::chrono::steady_clock::now() - wait_start > timeout) {
      success = false;
      message = "Test timeout - input field not found or unresponsive";
      break;
    }
    // Yield to allow ImGui event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Check final test status
  if (IsTestCompleted(test)) {
    if (test->Output.Status == ImGuiTestStatus_Success) {
      success = true;
    } else {
      success = false;
      if (message.empty()) {
        message = absl::StrFormat("Test failed with status: %d", 
                                  test->Output.Status);
      }
    }
  }
  
  // Note: Test cleanup will be handled by ImGuiTestEngine's FinishTests()
  // Do NOT call ImGuiTestEngine_UnregisterTest() here - it causes assertion failure

#else
  // ImGuiTestEngine not available - stub implementation
  bool success = true;
  std::string message = absl::StrFormat("[STUB] Typed '%s' into %s (ImGuiTestEngine not available)",
                                       request->text(), request->target());
#endif

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Wait(const WaitRequest* request,
                                                WaitResponse* response) {
  auto start = std::chrono::steady_clock::now();

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Validate test manager
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  // Get ImGuiTestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse condition: "window_visible:Overworld Editor" -> check if window is visible
  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid condition format. Use 'type:target' (e.g. 'window_visible:Overworld Editor')");
    return absl::OkStatus();
  }

  std::string condition_type = condition.substr(0, colon_pos);
  std::string condition_target = condition.substr(colon_pos + 1);
  
  int timeout_ms = request->timeout_ms() > 0 ? request->timeout_ms() : 5000; // Default 5s
  int poll_interval_ms = request->poll_interval_ms() > 0 ? request->poll_interval_ms() : 100; // Default 100ms

  // Create thread-safe shared state for communication
  auto wait_state = std::make_shared<WaitState>();
  
  auto test_data = std::make_shared<DynamicTestData>();
  test_data->test_func = [wait_state, condition_type, condition_target, 
                          timeout_ms, poll_interval_ms](ImGuiTestContext* ctx) {
    try {
      auto poll_start = std::chrono::steady_clock::now();
      auto timeout = std::chrono::milliseconds(timeout_ms);
      
      // Give ImGui one frame to process the menu click and create windows
      ctx->Yield();
      
      while (std::chrono::steady_clock::now() - poll_start < timeout) {
        bool current_state = false;

        // Check the condition type using thread-safe ctx methods
        if (condition_type == "window_visible") {
          // Use ctx->WindowInfo instead of ImGui::FindWindowByName for thread safety
          ImGuiTestItemInfo window_info = ctx->WindowInfo(condition_target.c_str(), 
                                                          ImGuiTestOpFlags_NoError);
          current_state = (window_info.ID != 0);
        } else if (condition_type == "element_visible") {
          ImGuiTestItemInfo item = ctx->ItemInfo(condition_target.c_str());
          current_state = (item.ID != 0 && item.RectClipped.GetWidth() > 0 && 
                          item.RectClipped.GetHeight() > 0);
        } else if (condition_type == "element_enabled") {
          ImGuiTestItemInfo item = ctx->ItemInfo(condition_target.c_str());
          current_state = (item.ID != 0 && !(item.ItemFlags & ImGuiItemFlags_Disabled));
        } else {
          wait_state->SetMessage(absl::StrFormat("Unknown condition type: %s", condition_type));
          wait_state->condition_met = false;
          return;
        }

        if (current_state) {
          wait_state->condition_met = true;
          wait_state->SetMessage(absl::StrFormat("Condition '%s:%s' met after %lld ms",
                                   condition_type, condition_target,
                                   std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - poll_start).count()));
          return;
        }

        // Sleep before next poll
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        ctx->Yield(); // Let ImGui process events
      }

      // Timeout reached
      wait_state->condition_met = false;
      wait_state->SetMessage(absl::StrFormat("Condition '%s:%s' not met after %d ms timeout",
                               condition_type, condition_target, timeout_ms));
    } catch (const std::exception& e) {
      wait_state->condition_met = false;
      wait_state->SetMessage(absl::StrFormat("Wait failed: %s", e.what()));
    }
  };

  // Register the test
  std::string test_name = absl::StrFormat("grpc_wait_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  
  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();
  
  // Queue test for async execution
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  
  // Poll for test completion (with extended timeout for the wait itself)
  auto extended_timeout = std::chrono::milliseconds(timeout_ms + 5000);
  auto wait_start = std::chrono::steady_clock::now();
  while (!IsTestCompleted(test)) {
    if (std::chrono::steady_clock::now() - wait_start > extended_timeout) {
      wait_state->condition_met = false;
      wait_state->SetMessage("Test execution timeout");
      break;
    }
    // Yield to allow ImGui event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Read final state from thread-safe shared state
  bool condition_met = wait_state->condition_met.load();
  std::string message = wait_state->GetMessage();
  
  // Check final test status
  if (IsTestCompleted(test)) {
    if (test->Output.Status == ImGuiTestStatus_Success) {
      // Status already set by test function
    } else {
      condition_met = false;
      if (message.empty()) {
        message = absl::StrFormat("Test failed with status: %d", 
                                  test->Output.Status);
      }
    }
  }
  
  // Note: Test cleanup will be handled by ImGuiTestEngine's FinishTests()
  // Do NOT call ImGuiTestEngine_UnregisterTest() here - it causes assertion failure

#else
  // ImGuiTestEngine not available - stub implementation
  bool condition_met = true;
  std::string message = absl::StrFormat("[STUB] Condition '%s' met (ImGuiTestEngine not available)",
                                       request->condition());
#endif

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(condition_met);
  response->set_message(message);
  response->set_elapsed_ms(elapsed.count());

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Assert(const AssertRequest* request,
                                                  AssertResponse* response) {
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  // Validate test manager
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    return absl::OkStatus();
  }

  // Get ImGuiTestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    return absl::OkStatus();
  }

  // Parse condition: "visible:Main Window" -> check if element is visible
  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');

  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid condition format. Use 'type:target' (e.g. 'visible:Main Window')");
    response->set_actual_value("N/A");
    response->set_expected_value("N/A");
    return absl::OkStatus();
  }

  std::string assertion_type = condition.substr(0, colon_pos);
  std::string assertion_target = condition.substr(colon_pos + 1);

  // Create thread-safe shared state for communication
  auto assert_state = std::make_shared<AssertState>();
  
  auto test_data = std::make_shared<DynamicTestData>();
  test_data->test_func = [assert_state, assertion_type, assertion_target](ImGuiTestContext* ctx) {
    try {
      bool passed = false;
      std::string msg, actual, expected;
      
      if (assertion_type == "visible") {
        // Check if window is visible
        ImGuiWindow* window = ImGui::FindWindowByName(assertion_target.c_str());
        bool is_visible = (window != nullptr && !window->Hidden);
        
        passed = is_visible;
        actual = is_visible ? "visible" : "hidden";
        expected = "visible";
        msg = passed 
          ? absl::StrFormat("'%s' is visible", assertion_target)
          : absl::StrFormat("'%s' is not visible", assertion_target);
          
      } else if (assertion_type == "enabled") {
        // Check if element is enabled
        ImGuiTestItemInfo item = ctx->ItemInfo(assertion_target.c_str());
        bool is_enabled = (item.ID != 0 && !(item.ItemFlags & ImGuiItemFlags_Disabled));
        
        passed = is_enabled;
        actual = is_enabled ? "enabled" : "disabled";
        expected = "enabled";
        msg = passed
          ? absl::StrFormat("'%s' is enabled", assertion_target)
          : absl::StrFormat("'%s' is not enabled", assertion_target);
          
      } else if (assertion_type == "exists") {
        // Check if element exists
        ImGuiTestItemInfo item = ctx->ItemInfo(assertion_target.c_str());
        bool exists = (item.ID != 0);
        
        passed = exists;
        actual = exists ? "exists" : "not found";
        expected = "exists";
        msg = passed
          ? absl::StrFormat("'%s' exists", assertion_target)
          : absl::StrFormat("'%s' not found", assertion_target);
          
      } else if (assertion_type == "text_contains") {
        // Check if text input contains expected text (requires expected_value in condition)
        // Format: "text_contains:MyInput:ExpectedText"
        size_t second_colon = assertion_target.find(':');
        if (second_colon == std::string::npos) {
          passed = false;
          msg = "text_contains requires format 'text_contains:target:expected_text'";
          actual = "N/A";
          expected = "N/A";
          assert_state->SetResult(passed, msg, actual, expected);
          return;
        }
        
        std::string input_target = assertion_target.substr(0, second_colon);
        std::string expected_text = assertion_target.substr(second_colon + 1);
        
        ImGuiTestItemInfo item = ctx->ItemInfo(input_target.c_str());
        if (item.ID != 0) {
          // Note: Text retrieval is simplified - actual implementation may need widget-specific handling
          std::string actual_text = "(text_retrieval_not_fully_implemented)";
          
          passed = (actual_text.find(expected_text) != std::string::npos);
          actual = actual_text;
          expected = absl::StrFormat("contains '%s'", expected_text);
          msg = passed
            ? absl::StrFormat("'%s' contains '%s'", input_target, expected_text)
            : absl::StrFormat("'%s' does not contain '%s' (actual: '%s')", 
                            input_target, expected_text, actual_text);
        } else {
          passed = false;
          msg = absl::StrFormat("Input '%s' not found", input_target);
          actual = "not found";
          expected = expected_text;
        }
        
      } else {
        passed = false;
        msg = absl::StrFormat("Unknown assertion type: %s", assertion_type);
        actual = "N/A";
        expected = "N/A";
      }
      
      // Store result in thread-safe state
      assert_state->SetResult(passed, msg, actual, expected);
      
    } catch (const std::exception& e) {
      assert_state->SetResult(false, 
                             absl::StrFormat("Assertion failed: %s", e.what()),
                             "exception", "N/A");
    }
  };

  // Register the test
  std::string test_name = absl::StrFormat("grpc_assert_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  
  ImGuiTest* test = IM_REGISTER_TEST(engine, "grpc", test_name.c_str());
  test->TestFunc = RunDynamicTest;
  test->UserData = test_data.get();
  
  // Queue test for async execution
  ImGuiTestEngine_QueueTest(engine, test, ImGuiTestRunFlags_RunFromGui);
  
  // Poll for test completion (with timeout)
  auto timeout = std::chrono::seconds(5);
  auto wait_start = std::chrono::steady_clock::now();
  while (!IsTestCompleted(test)) {
    if (std::chrono::steady_clock::now() - wait_start > timeout) {
      assert_state->SetResult(false, "Test timeout - assertion check timed out", 
                             "timeout", "N/A");
      break;
    }
    // Yield to allow ImGui event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Read final state from thread-safe shared state
  bool assertion_passed;
  std::string message, actual_value, expected_value;
  assert_state->GetResult(assertion_passed, message, actual_value, expected_value);
  
  // Check final test status
  if (IsTestCompleted(test)) {
    if (test->Output.Status == ImGuiTestStatus_Success) {
      // Status already set by test function
    } else {
      if (message.empty()) {
        assert_state->SetResult(false,
                               absl::StrFormat("Test failed with status: %d", 
                                              test->Output.Status),
                               "error", "N/A");
        assert_state->GetResult(assertion_passed, message, actual_value, expected_value);
      }
    }
  }
  
  // Note: Test cleanup will be handled by ImGuiTestEngine's FinishTests()
  // Do NOT call ImGuiTestEngine_UnregisterTest() here - it causes assertion failure

#else
  // ImGuiTestEngine not available - stub implementation
  bool assertion_passed = true;
  std::string message = absl::StrFormat("[STUB] Assertion '%s' passed (ImGuiTestEngine not available)",
                                       request->condition());
  std::string actual_value = "(stub)";
  std::string expected_value = "(stub)";
#endif

  response->set_success(assertion_passed);
  response->set_message(message);
  response->set_actual_value(actual_value);
  response->set_expected_value(expected_value);

  return absl::OkStatus();
}

absl::Status ImGuiTestHarnessServiceImpl::Screenshot(
    const ScreenshotRequest* request, ScreenshotResponse* response) {
  // TODO: Implement actual screenshot capture

  response->set_success(false);
  response->set_message("Screenshot not yet implemented");
  response->set_file_path("");
  response->set_file_size_bytes(0);

  return absl::OkStatus();
}

// ============================================================================
// ImGuiTestHarnessServer - Server Lifecycle
// ============================================================================

ImGuiTestHarnessServer& ImGuiTestHarnessServer::Instance() {
  static ImGuiTestHarnessServer* instance = new ImGuiTestHarnessServer();
  return *instance;
}

ImGuiTestHarnessServer::~ImGuiTestHarnessServer() {
  Shutdown();
}

absl::Status ImGuiTestHarnessServer::Start(int port, TestManager* test_manager) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  if (!test_manager) {
    return absl::InvalidArgumentError("TestManager cannot be null");
  }

  // Create the service implementation with TestManager reference
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>(test_manager);

  // Create the gRPC service wrapper (store as member to prevent it from going out of scope)
  grpc_service_ = std::make_unique<ImGuiTestHarnessServiceGrpc>(service_.get());

  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);

  grpc::ServerBuilder builder;

  // Listen on all interfaces (use 0.0.0.0 to avoid IPv6/IPv4 binding conflicts)
  builder.AddListeningPort(server_address,
                           grpc::InsecureServerCredentials());

  // Register service
  builder.RegisterService(grpc_service_.get());

  // Build and start
  server_ = builder.BuildAndStart();

  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start gRPC server on %s", server_address));
  }

  port_ = port;

  std::cout << "✓ ImGuiTestHarness gRPC server listening on " << server_address
            << " (with TestManager integration)\n";
  std::cout << "  Use 'grpcurl -plaintext -d '{\"message\":\"test\"}' "
            << server_address << " yaze.test.ImGuiTestHarness/Ping' to test\n";

  return absl::OkStatus();
}

void ImGuiTestHarnessServer::Shutdown() {
  if (server_) {
    std::cout << "⏹ Shutting down ImGuiTestHarness gRPC server...\n";
    server_->Shutdown();
    server_.reset();
    service_.reset();
    port_ = 0;
    std::cout << "✓ ImGuiTestHarness gRPC server stopped\n";
  }
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
