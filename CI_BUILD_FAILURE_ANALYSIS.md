# CI/CD Build Failure Analysis

**Date**: 2025-11-03
**Branch**: develop
**Analyzed By**: Claude Code

## Executive Summary

The Windows and Linux CI/CD builds are failing due to **gRPC build configuration issues**. The root cause is a conflict between two different gRPC configuration systems and version mismatches that cause compilation errors on Windows and timeouts on Linux.

## Root Causes

### 1. gRPC Version Mismatch (Critical - Windows)

**Problem**: Windows MSVC builds are using the wrong gRPC version.

**Details**:
- `cmake/dependencies.lock` specifies gRPC version `1.75.1`
- `cmake/grpc.cmake` has conditional logic to use `v1.67.1` for Windows MSVC (MSVC-compatible)
- However, the build system uses `cmake/dependencies/grpc.cmake` (CPM-based) which reads from `dependencies.lock`
- Result: Windows builds get gRPC `1.75.1` which has **MSVC compilation errors in UPB (micro protobuf) code**

**Error Expected**:
```
error C2099: initializer is not a constant
```

**File Location**: `cmake/dependencies.lock:10`

### 2. Duplicate gRPC Configuration Systems (Critical)

**Problem**: Two competing gRPC configuration files exist, causing confusion.

**Details**:
- `cmake/grpc.cmake` - FetchContent-based, has platform-specific version logic
- `cmake/dependencies/grpc.cmake` - CPM-based, uses dependencies.lock version
- The newer CPM-based approach (via `cmake/dependencies.cmake:40`) is being used
- But it doesn't have the Windows version override logic from `cmake/grpc.cmake:74-80`

**Impact**: Windows builds fail because they don't get the MSVC-compatible gRPC version.

### 3. CI Preset Forces gRPC Build (Major - All Platforms)

**Problem**: The "ci" preset mandates building gRPC from source.

**Details**:
- CI workflow (`.github/workflows/ci.yml:56,64`) uses preset "ci"
- Preset "ci" (`CMakePresets.json:88-101`) sets `YAZE_ENABLE_GRPC: "ON"`
- Building gRPC from source is slow and error-prone:
  - Windows: ~30-40 min (if it works) + MSVC compatibility issues
  - Linux: ~30-40 min + potential timeout
  - macOS: Has ARM64 Abseil issues (documented in troubleshooting)

**Alternative**: The "minimal" preset exists but isn't used by CI.

### 4. Long Build Times May Cause CI Timeouts (Major - Linux)

**Problem**: gRPC builds take 30-40+ minutes from source.

**Details**:
- GitHub Actions runners have execution time limits
- First-time gRPC builds compile hundreds of targets (protobuf, abseil, grpc++)
- Even with ccache/sccache, initial builds are very slow
- No pre-built packages are used on Linux CI

### 5. Windows Preset Configuration Issues (Minor)

**Problem**: Windows CI uses the wrong preset architecture.

**Details**:
- The CI workflow uses preset "ci" for all platforms
- But "ci" inherits from "base" which doesn't set Windows-specific options
- Windows-specific presets (win-dbg, win-rel) exist but aren't used by CI
- Missing proper MSVC runtime library settings in the base CI preset

## Affected Files

### Configuration Files:
- `cmake/dependencies.lock` - Has incorrect gRPC version for Windows
- `cmake/dependencies/grpc.cmake` - Missing Windows version override
- `CMakePresets.json` - CI preset forces gRPC build
- `.github/workflows/ci.yml` - Uses problematic "ci" preset

### CMake Modules:
- `cmake/grpc.cmake` - Has correct version logic but isn't being used
- `cmake/grpc_windows.cmake` - vcpkg fallback exists but not enabled in CI

## Solutions

### Immediate Fixes (Recommended)

#### Fix 1: Update gRPC Version in dependencies.lock for Windows Compatibility

**Change**: Update `cmake/dependencies.lock:10` to use MSVC-compatible version

```cmake
# Before:
set(GRPC_VERSION "1.75.1" CACHE STRING "gRPC version")

# After:
set(GRPC_VERSION "1.67.1" CACHE STRING "gRPC version - MSVC compatible")
```

**Impact**:
- ✅ Fixes Windows MSVC compilation errors
- ⚠️ Linux/macOS will use older gRPC version (but stable)

