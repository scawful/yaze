# Testing Guide

Comprehensive testing framework with efficient CI/CD integration and ROM-dependent test separation.

## Test Categories

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

### ROM-Dependent Tests (ROM_DEPENDENT)
**Only run in development with available ROM files**

- **AsarRomIntegrationTest**: Real ROM patching and symbol extraction
- **ROM-based integration tests**: Tests requiring actual game ROM files

**Characteristics:**
- Require specific ROM files to be present
- Test real-world functionality
- Automatically skipped in CI if ROM files unavailable

### Experimental Tests (EXPERIMENTAL)
**Run separately, allowed to fail**

- **CpuTest**: 65816 CPU emulation tests
- **Spc700Test**: SPC700 audio processor tests
- **ApuTest**: Audio Processing Unit tests
- **PpuTest**: Picture Processing Unit tests

**Characteristics:**
- May be unstable due to emulation complexity
- Test advanced/experimental features
- Allowed to fail without blocking releases

## Command Line Usage

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

## CMake Presets

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

## Writing Tests

### Stable Tests
```cpp
TEST(SnesTileTest, UnpackBppTile) {
    std::vector<uint8_t> tile_data = {0xAA, 0x55, 0xAA, 0x55};
    std::vector<uint8_t> result = UnpackBppTile(tile_data, 2);
    EXPECT_EQ(result.size(), 64);
    // Test specific pixel values...
}
```

### ROM-Dependent Tests
```cpp
YAZE_ROM_TEST(AsarIntegration, RealRomPatching) {
    auto rom_data = TestRomManager::LoadTestRom();
    if (!rom_data.has_value()) {
        GTEST_SKIP() << "ROM file not available";
    }
    
    AsarWrapper wrapper;
    wrapper.Initialize();
    
    auto result = wrapper.ApplyPatch("test.asm", *rom_data);
    EXPECT_TRUE(result.ok());
}
```

### Experimental Tests
```cpp
TEST(CpuTest, InstructionExecution) {
    // Complex emulation tests
    // May be timing-sensitive or platform-dependent
}
```

## CI/CD Integration

### GitHub Actions
```yaml
# Main CI pipeline
- name: Run Stable Tests
  run: ctest --label-regex "STABLE"

# Experimental tests (allowed to fail)
- name: Run Experimental Tests
  run: ctest --label-regex "EXPERIMENTAL"
  continue-on-error: true
```

### Test Execution Strategy
1. **Stable tests run first** - Quick feedback for developers
2. **Experimental tests run in parallel** - Don't block on unstable tests
3. **ROM tests skipped** - No dependency on external files
4. **Selective test execution** - Only run relevant tests for changes

## Test Development Guidelines

### Writing Stable Tests
- **Fast execution**: Aim for < 1 second per test
- **No external dependencies**: Self-contained test data
- **Deterministic**: Same results every run
- **Core functionality**: Test essential features only

### Writing ROM-Dependent Tests
- **Use TestRomManager**: Proper ROM file handling
- **Graceful skipping**: Skip if ROM not available
- **Real-world scenarios**: Test with actual game data
- **Label appropriately**: Always include ROM_DEPENDENT label

### Writing Experimental Tests
- **Complex scenarios**: Multi-component integration
- **Advanced features**: Emulation, complex algorithms
- **Performance tests**: May vary by system
- **GUI components**: May require display context

## Performance and Maintenance

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
