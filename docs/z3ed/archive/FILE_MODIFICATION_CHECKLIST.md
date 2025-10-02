# IT-01 Phase 2: File Modification Checklist

**Quick Reference**: Exactly which files to edit and what to change

## Files to Modify (4 files)

### 1. `src/app/core/imgui_test_harness_service.h`

**What to change**: Add TestManager member to service class

**Line ~20-30** (in ImGuiTestHarnessServiceImpl class):
```cpp
class ImGuiTestHarnessServiceImpl {
 public:
  // ADD THIS LINE:
  explicit ImGuiTestHarnessServiceImpl(TestManager* test_manager)
      : test_manager_(test_manager) {}

  absl::Status Ping(const PingRequest* request, PingResponse* response);
  absl::Status Click(const ClickRequest* request, ClickResponse* response);
  absl::Status Type(const TypeRequest* request, TypeResponse* response);
  absl::Status Wait(const WaitRequest* request, WaitResponse* response);
  absl::Status Assert(const AssertRequest* request, AssertResponse* response);
  absl::Status Screenshot(const ScreenshotRequest* request,
                         ScreenshotResponse* response);

 private:
  TestManager* test_manager_;  // ADD THIS LINE
};
```

**Line ~50** (in ImGuiTestHarnessServer class):
```cpp
class ImGuiTestHarnessServer {
 public:
  static ImGuiTestHarnessServer& Instance();

  // CHANGE THIS LINE - add second parameter:
  absl::Status Start(int port, TestManager* test_manager);
  
  // ... rest stays the same
};
```

---

### 2. `src/app/core/imgui_test_harness_service.cc`

**What to change**: Add includes and implement Click handler

**Top of file** (after existing includes, around line 10):
```cpp
// ADD THESE INCLUDES:
#include "app/test/test_manager.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"
```

**In `Start()` method** (around line 100):
```cpp
absl::Status ImGuiTestHarnessServer::Start(int port, TestManager* test_manager) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  // ADD THESE LINES:
  if (!test_manager) {
    return absl::InvalidArgumentError("TestManager cannot be null");
  }

  // CHANGE THIS LINE to pass test_manager:
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>(test_manager);

  // ... rest of method stays the same
```

**In `Click()` method** (around line 130):

Replace the entire stub implementation with the code from `IT-01-PHASE2-IMPLEMENTATION-GUIDE.md` (Step 2.2).

Quick version (see full guide for complete code):
```cpp
absl::Status ImGuiTestHarnessServiceImpl::Click(
    const ClickRequest* request,
    ClickResponse* response) {
  
  auto start = std::chrono::steady_clock::now();

  // Get TestEngine
  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse target: "button:Open ROM"
  std::string target = request->target();
  size_t colon_pos = target.find(':');
  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return absl::OkStatus();
  }

  std::string widget_label = target.substr(colon_pos + 1);

  // Create test context
  std::string context_name = absl::StrFormat("grpc_click_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, context_name.c_str());

  if (!ctx) {
    response->set_success(false);
    response->set_message("Failed to create test context");
    return absl::OkStatus();
  }

  // Find widget
  ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
      ctx, widget_label.c_str(), NULL);

  bool success = false;
  std::string message;

  if (!item) {
    message = absl::StrFormat("Widget not found: %s", widget_label);
  } else {
    // Convert click type
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;
    switch (request->type()) {
      case ClickRequest::LEFT: mouse_button = ImGuiMouseButton_Left; break;
      case ClickRequest::RIGHT: mouse_button = ImGuiMouseButton_Right; break;
      case ClickRequest::MIDDLE: mouse_button = ImGuiMouseButton_Middle; break;
    }
    
    // Click it!
    ImGuiTestEngine_ItemClick(ctx, item->ID, mouse_button);
    success = true;
    message = absl::StrFormat("Clicked '%s'", widget_label);
  }

  // Cleanup
  ImGuiTestEngine_DestroyContext(ctx);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}
```

---

### 3. `src/app/main.cc`

**What to change**: Pass TestManager when starting gRPC server

**Find the section** with `#ifdef YAZE_WITH_GRPC` (probably around line 200-300):

```cpp
#ifdef YAZE_WITH_GRPC
  if (absl::GetFlag(FLAGS_enable_test_harness)) {
    // ADD THESE LINES:
    auto* test_manager = yaze::test::TestManager::GetInstance();
    if (!test_manager) {
      std::cerr << "ERROR: TestManager not initialized. "
                << "Cannot start test harness.\n";
      return 1;
    }

    auto& harness = yaze::test::ImGuiTestHarnessServer::Instance();
    
    // CHANGE THIS LINE to pass test_manager:
    auto status = harness.Start(
        absl::GetFlag(FLAGS_test_harness_port),
        test_manager  // ADD THIS ARGUMENT
    );
    
    if (!status.ok()) {
      std::cerr << "Failed to start test harness: " 
                << status.message() << "\n";
      return 1;
    }
  }
#endif
```

---

### 4. Build and Test

After making the above changes:

```bash
# Rebuild
cd /Users/scawful/Code/yaze
cmake --build build-grpc-test --target yaze -j8

# Should compile without errors
# If compilation fails, check:
# - Include paths are correct
# - TestManager header is found
# - ImGuiTestEngine headers are found

# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze --enable_test_harness &

# Wait for startup (2-3 seconds)
sleep 3

# Test Ping first
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"message":"Hello"}' 127.0.0.1:50051 yaze.test.ImGuiTestHarness/Ping

# Should return: {"message":"Pong: Hello", "timestampMs":"...", "yazeVersion":"0.3.2"}

# Test Click (adjust button label to match YAZE UI)
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Click

# Expected: {"success":true, "message":"Clicked 'Overworld'", "executionTimeMs":5}
# And in YAZE GUI: The Overworld button should actually click!
```

---

## Compilation Troubleshooting

### Error: "TestManager not found"

**Fix**: Add include to service file:
```cpp
#include "app/test/test_manager.h"
```

### Error: "ImGuiTestEngine_CreateContext not declared"

**Fix**: Add includes:
```cpp
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"
```

### Error: "no matching function for call to Start"

**Fix**: Update main.cc to pass test_manager as second argument.

### Linker error: "undefined reference to ImGuiTestEngine"

**Fix**: Ensure CMake links ImGuiTestEngine:
```cmake
if(YAZE_WITH_GRPC AND TARGET ImGuiTestEngine)
  target_link_libraries(yaze PRIVATE ImGuiTestEngine)
endif()
```

---

## Testing Checklist

After compilation succeeds:

- [ ] Server starts without errors
- [ ] Ping RPC returns version
- [ ] Click RPC with fake button returns "Widget not found" (expected)
- [ ] Click RPC with real button returns success
- [ ] Button actually clicks in YAZE GUI

If first 4 work but button doesn't click:
- Check button label is exact match
- Try with a different button
- Enable ImGuiTestEngine debug output

---

## What's Next

Once Click works:
1. Implement Type handler (similar pattern)
2. Implement Wait handler (polling loop)
3. Implement Assert handler (state queries)
4. Create end-to-end test script

See `IT-01-PHASE2-IMPLEMENTATION-GUIDE.md` for full implementations.

---

**Estimated Time**: 
- Code changes: 1-2 hours
- Testing/debugging: 1-2 hours
- **Total: 2-4 hours for Click handler working end-to-end**

Good luck! 🚀