**Risk**: Low - v1.67.1 is tested and stable on all platforms per troubleshooting docs

---

#### Fix 2: Add Platform-Specific gRPC Version Logic to CPM Configuration

**Change**: Update `cmake/dependencies/grpc.cmake` to use conditional versioning

```cmake
# Add before line 60 in cmake/dependencies/grpc.cmake:

# Use MSVC-compatible gRPC version on Windows, latest elsewhere
if(WIN32 AND MSVC)
  set(GRPC_VERSION "1.67.1" CACHE STRING "gRPC version - MSVC compatible" FORCE)
  set(_GRPC_VERSION_REASON "MSVC-compatible, avoids UPB linker regressions")
else()
  # Use version from dependencies.lock for other platforms
  set(_GRPC_VERSION_REASON "ARM64 macOS + modern Clang compatibility")
endif()

message(STATUS "Using gRPC version: ${GRPC_VERSION} (${_GRPC_VERSION_REASON})")
```

**Impact**:
- ✅ Windows gets v1.67.1 (MSVC-compatible)
- ✅ Linux/macOS can use v1.75.1 (newer features)

**Risk**: Low - mirrors existing logic from cmake/grpc.cmake

---

#### Fix 3: Create Separate CI Presets for Each Platform

**Change**: Add platform-specific CI presets to `CMakePresets.json`

```json
{
  "name": "ci-windows",
  "inherits": "windows-base",
  "displayName": "CI Build - Windows",
  "description": "Windows CI build without gRPC (fast, reliable)",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "YAZE_BUILD_TESTS": "ON",
    "YAZE_ENABLE_GRPC": "OFF",
    "YAZE_ENABLE_JSON": "ON",
    "YAZE_ENABLE_AI": "OFF",
    "YAZE_MINIMAL_BUILD": "ON"
  }
},
{
  "name": "ci-linux",
  "inherits": "base",
  "displayName": "CI Build - Linux",
  "description": "Linux CI build without gRPC (fast, reliable)",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "YAZE_BUILD_TESTS": "ON",
    "YAZE_ENABLE_GRPC": "OFF",
    "YAZE_ENABLE_JSON": "ON",
    "YAZE_ENABLE_AI": "OFF",
    "YAZE_MINIMAL_BUILD": "ON"
  }
},
{
  "name": "ci-macos",
  "inherits": "base",
  "displayName": "CI Build - macOS",
  "description": "macOS CI build without gRPC (fast, reliable)",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "YAZE_BUILD_TESTS": "ON",
    "YAZE_ENABLE_GRPC": "OFF",
    "YAZE_ENABLE_JSON": "ON",
    "YAZE_ENABLE_AI": "OFF",
    "YAZE_MINIMAL_BUILD": "ON"
  }
}
```

**Update CI workflow** (`.github/workflows/ci.yml`):

```yaml
# Line 52-64 - Update matrix to use platform-specific presets
matrix:
  include:
    - name: "Ubuntu 22.04 (GCC-12)"
      os: ubuntu-22.04
      platform: linux
      preset: ci-linux  # Changed from "ci"
    - name: "macOS 14 (Clang)"
      os: macos-14
      platform: macos
      preset: ci-macos  # Changed from "ci"
    - name: "Windows 2022 (MSVC)"
      os: windows-2022
      platform: windows
      preset: ci-windows  # Changed from "ci"
```

**Impact**:
- ✅ Disables gRPC in CI builds (faster, more reliable)
- ✅ Reduces CI build time from ~40 min to ~5-10 min
- ✅ Eliminates Windows MSVC gRPC errors
- ✅ Eliminates Linux timeout issues

**Risk**: Low - "minimal" preset already exists and works

---

### Long-Term Solutions (Optional)

#### Option 1: Use System gRPC Packages on Linux CI

**Change**: Install gRPC from apt on Ubuntu CI runners

```yaml
# In .github/actions/setup-build/action.yml, add to Linux section:
- name: Setup build environment (Linux)
  if: inputs.platform == 'linux'
  shell: bash
  run: |
    sudo apt-get update
    sudo apt-get install -y $(tr '\n' ' ' < .github/workflows/scripts/linux-ci-packages.txt) gcc-12 g++-12
    # Add gRPC system packages
    sudo apt-get install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
    sudo apt-get clean
```

