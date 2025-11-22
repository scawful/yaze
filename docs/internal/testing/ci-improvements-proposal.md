# CI/CD Improvements Proposal

## Executive Summary

This document proposes specific improvements to the YAZE CI/CD pipeline to catch build failures earlier, reduce wasted CI time, and provide faster feedback to developers.

**Goals**:
- Reduce time-to-first-failure from ~15 minutes to <5 minutes
- Catch 90% of failures in fast jobs (<5 min)
- Reduce PR iteration time from hours to minutes
- Prevent platform-specific issues from reaching CI

**ROI**:
- **Time Saved**: ~10 minutes per failed build × ~30 failures/month = **5 hours/month**
- **Developer Experience**: Faster feedback → less context switching
- **CI Cost**: Minimal (fast jobs use fewer resources)

---

## Current CI Pipeline Analysis

### Current Jobs

| Job | Platform | Duration | Cost | Catches |
|-----|----------|----------|------|---------|
| build | Ubuntu/macOS/Windows | 15-20 min | High | Compilation errors |
| test | Ubuntu/macOS/Windows | 5 min | Medium | Test failures |
| windows-agent | Windows | 30 min | High | AI stack issues |
| code-quality | Ubuntu | 2 min | Low | Format/lint issues |
| memory-sanitizer | Ubuntu | 20 min | High | Memory bugs |
| z3ed-agent-test | macOS | 15 min | High | Agent integration |

**Total PR Time**: ~40 minutes (parallel), ~90 minutes (worst case)

### Issues with Current Pipeline

1. **Long feedback loop**: 15-20 minutes to find out if headers are missing
2. **Wasted resources**: Full 20-minute builds that fail in first 2 minutes
3. **No early validation**: CMake configuration succeeds, but compilation fails later
4. **Symbol conflicts detected late**: Link errors only appear after full compile
5. **Platform-specific issues**: Discovered after 15+ minutes per platform

---

## Proposed Improvements

### Improvement 1: Configuration Validation Job

**Goal**: Catch CMake errors in <2 minutes

**Implementation**:
```yaml
config-validation:
  name: "Config Validation - ${{ matrix.preset }}"
  runs-on: ${{ matrix.os }}
  strategy:
    fail-fast: true  # Stop immediately if any fails
    matrix:
      include:
        - os: ubuntu-22.04
          preset: ci-linux
        - os: macos-14
          preset: ci-macos
        - os: windows-2022
          preset: ci-windows

  steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup build environment
      uses: ./.github/actions/setup-build
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Validate CMake configuration
      run: |
        cmake --preset ${{ matrix.preset }} \
          -DCMAKE_VERBOSE_MAKEFILE=OFF

    - name: Check include paths
      run: |
        grep "INCLUDE_DIRECTORIES" build/CMakeCache.txt || \
          (echo "Include paths not configured" && exit 1)

    - name: Validate presets
      run: cmake --preset ${{ matrix.preset }} --list-presets
```

**Benefits**:
- ✅ Fails in <2 minutes for CMake errors
- ✅ Catches missing dependencies immediately
- ✅ Validates include path propagation
- ✅ Low resource usage (no compilation)

**What it catches**:
- CMake syntax errors
- Missing dependencies (immediate)
- Invalid preset definitions
- Include path misconfiguration

---

### Improvement 2: Compile-Only Job

**Goal**: Catch compilation errors in <5 minutes

**Implementation**:
```yaml
compile-check:
  name: "Compile Check - ${{ matrix.preset }}"
  runs-on: ${{ matrix.os }}
  needs: [config-validation]  # Run after config validation passes
  strategy:
    fail-fast: false
    matrix:
      include:
        - os: ubuntu-22.04
          preset: ci-linux
          platform: linux
        - os: macos-14
          preset: ci-macos
          platform: macos
        - os: windows-2022
          preset: ci-windows
          platform: windows

  steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup build environment
      uses: ./.github/actions/setup-build
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Configure project
      run: cmake --preset ${{ matrix.preset }}

    - name: Compile representative files
      run: |
        # Compile 10-20 key files to catch most header issues
        cmake --build build --target rom.cc.o bitmap.cc.o \
          overworld.cc.o resource_catalog.cc.o \
          dungeon.cc.o sprite.cc.o palette.cc.o \
          asar_wrapper.cc.o controller.cc.o canvas.cc.o \
          --parallel 4

    - name: Check for common issues
      run: |
        # Platform-specific checks
        if [ "${{ matrix.platform }}" = "windows" ]; then
          echo "Checking for /std:c++latest flag..."
          grep "std:c++latest" build/compile_commands.json || \
            echo "Warning: C++20 flag may be missing"
        fi
```

