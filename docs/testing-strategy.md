# Testing Strategy for Yaze v0.3.0

## Test Categories

Yaze uses a comprehensive testing strategy with different categories of tests to ensure quality while maintaining efficient CI/CD pipelines.

### Stable Tests (STABLE)
**Always run in CI/CD - Required for releases**

- **AsarWrapperTest**: Core Asar functionality tests
- **SnesTileTest**: SNES tile format handling
- **CompressionTest**: Data compression/decompression
- **SnesPaletteTest**: SNES palette operations
- **HexTest**: Hexadecimal utilities
- **AsarIntegrationTest**: Asar integration without ROM dependencies

**Characteristics:**
- Fast execution (< 30 seconds total)
- No external dependencies (ROMs, complex setup)
- High reliability and deterministic results
- Core functionality testing

### ROM-Dependent Tests (ROM_DEPENDENT)
**Only run in development with available ROM files**

- **AsarRomIntegrationTest**: Real ROM patching and symbol extraction
- **ROM-based integration tests**: Tests requiring actual game ROM files

**Characteristics:**
- Require specific ROM files to be present
- Test real-world functionality
- May be slower due to large file operations
- Automatically skipped in CI if ROM files unavailable

### Experimental Tests (EXPERIMENTAL)  
**Run separately, allowed to fail**

- **CpuTest**: 65816 CPU emulation tests (complex, may have timing issues)
- **Spc700Test**: SPC700 audio processor tests
- **ApuTest**: Audio Processing Unit tests
- **PpuTest**: Picture Processing Unit tests
- **Complex Integration Tests**: Multi-component integration tests

**Characteristics:**
- May be unstable due to emulation complexity
- Test advanced/experimental features
- Allowed to fail without blocking releases
- Run in separate CI job with `continue-on-error: true`

### GUI Tests (GUI_TEST)
**Tests requiring graphical components**

- **DungeonEditorIntegrationTest**: GUI-based dungeon editing
- **Editor Integration Tests**: Tests requiring ImGui components

**Characteristics:**
- Require display/graphics context
- May not work in headless CI environments
- Focus on user interface functionality

## CI/CD Strategy

### Main CI Pipeline
```yaml
# Always run - required for merge
- Run Stable Tests: --label-regex "STABLE"
  
# Optional - allowed to fail  
- Run Experimental Tests: --label-regex "EXPERIMENTAL" (continue-on-error: true)
```

### Development Testing
```bash
# Quick development testing
ctest --preset stable

# Full development testing with ROM
ctest --preset dev

# Test specific functionality
ctest --preset asar-only
```

### Release Testing
```bash
# Release candidate testing
ctest --preset stable --parallel
ctest --preset ci
```

## Test Execution Examples

### Command Line Usage

```bash
# Run only stable tests (release-ready)
ctest --test-dir build --label-regex "STABLE"

# Run experimental tests (allowed to fail)
ctest --test-dir build --label-regex "EXPERIMENTAL"

# Run Asar-specific tests
ctest --test-dir build -R "*Asar*"

# Run tests excluding ROM-dependent ones
ctest --test-dir build --label-exclude "ROM_DEPENDENT"

# Run with specific preset
ctest --preset stable
ctest --preset experimental
```

### CMake Preset Usage

```bash
# Development workflow
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# CI workflow  
cmake --preset ci
cmake --build --preset ci
ctest --preset ci

# Release workflow
cmake --preset release
cmake --build --preset release
ctest --preset stable
```

## Test Development Guidelines

### Writing Stable Tests
- **Fast execution**: Aim for < 1 second per test
- **No external dependencies**: Self-contained test data
- **Deterministic**: Same results every run
- **Core functionality**: Test essential features only

### Writing Experimental Tests
- **Complex scenarios**: Multi-component integration
- **Advanced features**: Emulation, complex algorithms
- **Performance tests**: May vary by system
- **GUI components**: May require display context

### Writing ROM-Dependent Tests
- **Use TestRomManager**: Proper ROM file handling
- **Graceful skipping**: Skip if ROM not available
- **Real-world scenarios**: Test with actual game data
- **Label appropriately**: Always include ROM_DEPENDENT label

## CI/CD Efficiency

### Fast Feedback Loop
1. **Stable tests run first** - Quick feedback for developers
2. **Experimental tests run in parallel** - Don't block on unstable tests
3. **ROM tests skipped** - No dependency on external files
4. **Selective test execution** - Only run relevant tests for changes

### Release Quality Gates
1. **All stable tests must pass** - No exceptions
2. **Experimental tests informational only** - Don't block releases
3. **ROM tests run manually** - When ROM files available
4. **Performance benchmarks** - Track regression trends

## Maintenance Strategy

### Regular Review
- **Monthly review** of experimental test failures
- **Promote stable experimental tests** to stable category
- **Deprecate obsolete tests** that no longer provide value
- **Update test categorization** as features mature

### Performance Monitoring
- **Track test execution times** for CI efficiency
- **Identify slow tests** for optimization or recategorization
- **Monitor CI resource usage** and adjust parallelism
- **Benchmark critical path tests** for performance regression

## Test Categories by Feature

### Asar Integration
- **Stable**: AsarWrapperTest, AsarIntegrationTest
- **ROM-Dependent**: AsarRomIntegrationTest
- **Focus**: Core assembly patching and symbol extraction

### Graphics System
- **Stable**: SnesTileTest, SnesPaletteTest, CompressionTest
- **Experimental**: Complex rendering tests
- **Focus**: SNES graphics format handling

### Emulation
- **Experimental**: CpuTest, Spc700Test, ApuTest, PpuTest
- **Focus**: Hardware emulation accuracy
- **Note**: Complex timing-sensitive tests

### Editor Components
- **GUI**: DungeonEditorIntegrationTest, Editor integration tests
- **Experimental**: Complex editor workflow tests
- **Focus**: User interface functionality

This strategy ensures efficient CI/CD while maintaining comprehensive test coverage for quality assurance.
