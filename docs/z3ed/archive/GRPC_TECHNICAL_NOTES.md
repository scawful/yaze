# gRPC Technical Notes

**Purpose**: Technical reference for gRPC integration issues and solutions  
**Date**: October 1, 2025  
**Status**: Reference only - issues resolved

---

## Build Configuration (Working)

**gRPC Version**: v1.62.0  
**Build Method**: FetchContent (CMake)  
**C++ Standard**: C++17 (gRPC), C++23 (YAZE)  
**Compiler**: Clang 18.1.8 (Homebrew)  
**Platform**: macOS ARM64, SDK 15.0  

### Critical CMake Settings

```cmake
# Force C++17 for gRPC build (avoid std::result_of removal in C++20)
set(CMAKE_CXX_STANDARD 17)

# Suppress Clang 18 template syntax warnings
add_compile_options(-Wno-error=missing-template-arg-list-after-template-kw)

# Prevent system package interference
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_absl TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_gRPC TRUE)
```

---

## Issues Encountered & Resolved

### Issue 1: Template Syntax Errors (Clang 15+)

**Error**:
```
error: a template argument list is expected after a name prefixed by the template keyword
[-Wmissing-template-arg-list-after-template-kw]
```

**Versions Affected**: gRPC v1.60.0, v1.62.0, v1.68.0, v1.70.1  
**Root Cause**: Clang 15+ enforces stricter C++ template syntax  
**Solution**: Add `-Wno-error=missing-template-arg-list-after-template-kw` compiler flag  
**Status**: ✅ Resolved - warnings demoted to non-fatal

### Issue 2: std::result_of Removed (C++20)

**Error**:
```
error: no template named 'result_of' in namespace 'std'
```

**Versions Affected**: All gRPC versions when built with C++20+  
**Root Cause**: 
- `std::result_of` deprecated in C++17, removed in C++20
- YAZE uses C++23, which propagated to gRPC build
- gRPC v1.62.0 still uses legacy `std::result_of`

**Solution**: Force gRPC to build with C++17 standard  
**Status**: ✅ Resolved - C++17 forced for gRPC, C++23 preserved for YAZE

### Issue 3: absl::if_constexpr Not Found

**Error**:
```
CMake Error: Target "protoc" links to: absl::if_constexpr but the target was not found.
```

**Versions Affected**: gRPC v1.68.0, v1.70.1  
**Root Cause**: Internal dependency version skew in gRPC  
**Solution**: Downgrade to gRPC v1.62.0 (stable version)  
**Status**: ✅ Resolved - v1.62.0 has consistent internal dependencies

### Issue 4: System Package Interference

**Error**:
```
CMake Error: Imported target "protobuf::libprotobuf" includes non-existent path
```

**Root Cause**: 
- Homebrew-installed protobuf/abseil found by CMake
- System versions conflict with gRPC's bundled versions

**Solution**: 
```cmake
set(CMAKE_DISABLE_FIND_PACKAGE_Protobuf TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_absl TRUE)
```

**Status**: ✅ Resolved - system packages ignored during gRPC build

### Issue 5: Incomplete Type (gRPC v1.60.0)

**Error**:
```
error: allocation of incomplete type 'grpc_core::HealthProducer::HealthChecker::HealthStreamEventHandler'
```

**Version Affected**: gRPC v1.60.0 only  
**Root Cause**: Bug in gRPC v1.60.0 health checking code  
**Solution**: Upgrade to v1.62.0  
**Status**: ✅ Resolved - bug fixed in v1.62.0

---

## Version Testing Results

| gRPC Version | C++ Std | Clang 18 | Result | Notes |
|--------------|---------|----------|--------|-------|
| v1.70.1 | C++23 | ❌ | Failed | `absl::if_constexpr` missing |
| v1.68.0 | C++23 | ❌ | Failed | `absl::if_constexpr` missing |
| v1.60.0 | C++23 | ❌ | Failed | Incomplete type error |
| v1.60.0 | C++17 | ❌ | Failed | Incomplete type persists |
| v1.62.0 | C++23 | ❌ | Failed | `std::result_of` removed |
| v1.62.0 | C++17 | ✅ | **SUCCESS** | All issues resolved |

---

## Performance Metrics

**Build Time (First)**: ~20 minutes (gRPC + Protobuf from source)  
**Build Time (Cached)**: ~5-10 seconds  
**Binary Size**: +74 MB (ARM64 with gRPC)  
**RPC Latency**: 2-5ms (Ping test)  
**Memory Overhead**: ~10 MB (server runtime)  

---

## Testing Checklist

### Build Verification
```bash
# Clean build
rm -rf build-grpc-test
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON

# Should complete in ~3-4 minutes (configuration)
# Look for: "-- Configuring done"

# Build
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)

# Should complete in ~20 minutes first time
# Look for: "Built target yaze"
```

### Runtime Verification
```bash
# Start server
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze --enable_test_harness &

# Test Ping
grpcurl -plaintext -d '{"message":"test"}' 127.0.0.1:50051 \
  yaze.test.ImGuiTestHarness/Ping

# Expected: {"message":"Pong: test", "timestampMs":"...", "yazeVersion":"0.3.2"}
```

### All RPCs
```bash
# List services
grpcurl -plaintext 127.0.0.1:50051 list

# Test each RPC
for rpc in Ping Click Type Wait Assert Screenshot; do
  echo "Testing $rpc..."
  grpcurl -plaintext -d '{}' 127.0.0.1:50051 \
    yaze.test.ImGuiTestHarness/$rpc || echo "Failed"
done
```

---

## Future Upgrade Path

### When to Upgrade gRPC

Monitor for:
- ✅ gRPC v1.70+ reaches LTS status
- ✅ Abseil stabilizes `if_constexpr` implementation
- ✅ gRPC removes `std::result_of` usage
- ✅ Community adoption (3+ months old)

### Testing New Versions

```bash
# 1. Update cmake/grpc.cmake
GIT_TAG v1.XX.0

# 2. Test with C++17 first
set(CMAKE_CXX_STANDARD 17)

# 3. Try C++20 if C++17 works
set(CMAKE_CXX_STANDARD 20)

# 4. Test all platforms
# - macOS ARM64 (Clang 18)
# - Ubuntu 22.04 (GCC 11)
# - Windows 11 (MSVC 2022)

# 5. Document any new issues
```

---

## Known Limitations

1. **C++23 Incompatibility**: gRPC v1.62.0 not compatible with C++20+
2. **Build Time**: First build takes 20 minutes (inevitable with FetchContent)
3. **Binary Size**: +74 MB overhead (acceptable for test harness feature)
4. **Compiler Warnings**: ~50 template warnings (non-fatal, gRPC issue)

---

## References

- gRPC Releases: https://github.com/grpc/grpc/releases
- gRPC C++ Docs: https://grpc.io/docs/languages/cpp/
- Abseil Compatibility: https://abseil.io/about/compatibility
- Protobuf Releases: https://github.com/protocolbuffers/protobuf/releases

---

**Maintainer Note**: This file documents resolved issues for historical reference.  
Current working configuration is in `cmake/grpc.cmake`.