**Benefits**:
- ✅ Catches header issues in ~5 minutes
- ✅ Tests actual compilation without full build
- ✅ Platform-specific early detection
- ✅ ~70% faster than full build

**What it catches**:
- Missing headers
- Include path problems
- Preprocessor errors
- Template instantiation issues
- Platform-specific compilation errors

---

### Improvement 3: Symbol Conflict Job

**Goal**: Detect ODR violations before linking

**Implementation**:
```yaml
symbol-check:
  name: "Symbol Check - ${{ matrix.platform }}"
  runs-on: ${{ matrix.os }}
  needs: [build]  # Run after full build completes
  strategy:
    matrix:
      include:
        - os: ubuntu-22.04
          platform: linux
        - os: macos-14
          platform: macos
        - os: windows-2022
          platform: windows

  steps:
    - uses: actions/checkout@v4

    - name: Download build artifacts
      uses: actions/download-artifact@v4
      with:
        name: build-${{ matrix.platform }}
        path: build

    - name: Check for symbol conflicts (Unix)
      if: matrix.platform != 'windows'
      run: ./scripts/verify-symbols.sh --build-dir build

    - name: Check for symbol conflicts (Windows)
      if: matrix.platform == 'windows'
      shell: pwsh
      run: .\scripts\verify-symbols.ps1 -BuildDir build

    - name: Upload conflict report
      if: failure()
      uses: actions/upload-artifact@v4
      with:
        name: symbol-conflicts-${{ matrix.platform }}
        path: build/symbol-report.txt
```

**Benefits**:
- ✅ Catches ODR violations before linking
- ✅ Detects FLAGS conflicts (Linux-specific)
- ✅ Platform-specific symbol issues
- ✅ Runs in parallel with tests (~3 minutes)

**What it catches**:
- Duplicate symbol definitions
- FLAGS_* conflicts (gflags)
- ODR violations
- Link-time errors (predicted)

---

### Improvement 4: Fail-Fast Strategy

**Goal**: Stop wasting resources on doomed builds

**Current Behavior**: All jobs run even if one fails
**Proposed Behavior**: Stop non-essential jobs if critical jobs fail

**Implementation**:
```yaml
jobs:
  # Critical path: These must pass
  config-validation:
    # ... (as above)

  compile-check:
    needs: [config-validation]
    strategy:
      fail-fast: true  # Stop all platforms if one fails

  build:
    needs: [compile-check]
    strategy:
      fail-fast: false  # Allow other platforms to continue

  # Non-critical: These can be skipped if builds fail
  integration-tests:
    needs: [build]
    if: success()  # Only run if build succeeded

  windows-agent:
    needs: [build, test]
    if: success() && github.event_name != 'pull_request'
```

**Benefits**:
- ✅ Saves ~60 minutes of CI time per failed build
- ✅ Faster feedback (no waiting for doomed jobs)
- ✅ Reduced resource usage

---

### Improvement 5: Preset Matrix Testing

**Goal**: Validate all presets can configure

**Implementation**:
```yaml
preset-validation:
  name: "Preset Validation"
  runs-on: ${{ matrix.os }}
  strategy:
    matrix:
      os: [ubuntu-22.04, macos-14, windows-2022]

  steps:
    - uses: actions/checkout@v4

    - name: Test all presets for platform
      run: |
        for preset in $(cmake --list-presets | grep ${{ matrix.os }} | awk '{print $1}'); do
          echo "Testing preset: $preset"
          cmake --preset "$preset" --list-presets || exit 1
        done
```

**Benefits**:
- ✅ Catches invalid preset definitions
- ✅ Validates CMake configuration across all presets
- ✅ Fast (<2 minutes)

---

## Proposed CI Pipeline (New)

### Job Dependencies

```
┌─────────────────────┐
│ config-validation   │ (2 min, fail-fast)
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│  compile-check      │ (5 min, fail-fast)
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│       build         │ (15 min, parallel)
└──────────┬──────────┘
           │
           ├──────────┬──────────┬──────────┐
           ▼          ▼          ▼          ▼
      ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
      │  test  │ │ symbol │ │quality │ │sanitize│
      │ (5 min)│ │(3 min) │ │(2 min) │ │(20 min)│
      └────────┘ └────────┘ └────────┘ └────────┘
```

### Time Comparison

**Current Pipeline**:
- First failure: ~15 minutes (compilation error)
- Total time: ~40 minutes (if all succeed)

**Proposed Pipeline**:
- First failure: ~2 minutes (CMake error) or ~5 minutes (compilation error)
- Total time: ~40 minutes (if all succeed)