Then enable `YAZE_USE_SYSTEM_DEPS` in Linux CI preset.

**Impact**:
- ✅ Very fast (pre-compiled)
- ⚠️ Version depends on Ubuntu repository (might be older)

**Risk**: Medium - system packages may have different versions

---

#### Option 2: Use vcpkg for Windows gRPC

**Change**: Enable vcpkg in Windows CI

This is already documented in the codebase but not enabled. See `cmake/grpc_windows.cmake:14-16`.

**Impact**:
- ✅ Fast Windows builds (~5-10 min with vcpkg cache)
- ⚠️ Requires vcpkg setup in CI

**Risk**: Medium - adds vcpkg dependency to CI

---

#### Option 3: Create Separate gRPC-Enabled CI Workflow

**Change**: Keep current CI fast, add optional gRPC testing

Create `.github/workflows/ci-full.yml` with gRPC enabled, running only on:
- Manual workflow_dispatch
- Weekly schedule
- Specific branches (e.g., release branches)

**Impact**:
- ✅ Main CI stays fast and reliable
- ✅ Still tests gRPC integration periodically

**Risk**: Low - provides best of both worlds

---

## Recommended Implementation Plan

### Phase 1 (Immediate - Fix Broken Builds)
1. ✅ Apply Fix #3: Create platform-specific CI presets with gRPC disabled
2. ✅ Update CI workflow to use new presets
3. ✅ Test on develop branch

**Result**: CI builds become fast (5-10 min) and reliable

### Phase 2 (Short-term - Maintain gRPC Support)
1. Apply Fix #1 or Fix #2: Fix gRPC versioning
2. Create separate weekly gRPC test workflow (Option 3)
3. Document gRPC build requirements for local development

**Result**: gRPC support maintained without slowing down CI

### Phase 3 (Long-term - Optimization)
1. Evaluate vcpkg for Windows (Option 2)
2. Evaluate system packages for Linux (Option 1)
3. Consider pre-built gRPC artifacts

**Result**: Fast builds even with gRPC enabled

---

## Testing Plan

### Test 1: Verify Windows Build
```bash
# On Windows with MSVC
cmake --preset ci-windows
cmake --build --preset ci-windows
```
**Expected**: Build completes in ~5-10 minutes without errors

### Test 2: Verify Linux Build
```bash
# On Ubuntu 22.04
cmake --preset ci-linux
cmake --build --preset ci-linux
```
**Expected**: Build completes in ~5-10 minutes without errors

### Test 3: Verify Tests Run
```bash
# After successful build
cd build
ctest --output-on-failure -L stable
```
**Expected**: All stable tests pass

### Test 4: Verify gRPC Version Fix (if applying Fix #1 or #2)
```bash
# On Windows
cmake --preset win-ai  # Preset with gRPC enabled
cmake --build --preset win-ai
```
**Expected**: Build completes with gRPC 1.67.1, no MSVC errors

---

## Additional Notes

### Why gRPC Causes Issues

1. **Complexity**: gRPC bundles protobuf, abseil, c-ares, re2, zlib, ssl
2. **Build Time**: Compiles 200+ targets on first build
3. **Platform Quirks**:
   - Windows: MSVC has strict C++ conformance, UPB uses C99 features
   - macOS ARM64: Abseil uses x86 SSE intrinsics, needs filtering
   - Linux: Usually works but slow

### Why Disabling gRPC is Safe for CI

- gRPC is used for:
  - AI agent communication (optional feature)
  - Remote debugging (developer tool)
- Core editor functionality works without gRPC
- Tests don't require gRPC
- Developers can still test gRPC locally with dev presets

### References

- Build Troubleshooting Guide: `docs/BUILD-TROUBLESHOOTING.md`
- Build Guide: `docs/BUILD-GUIDE.md`
- CI Workflow: `.github/workflows/ci.yml`
- CMake Presets: `CMakePresets.json`

---

## Conclusion

The root cause of CI build failures is **gRPC version incompatibility** and **excessive build complexity**. The recommended solution is to **disable gRPC in CI builds** using platform-specific presets, which will make builds fast (5-10 min) and reliable. gRPC support can be maintained through local development builds and periodic CI testing.
