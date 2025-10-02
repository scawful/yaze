# IT-01 Phase 2: ImGuiTestEngine Integration Guide

**Date**: October 1, 2025  
**Status**: Implementation Ready  
**Estimated Time**: 6-8 hours  
**Prerequisites**: ✅ Phase 1 Complete (gRPC infrastructure working)

## 📋 Overview

This guide walks through implementing actual GUI automation in the gRPC test harness by integrating with YAZE's existing ImGuiTestEngine infrastructure.

**What We're Building**: Transform stub RPC handlers into real GUI automation that can:
- Click buttons and UI elements
- Type text into input fields
- Wait for windows/elements to appear
- Assert UI state
- Capture screenshots

## 🎯 Success Criteria

By the end of this implementation:
- [ ] `Click` RPC can click actual ImGui widgets
- [ ] `Type` RPC can input text into fields
- [ ] `Wait` RPC polls for conditions with timeout
- [ ] `Assert` RPC validates UI state
- [ ] `Screenshot` RPC captures framebuffer (basic implementation)
- [ ] End-to-end test: "Open ROM via gRPC" works

## 📚 Architecture Review

### Current State
```
gRPC Client (grpcurl/z3ed)
    ↓ RPC call
ImGuiTestHarnessServer
    ↓ calls
ImGuiTestHarnessServiceImpl::Click()
    ↓ CURRENTLY: stub implementation
    ✅ TODO: integrate with ImGuiTestEngine
```

### Target State
```
gRPC Client (grpcurl/z3ed)
    ↓ RPC call
ImGuiTestHarnessServer
    ↓ calls
ImGuiTestHarnessServiceImpl::Click()
    ↓ accesses
TestManager::GetUITestEngine()
    ↓ calls
ImGuiTestEngine_ItemClick()
    ↓ interacts with
ImGui Widgets (actual GUI)
```

## 🔍 Understanding ImGuiTestEngine

YAZE already has ImGuiTestEngine integrated. Let's understand how it works:

### Key Components

1. **TestManager** (`src/app/test/test_manager.h`)
   - Singleton: `TestManager::GetInstance()`
   - Provides: `GetUITestEngine()` → returns `ImGuiTestEngine*`

2. **ImGuiTestEngine** (from `src/lib/imgui_test_engine/`)
   - C API for GUI automation
   - Key functions:
     - `ImGuiTestEngine_FindItemByLabel()` - Find widget by label
     - `ImGuiTestEngine_ItemClick()` - Simulate click
     - `ImGuiTestEngine_ItemInputValue()` - Input text
     - `ImGuiTestEngine_GetWindowByRef()` - Check window existence

3. **ImGuiTestContext** 
   - Per-test context object
   - Needed for most test engine calls
   - Created via `ImGuiTestEngine_CreateContext()`

### Sample ImGuiTestEngine Usage

```cpp
// From existing YAZE tests (conceptual example):

#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

// 1. Get engine instance
ImGuiTestEngine* engine = test_manager->GetUITestEngine();

// 2. Create test context
ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, "test_name");

// 3. Find a button by label
ImGuiTestItemInfo* button = ImGuiTestEngine_FindItemByLabel(
    ctx, 
    "Open ROM",  // Button label
    NULL         // Parent window (NULL = search all)
);

// 4. Click the button
if (button) {
    ImGuiTestEngine_ItemClick(ctx, button->ID, ImGuiMouseButton_Left);
}

// 5. Wait for a window to appear
ImGuiTestEngine_GetWindowByRef(ctx, "Overworld Editor", ImGuiTestOpFlags_None);

// 6. Input text
ImGuiTestItemInfo* input = ImGuiTestEngine_FindItemByLabel(ctx, "Filename", NULL);
if (input) {
    ImGuiTestEngine_ItemInputValue(ctx, input->ID, "zelda3.sfc");
}

// 7. Cleanup
ImGuiTestEngine_DestroyContext(ctx);
```