**Time Saved**:
- CMake errors: **13 minutes saved** (15 min → 2 min)
- Compilation errors: **10 minutes saved** (15 min → 5 min)
- Symbol conflicts: **Caught earlier** (no failed PRs)

---

## Implementation Plan

### Phase 1: Quick Wins (Week 1)

1. **Add config-validation job**
   - Copy composite actions
   - Add new job to `ci.yml`
   - Test on feature branch

2. **Add symbol-check script**
   - Already created: `scripts/verify-symbols.sh`
   - Add Windows version: `scripts/verify-symbols.ps1`
   - Test locally

3. **Update job dependencies**
   - Make `build` depend on `config-validation`
   - Add fail-fast to compile-check

**Deliverables**:
- ✅ Config validation catches CMake errors in <2 min
- ✅ Symbol checker available for CI
- ✅ Fail-fast prevents wasted CI time

### Phase 2: Compilation Checks (Week 2)

1. **Add compile-check job**
   - Identify representative files
   - Create compilation target list
   - Add to CI workflow

2. **Platform-specific smoke tests**
   - Windows: Check `/std:c++latest`
   - Linux: Check `-std=c++20`
   - macOS: Check framework links

**Deliverables**:
- ✅ Compilation errors caught in <5 min
- ✅ Platform-specific issues detected early

### Phase 3: Symbol Validation (Week 3)

1. **Add symbol-check job**
   - Integrate `verify-symbols.sh`
   - Upload conflict reports
   - Add to required checks

2. **Create symbol conflict guide**
   - Document common issues
   - Provide fix examples
   - Link from CI failures

**Deliverables**:
- ✅ ODR violations caught before merge
- ✅ FLAGS conflicts detected automatically

### Phase 4: Optimization (Week 4)

1. **Fine-tune fail-fast**
   - Identify critical vs optional jobs
   - Set up conditional execution
   - Test resource savings

2. **Add caching improvements**
   - Cache compiled objects
   - Share artifacts between jobs
   - Optimize dependency downloads

**Deliverables**:
- ✅ ~60 minutes CI time saved per failed build
- ✅ Faster PR iteration

---

## Success Metrics

### Before Improvements

| Metric | Value |
|--------|-------|
| Time to first failure | 15-20 min |
| CI failures per month | ~30 |
| Wasted CI time/month | ~8 hours |
| PR iteration time | 2-4 hours |
| Symbol conflicts caught | 0% (manual) |

### After Improvements (Target)

| Metric | Value |
|--------|-------|
| Time to first failure | **2-5 min** |
| CI failures per month | **<10** |
| Wasted CI time/month | **<2 hours** |
| PR iteration time | **30-60 min** |
| Symbol conflicts caught | **100%** |

### ROI Calculation

**Time Savings**:
- 20 failures/month × 10 min saved = **200 minutes/month**
- 10 failed PRs avoided = **~4 hours/month**
- **Total: ~5-6 hours/month saved**

**Developer Experience**:
- Faster feedback → less context switching
- Earlier error detection → easier debugging
- Fewer CI failures → less frustration

---

## Risks & Mitigations

### Risk 1: False Positives
**Risk**: New checks catch issues that aren't real problems
**Mitigation**:
- Test thoroughly before enabling as required
- Allow overrides for known false positives
- Iterate on filtering logic

### Risk 2: Increased Complexity
**Risk**: More jobs = harder to understand CI failures
**Mitigation**:
- Clear job names and descriptions
- Good error messages with links to docs
- Dependency graph visualization

### Risk 3: Slower PR Merges
**Risk**: More required checks = slower to merge
**Mitigation**:
- Make only critical checks required
- Run expensive checks post-merge
- Provide override mechanism for emergencies

---

## Alternative Approaches Considered

### Approach 1: Pre-commit Hooks
**Pros**: Catch issues before pushing
**Cons**: Developers can skip, not enforced
**Decision**: Provide optional hooks, but rely on CI

### Approach 2: GitHub Actions Matrix Expansion
**Pros**: Test more combinations
**Cons**: Significantly more CI time
**Decision**: Focus on critical paths, expand later if needed

### Approach 3: Self-Hosted Runners
**Pros**: Faster builds, more control
**Cons**: Maintenance overhead, security concerns
**Decision**: Stick with GitHub runners for now

---

## Related Work

### Similar Implementations
- **LLVM Project**: Uses compile-only jobs for fast feedback
- **Chromium**: Extensive smoke testing before full builds
- **Abseil**: Symbol conflict detection in CI

