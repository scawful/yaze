# IT-08: Enhanced Error Reporting Implementation Guide

**Status**: IT-08a Complete ✅ | IT-08b In Progress 🔄 | IT-08c Planned 📋  
**Date**: October 2, 2025  
**Overall Progress**: 33% Complete (1 of 3 phases)

---

## Phase Overview

| Phase | Task | Status | Time | Description |
|-------|------|--------|------|-------------|
| IT-08a | Screenshot RPC | ✅ Complete | 1.5h | SDL-based screenshot capture |
| IT-08b | Auto-Capture on Failure | 🔄 Active | 1-1.5h | Integrate with TestManager |
| IT-08c | Widget State Dumps | 📋 Planned | 30-45m | Capture UI context on failure |
| IT-08d | Error Envelope Standardization | 📋 Planned | 1-2h | Unified error format across services |
| IT-08e | CLI Error Improvements | 📋 Planned | 1h | Rich error output with artifacts |

**Total Estimated Time**: 5-7 hours  
**Time Spent**: 1.5 hours  
**Time Remaining**: 3.5-5.5 hours

---

## IT-08a: Screenshot RPC ✅ COMPLETE

**Date Completed**: October 2, 2025  
**Time**: 1.5 hours

### Implementation Summary

### What Was Built

Implemented the `Screenshot` RPC in the ImGuiTestHarness service with the following capabilities:

1. **SDL Renderer Integration**: Accesses the ImGui SDL2 backend renderer through `BackendRendererUserData`
2. **Framebuffer Capture**: Uses `SDL_RenderReadPixels` to capture the full window contents (1536x864, 32-bit ARGB)
3. **BMP File Output**: Saves screenshots as BMP files using SDL's built-in `SDL_SaveBMP` function
4. **Flexible Paths**: Supports custom output paths or auto-generates timestamped filenames (`/tmp/yaze_screenshot_<timestamp>.bmp`)
5. **Response Metadata**: Returns file path, file size (bytes), and image dimensions

### Technical Implementation

**Location**: `/Users/scawful/Code/yaze/src/app/core/service/imgui_test_harness_service.cc`

```cpp
// Helper struct matching imgui_impl_sdlrenderer2.cpp backend data
struct ImGui_ImplSDLRenderer2_Data {
  SDL_Renderer* Renderer;
};

absl::Status ImGuiTestHarnessServiceImpl::Screenshot(
    const ScreenshotRequest* request, ScreenshotResponse* response) {
  // 1. Get SDL renderer from ImGui backend
  ImGuiIO& io = ImGui::GetIO();
  auto* backend_data = static_cast<ImGui_ImplSDLRenderer2_Data*>(io.BackendRendererUserData);
  
  if (!backend_data || !backend_data->Renderer) {
    response->set_success(false);
    response->set_message("SDL renderer not available");
    return absl::FailedPreconditionError("No SDL renderer available");
  }
  
  SDL_Renderer* renderer = backend_data->Renderer;
  
  // 2. Get renderer output size
  int width, height;
  SDL_GetRendererOutputSize(renderer, &width, &height);
  
  // 3. Create surface to hold screenshot
  SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                              0x00FF0000, 0x0000FF00,
                                              0x000000FF, 0xFF000000);
  
  // 4. Read pixels from renderer (ARGB8888 format)
  SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                      surface->pixels, surface->pitch);
  
  // 5. Determine output path (custom or auto-generated)
  std::string output_path = request->output_path();
  if (output_path.empty()) {
    output_path = absl::StrFormat("/tmp/yaze_screenshot_%lld.bmp",
                                  absl::ToUnixMillis(absl::Now()));
  }
  
  // 6. Save to BMP file
  SDL_SaveBMP(surface, output_path.c_str());
  
  // 7. Get file size and clean up
  std::ifstream file(output_path, std::ios::binary | std::ios::ate);
  int64_t file_size = file.tellg();
  
  SDL_FreeSurface(surface);
  
  // 8. Return success response
  response->set_success(true);
  response->set_message(absl::StrFormat("Screenshot saved to %s (%dx%d)",
                                        output_path, width, height));
  response->set_file_path(output_path);
  response->set_file_size_bytes(file_size);
  
  return absl::OkStatus();
}
```

### Testing Results

**Test Command**:
```bash
grpcurl -plaintext \
  -import-path /Users/scawful/Code/yaze/src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"output_path": "/tmp/test_screenshot.bmp"}' \
  localhost:50052 yaze.test.ImGuiTestHarness/Screenshot
```

**Response**:
```json
{
  "success": true,
  "message": "Screenshot saved to /tmp/test_screenshot.bmp (1536x864)",
  "filePath": "/tmp/test_screenshot.bmp",
  "fileSizeBytes": "5308538"
}
```

