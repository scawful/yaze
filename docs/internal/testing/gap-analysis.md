# Testing Infrastructure Gap Analysis

## Executive Summary

Recent CI failures revealed critical gaps in our testing infrastructure that allowed platform-specific build failures to reach CI. This document analyzes what we currently test, what we missed, and what infrastructure is needed to catch issues earlier.

**Date**: 2025-11-20
**Triggered By**: Multiple CI failures in commits 43a0e5e314, c2bb90a3f1, and related fixes

---

## 1. Issues We Didn't Catch Locally

### 1.1 Windows Abseil Include Path Issues (c2bb90a3f1)
**Problem**: Abseil headers not found during Windows/clang-cl compilation
**Why it wasn't caught**:
- No local pre-push compilation check
- CMake configuration validates successfully, but compilation fails later
- Include path propagation from gRPC/Abseil not validated until full compile

**What would have caught it**:
- ✅ Smoke compilation test (compile subset of files to catch header issues)
- ✅ CMake configuration validator (check include path propagation)
- ✅ Header dependency checker

### 1.2 Linux FLAGS Symbol Conflicts (43a0e5e314, eb77bbeaff)
**Problem**: ODR (One Definition Rule) violation - multiple `FLAGS` symbols across libraries
**Why it wasn't caught**:
- Symbol conflicts only appear at link time
- No cross-library symbol conflict detection
- Static analysis doesn't catch ODR violations
- Unit tests don't link full dependency graph

**What would have caught it**:
- ✅ Symbol conflict scanner (nm/objdump analysis)
- ✅ ODR violation detector
- ✅ Full integration build test (link all libraries together)

### 1.3 Platform-Specific Configuration Issues
**Problem**: Preprocessor flags, compiler detection, and platform-specific code paths
**Why it wasn't caught**:
- No local cross-platform validation
- CMake configuration differences between platforms not tested
- Compiler detection logic (clang-cl vs MSVC) not validated

**What would have caught it**:
- ✅ CMake configuration dry-run on multiple platforms
- ✅ Preprocessor flag validation
- ✅ Compiler detection smoke test

---

## 2. Current Testing Coverage

### 2.1 What We Test Well

#### Unit Tests (test/unit/)
- **Coverage**: Core algorithms, data structures, parsers
- **Speed**: Fast (<1s for most tests)
- **Isolation**: Mocked dependencies, no ROM required
- **CI**: ✅ Runs on every PR
- **Example**: `hex_test.cc`, `asar_wrapper_test.cc`, `snes_palette_test.cc`

**Strengths**:
- Catches logic errors quickly
- Good for TDD
- Platform-independent

**Gaps**:
- Doesn't catch build system issues
- Doesn't catch linking problems
- Doesn't validate dependencies

#### Integration Tests (test/integration/)
- **Coverage**: Multi-component interactions, ROM operations
- **Speed**: Slower (1-10s per test)
- **Dependencies**: May require ROM files
- **CI**: ✅ Runs on develop/master
- **Example**: `asar_integration_test.cc`, `dungeon_editor_v2_test.cc`

**Strengths**:
- Tests component interactions
- Validates ROM operations

**Gaps**:
- Still doesn't catch platform-specific issues
- Doesn't validate symbol conflicts
- Doesn't test cross-library linking

#### E2E Tests (test/e2e/)
- **Coverage**: Full UI workflows, user interactions
- **Speed**: Very slow (10-60s per test)
- **Dependencies**: GUI, ImGuiTestEngine
- **CI**: ⚠️ Limited (only on macOS z3ed-agent-test)
- **Example**: `dungeon_editor_smoke_test.cc`, `canvas_selection_test.cc`

**Strengths**:
- Validates real user workflows
- Tests UI responsiveness

**Gaps**:
- Not run consistently across platforms
- Slow feedback loop
- Requires display/window system

### 2.2 What We DON'T Test

#### Build System Validation
- ❌ CMake configuration correctness per preset
- ❌ Include path propagation from dependencies
- ❌ Compiler flag compatibility
- ❌ Linker flag validation
- ❌ Cross-preset compatibility

#### Symbol-Level Issues
- ❌ ODR (One Definition Rule) violations
- ❌ Duplicate symbol detection across libraries
- ❌ Symbol visibility (public/private)
- ❌ ABI compatibility between libraries