## 🛠️ Implementation Plan

### Task 1: Access TestManager from gRPC Service (30 min)

**Goal**: Connect gRPC service to YAZE's TestManager singleton.

**Challenge**: The gRPC service is a standalone component that needs to access YAZE's core systems.

**Solution**: Pass TestManager reference during server initialization.

#### Step 1.1: Update Service Interface

Edit `src/app/core/imgui_test_harness_service.h`:

```cpp
#ifndef YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
#define YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_

#ifdef YAZE_WITH_GRPC

#include <memory>
#include <grpcpp/grpcpp.h>
#include "proto/imgui_test_harness.grpc.pb.h"
#include "absl/status/status.h"

// Forward declarations
namespace yaze {
namespace test {
class TestManager;
}
}

namespace yaze {
namespace test {

class ImGuiTestHarnessServiceImpl {
 public:
  // Constructor now takes TestManager reference
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
  TestManager* test_manager_;  // Non-owning pointer
};

class ImGuiTestHarnessServer {
 public:
  static ImGuiTestHarnessServer& Instance();

  // Updated: now requires TestManager
  absl::Status Start(int port, TestManager* test_manager);
  
  void Shutdown();
  bool IsRunning() const { return server_ != nullptr; }
  int Port() const { return port_; }

 private:
  ImGuiTestHarnessServer() = default;
  ~ImGuiTestHarnessServer();

  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<ImGuiTestHarnessServiceImpl> service_;
  std::unique_ptr<class ImGuiTestHarnessServiceGrpc> grpc_service_;
  int port_ = 0;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
```

#### Step 1.2: Update Server Startup

Edit `src/app/core/imgui_test_harness_service.cc`:

```cpp
absl::Status ImGuiTestHarnessServer::Start(int port, TestManager* test_manager) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }

  if (!test_manager) {
    return absl::InvalidArgumentError("TestManager cannot be null");
  }

  // Create service with TestManager reference
  service_ = std::make_unique<ImGuiTestHarnessServiceImpl>(test_manager);

  // ... rest of startup code remains the same
  
  grpc_service_ = std::make_unique<ImGuiTestHarnessServiceGrpc>(service_.get());

  std::string server_address = absl::StrFormat("0.0.0.0:%d", port);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(grpc_service_.get());

  server_ = builder.BuildAndStart();

  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start gRPC server on %s", server_address));
  }

  port_ = port;

  std::cout << "✓ ImGuiTestHarness gRPC server listening on " << server_address
            << " (with TestManager integration)\n";

  return absl::OkStatus();
}
```

#### Step 1.3: Update main.cc Server Startup

Edit `src/app/main.cc` (around the flag handling section):

```cpp
#ifdef YAZE_WITH_GRPC
  if (absl::GetFlag(FLAGS_enable_test_harness)) {
    // Get TestManager instance
    auto* test_manager = yaze::test::TestManager::GetInstance();
    if (!test_manager) {
      std::cerr << "ERROR: TestManager not initialized. "
                << "Cannot start test harness.\n";
      return 1;
    }

    auto& harness = yaze::test::ImGuiTestHarnessServer::Instance();
    auto status = harness.Start(
        absl::GetFlag(FLAGS_test_harness_port),
        test_manager  // Pass TestManager reference
    );
    
    if (!status.ok()) {
      std::cerr << "Failed to start test harness: " 
                << status.message() << "\n";
      return 1;
    }
  }
#endif
```

### Task 2: Implement Click Handler (2-3 hours)

**Goal**: Make `Click` RPC actually click ImGui widgets.

#### Step 2.1: Understand Target Format

We'll support these target formats:
- `"button:Open ROM"` - Click a button with label "Open ROM"
- `"menu:File→Open"` - Click menu item
- `"checkbox:Enable Feature"` - Toggle checkbox
- `"item:#widget_id"` - Click by ImGui ID