**File Verification**:
```bash
$ ls -lh /tmp/test_screenshot.bmp
-rw-r--r--  1 scawful  wheel   5.1M Oct  2 20:16 /tmp/test_screenshot.bmp

$ file /tmp/test_screenshot.bmp
/tmp/test_screenshot.bmp: PC bitmap, Windows 95/NT4 and newer format, 1536 x 864 x 32, cbSize 5308538, bits offset 122
```

✅ **Result**: Screenshot successfully captured, saved, and validated!

---

## Design Decisions

### Why BMP Format?

**Chosen**: SDL's built-in `SDL_SaveBMP` function  
**Rationale**:
- ✅ Zero external dependencies (no need for libpng, stb_image_write, etc.)
- ✅ Guaranteed to work on all platforms where SDL works
- ✅ Simple, reliable, and fast
- ✅ Adequate for debugging/error reporting (file size not critical)
- ⚠️ Larger file sizes (5.3MB vs ~500KB for PNG), but acceptable for temporary debug files

**Future Consideration**: If disk space becomes an issue, can add PNG encoding using stb_image_write (single-header library, easy to integrate)

### SDL Backend Integration

**Challenge**: How to access the SDL_Renderer from ImGui?  
**Solution**: 
- ImGui's `BackendRendererUserData` points to an `ImGui_ImplSDLRenderer2_Data` struct
- This struct contains the `Renderer` pointer as its first member
- Cast `BackendRendererUserData` to access the renderer safely

**Why Not Store Renderer Globally?**
- Multiple ImGui contexts could use different renderers
- Backend data pattern follows ImGui's architecture conventions
- More maintainable and future-proof

---

## Integration with Test System

### Current Usage (Manual RPC)

AI agents or CLI tools can manually capture screenshots:

```bash
# Capture screenshot after opening editor
z3ed agent test --prompt "Open Overworld Editor"
grpcurl ... yaze.test.ImGuiTestHarness/Screenshot
```

### Next Step: Auto-Capture on Failure

The screenshot RPC is now ready to be integrated with TestManager to automatically capture context when tests fail:

**Planned Implementation** (IT-08 Phase 2):
```cpp
// In TestManager::MarkHarnessTestCompleted()
if (test_result == IMGUI_TEST_STATUS_FAILED || 
    test_result == IMGUI_TEST_STATUS_TIMEOUT) {
  
  // Auto-capture screenshot
  ScreenshotRequest req;
  req.set_output_path(absl::StrFormat("/tmp/test_%s_failure.bmp", test_id));
  
  ScreenshotResponse resp;
  harness_service_->Screenshot(&req, &resp);
  
  test_history_[test_id].screenshot_path = resp.file_path();
  
  // Also capture widget state (IT-08 Phase 3)
  test_history_[test_id].widget_state = CaptureWidgetState();
}
```

---

---

## IT-08b: Auto-Capture on Test Failure 🔄 IN PROGRESS

**Goal**: Automatically capture screenshots and context when tests fail  
**Time Estimate**: 1-1.5 hours  
**Status**: Ready to implement

### Implementation Plan

#### Step 1: Modify TestManager (30 minutes)

**File**: `src/app/core/test_manager.cc`

Add screenshot capture in `MarkHarnessTestCompleted()`:

```cpp
void TestManager::MarkHarnessTestCompleted(const std::string& test_id,
                                           ImGuiTestStatus status) {
  auto& history_entry = test_history_[test_id];
  history_entry.status = status;
  history_entry.end_time = absl::Now();
  history_entry.execution_time_ms = absl::ToInt64Milliseconds(
      history_entry.end_time - history_entry.start_time);
  
  // Auto-capture screenshot on failure
  if (status == ImGuiTestStatus_Error || status == ImGuiTestStatus_Warning) {
    CaptureFailureContext(test_id);
  }
}

void TestManager::CaptureFailureContext(const std::string& test_id) {
  auto& history_entry = test_history_[test_id];
  
  // 1. Capture screenshot
  std::string screenshot_path = 
      absl::StrFormat("/tmp/yaze_test_%s_failure.bmp", test_id);
  
  if (harness_service_) {
    ScreenshotRequest req;
    req.set_output_path(screenshot_path);
    
    ScreenshotResponse resp;
    auto status = harness_service_->Screenshot(&req, &resp);
    
    if (status.ok()) {
      history_entry.screenshot_path = resp.file_path();
      history_entry.screenshot_size_bytes = resp.file_size_bytes();
    }
  }
  
  // 2. Capture widget state (IT-08c)
  // history_entry.widget_state = CaptureWidgetState();
  
  // 3. Capture execution context
  history_entry.failure_context = absl::StrFormat(
      "Frame: %d, Active Window: %s, Focused Widget: %s",
      ImGui::GetFrameCount(),
      ImGui::GetCurrentWindow() ? ImGui::GetCurrentWindow()->Name : "none",
      ImGui::GetActiveID());
}
```

