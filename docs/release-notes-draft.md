# yaze Release Notes (Draft)

**Release Version**: v0.2.0 (Proposed)
**Release Date**: 2025-11-20 (Target)
**Branch**: feat/http-api-phase2 → develop → master
**PR**: #49

---

## Overview

This release focuses on **build system stabilization** across all three major platforms (Windows, Linux, macOS), introduces a **groundbreaking HTTP REST API** for external agent access, and delivers major improvements to the **AI infrastructure**. After resolving 2+ weeks of Windows build blockers and implementing comprehensive testing infrastructure, yaze is ready for broader adoption.

**Key Highlights**:
- HTTP REST API server for automation and external tools
- Complete Windows build fixes (std::filesystem, exception handling)
- Unified AI model registry supporting multiple providers
- Comprehensive testing infrastructure with release checklists
- Symbol conflict resolution across all platforms
- Enhanced build system with 11 new CMake presets

---

## New Features

### HTTP REST API Server (Phase 2)

The z3ed CLI tool now includes an optional HTTP REST API server for external automation and integration:

**Features**:
- **Optional at build time**: Controlled via `YAZE_ENABLE_HTTP_API` CMake flag
- **Secure by default**: Defaults to localhost binding, opt-in for remote access
- **Conditional compilation**: Zero overhead when disabled
- **Well-documented**: Comprehensive API docs at `src/cli/service/api/README.md`

**Initial Endpoints**:
- `GET /api/v1/health` - Server health check
- `GET /api/v1/models` - List available AI models from all providers

**Usage**:
```bash
# Enable HTTP API at build time
cmake --preset mac-ai -DYAZE_ENABLE_HTTP_API=ON
cmake --build --preset mac-ai --target z3ed

# Launch with HTTP server
./build_ai/bin/z3ed --http-port=8080 --http-host=localhost

# Test endpoints
curl http://localhost:8080/api/v1/health
curl http://localhost:8080/api/v1/models
```

**CLI Flags**:
- `--http-port=<port>` - Port to listen on (default: 8080)
- `--http-host=<host>` - Host to bind to (default: localhost)

### Unified Model Registry

Cross-provider AI model management for consistent model discovery:

**Features**:
- Singleton `ModelRegistry` class for centralized model tracking
- Support for Ollama and Gemini providers (extensible design)
- Unified `ListAllModels()` API for all providers
- Model information caching for performance
- Foundation for future UI unification

**Developer API**:
```cpp
#include "cli/service/ai/model_registry.h"

// Get all models from all providers
auto all_models = ModelRegistry::Get().ListAllModels();

// Get models from specific provider
auto ollama_models = ModelRegistry::Get().ListModelsByProvider("ollama");
```

### Enhanced Build System

**11 New CMake Presets** across all platforms:

**macOS**:
- `mac-dbg`, `mac-dbg-v` - Debug builds (verbose variant)
- `mac-rel` - Release build
- `mac-dev` - Development build with ROM tests
- `mac-ai` - AI-enabled build with gRPC
- `mac-uni` - Universal binary (ARM64 + x86_64)

**Linux**:
- `lin-dbg`, `lin-dbg-v` - Debug builds (verbose variant)
- `lin-rel` - Release build
- `lin-dev` - Development build with ROM tests
- `lin-ai` - AI-enabled build with gRPC

**Windows**:
- Existing presets enhanced with better compiler detection

**Key Improvements**:
- Platform-specific optimization flags
- Consistent build directory naming
- Verbose variants for debugging build issues
- AI presets bundle gRPC, agent UI, and HTTP API support

### Comprehensive Testing Infrastructure

**New Documentation**:
- `docs/internal/testing/README.md` - Master testing guide
- `docs/public/developer/testing-quick-start.md` - 5-minute pre-push checklist
- `docs/internal/testing/integration-plan.md` - 6-week rollout plan
- `docs/internal/release-checklist-template.md` - Release validation template