#### Step 2.2: Implement Click Handler

Edit `src/app/core/imgui_test_harness_service.cc`:

```cpp
#include "app/test/test_manager.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_context.h"

absl::Status ImGuiTestHarnessServiceImpl::Click(
    const ClickRequest* request,
    ClickResponse* response) {
  
  auto start = std::chrono::steady_clock::now();

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

  // Create temporary test context
  std::string context_name = absl::StrFormat("grpc_click_%lld", 
      std::chrono::system_clock::now().time_since_epoch().count());
  ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, context_name.c_str());
  
  if (!ctx) {
    response->set_success(false);
    response->set_message("Failed to create test context");
    return absl::OkStatus();
  }

  // Find the widget by label
  ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
      ctx,
      widget_label.c_str(),
      NULL  // Search all windows
  );

  if (!item) {
    // Try searching with prefix if it's a menu item (e.g., "File→Open")
    if (widget_label.find("→") != std::string::npos || 
        widget_label.find("->") != std::string::npos) {
      // For menu items, ImGui might need the full path
      std::string menu_path = widget_label;
      // Replace → with / for ImGui test engine format
      size_t arrow_pos = menu_path.find("→");
      if (arrow_pos != std::string::npos) {
        menu_path.replace(arrow_pos, 3, "/");  // UTF-8 arrow is 3 bytes
      }
      item = ImGuiTestEngine_FindItemByLabel(ctx, menu_path.c_str(), NULL);
    }
  }

  bool success = false;
  std::string message;

  if (!item) {
    message = absl::StrFormat(
        "Widget not found: %s '%s'. "
        "Hint: Use ImGui label text exactly as shown in UI.",
        widget_type, widget_label);
  } else {
    // Convert click type
    ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;
    switch (request->type()) {
      case ClickRequest::LEFT:
        mouse_button = ImGuiMouseButton_Left;
        break;
      case ClickRequest::RIGHT:
        mouse_button = ImGuiMouseButton_Right;
        break;
      case ClickRequest::DOUBLE:
        // ImGui doesn't have direct double-click, simulate two clicks
        ImGuiTestEngine_ItemClick(ctx, item->ID, ImGuiMouseButton_Left);
        ImGuiTestEngine_ItemClick(ctx, item->ID, ImGuiMouseButton_Left);
        success = true;
        message = absl::StrFormat("Double-clicked %s '%s'", widget_type, widget_label);
        goto cleanup;  // Skip single click below
      case ClickRequest::MIDDLE:
        mouse_button = ImGuiMouseButton_Middle;
        break;
    }

    // Perform the click
    ImGuiTestEngine_ItemClick(ctx, item->ID, mouse_button);
    
    success = true;
    message = absl::StrFormat("Clicked %s '%s'", widget_type, widget_label);
  }

cleanup:
  // Cleanup context
  ImGuiTestEngine_DestroyContext(ctx);

  // Calculate execution time
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}
```

#### Step 2.3: Test Click Implementation

```bash
# Rebuild with changes
cmake --build build-grpc-test --target yaze -j8

# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze --enable_test_harness &

# Test clicking a button (adjust label to match actual YAZE UI)
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Open ROM","type":"LEFT"}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Click

# Expected: If "Open ROM" button exists, it will be clicked!
# You should see the file dialog open in YAZE
```

### Task 3: Implement Type Handler (1-2 hours)

**Goal**: Input text into ImGui input fields.

Edit `src/app/core/imgui_test_harness_service.cc`:

