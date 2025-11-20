# Windows Build Guide - Common Pitfalls and Solutions

**Last Updated**: 2025-11-20
**Maintainer**: CLAUDE_WIN_WARRIOR

## Overview

This guide documents Windows-specific build issues and their solutions, focusing on the unique challenges of building yaze with MSVC/clang-cl toolchains.

## Critical Configuration: Compiler Detection

### Issue: CMake Misidentifies clang-cl as GNU-like Compiler

**Symptom**:
```
-- The CXX compiler identification is Clang X.X.X with GNU-like command-line
error: cannot use 'throw' with exceptions disabled
```

**Root Cause**:
When `CC` and `CXX` are set to `sccache clang-cl` (with sccache wrapper), CMake's compiler detection probes `sccache.exe` and incorrectly identifies it as a GCC-like compiler instead of MSVC-compatible clang-cl.

**Result**:
- `/EHsc` (exception handling) flag not applied
- Wrong compiler feature detection
- Missing MSVC-specific definitions
- Build failures in code using exceptions

**Solution**:
Use CMAKE_CXX_COMPILER_LAUNCHER instead of wrapping the compiler command:

```powershell
# ❌ WRONG - Causes misdetection
echo "CC=sccache clang-cl" >> $env:GITHUB_ENV
echo "CXX=sccache clang-cl" >> $env:GITHUB_ENV

# ✅ CORRECT - Preserves clang-cl detection
echo "CC=clang-cl" >> $env:GITHUB_ENV
echo "CXX=clang-cl" >> $env:GITHUB_ENV
echo "CMAKE_CXX_COMPILER_LAUNCHER=sccache" >> $env:GITHUB_ENV
echo "CMAKE_C_COMPILER_LAUNCHER=sccache" >> $env:GITHUB_ENV
```

**Implementation**: See `.github/actions/setup-build/action.yml` lines 69-76

## MSVC vs clang-cl Differences

### Exception Handling

**MSVC Flag**: `/EHsc`
**Purpose**: Enable C++ exception handling
**Auto-applied**: Only when CMake correctly detects MSVC/clang-cl

```cmake
# In cmake/utils.cmake
if(MSVC)
    target_compile_options(yaze_common INTERFACE /EHsc)  # Line 44
endif()
```

### Runtime Library

**Setting**: `CMAKE_MSVC_RUNTIME_LIBRARY`
**Value**: `MultiThreaded$<$<CONFIG:Debug>:Debug>`
**Why**: Match vcpkg static triplets

```cmake
# CMakeLists.txt lines 13-15
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()
```

## Abseil Include Propagation

### Issue: Abseil Headers Not Found

**Symptom**:
```
fatal error: 'absl/status/status.h' file not found
```

**Cause**: Abseil's include directories not properly propagated through CMake targets

**Solution**: Ensure bundled Abseil is used and properly linked:
```cmake
# cmake/dependencies.cmake
CPMAddPackage(
  NAME abseil-cpp
  ...
)
target_link_libraries(my_target PUBLIC absl::status absl::statusor ...)
```

**Verification**:
```powershell
# Check compile commands include Abseil paths
cmake --build build --target my_target --verbose | Select-String "abseil"
```

## gRPC Build Time

**First Build**: 15-20 minutes (gRPC compilation)
**Incremental**: <5 minutes (with ccache/sccache)

**Optimization**:
1. Use vcpkg for prebuilt gRPC: `vcpkg install grpc:x64-windows-static`
2. Enable sccache: Already configured in CI
3. Use Ninja generator: Faster than MSBuild

## Path Length Limits

Windows has a 260-character path limit by default.

**Symptom**:
```
fatal error: filename or extension too long
```

**Solution**:
```powershell
# Enable long paths globally
git config --global core.longpaths true

# Or via registry (requires admin)
Set-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name 'LongPathsEnabled' -Value 1
```

**Already Applied**: CI setup (setup-build action line 58)

## Common Build Errors

### 1. "cannot use 'throw' with exceptions disabled"

**Diagnosis**: Compiler misdetection issue (see above)
**Fix**: Use CMAKE_CXX_COMPILER_LAUNCHER for sccache

### 2. "unresolved external symbol" errors

**Diagnosis**: Runtime library mismatch
**Check**:
```powershell
# Verify /MT or /MTd is used
cmake --build build --verbose | Select-String "/MT"
```

### 3. Abseil symbol conflicts (kError, kFatal, etc.)

**Diagnosis**: Multiple Abseil versions or improper include propagation
**Fix**: Use bundled Abseil, ensure proper target linking

### 4. std::filesystem not found

**Diagnosis**: clang-cl needs `/std:c++latest` explicitly
**Already Fixed**: CMake adds this flag automatically

## Debugging Build Issues

### 1. Check Compiler Detection
```powershell
cmake --preset ci-windows 2>&1 | Select-String "compiler identification"
# Should show: "The CXX compiler identification is Clang X.X.X"
# NOT: "with GNU-like command-line"
```

### 2. Verify Compile Commands
```powershell
cmake --build build --target yaze_agent --verbose | Select-String "/EHsc"
# Should show /EHsc flag in compile commands
```

### 3. Check Include Paths
```powershell
cmake --build build --target yaze_agent --verbose | Select-String "abseil"
# Should show -I flags pointing to abseil include dirs
```

### 4. Test Locally Before CI
```powershell
# Use same preset as CI
cmake --preset ci-windows
cmake --build --preset ci-windows
```

## CI-Specific Configuration

### Presets Used
- `ci-windows`: Core build (gRPC enabled, AI disabled)
- `ci-windows-ai`: Full stack (gRPC + AI runtime)

### Environment
- OS: windows-2022 (GitHub Actions)
- Compiler: clang-cl 20.1.8 (LLVM)
- Cache: sccache (500MB)
- Generator: Ninja Multi-Config

### Workflow File
`.github/workflows/ci.yml` lines 66-102 (Build job)

## Quick Troubleshooting Checklist

- [ ] Is CMAKE_MSVC_RUNTIME_LIBRARY set correctly?
- [ ] Is compiler detected as clang-cl (not GNU-like)?
- [ ] Is /EHsc present in compile commands?
- [ ] Are Abseil include paths in compile commands?
- [ ] Is sccache configured as launcher (not wrapper)?
- [ ] Are long paths enabled (git config)?
- [ ] Is correct preset used (ci-windows, not lin/mac)?

## Related Documentation

- Main build docs: `docs/public/build/build-from-source.md`
- Build troubleshooting: `docs/public/build/BUILD-TROUBLESHOOTING.md`
- Quick reference: `docs/public/build/quick-reference.md`
- CMake presets: `CMakePresets.json`
- Compiler flags: `cmake/utils.cmake`

## Contact

For Windows build issues, tag @CLAUDE_WIN_WARRIOR in coordination board.

---

**Change Log**:
- 2025-11-20: Initial guide - compiler detection fix for CI run #19529930066