**New Scripts**:
- `scripts/pre-push.sh` - Fast local validation (<2 minutes)
- `scripts/install-git-hooks.sh` - Easy git hook installation
- `scripts/agents/run-tests.sh` - Agent-friendly test runner
- `scripts/agents/smoke-build.sh` - Quick build verification
- `scripts/agents/test-http-api.sh` - HTTP API endpoint testing
- `scripts/agents/get-gh-workflow-status.sh` - CLI-based CI monitoring
- `scripts/agents/windows-smoke-build.ps1` - Windows smoke test helper

**CI/CD Enhancements**:
- `workflow_dispatch` trigger with `enable_http_api_tests` parameter
- Platform-specific build and test jobs
- Conditional HTTP API testing in CI
- Improved artifact uploads on failures

### Agent Collaboration Framework

**New Documentation**:
- `docs/internal/agents/coordination-board.md` - Multi-agent coordination protocol
- `docs/internal/agents/personas.md` - Agent role definitions
- `docs/internal/agents/initiative-template.md` - Task planning template
- `docs/internal/agents/claude-gemini-collaboration.md` - Team structures
- `docs/internal/agents/agent-leaderboard.md` - Contribution tracking
- `docs/internal/agents/gh-actions-remote.md` - Remote CI triggers

### Build Environment Improvements

**Sandbox/Offline Support**:
- Homebrew fallback for `yaml-cpp` (already existed, documented)
- Homebrew fallback for `googletest` (newly added)
- Better handling of network-restricted environments
- Updated `docs/public/build/build-from-source.md` with offline instructions

**Usage**:
```bash
# macOS: Install dependencies locally
brew install yaml-cpp googletest

# Configure with local dependencies
cmake --preset mac-dbg
```

---

## Bug Fixes

### Windows Platform Fixes

#### 1. std::filesystem Compilation Errors (2+ Week Blocker)

**Commits**: b556b155a5, 19196ca87c, cbdc6670a1, 84cdb09a5b, 43118254e6

**Problem**: Windows builds failing with `error: 'filesystem' file not found`
- clang-cl on GitHub Actions Windows Server 2022 couldn't find `std::filesystem`
- Compiler defaulted to pre-C++17 mode, exposing only `std::experimental::filesystem`
- Build logs showed `-std=c++23` (Unix-style) instead of `/std:c++latest` (MSVC-style)

**Root Cause**:
- clang-cl requires MSVC-style `/std:c++latest` flag to access modern MSVC STL
- Detection logic using `CMAKE_CXX_SIMULATE_ID` and `CMAKE_CXX_COMPILER_FRONTEND_VARIANT` wasn't triggering in CI

**Solution**:
- Apply `/std:c++latest` unconditionally on Windows (safe for both MSVC and clang-cl)
- Simplified approach after multiple detection attempts failed

**Impact**: Resolves all Windows std::filesystem compilation errors in:
- `src/util/platform_paths.h`
- `src/util/platform_paths.cc`
- `src/util/file_util.cc`
- All other files using `<filesystem>`

#### 2. Exception Handling Disabled (Critical)

**Commit**: 0835555d04

**Problem**: Windows build failing with `error: cannot use 'throw' with exceptions disabled`
- Code in `file_util.cc` and `platform_paths.cc` uses C++ exception handling
- clang-cl wasn't enabling exceptions by default

**Solution**:
- Add `/EHsc` compiler flag for clang-cl on Windows
- Flag enables C++ exception handling with standard semantics

**Impact**: Resolves compilation errors in all files using `throw`, `try`, `catch`

#### 3. Abseil Include Path Issues

**Commit**: c2bb90a3f1

**Problem**: clang-cl couldn't find Abseil headers like `absl/status/status.h`
- When `YAZE_ENABLE_GRPC=ON`, Abseil comes bundled with gRPC via CPM
- Include paths from bundled targets weren't propagating correctly with Ninja + clang-cl

**Solution**:
- Explicitly add Abseil source directory to `yaze_util` include paths on Windows
- Ensures clang-cl can find all Abseil headers

**Impact**: Fixes Windows build failures for all Abseil-dependent code

### Linux Platform Fixes

#### 1. FLAGS Symbol Conflicts (Critical Blocker)