```cpp
absl::Status ImGuiTestHarnessServiceImpl::Type(
    const TypeRequest* request,
    TypeResponse* response) {
  
  auto start = std::chrono::steady_clock::now();

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse target
  std::string target = request->target();
  size_t colon_pos = target.find(':');
  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return absl::OkStatus();
  }

  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);

  // Create test context
  std::string context_name = absl::StrFormat("grpc_type_%lld",
      std::chrono::system_clock::now().time_since_epoch().count());
  ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, context_name.c_str());

  if (!ctx) {
    response->set_success(false);
    response->set_message("Failed to create test context");
    return absl::OkStatus();
  }

  // Find the input field
  ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
      ctx, widget_label.c_str(), NULL);

  bool success = false;
  std::string message;

  if (!item) {
    message = absl::StrFormat("Input field not found: %s", widget_label);
  } else {
    // Clear existing text if requested
    if (request->clear_first()) {
      // Click to focus
      ImGuiTestEngine_ItemClick(ctx, item->ID, ImGuiMouseButton_Left);
      
      // Select all (Ctrl+A or Cmd+A on macOS)
      #ifdef __APPLE__
      ImGuiTestEngine_KeyDown(ctx, ImGuiMod_Super);  // Cmd key
      #else
      ImGuiTestEngine_KeyDown(ctx, ImGuiMod_Ctrl);
      #endif
      ImGuiTestEngine_KeyPress(ctx, ImGuiKey_A);
      #ifdef __APPLE__
      ImGuiTestEngine_KeyUp(ctx, ImGuiMod_Super);
      #else
      ImGuiTestEngine_KeyUp(ctx, ImGuiMod_Ctrl);
      #endif
      
      // Delete
      ImGuiTestEngine_KeyPress(ctx, ImGuiKey_Delete);
    }

    // Input the new text
    ImGuiTestEngine_ItemInputValue(ctx, item->ID, request->text().c_str());
    
    success = true;
    message = absl::StrFormat("Typed '%s' into %s", 
                             request->text(), widget_label);
  }

  ImGuiTestEngine_DestroyContext(ctx);

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(success);
  response->set_message(message);
  response->set_execution_time_ms(elapsed.count());

  return absl::OkStatus();
}
```

### Task 4: Implement Wait Handler (2 hours)

**Goal**: Poll for conditions with timeout.

```cpp
absl::Status ImGuiTestHarnessServiceImpl::Wait(
    const WaitRequest* request,
    WaitResponse* response) {
  
  auto start = std::chrono::steady_clock::now();

  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse condition: "window_visible:Overworld Editor"
  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');
  
  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid condition format. Use 'type:value'");
    return absl::OkStatus();
  }

  std::string condition_type = condition.substr(0, colon_pos);
  std::string condition_value = condition.substr(colon_pos + 1);

  // Get timeout and poll interval
  int timeout_ms = request->timeout_ms() > 0 ? request->timeout_ms() : 5000;
  int poll_interval_ms = request->poll_interval_ms() > 0 ? 
                         request->poll_interval_ms() : 100;

  // Create test context
  std::string context_name = absl::StrFormat("grpc_wait_%lld",
      std::chrono::system_clock::now().time_since_epoch().count());
  ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, context_name.c_str());

  if (!ctx) {
    response->set_success(false);
    response->set_message("Failed to create test context");
    return absl::OkStatus();
  }

  bool condition_met = false;
  auto poll_start = std::chrono::steady_clock::now();

  // Poll loop
  while (true) {
    auto elapsed_poll = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - poll_start);

    if (elapsed_poll.count() >= timeout_ms) {
      // Timeout
      break;
    }

    // Check condition based on type
    if (condition_type == "window_visible") {
      ImGuiWindow* window = ImGuiTestEngine_GetWindowByRef(
          ctx, condition_value.c_str(), ImGuiTestOpFlags_None);
      if (window != nullptr) {
        condition_met = true;
        break;
      }
    } else if (condition_type == "element_visible") {
      ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
          ctx, condition_value.c_str(), NULL);
      if (item != nullptr) {
        condition_met = true;
        break;
      }
    } else if (condition_type == "element_enabled") {
      ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
          ctx, condition_value.c_str(), NULL);
      if (item != nullptr && !(item->StatusFlags & ImGuiItemStatusFlags_Disabled)) {
        condition_met = true;
        break;
      }
    } else {
      // Unknown condition type
      response->set_success(false);
      response->set_message(
          absl::StrFormat("Unknown condition type: %s. "
                         "Supported: window_visible, element_visible, element_enabled",
                         condition_type));
      ImGuiTestEngine_DestroyContext(ctx);
      return absl::OkStatus();
    }

    // Sleep before next poll
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
  }

  ImGuiTestEngine_DestroyContext(ctx);

  auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  response->set_success(condition_met);
  response->set_message(
      condition_met 
          ? absl::StrFormat("Condition '%s' met after %lld ms", 
                           condition, total_elapsed.count())
          : absl::StrFormat("Timeout waiting for condition '%s' (waited %d ms)",
                           condition, timeout_ms));
  response->set_elapsed_ms(total_elapsed.count());

  return absl::OkStatus();
}
```

