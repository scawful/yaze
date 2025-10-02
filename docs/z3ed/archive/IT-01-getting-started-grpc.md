# IT-01 Getting Started: gRPC Implementation

**Goal**: Add gRPC-based ImGuiTestHarness to YAZE for automated GUI testing  
**Timeline**: 10-14 hours over 2-3 days  
**Current Phase**: Setup & Prototype

## Quick Start Checklist

- [ ] **Step 1**: Add gRPC to vcpkg.json (15 min)
- [ ] **Step 2**: Update CMakeLists.txt (30 min)
- [ ] **Step 3**: Create minimal .proto (15 min)
- [ ] **Step 4**: Build and verify (30 min)
- [ ] **Step 5**: Implement Ping service (1 hour)
- [ ] **Step 6**: Test with grpcurl (15 min)
- [ ] **Step 7**: Implement Click handler (2 hours)
- [ ] **Step 8**: Add remaining operations (3-4 hours)
- [ ] **Step 9**: CLI client integration (2 hours)
- [ ] **Step 10**: Windows testing (2-3 hours)

## Step-by-Step Implementation

### Step 0: Read Dependency Management Guide (5 min)

⚠️ **IMPORTANT**: Before adding gRPC, understand the existing infrastructure:
- **[DEPENDENCY_MANAGEMENT.md](DEPENDENCY_MANAGEMENT.md)** - Cross-platform build strategy

**Good News**: YAZE already has gRPC support via CMake FetchContent!
- ✅ `cmake/grpc.cmake` exists with gRPC v1.70.1 + Protobuf v29.3
- ✅ Builds from source (no vcpkg needed for gRPC)
- ✅ Works identically on macOS, Linux, Windows
- ✅ Statically linked (no DLL issues)

**What We Need To Do**:
1. Test that existing gRPC infrastructure works
2. Add CMake option to enable gRPC (currently always-off)
3. Create `.proto` schema for ImGuiTestHarness
4. Implement gRPC service using existing `target_add_protobuf()` helper

### Step 1: Verify Existing gRPC Infrastructure (30 min)

**Goal**: Confirm `cmake/grpc.cmake` works on your system

```bash
cd /Users/scawful/Code/yaze

# Check existing gRPC infrastructure
cat cmake/grpc.cmake | head -20
# Should show FetchContent_Declare for grpc v1.70.1

# Create isolated test build
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON

# This will:
# 1. Download gRPC v1.70.1 from GitHub (~100MB, 2-3 min)
# 2. Download Protobuf v29.3 from GitHub (~50MB, 1-2 min)  
# 3. Build both from source (~15-20 min first time)
# 4. Cache everything in build-grpc-test/ for reuse

# Watch for:
#   -- Fetching grpc...
#   -- Fetching protobuf...
#   -- Building CXX object (lots of output)...

# If this fails, gRPC infrastructure needs fixing first
```

**Expected Output**:
```
-- Fetching grpc...
-- Populating grpc
-- Fetching protobuf...
-- Populating protobuf
-- Building grpc (this will take 15-20 minutes)...
[lots of compilation output]
-- Configuring done
-- Generating done
```

**Success Criteria**:
- ✅ FetchContent downloads gRPC + Protobuf successfully
- ✅ Both build without errors
- ✅ No need to install anything (all in build directory)

**If This Fails**: Stop here, investigate errors in `build-grpc-test/CMakeFiles/CMakeError.log`

### Step 2: Add CMake Option for gRPC (15 min)

**Goal**: Make gRPC opt-in via CMake flag

The existing `cmake/grpc.cmake` is always included, but we need to make it optional.

Add to `CMakeLists.txt` (after `project(yaze VERSION ...)`, around line 50):

```cmake
# Optional gRPC support for ImGuiTestHarness
option(YAZE_WITH_GRPC "Enable gRPC-based ImGuiTestHarness (experimental)" OFF)

if(YAZE_WITH_GRPC)
  message(STATUS "✓ gRPC support enabled (FetchContent will download source)")
  message(STATUS "  Note: First build takes 15-20 minutes to compile gRPC")
  
  # Include existing gRPC infrastructure
  include(cmake/grpc.cmake)
  
  # Pass to source code
  add_compile_definitions(YAZE_WITH_GRPC)
  
  set(YAZE_HAS_GRPC TRUE)
else()
  message(STATUS "○ gRPC support disabled (set YAZE_WITH_GRPC=ON to enable)")
  set(YAZE_HAS_GRPC FALSE)
endif()
```

