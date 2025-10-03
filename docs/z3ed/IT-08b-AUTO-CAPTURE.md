# IT-08b: Auto-Capture on Test Failure - Implementation Guide

**Status**: 🔄 Ready to Implement  
**Priority**: High (Next Phase of IT-08)  
**Time Estimate**: 1-1.5 hours  
**Date**: October 2, 2025

---

## Overview

Automatically capture screenshots and execution context when tests fail, enabling better debugging and diagnostics for AI agents.

**Goal**: Every failed test produces:
- Screenshot of GUI state at failure
- Execution context (frame count, active windows, focused widgets)
- Foundation for IT-08c (widget state dumps)

---

## Implementation Steps

### Step 1: Update TestHistory Structure (15 minutes)

**File**: `src/app/core/test_manager.h`

Add failure diagnostics fields:

```cpp
struct TestHistory {
  std::string test_id;
  std::string test_name;
  ImGuiTestStatus status;
  absl::Time start_time;
  absl::Time end_time;
  int64_t execution_time_ms;
  std::vector<std::string> logs;
  std::map<std::string, std::string> metrics;
  
  // IT-08b: Failure diagnostics
  std::string screenshot_path;
  int64_t screenshot_size_bytes = 0;
  std::string failure_context;
  
  // IT-08c: Widget state (future)
  std::string widget_state;
};
```

### Step 2: Add CaptureFailureContext Method (30 minutes)

**File**: `src/app/core/test_manager.cc`

Add new method after `MarkHarnessTestCompleted`:

```cpp
void TestManager::CaptureFailureContext(const std::string& test_id) {
  if (test_history_.find(test_id) == test_history_.end()) {
    return;
  }
  
  auto& history = test_history_[test_id];
  
  // 1. Capture screenshot via harness service
  if (harness_service_) {
    std::string screenshot_path = 
        absl::StrFormat("/tmp/yaze_test_%s_failure.bmp", test_id);
    
    ScreenshotRequest req;
    req.set_output_path(screenshot_path);
    
    ScreenshotResponse resp;
    auto status = harness_service_->Screenshot(&req, &resp);
    
    if (status.ok() && resp.success()) {
      history.screenshot_path = resp.file_path();
      history.screenshot_size_bytes = resp.file_size_bytes();
    } else {
      YAZE_LOG(ERROR) << "Failed to capture screenshot for " << test_id 
                      << ": " << status.message();
    }
  }
  
  // 2. Capture execution context
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    ImGuiWindow* current_window = ImGui::GetCurrentWindow();
    std::string window_name = current_window ? current_window->Name : "none";
    
    ImGuiID active_id = ImGui::GetActiveID();
    ImGuiID hovered_id = ImGui::GetHoveredID();
    
    history.failure_context = absl::StrFormat(
        "Frame: %d, Window: %s, Active: %u, Hovered: %u",
        ImGui::GetFrameCount(),
        window_name,
        active_id,
        hovered_id);
  }
  
  // 3. Widget state capture (IT-08c - placeholder)
  // history.widget_state = CaptureWidgetState();
}
```

### Step 3: Integrate with MarkHarnessTestCompleted (15 minutes)

**File**: `src/app/core/test_manager.cc`

Modify existing method to call CaptureFailureContext:

```cpp
void TestManager::MarkHarnessTestCompleted(const std::string& test_id,
                                           ImGuiTestStatus status) {
  if (test_history_.find(test_id) == test_history_.end()) {
    return;
  }
  
  auto& history = test_history_[test_id];
  history.status = status;
  history.end_time = absl::Now();
  history.execution_time_ms = absl::ToInt64Milliseconds(
      history.end_time - history.start_time);
  
  // Auto-capture diagnostics on failure
  if (status == ImGuiTestStatus_Error || status == ImGuiTestStatus_Warning) {
    CaptureFailureContext(test_id);
  }
  
  // Notify waiting threads
  cv_.notify_all();
}
```

### Step 4: Update GetTestResults RPC (30 minutes)

**File**: `src/app/core/proto/imgui_test_harness.proto`

Add fields to response:

```proto
message GetTestResultsResponse {
  string test_id = 1;
  TestStatus status = 2;
  int64 execution_time_ms = 3;
  repeated string logs = 4;
  map<string, string> metrics = 5;
  
  // IT-08b: Failure diagnostics
  string screenshot_path = 6;
  int64 screenshot_size_bytes = 7;
  string failure_context = 8;
  
  // IT-08c: Widget state (future)
  string widget_state = 9;
}
```

**File**: `src/app/core/service/imgui_test_harness_service.cc`

Update implementation:

