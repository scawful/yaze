# Testing Infrastructure - Master Documentation

**Owner**: CLAUDE_TEST_COORD
**Status**: Active
**Last Updated**: 2025-11-20

## Overview

This document serves as the central hub for all testing infrastructure in the yaze project. It coordinates testing strategies across local development, CI/CD, and release validation workflows.

**ROM policy**: GitHub CI runs without ROMs and skips ROM-dependent suites. Run ROM tests locally with `YAZE_ENABLE_ROM_TESTS=ON` and `YAZE_TEST_ROM_*` paths, or force-skip with `YAZE_SKIP_ROM_TESTS=1`.

## Quick Links

- **Developer Quick Start**: [Testing Quick Start Guide](../../public/developer/testing-quick-start.md)
- **Build & Test Commands**: [Quick Reference](../../public/build/quick-reference.md)
- **Existing Testing Guide**: [Testing Guide](../../public/developer/testing-guide.md)
- **Release Checklist**: [Release Checklist](../release-checklist.md)
- **CI/CD Pipeline**: [.github/workflows/ci.yml](../../../.github/workflows/ci.yml)

## Testing Levels

### 1. Unit Tests (`test/unit/`)

**Purpose**: Fast, isolated component tests with no external dependencies.

**Characteristics**:
- Run in <10 seconds total
- No ROM files required
- No GUI initialization
- Primary CI validation layer
- Can run on any platform without setup

**Run Locally**:
```bash
# Build tests
cmake --build build --target yaze_test

# Run only unit tests
./build/bin/yaze_test --unit

# Run specific unit test
./build/bin/yaze_test --gtest_filter="*AsarWrapper*"
```

**Coverage Areas**:
- Core utilities (hex conversion, compression)
- Graphics primitives (tiles, palettes, colors)
- ROM data structures (without actual ROM)
- CLI resource catalog
- GUI widget logic (non-interactive)
- Zelda3 parsers and builders

### 2. Integration Tests (`test/integration/`)

**Purpose**: Test interactions between multiple components.

**Characteristics**:
- Run in <30 seconds total
- May require ROM file (subset marked as ROM-dependent)
- Test cross-module boundaries
- Secondary CI validation layer

**Run Locally**:
```bash
# Run all integration tests
./build/bin/yaze_test --integration

# Run with ROM-dependent tests
./build/bin/yaze_test --integration --rom-dependent --rom-vanilla roms/alttp_vanilla.sfc
```

**Coverage Areas**:
- Asar wrapper + ROM class integration
- Editor system interactions
- AI service integration
- Dungeon/Overworld data loading
- Multi-component rendering pipelines

### 3. End-to-End (E2E) Tests (`test/e2e/`)

**Purpose**: Full user workflows driven by ImGui Test Engine.

**Characteristics**:
- Run in 1-5 minutes
- Require GUI initialization (can run headless in CI)
- Most comprehensive validation
- Simulate real user interactions

**Run Locally**:
```bash
# Run E2E tests (headless)
./build/bin/yaze_test --e2e

# Run E2E tests with visible GUI (for debugging)
./build/bin/yaze_test --e2e --show-gui

# Run specific E2E workflow
./build/bin/yaze_test --e2e --gtest_filter="*DungeonEditorSmokeTest*"
```

**Coverage Areas**:
- Editor smoke tests (basic functionality)
- Canvas interaction workflows
- ROM loading and saving
- ZSCustomOverworld upgrades
- Complex multi-step user workflows

### 4. Benchmarks (`test/benchmarks/`)

**Purpose**: Performance measurement and regression tracking.

**Characteristics**:
- Not run in standard CI (optional job)
- Focus on speed, not correctness
- Track performance trends over time

**Run Locally**:
```bash
./build/bin/yaze_test --benchmark
```

## Test Organization Matrix

| Category | ROM Required | GUI Required | Typical Duration | CI Frequency |
|----------|--------------|--------------|------------------|--------------|
| Unit | No | No | <10s | Every commit |
| Integration | Sometimes | No | <30s | Every commit |
| E2E | Often | Yes (headless OK) | 1-5min | Every commit |
| Benchmarks | No | No | Variable | Weekly/on-demand |

## Test Suites and Labels

Tests are organized into CMake test suites with labels for filtering:

- **`stable`**: Fast tests with no ROM dependency (unit + some integration)
- **`unit`**: Only unit tests
- **`integration`**: Only integration tests
- **`e2e`**: End-to-end GUI tests
- **`rom_dependent`**: Tests requiring a real Zelda3 ROM file