**Why This Works**:
- `cmake/grpc.cmake` already uses FetchContent to download + build gRPC
- It provides `target_add_protobuf(target proto_file)` helper
- We just need to make it conditional

**Test the CMake change**:
```bash
# Reconfigure with gRPC enabled
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON

# Should show:
#   -- ✓ gRPC support enabled (FetchContent will download source)
#   -- Note: First build takes 15-20 minutes to compile gRPC
#   -- Fetching grpc...

# Without flag (default):
cmake -B build-default

# Should show:
#   -- ○ gRPC support disabled (set YAZE_WITH_GRPC=ON to enable)
```

### Step 2: Update CMakeLists.txt (30 min)

Add to root `CMakeLists.txt`:

```cmake
# Optional gRPC support for ImGuiTestHarness
option(YAZE_WITH_GRPC "Enable gRPC-based ImGuiTestHarness" OFF)

if(YAZE_WITH_GRPC)
  find_package(gRPC CONFIG REQUIRED)
  find_package(Protobuf CONFIG REQUIRED)
  
  message(STATUS "gRPC support enabled")
  message(STATUS "  gRPC version: ${gRPC_VERSION}")
  message(STATUS "  Protobuf version: ${Protobuf_VERSION}")
  
  # Function to generate C++ from .proto
  function(yaze_add_grpc_proto target proto_file)
    get_filename_component(proto_dir ${proto_file} DIRECTORY)
    get_filename_component(proto_name ${proto_file} NAME_WE)
    
    set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc")
    set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h")
    set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")
    set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")
    
    add_custom_command(
      OUTPUT ${proto_srcs} ${proto_hdrs} ${grpc_srcs} ${grpc_hdrs}
      COMMAND protobuf::protoc
        --proto_path=${proto_dir}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${proto_file}
      DEPENDS ${proto_file}
      COMMENT "Generating C++ from ${proto_file}"
    )
    
    target_sources(${target} PRIVATE ${proto_srcs} ${grpc_srcs})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  endfunction()
endif()
```

### Step 3: Create minimal .proto (15 min)

```bash
mkdir -p src/app/core/proto
```

Create `src/app/core/proto/imgui_test_harness.proto`:

```protobuf
syntax = "proto3";

package yaze.test;

// ImGuiTestHarness service for remote GUI testing
service ImGuiTestHarness {
  // Health check
  rpc Ping(PingRequest) returns (PingResponse);
  
  // TODO: Add Click, Type, Wait, Assert
}

message PingRequest {
  string message = 1;
}

message PingResponse {
  string message = 1;
  int64 timestamp_ms = 2;
}
```

### Step 4: Build and verify (20 min)

```bash
# Build YAZE with gRPC enabled
cd /Users/scawful/Code/yaze

cmake --build build-grpc-test --target yaze -j8

# First time: 15-20 minutes (compiling gRPC)
# Subsequent: ~30 seconds (using cached gRPC)

# Watch for:
#   [1/500] Building CXX object (gRPC compilation)
#   ...
#   [500/500] Linking CXX executable yaze
```

**Expected Outcomes**:

✅ **Success**:
- Build completes without errors
- Binary size: `build-grpc-test/bin/yaze.app` is ~10-15MB larger
- `YAZE_WITH_GRPC` preprocessor flag defined in code
- `target_add_protobuf()` function available

⚠️ **Common Issues**:
- **"error: 'absl::...' not found"**: gRPC needs abseil. Check `cmake/absl.cmake` is included.
- **Long compile time**: Normal! gRPC is a large library (~500 source files)
- **Out of disk space**: gRPC build artifacts ~2GB. Clean old builds: `rm -rf build/`

**Verify gRPC is linked**:
```bash
# macOS: Check for gRPC symbols
nm build-grpc-test/bin/yaze.app/Contents/MacOS/yaze | grep grpc | head -5
# Should show symbols like: _grpc_completion_queue_create

# Check binary size
ls -lh build-grpc-test/bin/yaze.app/Contents/MacOS/yaze
# Should be ~60-70MB (vs ~50MB without gRPC)
```

**Rollback**: If anything goes wrong:
```bash
# Delete test build, use original
rm -rf build-grpc-test
cmake --build build --target yaze -j8  # Still works!
```

### Step 5: Implement Ping service (1 hour)

Create `src/app/core/imgui_test_harness_service.h`:

```cpp
#ifndef YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
#define YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_

#ifdef YAZE_WITH_GRPC

#include <memory>
#include <grpcpp/grpcpp.h>
#include "proto/imgui_test_harness.grpc.pb.h"
#include "absl/status/status.h"

namespace yaze {
namespace test {

// Implementation of ImGuiTestHarness gRPC service
class ImGuiTestHarnessServiceImpl final 
    : public ImGuiTestHarness::Service {
 public:
  grpc::Status Ping(
      grpc::ServerContext* context,
      const PingRequest* request,
      PingResponse* response) override;
};

// Singleton server managing the gRPC service
class ImGuiTestHarnessServer {
 public:
  static ImGuiTestHarnessServer& Instance();
  
  // Start server on specified port (default 50051)
  absl::Status Start(int port = 50051);
  
  // Shutdown server gracefully
  void Shutdown();
  
  // Check if server is running
  bool IsRunning() const { return server_ != nullptr; }
  
  int Port() const { return port_; }
  
 private:
  ImGuiTestHarnessServer() = default;
  ~ImGuiTestHarnessServer() { Shutdown(); }
  
  std::unique_ptr<grpc::Server> server_;
  ImGuiTestHarnessServiceImpl service_;
  int port_ = 0;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_IMGUI_TEST_HARNESS_SERVICE_H_
```

Create `src/app/core/imgui_test_harness_service.cc`:

```cpp
#include "app/core/imgui_test_harness_service.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include "absl/strings/str_format.h"

namespace yaze {
namespace test {

// Implement Ping RPC
grpc::Status ImGuiTestHarnessServiceImpl::Ping(
    grpc::ServerContext* context,
    const PingRequest* request,
    PingResponse* response) {
  
  // Echo back the message
  response->set_message(
      absl::StrFormat("Pong: %s", request->message()));
  
  // Add timestamp
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  response->set_timestamp_ms(ms.count());
  
  return grpc::Status::OK;
}

// Singleton instance
ImGuiTestHarnessServer& ImGuiTestHarnessServer::Instance() {
  static ImGuiTestHarnessServer* instance = new ImGuiTestHarnessServer();
  return *instance;
}

absl::Status ImGuiTestHarnessServer::Start(int port) {
  if (server_) {
    return absl::FailedPreconditionError("Server already running");
  }
  
  std::string server_address = absl::StrFormat("127.0.0.1:%d", port);
  
  grpc::ServerBuilder builder;
  
  // Listen on localhost only (security)
  builder.AddListeningPort(server_address, 
                          grpc::InsecureServerCredentials());
  
  // Register service
  builder.RegisterService(&service_);
  
  // Build and start
  server_ = builder.BuildAndStart();
  
  if (!server_) {
    return absl::InternalError(
        absl::StrFormat("Failed to start gRPC server on %s", 
                       server_address));
  }
  
  port_ = port;
  
  std::cout << "✓ ImGuiTestHarness gRPC server listening on " 
            << server_address << "\n";
  
  return absl::OkStatus();
}

void ImGuiTestHarnessServer::Shutdown() {
  if (server_) {
    server_->Shutdown();
    server_.reset();
    port_ = 0;
    std::cout << "✓ ImGuiTestHarness gRPC server stopped\n";
  }
}

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
```

Update `src/CMakeLists.txt` or `src/app/app.cmake`:

```cmake
if(YAZE_WITH_GRPC)
  # Generate gRPC code from .proto
  yaze_add_grpc_proto(yaze 
    ${CMAKE_CURRENT_SOURCE_DIR}/app/core/proto/imgui_test_harness.proto)
  
  # Add service implementation
  target_sources(yaze PRIVATE
    app/core/imgui_test_harness_service.cc
    app/core/imgui_test_harness_service.h)
  
  # Link gRPC libraries
  target_link_libraries(yaze PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf)
  
  # Add compile definition
  target_compile_definitions(yaze PRIVATE YAZE_WITH_GRPC)
endif()
```

### Step 6: Test with grpcurl (15 min)

```bash
# Rebuild with service implementation
cmake --build build --target yaze -j8

# Start YAZE with --enable-test-harness flag
# (You'll need to add this flag handler in main.cc first)
./build/bin/yaze.app/Contents/MacOS/yaze --enable-test-harness &

# Install grpcurl if not already
brew install grpcurl

# Test the Ping RPC
grpcurl -plaintext -d '{"message": "Hello from grpcurl"}' \
  127.0.0.1:50051 yaze.test.ImGuiTestHarness/Ping

# Expected output:
# {
#   "message": "Pong: Hello from grpcurl",
#   "timestamp_ms": "1696204800000"
# }
```

**Success Criteria**: You should see the Pong response with a timestamp!

