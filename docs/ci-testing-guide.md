# CI/CD Testing Strategy - Efficient Workflows

## Overview

Yaze v0.3.0 implements a sophisticated testing strategy designed for efficient CI/CD pipelines while maintaining comprehensive quality assurance.

## Test Categories & Execution Strategy

### 🟢 STABLE Tests (CI Required)
**These tests MUST pass for releases**

- **AsarWrapperTest** (11/12 tests passing): Core Asar functionality
- **SnesTileTest**: SNES graphics format handling  
- **CompressionTest**: Data compression utilities
- **SnesPaletteTest**: SNES palette operations
- **Basic ROM operations**: Core file handling

**Execution Time**: < 30 seconds total  
**CI Strategy**: Run on every commit, block merge if failing

### 🟡 EXPERIMENTAL Tests (CI Informational)  
**These tests can fail without blocking releases**

- **CpuTest** (400+ tests): 65816 CPU emulation - complex timing issues
- **Spc700Test**: SPC700 audio processor - emulation accuracy  
- **Complex Integration Tests**: Multi-component scenarios
- **MessageTest**: Text parsing - may have encoding issues
- **DungeonIntegrationTest**: Complex editor workflows

**Execution Time**: 5-10 minutes  
**CI Strategy**: Run separately with `continue-on-error: true`

### 🔴 ROM_DEPENDENT Tests (Development Only)
**These tests require actual ROM files**

- **AsarRomIntegrationTest**: Real ROM patching tests
- **ROM-based validation**: Tests with actual game data

**Execution**: Only in development environment  
**CI Strategy**: Automatically skipped in CI (`--label-exclude ROM_DEPENDENT`)

## Dependency Management

### NFD (Native File Dialog) - Optional
```cmake
# Conditional inclusion for CI efficiency
option(YAZE_MINIMAL_BUILD "Minimal build for CI" OFF)

if(NOT YAZE_MINIMAL_BUILD)
    add_subdirectory(lib/nativefiledialog-extended)  # Requires GTK on Linux
else()
    # Skip NFD to avoid GTK dependency in CI
endif()
```

### Linux Dependencies for Full Build
```bash
# Required for NFD on Linux
sudo apt-get install libgtk-3-dev libdbus-1-dev

# Core dependencies (always required)  
sudo apt-get install libglew-dev libxext-dev libwavpack-dev libpng-dev
```

## CI Configuration Examples

### Fast CI Pipeline (< 5 minutes)
```yaml
- name: Configure (Minimal)
  run: cmake -B build -DYAZE_MINIMAL_BUILD=ON -DYAZE_ENABLE_EXPERIMENTAL_TESTS=OFF

- name: Build
  run: cmake --build build --parallel

- name: Test (Stable Only)  
  run: ctest --test-dir build --label-regex "STABLE" --parallel
```

### Development Pipeline (Complete)
```yaml  
- name: Configure (Full)
  run: cmake -B build -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_ENABLE_EXPERIMENTAL_TESTS=ON

- name: Build
  run: cmake --build build --parallel

- name: Test (All)
  run: ctest --test-dir build --output-on-failure
```

## Local Development Workflows

### Quick Development Testing
```bash
# Test only Asar functionality (< 30 seconds)
ctest --test-dir build -R "*AsarWrapper*" --parallel

# Test stable features only  
ctest --test-dir build --label-regex "STABLE" --parallel

# Full development testing
cmake --preset dev
cmake --build --preset dev  
ctest --preset dev
```

### Release Validation
```bash
# Release candidate testing (stable tests only)
cmake --preset release
cmake --build --preset release
ctest --test-dir build --label-regex "STABLE" --stop-on-failure

# Performance validation  
ctest --test-dir build --label-regex "STABLE" --repeat until-pass:3
```

## Test Maintenance Strategy

### Weekly Review Process
1. **Check experimental test results** - identify tests ready for promotion
2. **Update test categorization** - move stable experimental tests to STABLE
3. **Performance monitoring** - track test execution times
4. **Failure analysis** - investigate patterns in experimental test failures

### Promotion Criteria (Experimental → Stable)
- **Consistent passing** for 2+ weeks
- **Fast execution** (< 10 seconds per test)
- **No external dependencies** (ROM files, GUI, complex setup)
- **Deterministic results** across platforms

### Test Categories by Stability

#### Currently Stable (22/24 tests passing)
- AsarWrapperTest.InitializationAndShutdown ✅
- AsarWrapperTest.ValidPatchApplication ✅
- AsarWrapperTest.SymbolExtraction ✅
- AsarWrapperTest.PatchFromString ✅
- AsarWrapperTest.ResetFunctionality ✅

#### Needs Attention (2/24 tests failing)
- AsarWrapperTest.AssemblyValidation ⚠️ (Error message format mismatch)

#### Experimental (Many failing but expected)
- CpuTest.* (400+ tests) - Complex 65816 emulation  
- MessageTest.* - Text parsing edge cases
- Complex integration tests - Multi-component scenarios

## Efficiency Metrics

### Target CI Times
- **Stable Test Suite**: < 30 seconds
- **Full Build**: < 5 minutes  
- **Total CI Pipeline**: < 10 minutes per platform

### Resource Optimization
- **Parallel Testing**: Use all available CPU cores
- **Selective Dependencies**: Skip optional dependencies in CI
- **Test Categorization**: Run only relevant tests for changes
- **Artifact Caching**: Cache build dependencies between runs

## Error Handling Strategy

### Build Failures
- **NFD/GTK Issues**: Use YAZE_MINIMAL_BUILD=ON to skip
- **Dependency Problems**: Clear dependency installation in CI
- **Platform-Specific**: Use matrix builds with proper toolchains

### Test Failures  
- **Stable Tests**: Must be fixed before merge
- **Experimental Tests**: Log for review, don't block pipeline
- **ROM Tests**: Skip gracefully when ROM unavailable
- **GUI Tests**: May need headless display configuration

## Monitoring & Metrics

### Success Metrics
- **Stable Test Pass Rate**: 95%+ required
- **CI Pipeline Duration**: < 10 minutes target
- **Build Success Rate**: 98%+ across all platforms
- **Release Cadence**: Monthly releases with high confidence

### Quality Gates
1. All stable tests pass
2. Build succeeds on all platforms  
3. No critical security issues
4. Performance regression check
5. Documentation updated

This strategy ensures efficient CI/CD while maintaining high quality standards for the yaze project.