**Commits**: eb77bbeaff, 43a0e5e314

**Problem**: Linux build failing with multiple definition errors
- `FLAGS_rom` and `FLAGS_norom` defined in both `flags.cc` and `emu_test.cc`
- `FLAGS_quiet` undefined reference errors
- ODR (One Definition Rule) violations

**Root Cause**:
- `yaze_emu_test` linked to `yaze_editor` → `yaze_agent` → `flags.cc`
- Emulator test defined its own flags conflicting with agent flags
- `FLAGS_quiet` was defined in `cli_main.cc` instead of shared `flags.cc`

**Solutions**:
1. Move `FLAGS_quiet` definition to `flags.cc` (shared location)
2. Change `cli_main.cc` to use `ABSL_DECLARE_FLAG` (declaration only)
3. Rename `emu_test.cc` flags to unique names (`FLAGS_emu_test_rom`)
4. Remove `yaze_editor` and `yaze_app_core_lib` dependencies from `yaze_emu_test`

**Impact**: Resolves all Linux symbol conflict errors, clean builds on Ubuntu 22.04

#### 2. Circular Dependency in Graphics Libraries

**Commit**: 0812a84a22

**Problem**: Circular dependency between `yaze_gfx_render`, `yaze_gfx_core`, and `yaze_gfx_debug`
- `AtlasRenderer` (in render) depends on `Bitmap` (core) and `PerformanceProfiler` (debug)
- `PerformanceDashboard` (in debug) calls `AtlasRenderer::Get()`
- Circular dependency chain: render → core → debug → render

**Solution**:
- Move `atlas_renderer.cc` from `GFX_RENDER_SRC` to `GFX_CORE_SRC`
- `atlas_renderer` now lives in layer 4 (core) where it can access both debug and render
- Eliminates circular dependency while preserving functionality

**Impact**: Clean dependency graph, faster link times

#### 3. Missing yaze_gfx_render Dependency

**Commit**: e36d81f357

**Problem**: Linker error where `yaze_gfx_debug.a` called `AtlasRenderer` methods but wasn't linking against `yaze_gfx_render`

**Solution**: Add `yaze_gfx_render` to `yaze_gfx_debug` dependencies

**Impact**: Fixes undefined reference errors on Linux

### macOS Platform Fixes

#### 1. z3ed Linker Error

**Commit**: 9c562df277