See `test/CMakeLists.txt` for suite definitions.

## Local Testing Workflows

### Pre-Commit: Quick Validation (<30s)

```bash
# Build and run stable tests only
cmake --build build --target yaze_test
./build/bin/yaze_test --unit

# Alternative: use helper script
scripts/agents/run-tests.sh mac-dbg --output-on-failure
```

### Pre-Push: Comprehensive Validation (<5min)

```bash
# Run all tests except ROM-dependent
./build/bin/yaze_test

# Run all tests including ROM-dependent
./build/bin/yaze_test --rom-dependent --rom-vanilla roms/alttp_vanilla.sfc

# Alternative: use ctest with preset
ctest --preset dev
```

### Pre-Release: Full Platform Matrix

See [Release Checklist](../release-checklist.md) for complete validation requirements.

## CI/CD Testing Strategy

### PR Validation Pipeline

**Workflow**: `.github/workflows/ci.yml`

**Jobs**:
1. **Build** (3 platforms: Linux, macOS, Windows)
   - Compile all targets with warnings-as-errors
   - Verify no build regressions

2. **Test** (3 platforms)
   - Run `stable` test suite (fast, no ROM)
   - Run `unit` test suite
   - Run `integration` test suite (non-ROM-dependent)
   - Upload test results and artifacts

3. **Code Quality**
   - clang-format verification
   - cppcheck static analysis
   - clang-tidy linting

4. **z3ed Agent** (optional, scheduled)
   - Full AI-enabled build with gRPC
   - HTTP API testing (when enabled)

**Preset Usage**:
- Linux: `ci-linux`
- macOS: `ci-macos`
- Windows: `ci-windows`

### Remote Workflow Triggers

Agents and developers can trigger workflows remotely:

```bash
# Trigger CI with HTTP API tests enabled
scripts/agents/run-gh-workflow.sh ci.yml -f enable_http_api_tests=true

# Trigger CI with artifact uploads
scripts/agents/run-gh-workflow.sh ci.yml -f upload_artifacts=true
```

See [GH Actions Remote Guide](../agents/archive/utility-tools/gh-actions-remote.md) for details.

### Test Result Artifacts

- Test XML reports uploaded on failure
- Build logs available in job output
- Windows binaries uploaded for debugging

## Platform-Specific Test Considerations

### macOS

- **Stable**: All tests pass reliably
- **Known Issues**: None active
- **Recommended Preset**: `mac-dbg` (debug), `mac-ai` (with gRPC)
- **Smoke Build**: `scripts/agents/smoke-build.sh mac-dbg`

### Linux

- **Stable**: All tests pass reliably
- **Known Issues**: Previous FLAGS symbol conflicts resolved (commit 43a0e5e314)
- **Recommended Preset**: `lin-dbg` (debug), `lin-ai` (with gRPC)
- **Smoke Build**: `scripts/agents/smoke-build.sh lin-dbg`

### Windows

- **Stable**: Build fixes applied (commit 43118254e6)
- **Known Issues**: Previous std::filesystem errors resolved
- **Recommended Preset**: `win-dbg` (debug), `win-ai` (with gRPC)
- **Smoke Build**: `pwsh -File scripts/agents/windows-smoke-build.ps1 -Preset win-dbg`

## Test Writing Guidelines

### Where to Add New Tests

1. **New class `MyClass`**: Add `test/unit/my_class_test.cc`
2. **Testing with ROM**: Add `test/integration/my_class_rom_test.cc`
3. **Testing UI workflow**: Add `test/e2e/my_class_workflow_test.cc`

### Test Structure

All test files should follow this pattern:

```cpp
#include <gtest/gtest.h>
#include "path/to/my_class.h"

namespace yaze {
namespace test {

TEST(MyClassTest, BasicFunctionality) {
  MyClass obj;
  EXPECT_TRUE(obj.DoSomething());
}

TEST(MyClassTest, EdgeCases) {
  MyClass obj;
  EXPECT_FALSE(obj.HandleEmpty());
}

}  // namespace test
}  // namespace yaze
```

### Mocking

Use `test/mocks/` for mock objects:
- `mock_rom.h`: Mock ROM class for testing without actual ROM files
- Add new mocks as needed for isolating components

### Test Utilities

Common helpers in `test/test_utils.h`:
- `LoadRomInTest()`: Load a ROM file in GUI test context
- `OpenEditorInTest()`: Open an editor for E2E testing
- `CreateTestCanvas()`: Initialize a canvas for testing

## Troubleshooting Test Failures

### Common Issues

