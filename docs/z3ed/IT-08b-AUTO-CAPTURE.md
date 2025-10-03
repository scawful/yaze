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
  
  # IT-08b: Auto-Capture on Test Failure

  **Status**: ✅ Complete  
  **Completed**: October 2, 2025  
  **Owner**: Harness Platform Team  
  **Depends On**: IT-08a (Screenshot RPC), IT-05 (execution history store)

  ---

  ## Summary

  Harness failures now emit rich diagnostics automatically. Whenever a GUI test
  transitions into `FAILED` or `TIMEOUT` we capture:

  - A full-frame SDL screenshot written to a stable per-test artifact folder
  - ImGui execution context (frame number, active/nav/hovered windows & IDs)
  - Serialized widget hierarchy snapshot (`CaptureWidgetState`) for IT-08c
  - Append-only log entries surfaced through `GetTestResults`

  All artifacts are exposed through both the gRPC API and the `z3ed agent test
  results` command (JSON/YAML), enabling AI agents and humans to retrieve the same
  diagnostics without extra RPC calls.

  ---

  ## What Shipped

  ### Shared Screenshot Helper
  - New helper (`screenshot_utils.{h,cc}`) centralizes SDL capture logic.
  - Generates deterministic default paths under
    `${TMPDIR}/yaze/test-results/<test_id>/failure_<timestamp>.bmp`.
  - Reused by the manual `Screenshot` RPC to avoid duplicate code.

  ### TestManager Auto-Capture Pipeline
  - `CaptureFailureContext` now:
    - Computes ImGui context metadata even when the test finishes on a worker
      thread.
    - Allocates artifact folders per test ID and requests a screenshot via the
      shared helper (guarded when gRPC is disabled).
    - Persists screenshot path, byte size, failure context, and widget state back
      into `HarnessTestExecution` while keeping aggregate caches in sync.
    - Emits structured harness logs for success/failure of the auto-capture.

  ### CLI & Client Updates
  - `GuiAutomationClient::GetTestResults` propagates new proto fields:
    `screenshot_path`, `screenshot_size_bytes`, `failure_context`, `widget_state`.
  - `z3ed agent test results` shows diagnostics in both human (YAML) and machine
    (JSON) modes, including `null` markers when artifacts are unavailable.
  - JSON output is now agent-ready: screenshot path + size enable downstream
    fetchers, failure context aids chain-of-thought prompts, widget state allows
    LLMs to reason about UI layout when debugging.

  ### Build Integration
  - gRPC build stanza now compiles the new helper files so both harness server and
    in-process capture use the same implementation.

  ---

  ## Developer Notes

  | Concern | Resolution |
  |---------|------------|
  | Deadlocks while capturing | Screenshot helper runs outside `harness_history_mutex_`; mutex is reacquired only for bookkeeping. |
  | Non-gRPC builds | Auto-capture logs a descriptive "unavailable" message and skips the SDL call, keeping deterministic behaviour when harness is stubbed. |
  | Artifact collisions | Paths are timestamped and namespaced per test ID; directories are created idempotently with error-code handling. |
  | Large widget dumps | Stored as JSON strings; CLI wraps them with quoting so they can be piped to `jq`/`yq` safely. |

  ---

  ## Usage

  1. Trigger a harness failure (e.g. click a nonexistent widget):
     ```bash
     z3ed agent test --prompt "Click widget:nonexistent"
     ```
  2. Fetch diagnostics:
     ```bash
     z3ed agent test results --test-id grpc_click_deadbeef --include-logs --format json
     ```
  3. Inspect artifacts:
     ```bash
     open "$(jq -r '.screenshot_path' results.json)"
     ```

  Example YAML excerpt:

  ```yaml
  screenshot_path: "/var/folders/.../yaze/test-results/grpc_click_deadbeef/failure_1727890045123.bmp"
  screenshot_size_bytes: 5308538
  failure_context: "frame=1287 current_window=MainWindow nav_window=Agent hovered_window=Agent active_id=0x00000000 hovered_id=0x00000000"
  widget_state: '{"active_window":"MainWindow","visible_windows":["MainWindow","Agent"],"focused_widget":null}'
  ```

  ---

  ## Validation

  - Manual harness failure emits screenshot + widget dump under `/tmp`.
  - `GetTestResults` returns the new fields (verified via `grpcurl`).
  - CLI JSON/YAML output includes diagnostics with correct escaping.
  - Non-gRPC build path compiles (guarded sections).

  ---

  ## Follow-Up

  - IT-08c leverages the persisted widget JSON to produce HTML bundles.
  - IT-08d will standardize error envelopes across CLI/services using these
    diagnostics.
  - Investigate persisting artifacts under configurable directories
    (`--artifact-dir`) for CI separation.

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