### Step 7: Add --enable-test-harness flag (30 min)

Add to `src/app/main.cc`:

```cpp
#include "absl/flags/flag.h"

#ifdef YAZE_WITH_GRPC
#include "app/core/imgui_test_harness_service.h"

ABSL_FLAG(bool, enable_test_harness, false,
          "Start gRPC test harness server for automated testing");
ABSL_FLAG(int, test_harness_port, 50051,
          "Port for gRPC test harness (default 50051)");
#endif

// In main() after SDL/ImGui initialization:
#ifdef YAZE_WITH_GRPC
  if (absl::GetFlag(FLAGS_enable_test_harness)) {
    auto& harness = yaze::test::ImGuiTestHarnessServer::Instance();
    auto status = harness.Start(absl::GetFlag(FLAGS_test_harness_port));
    if (!status.ok()) {
      std::cerr << "Failed to start test harness: " 
                << status.message() << "\n";
      return 1;
    }
  }
#endif
```

### Step 8: Implement Click handler (2 hours)

Extend `.proto`:

```protobuf
service ImGuiTestHarness {
  rpc Ping(PingRequest) returns (PingResponse);
  rpc Click(ClickRequest) returns (ClickResponse);  // NEW
}

message ClickRequest {
  string target = 1;  // e.g. "button:Open ROM"
  ClickType type = 2;
  
  enum ClickType {
    LEFT = 0;
    RIGHT = 1;
    DOUBLE = 2;
  }
}

message ClickResponse {
  bool success = 1;
  string message = 2;
  int32 execution_time_ms = 3;
}
```

Implement in service:

```cpp
grpc::Status ImGuiTestHarnessServiceImpl::Click(
    grpc::ServerContext* context,
    const ClickRequest* request,
    ClickResponse* response) {
  
  auto start = std::chrono::steady_clock::now();
  
  // Parse target: "button:Open ROM" -> type=button, label="Open ROM"
  std::string target = request->target();
  size_t colon_pos = target.find(':');
  
  if (colon_pos == std::string::npos) {
    response->set_success(false);
    response->set_message("Invalid target format. Use 'type:label'");
    return grpc::Status::OK;
  }
  
  std::string widget_type = target.substr(0, colon_pos);
  std::string widget_label = target.substr(colon_pos + 1);
  
  // TODO: Integrate with ImGuiTestEngine
  // For now, just simulate success
  
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  
  response->set_success(true);
  response->set_message(
      absl::StrFormat("Clicked %s '%s'", widget_type, widget_label));
  response->set_execution_time_ms(elapsed.count());
  
  return grpc::Status::OK;
}
```

## Next Steps After Prototype

Once you have Ping + Click working:

1. **Add remaining operations** (Type, Wait, Assert, Screenshot) - 3-4 hours
2. **CLI integration** (`z3ed agent test`) - 2 hours
3. **Windows testing** - 2-3 hours
4. **Documentation** - 1 hour

## Windows Testing Checklist

For Windows contributors:

```powershell
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install

# Install dependencies
C:\vcpkg\vcpkg install grpc:x64-windows protobuf:x64-windows

# Build YAZE
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DYAZE_WITH_GRPC=ON -A x64
cmake --build build --config Release

# Test
.\build\bin\Release\yaze.exe --enable-test-harness
```

## Troubleshooting

### "gRPC not found"
```bash
vcpkg install grpc:arm64-osx  # or x64-osx, x64-windows
vcpkg integrate install
```

### "protoc not found"
```bash
vcpkg install protobuf:arm64-osx
export PATH=$PATH:$(vcpkg list protobuf | grep 'tools' | cut -d: -f1)/tools/protobuf
```

### Build errors on Windows
- Use Developer Command Prompt for Visual Studio
- Ensure CMake 3.20+
- Try clean build: `rmdir /s /q build`

## Success Metrics

✅ **Phase 1 Complete When**:
- [ ] gRPC builds without errors
- [ ] Ping RPC responds via grpcurl
- [ ] YAZE starts with `--enable-test-harness` flag

✅ **Phase 2 Complete When**:
- [ ] Click RPC simulates button click
- [ ] Type RPC sends text input
- [ ] Wait RPC polls for conditions
- [ ] Assert RPC validates state

✅ **Phase 3 Complete When**:
- [ ] Windows build succeeds
- [ ] Windows contributor can test
- [ ] CI job runs on Windows

---

**Estimated Total**: 10-14 hours  
**Current Status**: Ready to start Step 1  
**Next Session**: Add gRPC to vcpkg.json and build