### Task 5: Implement Assert Handler (1-2 hours)

**Goal**: Validate UI state and return detailed results.

```cpp
absl::Status ImGuiTestHarnessServiceImpl::Assert(
    const AssertRequest* request,
    AssertResponse* response) {
  
  if (!test_manager_) {
    response->set_success(false);
    response->set_message("TestManager not available");
    return absl::OkStatus();
  }

  ImGuiTestEngine* engine = test_manager_->GetUITestEngine();
  if (!engine) {
    response->set_success(false);
    response->set_message("ImGuiTestEngine not initialized");
    return absl::OkStatus();
  }

  // Parse condition: "visible:MainWindow" or "enabled:button:Save"
  std::string condition = request->condition();
  size_t colon_pos = condition.find(':');
  
  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid condition format");
    return absl::OkStatus();
  }

  std::string assertion_type = condition.substr(0, colon_pos);
  std::string target = condition.substr(colon_pos + 1);

  // Create test context
  std::string context_name = absl::StrFormat("grpc_assert_%lld",
      std::chrono::system_clock::now().time_since_epoch().count());
  ImGuiTestContext* ctx = ImGuiTestEngine_CreateContext(engine, context_name.c_str());

  if (!ctx) {
    response->set_success(false);
    response->set_message("Failed to create test context");
    return absl::OkStatus();
  }

  bool assertion_passed = false;
  std::string actual_value;
  std::string message;

  if (assertion_type == "visible") {
    // Check if window or element is visible
    ImGuiWindow* window = ImGuiTestEngine_GetWindowByRef(
        ctx, target.c_str(), ImGuiTestOpFlags_None);
    assertion_passed = (window != nullptr);
    actual_value = assertion_passed ? "visible" : "not visible";
    message = absl::StrFormat("Assertion '%s' %s", 
                             condition,
                             assertion_passed ? "passed" : "failed");
  } else if (assertion_type == "enabled") {
    // Check if element is enabled
    ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
        ctx, target.c_str(), NULL);
    if (item) {
      assertion_passed = !(item->StatusFlags & ImGuiItemStatusFlags_Disabled);
      actual_value = assertion_passed ? "enabled" : "disabled";
    } else {
      assertion_passed = false;
      actual_value = "not found";
    }
    message = absl::StrFormat("Assertion '%s' %s (actual: %s)",
                             condition,
                             assertion_passed ? "passed" : "failed",
                             actual_value);
  } else {
    message = absl::StrFormat("Unknown assertion type: %s", assertion_type);
  }

  ImGuiTestEngine_DestroyContext(ctx);

  response->set_success(assertion_passed);
  response->set_message(message);
  response->set_actual_value(actual_value);
  response->set_expected_value(request->expected());

  return absl::OkStatus();
}
```

### Task 6: Implement Screenshot Handler (2-3 hours)

**Goal**: Capture framebuffer and return as PNG/JPEG.

**Note**: This is more complex and requires access to OpenGL/rendering context. For now, implement a basic version.

