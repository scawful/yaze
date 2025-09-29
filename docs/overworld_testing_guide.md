# YAZE Overworld Testing Guide

## Overview

This guide provides comprehensive documentation for testing the YAZE overworld implementation, including unit tests, integration tests, end-to-end tests, and golden data validation. The testing framework ensures that the YAZE C++ implementation correctly mirrors the ZScream C# logic.

## Test Architecture

### 1. Golden Data System

The golden data system provides a "source of truth" for ROM validation:

- **Golden Data Extractor**: Extracts all overworld-related values from ROMs
- **Before/After Validation**: Compares ROM states before and after edits
- **Reference Data**: Provides expected values for test validation

### 2. Test Categories

#### Unit Tests (`test/unit/zelda3/`)
- **`overworld_test.cc`**: Basic overworld functionality tests
- **`overworld_integration_test.cc`**: Comprehensive integration tests
- **`extract_vanilla_values.cc`**: ROM value extraction utility

#### Integration Tests (`test/e2e/`)
- **`overworld/overworld_e2e_test.cc`**: End-to-end workflow tests
- **`zscustomoverworld/zscustomoverworld_upgrade_test.cc`**: ASM version upgrade tests

#### Golden Data Tools
- **`overworld_golden_data_extractor.cc`**: Comprehensive ROM data extraction
- **`run_overworld_tests.sh`**: Orchestrated test runner script

## Quick Start

### Prerequisites

1. **Build YAZE**: Ensure the project is built with tests enabled
2. **ROM File**: Have a valid Zelda 3 ROM file (zelda3.sfc)
3. **Environment**: Set up test environment variables

### Running Tests

#### Basic Test Run
```bash
# Run all overworld tests with a ROM
./scripts/run_overworld_tests.sh zelda3.sfc

# Generate detailed report
./scripts/run_overworld_tests.sh zelda3.sfc --generate-report

# Clean up test files after completion
./scripts/run_overworld_tests.sh zelda3.sfc --cleanup
```

#### Selective Test Execution
```bash
# Skip unit tests, run only integration and E2E
./scripts/run_overworld_tests.sh zelda3.sfc --skip-unit-tests

# Extract golden data only
./scripts/run_overworld_tests.sh zelda3.sfc --skip-unit-tests --skip-integration --skip-e2e

# Run with custom ROM path
export YAZE_TEST_ROM_PATH="/path/to/custom/rom.sfc"
./scripts/run_overworld_tests.sh /path/to/custom/rom.sfc
```

## Test Components

### 1. Golden Data Extractor

The `overworld_golden_data_extractor` tool extracts comprehensive data from ROMs:

```bash
# Extract golden data from a ROM
./build/bin/overworld_golden_data_extractor zelda3.sfc golden_data.h
```

**Extracted Data Includes:**
- Basic ROM information (title, size, checksums)
- ASM version and feature flags
- Overworld map properties (graphics, palettes, sizes)
- Tile data (Tile16/Tile32 expansion status)
- Entrance/hole/exit coordinate data
- Item and sprite data
- Map tiles (compressed data)
- Palette and music data
- Overlay data

### 2. Integration Tests

The integration tests validate core overworld functionality:

```cpp
// Example: Test coordinate calculation accuracy
TEST_F(OverworldIntegrationTest, ZScreamCoordinateCompatibility) {
  // Load overworld data
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());
  
  // Validate coordinate calculations match ZScream exactly
  const auto& entrances = overworld_->entrances();
  for (int i = 0; i < 10; i++) {
    const auto& entrance = entrances[i];
    
    // ZScream coordinate calculation logic
    uint16_t map_pos = entrance.map_pos();
    uint16_t map_id = entrance.map_id();
    
    int position = map_pos >> 1;
    int x_coord = position % 64;
    int y_coord = position >> 6;
    int expected_x = (x_coord * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512);
    int expected_y = (y_coord * 16) + (((map_id % 64) / 8) * 512);
    
    EXPECT_EQ(entrance.x(), expected_x);
    EXPECT_EQ(entrance.y(), expected_y);
  }
}
```

### 3. End-to-End Tests

E2E tests validate complete workflows:

```cpp
// Example: Test ASM version upgrade workflow
TEST_F(OverworldE2ETest, ApplyZSCustomOverworldV3) {
  // Load vanilla ROM
  std::unique_ptr<Rom> rom = std::make_unique<Rom>();
  ASSERT_OK(rom->LoadFromFile(vanilla_test_path_));
  
  // Apply ZSCustomOverworld v3 ASM
  ASSERT_OK(rom->WriteByte(0x140145, 0x03)); // Set ASM version to v3
  
  // Enable v3 features
  ASSERT_OK(rom->WriteByte(0x140146, 0x01)); // Enable main palettes
  ASSERT_OK(rom->WriteByte(0x140147, 0x01)); // Enable area-specific BG
  // ... more feature flags
  
  // Save and reload
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = edited_test_path_}));
  
  // Validate changes persisted
  std::unique_ptr<Rom> reloaded_rom = std::make_unique<Rom>();
  ASSERT_OK(reloaded_rom->LoadFromFile(edited_test_path_));
  
  auto asm_version = reloaded_rom->ReadByte(0x140145);
  EXPECT_EQ(*asm_version, 0x03);
}
```

## Test Validation Points

### 1. ZScream Compatibility

Tests ensure YAZE behavior matches ZScream exactly:

- **Coordinate Calculations**: Entrance/hole coordinates use identical formulas
- **ASM Version Detection**: Proper handling of vanilla vs ZSCustomOverworld ROMs
- **Data Structure Loading**: Same data organization and access patterns
- **Expansion Detection**: Correct identification of expanded vs vanilla data

