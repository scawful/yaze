# IT-01: ImGuiTestHarness - gRPC Evaluation

**Date**: October 1, 2025  
**Task**: Evaluate gRPC as IPC transport for ImGuiTestHarness  
**Decision**: ✅ RECOMMENDED - gRPC with mitigation strategies for Windows

## Executive Summary

**Recommendation**: Use gRPC with C++ for ImGuiTestHarness IPC layer.

**Key Advantages**:
- Production-grade, battle-tested (used across Google infrastructure)
- Excellent cross-platform support (Windows, macOS, Linux)
- Built-in code generation from Protocol Buffers
- Strong typing and schema evolution
- HTTP/2 based (efficient, multiplexed, streaming support)
- Rich ecosystem and tooling

**Key Challenges & Mitigations**:
- Build complexity on Windows → Use vcpkg for dependency management
- Large dependency footprint → Conditional compilation, static linking
- C++ API complexity → Wrap in simple facade

## Option Comparison Matrix

| Criterion | gRPC | HTTP/REST | Unix Socket | stdin/stdout |
|-----------|------|-----------|-------------|--------------|
| **Cross-Platform** | ✅ Excellent | ✅ Excellent | ⚠️ Unix-only | ✅ Universal |
| **Windows Support** | ✅ Native | ✅ Native | ❌ No | ✅ Native |
| **Performance** | ✅ Fast (HTTP/2) | ⚠️ Moderate | ✅ Fastest | ⚠️ Slow |
| **Type Safety** | ✅ Strong (Protobuf) | ⚠️ Manual (JSON) | ❌ Manual | ❌ Manual |
| **Streaming** | ✅ Built-in | ⚠️ SSE/WebSocket | ✅ Yes | ⚠️ Awkward |
| **Code Gen** | ✅ Automatic | ❌ Manual | ❌ Manual | ❌ Manual |
| **Debugging** | ✅ Good (grpcurl) | ✅ Excellent (curl) | ⚠️ Moderate | ✅ Easy |
| **Build Complexity** | ⚠️ Moderate | ✅ Low | ✅ Low | ✅ None |
| **Dependency Size** | ⚠️ Large (~50MB) | ✅ Small | ✅ None | ✅ None |
| **Learning Curve** | ⚠️ Moderate | ✅ Low | ⚠️ Low | ✅ None |

## Detailed Analysis

### 1. gRPC Architecture for ImGuiTestHarness

```
┌──────────────────────────────────────────────────────────┐
│ Client Layer (z3ed CLI or Python script)                 │
├──────────────────────────────────────────────────────────┤
│  ImGuiTestHarnessClient (generated from .proto)          │
│    • Click(target)                                       │
│    • Type(target, value)                                 │
│    • Wait(condition, timeout)                            │
│    • Assert(condition, expected)                         │
│    • Screenshot() -> bytes                               │
└────────────────────┬─────────────────────────────────────┘
                     │ gRPC over HTTP/2
                     │ localhost:50051
┌────────────────────▼─────────────────────────────────────┐
│ Server Layer (embedded in YAZE)                          │
├──────────────────────────────────────────────────────────┤
│  ImGuiTestHarnessService (implements .proto interface)   │
│    • HandleClick() -> finds ImGui widget, simulates     │
│    • HandleType() -> sends input events                  │
│    • HandleWait() -> polls condition with timeout        │
│    • HandleAssert() -> evaluates condition, returns     │
│    • HandleScreenshot() -> captures framebuffer          │
└──────────────────────────────────────────────────────────┘
                     │
┌────────────────────▼─────────────────────────────────────┐
│ ImGui Integration Layer                                   │
├──────────────────────────────────────────────────────────┤
│  • ImGuiTestEngine (existing)                            │
│  • Widget lookup by ID/label                             │
│  • Input event injection                                 │
│  • State queries                                          │
└──────────────────────────────────────────────────────────┘
```

### 2. Protocol Buffer Schema