```cpp
#include <fstream>
#include "imgui_impl_opengl3.h"  // Or appropriate backend

absl::Status ImGuiTestHarnessServiceImpl::Screenshot(
    const ScreenshotRequest* request,
    ScreenshotResponse* response) {
  
  // TODO: Implement actual screenshot capture
  // This requires:
  // 1. Access to OpenGL framebuffer or SDL surface
  // 2. Image encoding library (stb_image_write or similar)
  // 3. Base64 encoding for transmission via gRPC

  // For Phase 2, return a placeholder
  response->set_success(false);
  response->set_message("Screenshot capture not yet implemented. "
                       "Requires framebuffer access and image encoding.");
  response->set_file_path("");
  response->set_file_size_bytes(0);

  return absl::OkStatus();
}
```

**Future Enhancement**: Full screenshot implementation
```cpp
// Pseudo-code for future implementation:
// 1. Get framebuffer dimensions
// 2. Read pixels: glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels)
// 3. Flip vertically (OpenGL origin is bottom-left)
// 4. Encode as PNG: stbi_write_png(filename, width, height, 3, pixels, width*3)
// 5. Read file and return bytes in response
// 6. Optionally: base64 encode for direct inclusion in response
```

## 🧪 Testing & Validation

### Test 1: Click a Button

```bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze --enable_test_harness --rom assets/zelda3.sfc &

# Wait for GUI to load (2-3 seconds)
sleep 3

# Try clicking the "Overworld" button (adjust label as needed)
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Click

# Expected: Overworld editor opens in YAZE
```

### Test 2: Type into Input Field

```bash
# Click "Open ROM" to open file dialog
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Open ROM","type":"LEFT"}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Click

# Wait for dialog to appear
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Open File","timeout_ms":3000,"poll_interval_ms":100}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Wait

# Type filename
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"input:Filename","text":"zelda3.sfc","clear_first":true}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Type
```

### Test 3: End-to-End Workflow

Create `test_scripts/open_rom_via_grpc.sh`:

```bash
#!/bin/bash
# Test script: Open ROM file via gRPC

set -e  # Exit on error

SERVER="127.0.0.1:50051"
PROTO_PATH="src/app/core/proto"
PROTO_FILE="imgui_test_harness.proto"

echo "🧪 Testing: Open ROM via gRPC"

echo "1. Ping test harness..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"message":"test"}' $SERVER yaze.test.ImGuiTestHarness/Ping

echo "2. Click 'Open ROM' button..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"button:Open ROM","type":"LEFT"}' \
  $SERVER yaze.test.ImGuiTestHarness/Click

echo "3. Wait for file dialog..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"window_visible:Open File","timeout_ms":5000}' \
  $SERVER yaze.test.ImGuiTestHarness/Wait

echo "4. Type filename..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"input:File path","text":"assets/zelda3.sfc","clear_first":true}' \
  $SERVER yaze.test.ImGuiTestHarness/Type

echo "5. Click 'OK' button..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"target":"button:OK","type":"LEFT"}' \
  $SERVER yaze.test.ImGuiTestHarness/Click

echo "6. Assert ROM is loaded..."
grpcurl -plaintext -import-path $PROTO_PATH -proto $PROTO_FILE \
  -d '{"condition":"visible:Overworld Editor","expected":"true"}' \
  $SERVER yaze.test.ImGuiTestHarness/Assert

echo "✅ Test complete!"
```

Run test:
```bash
chmod +x test_scripts/open_rom_via_grpc.sh
./test_scripts/open_rom_via_grpc.sh
```

## 🐛 Troubleshooting

### Issue 1: "ImGuiTestEngine not initialized"

**Symptom**: RPC returns error about TestEngine not available.

**Solution**: Ensure TestManager initializes TestEngine during startup.

Check `src/app/test/test_manager.cc`:
```cpp
void TestManager::Initialize() {
  // Should create ImGuiTestEngine
  ui_test_engine_ = ImGuiTestEngine_CreateContext();
  // ...
}
```

