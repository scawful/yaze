# YAZE Testing Strategy

## Purpose

This document defines the comprehensive testing strategy for YAZE, explaining what each test level catches, when to run tests, and how to debug failures. It serves as the authoritative guide for developers and AI agents.

**Last Updated**: 2025-11-20

---

## Table of Contents

1. [Testing Philosophy](#1-testing-philosophy)
2. [Test Pyramid](#2-test-pyramid)
3. [Test Categories](#3-test-categories)
4. [When to Run Tests](#4-when-to-run-tests)
5. [Test Organization](#5-test-organization)
6. [Platform-Specific Testing](#6-platform-specific-testing)
7. [CI/CD Testing](#7-cicd-testing)
8. [Debugging Test Failures](#8-debugging-test-failures)

---

## 1. Testing Philosophy

### Core Principles

1. **Fast Feedback**: Developers should get test results in <2 minutes locally
2. **Fail Early**: Catch issues at the lowest/fastest test level possible
3. **Confidence**: Tests should give confidence that code works across platforms
4. **Automation**: All tests should be automatable in CI
5. **Clarity**: Test failures should clearly indicate what broke and where

### Testing Goals

- **Prevent Regressions**: Ensure new changes don't break existing functionality
- **Catch Build Issues**: Detect compilation/linking problems before CI
- **Validate Logic**: Verify algorithms and data structures work correctly
- **Test Integration**: Ensure components work together
- **Validate UX**: Confirm UI workflows function as expected

---

## 2. Test Pyramid

YAZE uses a **5-level testing pyramid**, from fastest (bottom) to slowest (top):

```
                    ┌─────────────────────┐
                    │   E2E Tests (E2E)   │ Minutes    │ Few tests
                    │  Full UI workflows  │            │ High value
                    ├─────────────────────┤            │
                 ┌─ │ Integration (INT)   │ Seconds    │
                 │  │ Multi-component     │            │
                 │  ├─────────────────────┤            │
      Tests      │  │   Unit Tests (UT)   │ <1 second  │
                 │  │  Isolated logic     │            │
                 └─ ├─────────────────────┤            │
                    │ Symbol Validation   │ Minutes    │
                    │ ODR, conflicts      │            ▼
                    ├─────────────────────┤
                    │ Smoke Compilation   │ ~2 min
                    │ Header checks       │
      Build        ├─────────────────────┤
      Checks       │ Config Validation   │ ~10 sec
                   │ CMake, includes     │
                   ├─────────────────────┤
                   │ Static Analysis     │ <1 sec     │ Many checks
                   │ Format, lint        │            │ Fast feedback
                   └─────────────────────┘            ▼
```

---

## 3. Test Categories

### Level 0: Static Analysis (< 1 second)

**Purpose**: Catch trivial issues before compilation

**Tools**:
- `clang-format` - Code formatting
- `clang-tidy` - Static analysis (subset of files)
- `cppcheck` - Additional static checks

**What It Catches**:
- ✅ Formatting violations
- ✅ Common code smells
- ✅ Potential null pointer dereferences
- ✅ Unused variables

**What It Misses**:
- ❌ Build system issues
- ❌ Linking problems
- ❌ Runtime logic errors

**Run Locally**:
```bash
# Format check (don't modify)
cmake --build build --target yaze-format-check

# Static analysis on changed files
git diff --name-only HEAD | grep -E '\.(cc|h)$' | \
  xargs clang-tidy-14 --header-filter='src/.*'
```

**Run in CI**: ✅ Every PR (code-quality job)

---

### Level 1: Configuration Validation (< 10 seconds)

**Purpose**: Validate CMake configuration without full compilation

**What It Catches**:
- ✅ CMake syntax errors
- ✅ Missing dependencies (immediate)
- ✅ Invalid preset combinations
- ✅ Include path misconfigurations

**What It Misses**:
- ❌ Actual compilation errors
- ❌ Header availability issues
- ❌ Linking problems

**Run Locally**:
```bash
# Validate a preset
./scripts/pre-push-test.sh --config-only

# Test multiple presets
for preset in mac-dbg mac-rel mac-ai; do
  cmake --preset "$preset" --list-presets > /dev/null
done
```

**Run in CI**: 🔄 Proposed (new job)

---

### Level 2: Smoke Compilation (< 2 minutes)

**Purpose**: Quick compilation check to catch header/include issues

**What It Catches**:
- ✅ Missing headers
- ✅ Include path problems
- ✅ Preprocessor errors
- ✅ Template instantiation issues
- ✅ Platform-specific compilation

**What It Misses**:
- ❌ Linking errors
- ❌ Symbol conflicts
- ❌ Runtime behavior

**Strategy**:
- Compile 1-2 representative files per library
- Focus on files with many includes
- Test platform-specific code paths

**Run Locally**:
```bash
./scripts/pre-push-test.sh --smoke-only
```

**Run in CI**: 🔄 Proposed (compile-only job, <5 min)

---

### Level 3: Symbol Validation (< 5 minutes)

**Purpose**: Detect symbol conflicts and ODR violations

**What It Catches**:
- ✅ Duplicate symbol definitions
- ✅ ODR (One Definition Rule) violations
- ✅ Missing symbols (link errors)
- ✅ Symbol visibility issues

**What It Misses**:
- ❌ Runtime logic errors
- ❌ Performance issues
- ❌ Memory leaks

**Tools**:
- `nm` (Unix/macOS) - Symbol inspection
- `dumpbin /symbols` (Windows) - Symbol inspection
- `c++filt` - Symbol demangling

**Run Locally**:
```bash
./scripts/verify-symbols.sh
```

**Run in CI**: 🔄 Proposed (symbol-check job)

---

### Level 4: Unit Tests (< 1 second each)

**Purpose**: Fast, isolated testing of individual components

**Location**: `test/unit/`

**Characteristics**:
- No external dependencies (ROM, network, filesystem)
- Mocked dependencies via test doubles
- Single-component focus
- Deterministic (no flaky tests)

**What It Catches**:
- ✅ Algorithm correctness
- ✅ Data structure behavior
- ✅ Edge cases and error handling
- ✅ Isolated component logic

**What It Misses**:
- ❌ Component interactions
- ❌ ROM data handling
- ❌ UI workflows
- ❌ Platform-specific issues

**Examples**:
- `test/unit/core/hex_test.cc` - Hex conversion logic
- `test/unit/gfx/snes_palette_test.cc` - Palette operations
- `test/unit/zelda3/object_parser_test.cc` - Object parsing

**Run Locally**:
```bash
./build/bin/yaze_test --unit
```

**Run in CI**: ✅ Every PR (test job)

**Writing Guidelines**:
```cpp
// GOOD: Fast, isolated, no dependencies
TEST(UnitTest, SnesPaletteConversion) {
  gfx::SnesColor color(0x7C00);  // Red in SNES format
  EXPECT_EQ(color.red(), 31);
  EXPECT_EQ(color.rgb(), 0xFF0000);
}

// BAD: Depends on ROM file
TEST(UnitTest, LoadOverworldMapColors) {
  Rom rom;
  rom.LoadFromFile("zelda3.sfc");  // ❌ External dependency
  auto colors = rom.ReadPalette(0x1BD308);
  EXPECT_EQ(colors.size(), 128);
}
```

---

### Level 5: Integration Tests (1-10 seconds each)

**Purpose**: Test interactions between components

**Location**: `test/integration/`

**Characteristics**:
- Multi-component interactions
- May require ROM files (optional)
- Real implementations (minimal mocking)
- Slower but more realistic

**What It Catches**:
- ✅ Component interaction bugs
- ✅ Data flow between systems
- ✅ ROM operations
- ✅ Resource management

**What It Misses**:
- ❌ Full UI workflows
- ❌ User interactions
- ❌ Visual rendering

**Examples**:
- `test/integration/asar_integration_test.cc` - Asar patching + ROM
- `test/integration/dungeon_editor_v2_test.cc` - Dungeon editor logic
- `test/integration/zelda3/overworld_integration_test.cc` - Overworld loading

**Run Locally**:
```bash
./build/bin/yaze_test --integration
```

**Run in CI**: ⚠️ Limited (develop/master only, not PRs)

**Writing Guidelines**:
```cpp
// GOOD: Tests component interaction
TEST(IntegrationTest, AsarPatchRom) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile("zelda3.sfc"));

  AsarWrapper asar;
  auto result = asar.ApplyPatch("test.asm", rom);
  ASSERT_TRUE(result.ok());

  // Verify ROM was patched correctly
  EXPECT_EQ(rom.ReadByte(0x12345), 0xAB);
}
```

---

### Level 6: End-to-End (E2E) Tests (10-60 seconds each)

**Purpose**: Validate full user workflows through the UI

**Location**: `test/e2e/`

**Characteristics**:
- Full application stack
- Real UI (ImGui + SDL)
- User interaction simulation
- Requires display/window system

**What It Catches**:
- ✅ Complete user workflows
- ✅ UI responsiveness
- ✅ Visual rendering (screenshots)
- ✅ Cross-editor interactions

**What It Misses**:
- ❌ Performance issues
- ❌ Memory leaks (unless with sanitizers)
- ❌ Platform-specific edge cases

**Tools**:
- `ImGuiTestEngine` - UI automation
- `ImGui_TestEngineHook_*` - Test engine integration

**Examples**:
- `test/e2e/dungeon_editor_smoke_test.cc` - Open dungeon editor, load ROM
- `test/e2e/canvas_selection_test.cc` - Select tiles on canvas
- `test/e2e/overworld/overworld_e2e_test.cc` - Overworld editing workflow

**Run Locally**:
```bash
# Headless (fast)
./build/bin/yaze_test --e2e

# With GUI visible (slow, for debugging)
./build/bin/yaze_test --e2e --show-gui --normal
```

**Run in CI**: ⚠️ macOS only (z3ed-agent-test job)

**Writing Guidelines**:
```cpp
void E2ETest_DungeonEditorSmokeTest(ImGuiTestContext* ctx) {
  ctx->SetRef("DockSpaceViewport");

  // Open File menu
  ctx->MenuCheck("File/Load ROM", true);

  // Enter ROM path
  ctx->ItemInput("##rom_path");
  ctx->KeyCharsAppend("zelda3.sfc");

  // Click Load button
  ctx->ItemClick("Load");

  // Verify editor opened
  ctx->WindowFocus("Dungeon Editor");
  IM_CHECK(ctx->WindowIsOpen("Dungeon Editor"));
}
```

---

## 4. When to Run Tests

### 4.1 During Development (Continuous)

**Frequency**: After every significant change

**Run**:
- Level 0: Static analysis (IDE integration)
- Level 4: Unit tests for changed components

**Tools**:
- VSCode C++ extension (clang-tidy)
- File watchers (`entr`, `watchexec`)

```bash
# Watch mode for unit tests
find src test -name "*.cc" | entr -c ./build/bin/yaze_test --unit
```

---

### 4.2 Before Committing (Pre-Commit)

**Frequency**: Before `git commit`

**Run**:
- Level 0: Format check
- Level 4: Unit tests for changed files

**Setup** (optional):
```bash
# Install pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Format check
if ! cmake --build build --target yaze-format-check; then
  echo "❌ Format check failed. Run: cmake --build build --target yaze-format"
  exit 1
fi
EOF
chmod +x .git/hooks/pre-commit
```

---

### 4.3 Before Pushing (Pre-Push)

**Frequency**: Before `git push` to remote

**Run**:
- Level 0: Static analysis
- Level 1: Configuration validation
- Level 2: Smoke compilation
- Level 3: Symbol validation
- Level 4: All unit tests

**Time Budget**: < 2 minutes

**Command**:
```bash
# Unix/macOS
./scripts/pre-push-test.sh

# Windows
.\scripts\pre-push-test.ps1
```

**What It Prevents**:
- 90% of CI build failures
- ODR violations
- Include path issues
- Symbol conflicts

---

### 4.4 After Pull Request Creation

**Frequency**: Automatically on every PR

**Run** (CI):
- Level 0: Static analysis (code-quality job)
- Level 2: Full compilation (build job)
- Level 4: Unit tests (test job)
- Level 4: Stable tests (test job)

**Time**: 15-20 minutes

**Outcome**: ✅ Required for merge

---

### 4.5 After Merge to Develop/Master

**Frequency**: Post-merge (develop/master only)

**Run** (CI):
- All PR checks
- Level 5: Integration tests
- Level 6: E2E tests (macOS)
- Memory sanitizers (Linux)
- Full AI stack tests (Windows/macOS)

**Time**: 30-45 minutes

**Outcome**: ⚠️ Optional (but monitored)

---

### 4.6 Before Release

**Frequency**: Release candidates

**Run**:
- All CI tests
- Manual exploratory testing
- Performance benchmarks
- Cross-platform smoke testing

**Checklist**: See `docs/internal/release-checklist.md`

---

## 5. Test Organization

### Directory Structure

```
test/
├── unit/                   # Level 4: Fast, isolated tests
│   ├── core/              # Core utilities
│   ├── gfx/               # Graphics system
│   ├── zelda3/            # Game logic
│   ├── cli/               # CLI components
│   ├── gui/               # GUI widgets
│   └── emu/               # Emulator
│
├── integration/           # Level 5: Multi-component tests
│   ├── ai/                # AI integration
│   ├── editor/            # Editor systems
│   └── zelda3/            # Game system integration
│
├── e2e/                   # Level 6: Full workflow tests
│   ├── overworld/         # Overworld editor E2E
│   ├── zscustomoverworld/ # ZSCustomOverworld E2E
│   └── rom_dependent/     # ROM-required E2E
│
├── benchmarks/            # Performance tests
├── mocks/                 # Test doubles
└── test_utils.cc          # Test utilities
```

### Naming Conventions

**Files**:
- Unit: `<component>_test.cc`
- Integration: `<feature>_integration_test.cc`
- E2E: `<workflow>_e2e_test.cc`

**Test Names**:
```cpp
// Unit
TEST(UnitTest, ComponentName_Behavior_ExpectedOutcome) { }

// Integration
TEST(IntegrationTest, SystemName_Interaction_ExpectedOutcome) { }

// E2E
void E2ETest_WorkflowName_StepDescription(ImGuiTestContext* ctx) { }
```

### Test Labels (CTest)

Tests are labeled for selective execution:

- `stable` - No ROM required, fast
- `unit` - Unit tests only
- `integration` - Integration tests
- `e2e` - End-to-end tests
- `rom_dependent` - Requires ROM file

```bash
# Run only stable tests
ctest --preset stable

# Run unit tests
./build/bin/yaze_test --unit

# Run ROM-dependent tests
./build/bin/yaze_test --rom-dependent --rom-path zelda3.sfc
```

---

## 6. Platform-Specific Testing

### 6.1 Cross-Platform Considerations

**Different Linker Behavior**:
- macOS: More permissive (weak symbols)
- Linux: Strict ODR enforcement
- Windows: MSVC vs clang-cl differences

**Strategy**: Test on Linux for strictest validation

**Different Compilers**:
- GCC (Linux): `-Werror=odr`
- Clang (macOS/Linux): More warnings
- clang-cl (Windows): MSVC compatibility mode

**Strategy**: Use verbose presets (`*-dbg-v`) to see all warnings

### 6.2 Local Cross-Platform Testing

**For macOS Developers**:
```bash
# Test Linux build locally (future: Docker)
docker run --rm -v $(pwd):/workspace yaze-linux-builder \
  cmake --preset lin-dbg && cmake --build build --target yaze
```

**For Linux Developers**:
```bash
# Test macOS build locally (requires macOS VM)
# Future: GitHub Actions remote testing
```

**For Windows Developers**:
```powershell
# Test via WSL (Linux build)
wsl bash -c "cmake --preset lin-dbg && cmake --build build"
```

---

## 7. CI/CD Testing

### 7.1 Current CI Matrix

| Job | Platform | Preset | Duration | Runs On |
|-----|----------|--------|----------|---------|
| build | Ubuntu 22.04 | ci-linux | ~15 min | All PRs |
| build | macOS 14 | ci-macos | ~20 min | All PRs |
| build | Windows 2022 | ci-windows | ~25 min | All PRs |
| test | Ubuntu 22.04 | ci-linux | ~5 min | All PRs |
| test | macOS 14 | ci-macos | ~5 min | All PRs |
| test | Windows 2022 | ci-windows | ~5 min | All PRs |
| windows-agent | Windows 2022 | ci-windows-ai | ~30 min | Post-merge |
| code-quality | Ubuntu 22.04 | - | ~2 min | All PRs |
| memory-sanitizer | Ubuntu 22.04 | sanitizer | ~20 min | PRs |
| z3ed-agent-test | macOS 14 | mac-ai | ~15 min | Develop/master |

### 7.2 Proposed CI Improvements

**New Jobs**:

1. **compile-only** (< 5 min)
   - Run BEFORE full build
   - Compile 10-20 representative files
   - Fast feedback on include issues

2. **symbol-check** (< 3 min)
   - Run AFTER build
   - Detect ODR violations
   - Platform-specific (Linux most strict)

3. **config-validation** (< 2 min)
   - Test all presets can configure
   - Validate include paths
   - Catch CMake errors early

**Benefits**:
- 90% of issues caught in <5 minutes
- Reduced wasted CI time
- Faster developer feedback

---

## 8. Debugging Test Failures

### 8.1 Local Test Failures

**Unit Test Failure**:
```bash
# Run specific test
./build/bin/yaze_test "TestSuiteName.TestName"

# Run with verbose output
./build/bin/yaze_test --verbose "TestSuiteName.*"

# Run with debugger
lldb -- ./build/bin/yaze_test "TestSuiteName.TestName"
```

**Integration Test Failure**:
```bash
# Ensure ROM is available
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc
./build/bin/yaze_test --integration --verbose
```

**E2E Test Failure**:
```bash
# Run with GUI visible (slow motion)
./build/bin/yaze_test --e2e --show-gui --cinematic

# Take screenshots on failure
YAZE_E2E_SCREENSHOT_DIR=/tmp/screenshots \
  ./build/bin/yaze_test --e2e
```

### 8.2 CI Test Failures

**Step 1: Identify Job**
- Which platform failed? (Linux/macOS/Windows)
- Which job failed? (build/test/code-quality)
- Which test failed? (check CI logs)

**Step 2: Reproduce Locally**
```bash
# Use matching CI preset
cmake --preset ci-linux  # or ci-macos, ci-windows
cmake --build build

# Run same test
./build/bin/yaze_test --unit
```

**Step 3: Platform-Specific Issues**

**If Windows-only failure**:
- Check for MSVC/clang-cl differences
- Validate include paths (Abseil, gRPC)
- Check preprocessor macros (`_WIN32`, etc.)

**If Linux-only failure**:
- Check for ODR violations (duplicate symbols)
- Validate linker flags
- Check for gflags `FLAGS` conflicts

**If macOS-only failure**:
- Check for framework dependencies
- Validate Objective-C++ code
- Check for Apple SDK issues

### 8.3 Build Failures

**CMake Configuration Failure**:
```bash
# Verbose CMake output
cmake --preset ci-linux -DCMAKE_VERBOSE_MAKEFILE=ON

# Check CMake cache
cat build/CMakeCache.txt | grep ERROR

# Check include paths
cmake --build build --target help | grep INCLUDE
```

**Compilation Failure**:
```bash
# Verbose compilation
cmake --build build --preset ci-linux -v

# Single file compilation
cd build
ninja -v path/to/file.cc.o
```

**Linking Failure**:
```bash
# Check symbols in library
nm -gU build/lib/libyaze_core.a | grep FLAGS

# Check duplicate symbols
./scripts/verify-symbols.sh --verbose

# Check ODR violations
nm build/lib/*.a | c++filt | grep " [TDR] " | sort | uniq -d
```

### 8.4 Common Failure Patterns

**Pattern 1: "FLAGS redefined"**
- **Cause**: gflags creates `FLAGS_*` symbols in multiple TUs
- **Solution**: Define FLAGS in exactly one .cc file
- **Prevention**: Run `./scripts/verify-symbols.sh`

**Pattern 2: "Abseil headers not found"**
- **Cause**: Include paths not propagated from gRPC
- **Solution**: Add explicit Abseil include directory
- **Prevention**: Run smoke compilation test

**Pattern 3: "std::filesystem not available"**
- **Cause**: Missing C++17/20 standard flag
- **Solution**: Add `/std:c++latest` (Windows) or `-std=c++20`
- **Prevention**: Validate compiler flags in CMake

**Pattern 4: "Multiple definition of X"**
- **Cause**: Header-only library included in multiple TUs
- **Solution**: Use `inline` or move to single TU
- **Prevention**: Symbol conflict checker

---

## 9. Best Practices

### 9.1 Writing Tests

1. **Fast**: Unit tests should complete in <100ms
2. **Isolated**: No external dependencies (files, network, ROM)
3. **Deterministic**: Same input → same output, always
4. **Clear**: Test name describes what is tested
5. **Focused**: One assertion per test (ideally)

### 9.2 Test Data

**Good**:
```cpp
// Inline test data
const uint8_t palette_data[] = {0x00, 0x7C, 0xFF, 0x03};
auto palette = gfx::SnesPalette(palette_data, 4);
```

**Bad**:
```cpp
// External file dependency
auto palette = gfx::SnesPalette::LoadFromFile("test_palette.bin");  // ❌
```

### 9.3 Assertions

**Prefer `EXPECT_*` over `ASSERT_*`**:
- `EXPECT_*` continues on failure (more info)
- `ASSERT_*` stops immediately (for fatal errors)

```cpp
// Good: Continue testing after failure
EXPECT_EQ(color.red(), 31);
EXPECT_EQ(color.green(), 0);
EXPECT_EQ(color.blue(), 0);

// Bad: Only see first failure
ASSERT_EQ(color.red(), 31);
ASSERT_EQ(color.green(), 0);  // Never executed if red fails
```

---

## 10. Resources

### Documentation
- **Gap Analysis**: `docs/internal/testing/gap-analysis.md`
- **Pre-Push Checklist**: `docs/internal/testing/pre-push-checklist.md`
- **Quick Reference**: `docs/public/build/quick-reference.md`

### Scripts
- **Pre-Push Test**: `scripts/pre-push-test.sh` (Unix/macOS)
- **Pre-Push Test**: `scripts/pre-push-test.ps1` (Windows)
- **Symbol Checker**: `scripts/verify-symbols.sh`

### CI Configuration
- **Workflow**: `.github/workflows/ci.yml`
- **Composite Actions**: `.github/actions/`

### Tools
- **Test Runner**: `test/yaze_test.cc`
- **Test Utilities**: `test/test_utils.h`
- **Google Test**: https://google.github.io/googletest/
- **ImGui Test Engine**: https://github.com/ocornut/imgui_test_engine