#### Step 2: Update TestHistory Structure (15 minutes)

**File**: `src/app/core/test_manager.h`

Add failure context fields:

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
  std::string widget_state;  // IT-08c
};
```

#### Step 3: Update GetTestResults RPC (30 minutes)

**File**: `src/app/core/service/imgui_test_harness_service.cc`

Include screenshot path in results:

```cpp
absl::Status ImGuiTestHarnessServiceImpl::GetTestResults(
    const GetTestResultsRequest* request,
    GetTestResultsResponse* response) {
  
  const auto& history = test_manager_->GetTestHistory(request->test_id());
  
  // ... existing result population ...
  
  // Add failure diagnostics
  if (!history.screenshot_path.empty()) {
    response->set_screenshot_path(history.screenshot_path);
    response->set_screenshot_size_bytes(history.screenshot_size_bytes);
  }
  
  if (!history.failure_context.empty()) {
    response->set_failure_context(history.failure_context);
  }
  
  return absl::OkStatus();
}
```

#### Step 4: Update Proto Schema (15 minutes)

**File**: `src/app/core/proto/imgui_test_harness.proto`

Add fields to GetTestResultsResponse:

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
  string widget_state = 9;  // IT-08c
}
```

### Testing

```bash
# 1. Build with changes
cmake --build build-grpc-test --target yaze -j8

# 2. Start test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# 3. Trigger a failing test
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"nonexistent_widget","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# 4. Check for screenshot
ls -lh /tmp/yaze_test_*_failure.bmp

# 5. Query test results
grpcurl -plaintext \
  -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"test_id":"grpc_click_<timestamp>"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/GetTestResults

# Expected: screenshot_path and failure_context populated
```

### Success Criteria

- ✅ Screenshots auto-captured on test failure
- ✅ Screenshot path stored in test history
- ✅ GetTestResults returns screenshot metadata
- ✅ No performance impact on passing tests
- ✅ Screenshots cleaned up after test completion (optional)

---

## IT-08c: Widget State Dumps 📋 PLANNED

**Goal**: Capture UI hierarchy and state on test failures  
**Time Estimate**: 30-45 minutes  
**Status**: Specification phase

### Implementation Plan

#### Step 1: Create Widget State Capture Utility (30 minutes)

**File**: `src/app/core/widget_state_capture.h` (new file)

```cpp
#ifndef YAZE_CORE_WIDGET_STATE_CAPTURE_H
#define YAZE_CORE_WIDGET_STATE_CAPTURE_H

#include <string>
#include "imgui/imgui.h"

namespace yaze {
namespace core {

struct WidgetState {
  std::string focused_window;
  std::string focused_widget;
  std::string hovered_widget;
  std::vector<std::string> visible_windows;
  std::vector<std::string> open_menus;
  std::string active_popup;
};

std::string CaptureWidgetState();
std::string SerializeWidgetStateToJson(const WidgetState& state);

}  // namespace core
}  // namespace yaze

#endif
```

**File**: `src/app/core/widget_state_capture.cc` (new file)

```cpp
#include "src/app/core/widget_state_capture.h"
#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace core {

std::string CaptureWidgetState() {
  WidgetState state;
  
  // Capture focused window
  ImGuiWindow* current = ImGui::GetCurrentWindow();
  if (current) {
    state.focused_window = current->Name;
  }
  
  // Capture active widget
  ImGuiID active_id = ImGui::GetActiveID();
  if (active_id != 0) {
    state.focused_widget = absl::StrFormat("ID_%u", active_id);
  }
  
  // Capture hovered widget
  ImGuiID hovered_id = ImGui::GetHoveredID();
  if (hovered_id != 0) {
    state.hovered_widget = absl::StrFormat("ID_%u", hovered_id);
  }
  
  // Traverse window list
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  for (ImGuiWindow* window : ctx->Windows) {
    if (window->Active && !window->Hidden) {
      state.visible_windows.push_back(window->Name);
    }
  }
  
  return SerializeWidgetStateToJson(state);
}

std::string SerializeWidgetStateToJson(const WidgetState& state) {
  nlohmann::json j;
  j["focused_window"] = state.focused_window;
  j["focused_widget"] = state.focused_widget;
  j["hovered_widget"] = state.hovered_widget;
  j["visible_windows"] = state.visible_windows;
  j["open_menus"] = state.open_menus;
  j["active_popup"] = state.active_popup;
  return j.dump(2);  // Pretty print with indent
}

}  // namespace core
}  // namespace yaze
```