### Issue 2: "Widget not found"

**Symptom**: Click RPC can't find button/element.

**Debug Steps**:
1. Check exact label in ImGui code (case-sensitive)
2. Try with window prefix: `"window:MainWindow/button:Save"`
3. Use ImGui Demo to see ID format
4. Enable ImGui test engine debug output

**Solution**: Match label exactly:
```cpp
// ImGui code:
if (ImGui::Button("Open ROM##file_menu")) { ... }

// gRPC target should be:
"button:Open ROM##file_menu"
// Or without ID suffix if unique:
"button:Open ROM"
```

### Issue 3: Click happens too fast

**Symptom**: Click doesn't register, element not ready yet.

**Solution**: Add small delay before click:
```cpp
// In Click handler, before ItemClick:
std::this_thread::sleep_for(std::chrono::milliseconds(50));
ImGuiTestEngine_ItemClick(ctx, item->ID, mouse_button);
```

### Issue 4: Context creation fails

**Symptom**: `ImGuiTestEngine_CreateContext()` returns NULL.

**Solution**: Check TestEngine is properly initialized:
```cpp
// In TestManager initialization:
if (ui_test_engine_) {
  ImGuiTestEngine_Start(ui_test_engine_, ImGui::GetCurrentContext());
}
```

## 📊 Performance Considerations

### Memory Management
- Test contexts are created and destroyed per RPC
- Each context ~10KB overhead
- For high-frequency operations, consider context pooling

### Thread Safety
- ImGui is NOT thread-safe
- All ImGui operations must run on main thread
- **TODO**: Consider message queue for cross-thread RPC handling

**Thread-Safe Implementation** (future enhancement):
```cpp
// Add message queue in TestManager:
class TestManager {
  struct GuiCommand {
    std::function<void()> action;
    std::promise<bool> result;
  };
  
  std::queue<GuiCommand> gui_command_queue_;
  std::mutex queue_mutex_;
  
  void ProcessGuiCommands() {
    // Called every frame on main thread
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!gui_command_queue_.empty()) {
      auto cmd = gui_command_queue_.front();
      gui_command_queue_.pop();
      cmd.action();
    }
  }
};

// In RPC handler:
std::promise<bool> promise;
auto future = promise.get_future();

test_manager_->EnqueueGuiCommand([&]() {
  // This runs on main thread
  ImGuiTestEngine_ItemClick(...);
  promise.set_value(true);
});

future.wait();  // Block RPC until GUI operation completes
```

## ✅ Success Checklist

After completing implementation, verify:

- [ ] All RPC handlers compile without errors
- [ ] Server starts with `--enable_test_harness` flag
- [ ] Ping RPC returns YAZE version
- [ ] Click RPC can click at least one button successfully
- [ ] Type RPC can input text into at least one field
- [ ] Wait RPC can wait for window to appear
- [ ] Assert RPC can validate window visibility
- [ ] End-to-end test script runs without errors
- [ ] No crashes when clicking non-existent widgets
- [ ] Error messages are helpful for debugging

## 🚀 Next Steps

After IT-01 Phase 2 completion:

1. **Integration with z3ed CLI** (`agent test` command)
2. **Python client library** for easier scripting
3. **Policy evaluation** (AW-04) for gating operations
4. **Windows testing** to verify cross-platform
5. **CI integration** for automated GUI tests

## 📚 References

- **ImGuiTestEngine API**: `src/lib/imgui_test_engine/imgui_te_engine.h`
- **TestManager**: `src/app/test/test_manager.h`
- **gRPC Proto**: `src/app/core/proto/imgui_test_harness.proto`
- **YAZE Test Examples**: `src/app/test/*_test.cc`

---

**Document Version**: 1.0  
**Last Updated**: October 1, 2025  
**Estimated Completion**: 6-8 hours active coding  
**Status**: Ready for implementation 🚀