### Best Practices
1. **Fail Fast**: Stop early if critical checks fail
2. **Layered Testing**: Quick checks first, expensive checks later
3. **Clear Feedback**: Good error messages with actionable advice
4. **Caching**: Reuse work across jobs when possible

---

## Appendix A: New CI Jobs (YAML)

### Config Validation Job
```yaml
config-validation:
  name: "Config Validation - ${{ matrix.name }}"
  runs-on: ${{ matrix.os }}
  strategy:
    fail-fast: true
    matrix:
      include:
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          preset: ci-linux
          platform: linux
        - name: "macOS 14"
          os: macos-14
          preset: ci-macos
          platform: macos
        - name: "Windows 2022"
          os: windows-2022
          preset: ci-windows
          platform: windows

  steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup build environment
      uses: ./.github/actions/setup-build
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Validate CMake configuration
      run: cmake --preset ${{ matrix.preset }}

    - name: Check configuration
      shell: bash
      run: |
        # Check include paths
        grep "INCLUDE_DIRECTORIES" build/CMakeCache.txt

        # Check preset is valid
        cmake --preset ${{ matrix.preset }} --list-presets
```

### Compile Check Job
```yaml
compile-check:
  name: "Compile Check - ${{ matrix.name }}"
  runs-on: ${{ matrix.os }}
  needs: [config-validation]
  strategy:
    fail-fast: true
    matrix:
      include:
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          preset: ci-linux
          platform: linux
        - name: "macOS 14"
          os: macos-14
          preset: ci-macos
          platform: macos
        - name: "Windows 2022"
          os: windows-2022
          preset: ci-windows
          platform: windows

  steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup build environment
      uses: ./.github/actions/setup-build
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Configure project
      run: cmake --preset ${{ matrix.preset }}

    - name: Smoke compilation test
      shell: bash
      run: ./scripts/pre-push-test.sh --smoke-only --preset ${{ matrix.preset }}
```

### Symbol Check Job
```yaml
symbol-check:
  name: "Symbol Check - ${{ matrix.name }}"
  runs-on: ${{ matrix.os }}
  needs: [build]
  strategy:
    matrix:
      include:
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          platform: linux
        - name: "macOS 14"
          os: macos-14
          platform: macos

  steps:
    - name: Checkout code
      uses: actions/checkout@v4

    - name: Download build artifacts
      uses: actions/download-artifact@v4
      with:
        name: build-${{ matrix.platform }}
        path: build

    - name: Check for symbol conflicts
      shell: bash
      run: ./scripts/verify-symbols.sh --build-dir build

    - name: Upload conflict report
      if: failure()
      uses: actions/upload-artifact@v4
      with:
        name: symbol-conflicts-${{ matrix.platform }}
        path: build/symbol-report.txt
```

---

## Appendix B: Cost Analysis

### Current Monthly CI Usage (Estimated)

| Job | Duration | Runs/Month | Total Time |
|-----|----------|------------|------------|
| build (3 platforms) | 15 min × 3 | 100 PRs | **75 hours** |
| test (3 platforms) | 5 min × 3 | 100 PRs | **25 hours** |
| windows-agent | 30 min | 30 | **15 hours** |
| code-quality | 2 min | 100 PRs | **3.3 hours** |
| memory-sanitizer | 20 min | 50 PRs | **16.7 hours** |
| z3ed-agent-test | 15 min | 30 | **7.5 hours** |
| **Total** | | | **142.5 hours** |

### Proposed Monthly CI Usage

| Job | Duration | Runs/Month | Total Time |
|-----|----------|------------|------------|
| config-validation (3) | 2 min × 3 | 100 PRs | **10 hours** |
| compile-check (3) | 5 min × 3 | 100 PRs | **25 hours** |
| build (3 platforms) | 15 min × 3 | 80 PRs | **60 hours** (↓20%) |
| test (3 platforms) | 5 min × 3 | 80 PRs | **20 hours** (↓20%) |
| symbol-check (2) | 3 min × 2 | 80 PRs | **8 hours** |
| windows-agent | 30 min | 25 | **12.5 hours** (↓17%) |
| code-quality | 2 min | 100 PRs | **3.3 hours** |
| memory-sanitizer | 20 min | 40 PRs | **13.3 hours** (↓20%) |
| z3ed-agent-test | 15 min | 25 | **6.25 hours** (↓17%) |
| **Total** | | | **158.4 hours** (+11%) |

**Net Change**: +16 hours/month (11% increase)

**BUT**:
- Fewer failed builds (20% reduction)
- Faster feedback (10-15 min saved per failure)
- Better developer experience (invaluable)

**Conclusion**: Slight increase in total CI time, but significant improvement in efficiency and developer experience