```protobuf
// src/app/core/imgui_test_harness.proto
syntax = "proto3";

package yaze.test;

// Main service interface
service ImGuiTestHarness {
  // Basic interactions
  rpc Click(ClickRequest) returns (ClickResponse);
  rpc Type(TypeRequest) returns (TypeResponse);
  rpc Wait(WaitRequest) returns (WaitResponse);
  rpc Assert(AssertRequest) returns (AssertResponse);
  
  // Advanced features
  rpc Screenshot(ScreenshotRequest) returns (ScreenshotResponse);
  rpc GetState(GetStateRequest) returns (GetStateResponse);
  
  // Streaming for long operations
  rpc WatchEvents(WatchEventsRequest) returns (stream Event);
}

message ClickRequest {
  string target = 1;  // e.g. "button:Open ROM", "menu:File→Open"
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

message TypeRequest {
  string target = 1;  // e.g. "input:filename"
  string value = 2;
  bool clear_first = 3;
}

message TypeResponse {
  bool success = 1;
  string message = 2;
}

message WaitRequest {
  string condition = 1;  // e.g. "window_visible:Overworld Editor"
  int32 timeout_ms = 2;
  int32 poll_interval_ms = 3;
}

message WaitResponse {
  bool success = 1;
  string message = 2;
  int32 actual_wait_ms = 3;
}

message AssertRequest {
  string condition = 1;  // e.g. "color_at:100,200"
  string expected = 2;   // e.g. "#FF0000"
}

message AssertResponse {
  bool passed = 1;
  string message = 2;
  string actual = 3;    // Actual value found
}

message ScreenshotRequest {
  string region = 1;  // Optional: "window:Overworld", "" for full screen
  string format = 2;  // "png", "jpg"
}

message ScreenshotResponse {
  bytes image_data = 1;
  int32 width = 2;
  int32 height = 3;
}

message GetStateRequest {
  string query = 1;  // e.g. "window:Overworld.visible"
}

message GetStateResponse {
  string value = 1;
  string type = 2;  // "bool", "int", "string", etc.
}

message Event {
  string type = 1;      // "window_opened", "button_clicked", etc.
  string target = 2;
  int64 timestamp = 3;
  map<string, string> metadata = 4;
}

message WatchEventsRequest {
  repeated string event_types = 1;  // Filter events
}
```

### 3. Windows Cross-Platform Strategy

#### Challenge 1: Build System Integration
**Problem**: gRPC requires CMake, Protobuf compiler, and proper library linking on Windows.