**Problem**: z3ed CLI tool failing to link with `library 'yaze_app_core_lib' not found`
- z3ed (via yaze_agent) depends on `yaze_app_core_lib`
- Library only created when `YAZE_BUILD_APP=ON` (which doesn't exist)
- Standalone z3ed builds failed

**Root Cause**:
- `yaze_app_core_lib` creation guarded by incorrect condition
- Should be available whenever agent features needed

**Solution**:
1. Create `src/app/app_core.cmake` with `yaze_app_core_lib` creation
2. Modify `src/app/app.cmake` to include `app_core.cmake`, then conditionally build `yaze` executable
3. Include `app/app.cmake` whenever `YAZE_BUILD_GUI OR YAZE_BUILD_Z3ED OR YAZE_BUILD_TESTS`

**Impact**: z3ed builds successfully on macOS, clean separation of library vs executable

### Code Quality Fixes

#### 1. clang-format Violations (CI Blocker)

**Commits**: bb5e2002c2, fa3da8fc27, 14d1f5de4c

**Problem**: CI failing with 38+ formatting violations
- TUI files had indentation issues
- Third-party libraries (src/lib/*) were being formatted

**Solutions**:
1. Update `CMakeLists.txt` to exclude `src/lib/*` from format targets
2. Apply clang-format to all source files
3. Fix specific violations in `chat_tui.cc`, `tui.cc`, `unified_layout.cc`

**Impact**: Clean code formatting, CI Code Quality job passes

#### 2. Flag Parsing Error Handling

**Commit**: 99e6106721

**Problem**: Inconsistent error handling during flag parsing

**Solution**:
- Add `detail::FlagParseFatal` utility function for fatal errors
- Replace runtime error throws with consistent `FlagParseFatal` calls
- Improve error reporting and program termination

**Impact**: Better error messages, consistent failure handling

---

## Infrastructure Improvements

### Build System Enhancements

**CMake Configuration**:
- Add `YAZE_ENABLE_HTTP_API` option (defaults to `${YAZE_ENABLE_AGENT_CLI}`)
- Add `YAZE_HTTP_API_ENABLED` compile definition when enabled
- Add `YAZE_AI_RUNTIME_AVAILABLE` flag for conditional AI features
- Enhanced conditional compilation support

**Abseil Linking Fix** (Critical):
- Fix Abseil linking bug in `src/util/util.cmake`
- Abseil targets now properly linked when `YAZE_ENABLE_GRPC=OFF`
- Resolves undefined reference errors on all platforms

**Submodule Reorganization**:
- Moved all third-party libraries from `src/lib/` and `third_party/` to unified `ext/` directory
- Better organization and clarity in dependency management
- Updated all CMake paths to point to `ext/`

**Libraries moved**:
- `ext/SDL` (was `src/lib/SDL`)
- `ext/imgui` (was `src/lib/imgui`)
- `ext/asar` (was `src/lib/asar`)
- `ext/httplib` (was `third_party/httplib`)
- `ext/json` (was `third_party/json`)
- `ext/nativefiledialog-extended` (was `src/lib/nativefiledialog-extended`)

### Documentation Overhaul

**New User Documentation**:
- `docs/public/build/quick-reference.md` - Single source of truth for build commands
- `docs/public/developer/testing-quick-start.md` - 5-minute pre-push guide
- `docs/public/examples/README.md` - Usage examples

**New Internal Documentation**:
- Complete agent coordination framework (6 documents)
- Comprehensive testing infrastructure (3 documents)
- Release process documentation (2 documents)
- AI infrastructure handoff documents (2 documents)

**Updated Documentation**:
- `docs/public/build/build-from-source.md` - macOS offline build instructions
- `README.md` - Updated version, features, and build instructions
- `CLAUDE.md` - Enhanced with build quick reference links
- `GEMINI.md` - Added for Gemini-specific guidance

**New Project Documentation**:
- `CONTRIBUTING.md` - Contribution guidelines
- `AGENTS.md` - Agent coordination requirements
- Agent-specific guidance files

### CI/CD Pipeline Improvements

**GitHub Actions Enhancements**:
- Add `workflow_dispatch` trigger with `enable_http_api_tests` boolean input
- Conditional HTTP API test step in test job
- Platform-specific test execution (stable, unit, integration)
- Improved artifact uploads on build/test failures
- CPM dependency caching for faster builds
- sccache/ccache for incremental compilation

**New Workflows**:
- Remote CI triggering via `gh workflow run`
- Optional HTTP API testing in CI
- Better status monitoring for agents

### Testing Infrastructure

**Test Organization**:
- Clear separation: unit (fast), integration (ROM), e2e (GUI), benchmarks
- Platform-specific test execution
- ROM-dependent test gating
- Environment variable configuration support

**Test Helpers**:
- `scripts/pre-push.sh` - Fast local validation
- `scripts/agents/run-tests.sh` - Consistent test execution
- Cross-platform test preset support
- Visual Studio generator detection

**CI Integration**:
- Platform matrix testing (Ubuntu 22.04, macOS 14, Windows 2022)
- Test result uploads
- Failure artifact collection
- Performance regression tracking

---

## Breaking Changes

**None** - This release maintains full backward compatibility with existing ROMs, save files, configuration files, and plugin APIs.

---

## Known Issues

### gRPC Network Fetch in Sandboxed Environments

**Issue**: Smoke builds fail in network-restricted environments (e.g., Claude Code sandbox) due to gRPC GitHub fetch
**Workaround**: Use GitHub Actions CI for validation instead of local builds
**Status**: Won't fix - gRPC is too large for Homebrew fallback approach

### Platform-Specific Considerations

**Windows**:
- Requires Visual Studio 2022 with "Desktop development with C++" workload
- gRPC builds take 15-20 minutes first time (use vcpkg for faster builds)
- Watch for path length limits: Enable long paths with `git config --global core.longpaths true`

**macOS**:
- gRPC v1.67.1 is the tested stable version for ARM64
- Bundled Abseil used by default to avoid deployment target mismatches

**Linux**:
- Requires GCC 12+ or Clang 16+
- Install dependencies: `libgtk-3-dev`, `libdbus-1-dev`, `pkg-config`

---

## Migration Guide

No migration required - this release is fully backward compatible.

---

## Upgrade Instructions

### For Users

1. **Download the latest release** from GitHub Releases (when available)
2. **Extract the archive** to your preferred location
3. **Run the application**:
   - Windows: `yaze.exe`
   - macOS: `yaze.app`
   - Linux: `./yaze`

### For Developers

#### Updating from Previous Version

```bash
# Update your repository
git checkout develop
git pull origin develop

# Update submodules (important - new ext/ structure)
git submodule update --init --recursive

# Clean old build (recommended due to submodule moves)
rm -rf build build_test

# Verify build environment
./scripts/verify-build-environment.sh --fix  # macOS/Linux
.\scripts\verify-build-environment.ps1 -FixIssues  # Windows

# Build with new presets
cmake --preset mac-dbg  # or lin-dbg, win-dbg
cmake --build --preset mac-dbg --target yaze
```

#### Testing HTTP API Features

```bash
# Build with HTTP API enabled
cmake --preset mac-ai -DYAZE_ENABLE_HTTP_API=ON
cmake --build --preset mac-ai --target z3ed

# Launch with HTTP server
./build/bin/z3ed --http-port=8080

# Test in another terminal
curl http://localhost:8080/api/v1/health
curl http://localhost:8080/api/v1/models
```

---

## Credits

This release was made possible through collaboration between multiple AI agents and human oversight:

**Development**:
- CLAUDE_AIINF - Windows build fixes, Linux symbol resolution, HTTP API implementation
- CLAUDE_CORE - Code quality fixes, UI infrastructure
- CLAUDE_TEST_COORD - Testing infrastructure, release checklists
- GEMINI_AUTOM - CI/CD enhancements, Windows exception handling fix
- CODEX - Documentation coordination, release preparation

**Platform Testing**:
- CLAUDE_MAC_BUILD - macOS platform validation
- CLAUDE_LIN_BUILD - Linux platform validation
- CLAUDE_WIN_BUILD - Windows platform validation

**Project Maintainer**:
- scawful - Project oversight, requirements, and direction

---

## Statistics

**Commits**: 31 commits on feat/http-api-phase2 branch
**Files Changed**: 400+ files modified
**Lines Changed**: ~50,000 lines (additions + deletions)
**Build Fixes**: 8 critical platform-specific fixes
**New Features**: 2 major (HTTP API, Model Registry)
**New Documentation**: 15+ new docs, 10+ updated
**New Scripts**: 7 helper scripts for testing and CI
**Test Infrastructure**: Complete overhaul with 6-week rollout plan

---

## Looking Forward

### Next Release (v0.3.0)

**Planned Features**:
- UI unification using ModelRegistry (Phase 3)
- Additional HTTP API endpoints (ROM operations, dungeon/overworld editing)
- Enhanced agent collaboration features
- Performance optimizations
- More comprehensive test coverage

**Infrastructure Goals**:
- Phase 2-5 testing infrastructure rollout (12 weeks remaining)
- Symbol conflict detection automation
- CMake configuration validation
- Platform matrix testing expansion

### Long-Term Roadmap

See `docs/internal/roadmaps/2025-11-modernization.md` for detailed plans.

---

## Release Notes History

- **v0.2.0 (2025-11-20)**: HTTP API, build system stabilization, testing infrastructure
- **v0.1.0 (Previous)**: Initial release with GUI editor, Asar integration, ZSCustomOverworld support

---

**Prepared by**: CODEX_RELEASE_PREP
**Date**: 2025-11-20
**Status**: DRAFT - Ready for review