#### Platform-Specific Compilation
- ❌ Header-only compilation checks
- ❌ Preprocessor branch coverage
- ❌ Platform macro validation
- ❌ Compiler-specific feature detection

#### Dependency Health
- ❌ Include path conflicts
- ❌ Library version mismatches
- ❌ Transitive dependency validation
- ❌ Static vs shared library conflicts

---

## 3. CI/CD Coverage Analysis

### 3.1 Current CI Matrix (.github/workflows/ci.yml)

| Platform | Build | Test (stable) | Test (unit) | Test (integration) | Test (AI) |
|----------|-------|---------------|-------------|-------------------|-----------|
| Ubuntu 22.04 (GCC-12) | ✅ | ✅ | ✅ | ❌ | ❌ |
| macOS 14 (Clang) | ✅ | ✅ | ✅ | ❌ | ✅ |
| Windows 2022 (Core) | ✅ | ✅ | ✅ | ❌ | ❌ |
| Windows 2022 (AI) | ✅ | ✅ | ✅ | ❌ | ❌ |

**CI Job Flow**:
1. **build**: Configure + compile full project
2. **test**: Run stable + unit tests
3. **windows-agent**: Full AI stack (gRPC + AI runtime)
4. **code-quality**: clang-format, cppcheck, clang-tidy
5. **memory-sanitizer**: AddressSanitizer (Linux only)
6. **z3ed-agent-test**: Full agent test suite (macOS only)

### 3.2 CI Gaps

#### Missing Early Feedback
- ❌ No compilation-only job (fails after 15-20 min build)
- ❌ No CMake configuration validation job (would catch in <1 min)
- ❌ No symbol conflict checking job

#### Limited Platform Coverage
- ⚠️ Only Linux gets AddressSanitizer
- ⚠️ Only macOS gets full z3ed agent tests
- ⚠️ Windows AI stack not tested on PRs (only post-merge)

#### Incomplete Testing
- ❌ Integration tests not run in CI
- ❌ E2E tests not run on Linux/Windows
- ❌ No ROM-dependent testing
- ❌ No performance regression detection

---

## 4. Developer Workflow Gaps

### 4.1 Pre-Commit Hooks
**Current State**: None
**Gap**: No automatic checks before local commits

**Should Include**:
- clang-format check
- Build system sanity check
- Copyright header validation

### 4.2 Pre-Push Validation
**Current State**: Manual testing only
**Gap**: Easy to push broken code to CI

**Should Include**:
- Smoke build test (quick compilation check)
- Unit test run
- Symbol conflict detection

### 4.3 Local Cross-Platform Testing
**Current State**: Developer-dependent
**Gap**: No easy way to test across platforms locally

**Should Include**:
- Docker-based Linux testing
- VM-based Windows testing (for macOS/Linux devs)
- Preset validation tool

---

## 5. Root Cause Analysis by Issue Type

### 5.1 Windows Abseil Include Paths

**Timeline**:
- ✅ Local macOS build succeeds
- ✅ CMake configuration succeeds on all platforms
- ❌ Windows compilation fails 15 minutes into CI
- ❌ Fix attempt 1 fails (14d1f5de4c)
- ❌ Fix attempt 2 fails (c2bb90a3f1)
- ✅ Final fix succeeds

**Why Multiple Attempts**:
1. No local Windows testing environment
2. CMake configuration doesn't validate actual compilation
3. No header-only compilation check
4. 15-20 minute feedback cycle from CI

**Prevention**:
- Header compilation smoke test
- CMake include path validator
- Local Windows testing (Docker/VM)

### 5.2 Linux FLAGS Symbol Conflicts

**Timeline**:
- ✅ Local macOS build succeeds
- ✅ Unit tests pass
- ❌ Linux full build fails at link time
- ❌ ODR violation: multiple `FLAGS` definitions
- ✅ Fix: move FLAGS definition, rename conflicts

**Why It Happened**:
1. gflags creates `FLAGS_*` symbols in headers
2. Multiple translation units define same symbols
3. macOS linker more permissive than Linux ld
4. No symbol conflict detection