**Solution**: Use vcpkg (Microsoft's C++ package manager)
```cmake
# CMakeLists.txt
if(YAZE_WITH_GRPC)
  # vcpkg will handle gRPC + protobuf + dependencies
  find_package(gRPC CONFIG REQUIRED)
  find_package(Protobuf CONFIG REQUIRED)
  
  # Generate C++ code from .proto
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS 
    src/app/core/imgui_test_harness.proto)
  
  # Generate gRPC stubs
  grpc_generate_cpp(GRPC_SRCS GRPC_HDRS 
    ${CMAKE_CURRENT_BINARY_DIR}
    src/app/core/imgui_test_harness.proto)
  
  target_sources(yaze PRIVATE 
    ${PROTO_SRCS} ${GRPC_SRCS}
    src/app/core/imgui_test_harness_service.cc)
  
  target_link_libraries(yaze PRIVATE
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf)
endif()
```

**Windows Setup Instructions**:
```powershell
# Install vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install gRPC (will auto-install dependencies)
.\vcpkg install grpc:x64-windows

# Configure CMake with vcpkg toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DYAZE_WITH_GRPC=ON
```

#### Challenge 2: DLL Hell on Windows
**Problem**: gRPC has many dependencies (zlib, OpenSSL, etc.) that can conflict.

**Solution**: Static linking + isolated builds
```cmake
# Force static linking on Windows
if(WIN32 AND YAZE_WITH_GRPC)
  set(gRPC_USE_STATIC_LIBS ON)
  set(Protobuf_USE_STATIC_LIBS ON)
  
  # Embed dependencies to avoid DLL conflicts
  target_compile_definitions(yaze PRIVATE
    PROTOBUF_USE_DLLS=0
    GPR_STATIC_LINKING=1)
endif()
```

#### Challenge 3: Visual Studio Compatibility
**Problem**: gRPC C++ requires C++14 minimum, MSVC quirks.

**Solution**: Already handled - YAZE uses C++17, vcpkg handles MSVC
```cmake
# vcpkg.json (project root)
{
  "name": "yaze",
  "version-string": "1.0.0",
  "dependencies": [
    "sdl2",
    "imgui",
    "abseil",
    {
      "name": "grpc",
      "features": ["codegen"],
      "platform": "!android"
    }
  ]
}
```

### 4. Implementation Plan

#### Phase 1: Prototype (2-3 hours)

**Step 1.1**: Add gRPC to vcpkg.json
```json
{
  "dependencies": [
    "grpc",
    "protobuf"
  ],
  "overrides": [
    {
      "name": "grpc",
      "version": "1.60.0"
    }
  ]
}
```

**Step 1.2**: Create minimal .proto
```protobuf
syntax = "proto3";
package yaze.test;

service ImGuiTestHarness {
  rpc Ping(PingRequest) returns (PingResponse);
}

message PingRequest {
  string message = 1;
}

message PingResponse {
  string message = 1;
  int64 timestamp = 2;
}
```

**Step 1.3**: Implement service
```cpp
// src/app/core/imgui_test_harness_service.h
#pragma once

#ifdef YAZE_WITH_GRPC

#include <grpcpp/grpcpp.h>
#include "imgui_test_harness.grpc.pb.h"

namespace yaze {
namespace test {

class ImGuiTestHarnessServiceImpl final 
    : public ImGuiTestHarness::Service {
 public:
  grpc::Status Ping(
      grpc::ServerContext* context,
      const PingRequest* request,
      PingResponse* response) override;
  
  // TODO: Add other methods (Click, Type, Wait, Assert)
};

class ImGuiTestHarnessServer {
 public:
  static ImGuiTestHarnessServer& Instance();
  
  absl::Status Start(int port = 50051);
  void Shutdown();
  bool IsRunning() const { return server_ != nullptr; }
  
 private:
  std::unique_ptr<grpc::Server> server_;
  ImGuiTestHarnessServiceImpl service_;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_WITH_GRPC
```

**Step 1.4**: Test on macOS first
```bash
# Build with gRPC
cmake -B build -DYAZE_WITH_GRPC=ON
cmake --build build --target yaze

# Start YAZE with harness
./build/bin/yaze --enable-test-harness

# Test with grpcurl (install via brew install grpcurl)
grpcurl -plaintext -d '{"message": "hello"}' \
  localhost:50051 yaze.test.ImGuiTestHarness/Ping
```

#### Phase 2: Full Implementation (4-6 hours)

**Step 2.1**: Complete .proto with all operations (see schema above)

**Step 2.2**: Implement Click/Type/Wait/Assert handlers
```cpp
grpc::Status ImGuiTestHarnessServiceImpl::Click(
    grpc::ServerContext* context,
    const ClickRequest* request,
    ClickResponse* response) {
  
  auto start = std::chrono::steady_clock::now();
  
  // Parse target: "button:Open ROM" -> type=button, label="Open ROM"
  auto [widget_type, widget_label] = ParseTarget(request->target());
  
  // Find widget via ImGuiTestEngine
  ImGuiTestContext* test_ctx = ImGuiTestEngine_GetTestContext();
  ImGuiTestItemInfo* item = ImGuiTestEngine_FindItemByLabel(
      test_ctx, widget_label.c_str());
  
  if (!item) {
    response->set_success(false);
    response->set_message(
        absl::StrFormat("Widget not found: %s", request->target()));
    return grpc::Status::OK;
  }
  
  // Simulate click
  ImGuiTestEngine_ItemClick(test_ctx, item, request->type());
  
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  
  response->set_success(true);
  response->set_message("Click successful");
  response->set_execution_time_ms(elapsed.count());
  
  return grpc::Status::OK;
}
```

**Step 2.3**: Add CLI client helper
```cpp
// src/cli/handlers/agent_test.cc
#ifdef YAZE_WITH_GRPC

absl::Status AgentTest::RunWithGrpc(absl::string_view prompt) {
  // Connect to gRPC server
  auto channel = grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials());
  auto stub = ImGuiTestHarness::NewStub(channel);
  
  // Generate test commands from prompt via AI
  auto commands = GenerateTestCommands(prompt);
  
  for (const auto& cmd : commands) {
    if (cmd.type == "click") {
      ClickRequest request;
      request.set_target(cmd.target);
      ClickResponse response;
      
      grpc::ClientContext context;
      auto status = stub->Click(&context, request, &response);
      
      if (!status.ok()) {
        return absl::InternalError(
            absl::StrFormat("gRPC error: %s", status.error_message()));
      }
      
      if (!response.success()) {
        return absl::FailedPreconditionError(response.message());
      }
      
      std::cout << "✓ " << cmd.target << " (" 
                << response.execution_time_ms() << "ms)\n";
    }
    // TODO: Handle type, wait, assert
  }
  
  return absl::OkStatus();
}

#endif  // YAZE_WITH_GRPC
```

#### Phase 3: Windows Testing (2-3 hours)

**Step 3.1**: Create Windows build instructions
```markdown
## Windows Build Instructions

### Prerequisites
1. Visual Studio 2019 or 2022 with C++ workload
2. CMake 3.20+
3. vcpkg

### Build Steps
```powershell
# Clone vcpkg (if not already)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install

# Install dependencies
C:\vcpkg\vcpkg install grpc:x64-windows abseil:x64-windows sdl2:x64-windows

# Configure and build YAZE
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DYAZE_WITH_GRPC=ON -A x64
cmake --build build --config Release --target yaze

# Run with test harness
.\build\bin\Release\yaze.exe --enable-test-harness
```
```

**Step 3.2**: Test on Windows VM or ask contributor to test
- Share branch with Windows contributor
- Provide detailed build instructions
- Iterate on any Windows-specific issues

**Step 3.3**: Add CI checks
```yaml
# .github/workflows/build.yml
jobs:
  build-windows-grpc:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git
          .\vcpkg\bootstrap-vcpkg.bat
      
      - name: Install dependencies
        run: .\vcpkg\vcpkg install grpc:x64-windows abseil:x64-windows
      
      - name: Configure
        run: |
          cmake -B build -DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake `
            -DYAZE_WITH_GRPC=ON -A x64
      
      - name: Build
        run: cmake --build build --config Release
      
      - name: Test
        run: .\build\bin\Release\yaze_test.exe
```

### 5. Windows-Specific Mitigations

#### Mitigation 1: Graceful Degradation
```cpp
// Compile-time feature flag
#ifdef YAZE_WITH_GRPC
  // Full gRPC implementation
  ImGuiTestHarnessServer::Instance().Start(50051);
#else
  // Fallback: JSON over stdin/stdout
  ImGuiTestHarness::StartStdioMode();
#endif
```

**Benefits**:
- Windows contributors can build without gRPC if needed
- macOS/Linux developers get full gRPC experience
- Gradual migration path

#### Mitigation 2: Pre-built Binaries
```bash
# For Windows contributors who struggle with vcpkg
# Provide pre-built YAZE.exe with gRPC embedded
# Download from: https://github.com/scawful/yaze/releases/tag/v1.0.0-grpc
```

#### Mitigation 3: Docker Alternative
```dockerfile
# Dockerfile.windows
FROM mcr.microsoft.com/windows/servercore:ltsc2022
WORKDIR /app
COPY . .
RUN vcpkg install grpc:x64-windows
RUN cmake --build build
CMD ["yaze.exe", "--enable-test-harness"]
```

**Benefits**:
- Isolated environment
- Reproducible builds
- No local dependency management

#### Mitigation 4: Clear Documentation
```markdown
# docs/building-with-grpc.md

## Troubleshooting Windows Builds

### Issue: vcpkg install fails
**Solution**: Run PowerShell as Administrator, disable antivirus temporarily

### Issue: Protobuf generation errors
**Solution**: Ensure protoc.exe is in PATH:
  set PATH=%PATH%;C:\vcpkg\installed\x64-windows\tools\protobuf

### Issue: Linker errors (LNK2019)
**Solution**: Clean build directory and rebuild:
  rmdir /s /q build
  cmake -B build -DCMAKE_TOOLCHAIN_FILE=... -DYAZE_WITH_GRPC=ON
  cmake --build build --config Release
```

### 6. Performance Characteristics

**Latency Benchmarks** (estimated):
- Local gRPC call: ~0.5-1ms (HTTP/2, binary protobuf)
- JSON/REST call: ~1-2ms (HTTP/1.1, text JSON)
- Unix socket: ~0.1ms (direct kernel IPC)
- stdin/stdout: ~5-10ms (process pipe, buffering)

**Memory Overhead**:
- gRPC server: ~10MB resident
- Per-connection: ~100KB
- Protobuf messages: ~1KB each

**For YAZE testing**: Latency is acceptable since tests run in milliseconds-to-seconds range.

### 7. Advantages Over Alternatives

#### vs HTTP/REST:
✅ **Better**: Type safety, code generation, streaming, HTTP/2 multiplexing  
✅ **Better**: Smaller wire format (protobuf vs JSON)  
❌ **Worse**: More complex setup

#### vs Unix Domain Socket:
✅ **Better**: Cross-platform (Windows support)  
✅ **Better**: Built-in authentication, TLS  
❌ **Worse**: Slightly higher latency (~0.5ms vs 0.1ms)

#### vs stdin/stdout:
✅ **Better**: Bidirectional, streaming, type safety  
✅ **Better**: Multiple concurrent clients  
❌ **Worse**: More dependencies

### 8. Risks & Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Windows build complexity | High | Medium | vcpkg + documentation + CI |
| Large binary size | Medium | High | Conditional compile + static link |
| Learning curve for contributors | Low | Medium | Good docs + examples |
| gRPC version conflicts | Medium | Low | Pin version in vcpkg.json |
| Network firewall issues | Low | Low | localhost-only binding |

### 9. Decision Matrix

| Factor | Weight | gRPC Score | Weighted |
|--------|--------|------------|----------|
| Cross-platform | 10 | 9 | 90 |
| Windows support | 10 | 8 | 80 |
| Type safety | 8 | 10 | 80 |
| Performance | 7 | 9 | 63 |
| Build simplicity | 6 | 5 | 30 |
| Debugging | 7 | 8 | 56 |
| Future extensibility | 9 | 10 | 90 |
| **TOTAL** | **57** | - | **489/570** |

**Score**: 85.8% - **STRONG RECOMMENDATION**

## Conclusion

✅ **Recommendation**: Proceed with gRPC for ImGuiTestHarness IPC.

**Rationale**:
1. **Cross-platform**: Native Windows support via vcpkg eliminates primary concern
2. **Google ecosystem**: Leverages your expertise, production-grade tool
3. **Type safety**: Protobuf schema prevents runtime errors
4. **Future-proof**: Supports streaming, authentication, observability out of box
5. **Mitigated risks**: Build complexity addressed via vcpkg + documentation

**Implementation Order**:
1. ✅ Phase 1: Prototype on macOS (2-3 hours) - validate approach
2. ✅ Phase 2: Full implementation (4-6 hours) - all operations
3. ✅ Phase 3: Windows testing (2-3 hours) - verify cross-platform
4. ✅ Phase 4: Documentation (1-2 hours) - onboarding guide

**Total Estimate**: 10-14 hours for complete IT-01 with gRPC

## Next Steps

1. **Immediate**: Add gRPC to vcpkg.json and test build on macOS
2. **This week**: Implement Ping/Click/Type operations
3. **Next week**: Test on Windows, gather contributor feedback
4. **Future**: Add streaming, authentication, telemetry

---

**Approved by**: @scawful (Googler, gRPC advocate)  
**Status**: ✅ Ready to implement  
**Priority**: P1 - Critical for agent testing workflow