### 2. ROM State Validation

Tests validate ROM integrity:

- **Before/After Comparison**: Ensure edits are properly saved and loaded
- **Feature Flag Persistence**: ZSCustomOverworld features remain enabled after save
- **Data Integrity**: Original data is preserved where expected
- **Checksum Validation**: ROM remains valid after modifications

### 3. Performance and Stability

Tests ensure robustness:

- **Multiple Load Cycles**: Repeated load/unload operations
- **Memory Management**: Proper cleanup of resources
- **Error Handling**: Graceful handling of invalid data
- **Thread Safety**: Concurrent access patterns (if applicable)

## Environment Variables

### Test Configuration

```bash
# ROM path for testing
export YAZE_TEST_ROM_PATH="/path/to/rom.sfc"

# Skip ROM-dependent tests
export YAZE_SKIP_ROM_TESTS=1

# Test output verbosity
export GTEST_VERBOSE=1
```

### Build Configuration

```bash
# Enable tests in CMake
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..

# Build with specific test targets
cmake --build build --target overworld_golden_data_extractor
cmake --build build --target extract_vanilla_values
```

## Test Reports

### Generated Reports

The test runner generates comprehensive reports:

```markdown
# YAZE Overworld Test Report

**Generated:** 2024-01-15 14:30:00
**ROM:** zelda3.sfc
**ROM Size:** 2097152 bytes

## Test Results Summary

| Test Category | Status | Details |
|---------------|--------|---------|
| Golden Data Extraction | SUCCESS: golden_data.h | |
| Unit Tests | PASSED | |
| Integration Tests | PASSED | |
| E2E Tests | PASSED | |

## ROM Information

### ROM Header
[Hex dump of ROM header for validation]
```

### Report Location

Reports are saved to `test/reports/` with timestamps:
- `overworld_test_report_YYYYMMDD_HHMMSS.md`

## Troubleshooting

### Common Issues

#### 1. ROM Not Found
```
Error: ROM file not found: zelda3.sfc
```
**Solution**: Provide correct ROM path or set `YAZE_TEST_ROM_PATH`

#### 2. Build Failures
```
Error: Failed to build golden data extractor
```
**Solution**: Ensure project is built with tests enabled:
```bash
cmake -DBUILD_TESTS=ON ..
cmake --build build
```

#### 3. Test Failures
```
Error: Some tests failed. Check the results above.
```
**Solution**: 
- Check individual test logs for specific failures
- Verify ROM file is valid Zelda 3 ROM
- Ensure test environment is properly configured

### Debug Mode

Run tests with maximum verbosity:

```bash
# Enable debug output
export GTEST_VERBOSE=1
export YAZE_DEBUG=1

# Run with verbose output
./scripts/run_overworld_tests.sh zelda3.sfc --generate-report
```

## Advanced Usage

### Custom Test Scenarios

#### 1. Testing Custom ROMs
```bash
# Test with modified ROM
./scripts/run_overworld_tests.sh /path/to/modified_rom.sfc

# Extract golden data for comparison
./build/bin/overworld_golden_data_extractor /path/to/modified_rom.sfc modified_golden_data.h
```

#### 2. Regression Testing
```bash
# Extract golden data from known good ROM
./build/bin/overworld_golden_data_extractor known_good_rom.sfc reference_golden_data.h

# Test modified ROM against reference
./scripts/run_overworld_tests.sh modified_rom.sfc --generate-report
```

#### 3. Performance Testing
```bash
# Run performance-focused tests
export YAZE_PERFORMANCE_TESTS=1
./scripts/run_overworld_tests.sh zelda3.sfc
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Overworld Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Build YAZE
      run: |
        cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..
        cmake --build build
    
    - name: Download Test ROM
      run: |
        # Download test ROM (placeholder - use actual ROM)
        wget -O zelda3.sfc https://example.com/test_rom.sfc
    
    - name: Run Overworld Tests
      run: |
        chmod +x scripts/run_overworld_tests.sh
        ./scripts/run_overworld_tests.sh zelda3.sfc --generate-report
    
    - name: Upload Test Reports
      uses: actions/upload-artifact@v3
      with:
        name: test-reports
        path: test/reports/
```

## Contributing

### Adding New Tests

1. **Unit Tests**: Add to `test/unit/zelda3/overworld_test.cc`
2. **Integration Tests**: Add to `test/unit/zelda3/overworld_integration_test.cc`
3. **E2E Tests**: Add to `test/e2e/overworld/overworld_e2e_test.cc`

### Test Guidelines

- **Naming**: Use descriptive test names that explain the scenario
- **Documentation**: Include comments explaining complex test logic
- **Isolation**: Each test should be independent and not rely on others
- **Cleanup**: Ensure tests clean up resources and temporary files

### Example Test Structure

```cpp
// Test descriptive name that explains the scenario
TEST_F(OverworldIntegrationTest, Tile32ExpansionDetectionWithV3ASM) {
  // Setup: Configure test environment
  mock_rom_data_[0x01772E] = 0x04; // Set expansion flag
  
  // Execute: Run the code under test
  auto status = overworld_->Load(rom_.get());
  ASSERT_TRUE(status.ok());
  
  // Verify: Check expected outcomes
  EXPECT_TRUE(overworld_->expanded_tile32());
  EXPECT_FALSE(overworld_->expanded_tile16());
  
  // Cleanup: (handled by test framework)
}
```

## Conclusion

The YAZE overworld testing framework provides comprehensive validation of the C++ implementation against the ZScream C# reference. The golden data system ensures consistency, while the multi-layered test approach (unit, integration, E2E) provides confidence in the implementation's correctness and robustness.

For questions or issues, refer to the test logs and generated reports, or consult the YAZE development team.