#### Step 2: Integrate with TestManager (15 minutes)

Update `CaptureFailureContext()` in `test_manager.cc`:

```cpp
void TestManager::CaptureFailureContext(const std::string& test_id) {
  auto& history_entry = test_history_[test_id];
  
  // 1. Screenshot (IT-08b)
  // ... existing code ...
  
  // 2. Widget state (IT-08c)
  history_entry.widget_state = core::CaptureWidgetState();
  
  // 3. Execution context
  // ... existing code ...
}
```

### Output Example

```json
{
  "focused_window": "Overworld Editor",
  "focused_widget": "ID_12345",
  "hovered_widget": "ID_67890",
  "visible_windows": [
    "Main Window",
    "Overworld Editor",
    "Palette Editor"
  ],
  "open_menus": [],
  "active_popup": ""
}
```

---

## IT-08d: Error Envelope Standardization 📋 PLANNED

**Goal**: Unified error format across z3ed, TestManager, EditorManager  
**Time Estimate**: 1-2 hours  
**Status**: Design phase

### Proposed Error Envelope

```cpp
// Shared error structure
struct ErrorContext {
  absl::Status status;
  std::string component;  // "TestHarness", "EditorManager", "z3ed"
  std::string operation;  // "Click", "LoadROM", "RunTest"
  std::map<std::string, std::string> metadata;
  std::vector<std::string> artifact_paths;  // Screenshots, logs, etc.
  std::string actionable_hint;  // User-facing suggestion
};
```

### Integration Points

1. **TestManager**: Wrap failures in ErrorContext
2. **EditorManager**: Use ErrorContext for all operations
3. **z3ed CLI**: Parse ErrorContext and format for display
4. **ProposalDrawer**: Display ErrorContext in GUI modal

---

## IT-08e: CLI Error Improvements 📋 PLANNED

**Goal**: Rich error output in z3ed CLI  
**Time Estimate**: 1 hour  
**Status**: Design phase

### Enhanced CLI Output

```bash
$ z3ed agent test --prompt "Open Overworld editor"

❌ Test Failed: grpc_click_1696357200
   Component: ImGuiTestHarness
   Operation: Click widget "Overworld"
   
   Error: Widget not found
   
   Artifacts:
   • Screenshot: /tmp/yaze_test_grpc_click_1696357200_failure.bmp
   • Widget State: /tmp/yaze_test_grpc_click_1696357200_state.json
   • Logs: /tmp/yaze_test_grpc_click_1696357200.log
   
   Context:
   • Visible Windows: Main Window, Debug
   • Focused Window: Main Window
   • Active Widget: None
   
   Suggestion:
   → Check if ROM is loaded (File → Open ROM)
   → Verify Overworld editor button is visible
   → Use 'z3ed agent gui discover' to list available widgets
```

---

## Progress Tracking

### Completed ✅
- IT-08a: Screenshot RPC (1.5 hours)

### In Progress 🔄
- IT-08b: Auto-capture on failure (next priority)

### Planned 📋
- IT-08c: Widget state dumps
- IT-08d: Error envelope standardization
- IT-08e: CLI error improvements

### Time Investment
- **Spent**: 1.5 hours (IT-08a)
- **Remaining**: 3.5-5.5 hours (IT-08b/c/d/e)
- **Total**: 5-7 hours (as estimated)

---

## Next Steps

**Immediate** (IT-08b - 1-1.5 hours):
1. Modify TestManager to capture screenshots on failure
2. Update TestHistory structure
3. Update GetTestResults RPC
4. Test with intentional failures

**Short-term** (IT-08c - 30-45 minutes):
1. Create widget state capture utility
2. Integrate with TestManager
3. Add to GetTestResults RPC

**Medium-term** (IT-08d/e - 2-3 hours):
1. Design unified error envelope
2. Implement across all services
3. Update CLI output formatting
4. Add ProposalDrawer error modal

---

## References

- **Implementation Plan**: [E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)
- **Test Harness Guide**: [IT-05-IMPLEMENTATION-GUIDE.md](IT-05-IMPLEMENTATION-GUIDE.md)
- **Source Files**: 
  - `src/app/core/service/imgui_test_harness_service.cc`
  - `src/app/core/test_manager.{h,cc}`
  - `src/app/core/proto/imgui_test_harness.proto`

---

**Last Updated**: October 2, 2025  
**Current Phase**: IT-08b (Auto-capture on failure)  
**Overall Progress**: 33% Complete (1 of 3 core phases)

---

**Report Generated**: October 2, 2025  
**Author**: GitHub Copilot (AI Assistant)  
**Project**: YAZE - Yet Another Zelda3 Editor  
**Component**: z3ed CLI Tool - Test Automation Harness