```cpp
absl::Status ImGuiTestHarnessServiceImpl::GetTestResults(
    const GetTestResultsRequest* request,
    GetTestResultsResponse* response) {
  
  const std::string& test_id = request->test_id();
  auto history = test_manager_->GetTestHistory(test_id);
  
  if (!history.has_value()) {
    return absl::NotFoundError(
        absl::StrFormat("Test not found: %s", test_id));
  }
  
  const auto& h = history.value();
  
  // Basic info
  response->set_test_id(h.test_id);
  response->set_status(ConvertImGuiTestStatusToProto(h.status));
  response->set_execution_time_ms(h.execution_time_ms);
  
  // Logs and metrics
  for (const auto& log : h.logs) {
    response->add_logs(log);
  }
  for (const auto& [key, value] : h.metrics) {
    (*response->mutable_metrics())[key] = value;
  }
  
  // IT-08b: Failure diagnostics
  if (!h.screenshot_path.empty()) {
    response->set_screenshot_path(h.screenshot_path);
    response->set_screenshot_size_bytes(h.screenshot_size_bytes);
  }
  if (!h.failure_context.empty()) {
    response->set_failure_context(h.failure_context);
  }
  
  // IT-08c: Widget state (future)
  if (!h.widget_state.empty()) {
    response->set_widget_state(h.widget_state);
  }
  
  return absl::OkStatus();
}
```

---

## Testing

### Build and Start Test Harness

```bash
# 1. Rebuild with changes
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)

# 2. Start test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

### Trigger Test Failure

```bash
# 3. Trigger a failing test (nonexistent widget)
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"nonexistent_widget","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Response should indicate failure
```

### Verify Screenshot Captured

```bash
# 4. Check for auto-captured screenshot
ls -lh /tmp/yaze_test_*_failure.bmp

# Expected: BMP file created (5.3MB)
```

### Query Test Results

```bash
# 5. Get test results (replace <test_id> with actual ID from Click response)
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"test_id":"<test_id>"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/GetTestResults

# Expected output:
{
  "testId": "grpc_click_12345678",
  "status": "FAILED",
  "executionTimeMs": "1234",
  "logs": [...],
  "screenshotPath": "/tmp/yaze_test_grpc_click_12345678_failure.bmp",
  "screenshotSizeBytes": "5308538",
  "failureContext": "Frame: 1234, Window: Main Window, Active: 0, Hovered: 0"
}
```

### End-to-End Test Script

Create `scripts/test_auto_capture.sh`:

```bash
#!/bin/bash
set -e

echo "=== IT-08b Auto-Capture Test ==="

# Clean up old screenshots
rm -f /tmp/yaze_test_*_failure.bmp

# Start YAZE with test harness
echo "Starting YAZE..."
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
YAZE_PID=$!

# Wait for server to start
sleep 3

# Trigger failing test
echo "Triggering test failure..."
TEST_ID=$(grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"nonexistent_widget","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click | \
  jq -r '.testId')

echo "Test ID: $TEST_ID"

# Wait for test to complete
sleep 2

# Check screenshot captured
if [ -f "/tmp/yaze_test_${TEST_ID}_failure.bmp" ]; then
  echo "✅ Screenshot captured: /tmp/yaze_test_${TEST_ID}_failure.bmp"
else
  echo "❌ Screenshot NOT captured"
  kill $YAZE_PID
  exit 1
fi

# Query test results
echo "Querying test results..."
RESULTS=$(grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d "{\"test_id\":\"$TEST_ID\"}" \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/GetTestResults)

echo "$RESULTS"

# Verify fields present
if echo "$RESULTS" | jq -e '.screenshotPath' > /dev/null; then
  echo "✅ Screenshot path in results"
else
  echo "❌ Screenshot path missing"
  kill $YAZE_PID
  exit 1
fi

if echo "$RESULTS" | jq -e '.failureContext' > /dev/null; then
  echo "✅ Failure context in results"
else
  echo "❌ Failure context missing"
  kill $YAZE_PID
  exit 1
fi

echo "=== All tests passed! ==="

# Cleanup
kill $YAZE_PID
```

---

## Success Criteria

- ✅ Screenshots auto-captured on test failure (Error or Warning status)
- ✅ Screenshot path stored in TestHistory
- ✅ Failure context captured (frame, window, widgets)
- ✅ GetTestResults RPC returns screenshot_path and failure_context
- ✅ No performance impact on passing tests (capture only on failure)
- ✅ Clean error handling if screenshot capture fails

---

## Files Modified

1. `src/app/core/test_manager.h` - TestHistory structure
2. `src/app/core/test_manager.cc` - CaptureFailureContext method
3. `src/app/core/proto/imgui_test_harness.proto` - GetTestResultsResponse fields
4. `src/app/core/service/imgui_test_harness_service.cc` - GetTestResults implementation

---

## Next Steps

**After IT-08b Complete**:
1. IT-08c: Widget State Dumps (30-45 minutes)
2. IT-08d: Error Envelope Standardization (1-2 hours)
3. IT-08e: CLI Error Improvements (1 hour)

**Documentation Updates**:
1. Update `IT-08-IMPLEMENTATION-GUIDE.md` with IT-08b complete status
2. Update `E6-z3ed-implementation-plan.md` progress tracking
3. Update `README.md` with new capabilities

---

**Last Updated**: October 2, 2025  
**Status**: Ready to implement  
**Estimated Completion**: October 2-3, 2025 (1-1.5 hours)