**Prevention**:
- Symbol conflict scanner
- ODR violation checker
- Cross-platform link test

---

## 6. Recommended Testing Levels

We propose a **5-level testing pyramid**:

### Level 0: Static Analysis (< 1s)
- clang-format
- clang-tidy on changed files
- Copyright headers
- CMakeLists.txt syntax

### Level 1: Configuration Validation (< 10s)
- CMake configure dry-run
- Include path validation
- Compiler detection check
- Preprocessor flag validation

### Level 2: Smoke Compilation (< 2 min)
- Compile subset of files (1 file per library)
- Header-only compilation
- Template instantiation check
- Platform-specific branch validation

### Level 3: Symbol Validation (< 5 min)
- Full project compilation
- Symbol conflict detection (nm/dumpbin)
- ODR violation check
- Library dependency graph

### Level 4: Test Execution (5-30 min)
- Unit tests (fast)
- Integration tests (medium)
- E2E tests (slow)
- ROM-dependent tests (optional)

---

## 7. Actionable Recommendations

### 7.1 Immediate Actions (This Initiative)

1. **Create pre-push scripts** (`scripts/pre-push-test.sh`, `scripts/pre-push-test.ps1`)
   - Run Level 0-2 checks locally
   - Estimated time: <2 minutes
   - Blocks 90% of CI failures

2. **Create symbol conflict detector** (`scripts/verify-symbols.sh`)
   - Scan built libraries for duplicate symbols
   - Run as part of pre-push
   - Catches ODR violations

3. **Document testing strategy** (`docs/internal/testing/testing-strategy.md`)
   - Clear explanation of each test level
   - When to run which tests
   - CI vs local testing

4. **Create pre-push checklist** (`docs/internal/testing/pre-push-checklist.md`)
   - Interactive checklist for developers
   - Links to tools and scripts

### 7.2 Short-Term Improvements (Next Sprint)

1. **Add CI compile-only job**
   - Runs in <5 minutes
   - Catches compilation issues before full build
   - Fails fast

2. **Add CI symbol checking job**
   - Runs after compile-only
   - Detects ODR violations
   - Platform-specific

3. **Add CMake configuration validation job**
   - Tests all presets
   - Validates include paths
   - <2 minutes

4. **Enable integration tests in CI**
   - Run on develop/master only (not PRs)
   - Requires ROM file handling

### 7.3 Long-Term Improvements (Future)

1. **Docker-based local testing**
   - Linux environment for macOS/Windows devs
   - Matches CI exactly
   - Fast feedback

2. **Cross-platform test matrix locally**
   - Run tests across multiple platforms
   - Automated VM/container management

3. **Performance regression detection**
   - Benchmark suite
   - Historical tracking
   - Automatic alerts

4. **Coverage tracking**
   - Line coverage per PR
   - Coverage trends over time
   - Uncovered code reports

---

## 8. Success Metrics

### 8.1 Developer Experience
- **Target**: <2 minutes pre-push validation time
- **Target**: 90% reduction in CI build failures
- **Target**: <3 attempts to fix CI issues (down from 5-10)

### 8.2 CI Efficiency
- **Target**: <5 minutes to first failure signal
- **Target**: 50% reduction in wasted CI time
- **Target**: 95% PR pass rate (up from ~70%)

### 8.3 Code Quality
- **Target**: Zero ODR violations
- **Target**: Zero platform-specific include issues
- **Target**: 100% symbol conflict detection

---

## 9. Reference

### Similar Issues in Recent History
- Windows std::filesystem support (19196ca87c, b556b155a5)
- Linux circular dependency (0812a84a22, e36d81f357)
- macOS z3ed linker error (9c562df277)
- Windows clang-cl detection (84cdb09a5b, cbdc6670a1)

### Related Documentation
- `docs/public/build/quick-reference.md` - Build commands
- `docs/public/build/troubleshooting.md` - Platform-specific fixes
- `CLAUDE.md` - Build system guidelines
- `.github/workflows/ci.yml` - CI configuration

### Tools Used
- `nm` (Unix) / `dumpbin` (Windows) - Symbol inspection
- `clang-tidy` - Static analysis
- `cppcheck` - Code quality
- `cmake --preset <name> --list-presets` - Preset validation