#### 1. ROM-Dependent Test Failures

**Symptom**: Tests fail with "ROM file not found" or data mismatches

**Solution**:
```bash
# Set ROM path environment variable
export YAZE_TEST_ROM_VANILLA=/path/to/alttp_vanilla.sfc

# Or pass directly
./build/bin/yaze_test --rom-vanilla /path/to/alttp_vanilla.sfc
```

#### 2. GUI Test Failures in CI

**Symptom**: E2E tests fail in headless CI environment

**Solution**: Tests should work headless by default. If failing, check:
- ImGui Test Engine initialization
- SDL video driver (uses "dummy" in headless mode)
- Test marked with proper `YAZE_GUI_TEST_TARGET` definition

#### 3. Platform-Specific Failures

**Symptom**: Tests pass locally but fail in CI on specific platform

**Solution**:
1. Check CI logs for platform-specific errors
2. Run locally with same preset (`ci-linux`, `ci-macos`, `ci-windows`)
3. Use remote workflow trigger to reproduce in CI environment

#### 4. Flaky Tests

**Symptom**: Tests pass sometimes, fail other times

**Solution**:
- Check for race conditions in multi-threaded code
- Verify test isolation (no shared state between tests)
- Add test to `.github/workflows/ci.yml` exclusion list temporarily
- File issue with `flaky-test` label

### Getting Help

1. Check existing issues: https://github.com/scawful/yaze/issues
2. Review test logs in CI job output
3. Ask in coordination board: `docs/internal/agents/coordination-board.md`
4. Tag `CLAUDE_TEST_COORD` for testing infrastructure issues

## Test Infrastructure Roadmap

### Completed

- ✅ Unit, integration, and E2E test organization
- ✅ ImGui Test Engine integration for GUI testing
- ✅ Platform-specific CI matrix (Linux, macOS, Windows)
- ✅ Smoke build helpers for agents
- ✅ Remote workflow triggers
- ✅ Test result artifact uploads

### In Progress

- 🔄 Pre-push testing hooks
- 🔄 Symbol conflict detection tools
- 🔄 CMake configuration validation
- 🔄 Platform matrix testing tools

### Planned

- 📋 Automated test coverage reporting
- 📋 Performance regression tracking
- 📋 Fuzz testing integration
- 📋 ROM compatibility test matrix (different ROM versions)
- 📋 GPU/graphics driver test matrix

## Helper Scripts

All helper scripts are in `scripts/agents/`:

| Script | Purpose | Usage |
|--------|---------|-------|
| `run-tests.sh` | Build and run tests for a preset | `scripts/agents/run-tests.sh mac-dbg` |
| `smoke-build.sh` | Quick build verification | `scripts/agents/smoke-build.sh mac-dbg yaze` |
| `run-gh-workflow.sh` | Trigger remote CI workflow | `scripts/agents/run-gh-workflow.sh ci.yml` |
| `test-http-api.sh` | Test HTTP API endpoints | `scripts/agents/test-http-api.sh` |
| `windows-smoke-build.ps1` | Windows smoke build (PowerShell) | `pwsh -File scripts/agents/windows-smoke-build.ps1` |

See [scripts/agents/README.md](../../../scripts/agents/README.md) for details.

## Coordination Protocol

**IMPORTANT**: AI agents working on testing infrastructure must follow the coordination protocol:

1. **Before starting work**: Check `docs/internal/agents/coordination-board.md` for active tasks
2. **Update board**: Add entry with scope, status, and expected changes
3. **Avoid conflicts**: Request coordination if touching same files as another agent
4. **Log results**: Update board with completion status and any issues found

See [Coordination Board](../agents/coordination-board.md) for current status.

## Contact & Ownership

- **Testing Infrastructure Lead**: CLAUDE_TEST_COORD
- **Platform Specialists**:
  - Windows: CLAUDE_AIINF
  - Linux: CLAUDE_AIINF
  - macOS: CLAUDE_MAC_BUILD
- **Release Coordination**: CLAUDE_RELEASE_COORD

## References

- [Testing Guide](../../public/developer/testing-guide.md) - User-facing testing documentation
- [Testing Quick Start](../../public/developer/testing-quick-start.md) - Developer quick reference
- [Build Quick Reference](../../public/build/quick-reference.md) - Build commands and presets
- [Release Checklist](../release-checklist.md) - Pre-release testing requirements
- [CI/CD Pipeline](.github/workflows/ci.yml) - Automated testing configuration

---

**Next Steps**: See [Integration Plan](integration-plan.md) for rolling out new testing infrastructure improvements.
