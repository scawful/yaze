# Integration Test Suite Guide

This guide explains how to use yaze's integration test suite to validate ROM loading, overworld functionality, and ensure compatibility between vanilla and ZSCustomOverworld ROMs.

## Table of Contents

1. [Overview](#overview)
2. [Test Structure](#test-structure)
3. [Running Tests](#running-tests)
4. [Understanding Results](#understanding-results)
5. [Adding New Tests](#adding-new-tests)
6. [Debugging Failed Tests](#debugging-failed-tests)
7. [Best Practices](#best-practices)

## Overview

The integration test suite validates that yaze correctly loads and processes ROM data by comparing against known values from vanilla ROMs and ensuring that ZSCustomOverworld features work as expected.

### Key Components

- **Vanilla ROM Tests**: Validate loading of original Zelda 3 ROMs
- **ZSCustomOverworld Tests**: Validate v2/v3 feature compatibility
- **Sprite Position Tests**: Verify sprite coordinate systems
- **Overworld Map Tests**: Test map loading and property access

## Test Structure

### Test Files

```
test/zelda3/
├── overworld_test.cc              # Unit tests for OverworldMap class
├── overworld_integration_test.cc  # Integration tests with real ROMs
├── sprite_position_test.cc        # Sprite coordinate system tests
├── comprehensive_integration_test.cc  # Full ROM validation
└── extract_vanilla_values.cc      # Utility to extract test values
```

### Test Categories

#### 1. Unit Tests (`overworld_test.cc`)

Test individual components in isolation:

```cpp
TEST_F(OverworldTest, OverworldMapInitialization) {
  OverworldMap map(0, rom_.get());
  
  EXPECT_EQ(map.area_graphics(), 0);
  EXPECT_EQ(map.area_palette(), 0);
  EXPECT_EQ(map.area_size(), AreaSizeEnum::SmallArea);
}
```

#### 2. Integration Tests (`overworld_integration_test.cc`)

Test with real ROM files:

```cpp
TEST_F(OverworldIntegrationTest, VanillaROMLoading) {
  // Load vanilla ROM
  auto rom = LoadVanillaROM();
  Overworld overworld(rom.get());
  
  // Test specific map properties
  auto* map = overworld.overworld_map(0);
  EXPECT_EQ(map->area_graphics(), expected_graphics);
  EXPECT_EQ(map->area_palette(), expected_palette);
}
```

#### 3. Sprite Tests (`sprite_position_test.cc`)

Validate sprite coordinate systems:

```cpp
TEST_F(SpritePositionTest, SpriteCoordinateSystem) {
  // Load ROM and test sprite positioning
  auto rom = LoadTestROM();
  Overworld overworld(rom.get());
  
  // Verify sprites are positioned correctly for each world
  for (int game_state = 0; game_state < 3; game_state++) {
    auto& sprites = *overworld.mutable_sprites(game_state);
    // Test sprite coordinates and world filtering
  }
}
```

## Running Tests

### Prerequisites

1. **Build yaze**: Ensure yaze is built with test support
2. **ROM Files**: Have test ROM files available
3. **Test Data**: Generated test values from vanilla ROMs

### Basic Test Execution

```bash
# Run all tests
cd /Users/scawful/Code/yaze/build
./bin/yaze_test

# Run specific test suite
./bin/yaze_test --gtest_filter="OverworldTest.*"

# Run specific test
./bin/yaze_test --gtest_filter="SpritePositionTest.SpriteCoordinateSystem"

# Run with verbose output
./bin/yaze_test --gtest_filter="OverworldIntegrationTest.*" --gtest_output=xml:test_results.xml
```

### Test Filtering

Use Google Test filters to run specific tests:

```bash
# Run only overworld tests
./bin/yaze_test --gtest_filter="Overworld*"

# Run only integration tests
./bin/yaze_test --gtest_filter="*Integration*"

# Run tests matching pattern
./bin/yaze_test --gtest_filter="*Sprite*"

# Exclude specific tests
./bin/yaze_test --gtest_filter="OverworldTest.*:-OverworldTest.OverworldMapDestroy"
```

### Parallel Execution

```bash
# Run tests in parallel (faster)
./bin/yaze_test --gtest_parallel=4

# Run with specific number of workers
./bin/yaze_test --gtest_workers=8
```

## Understanding Results

### Test Output Format

```
[==========] Running 3 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 3 tests from OverworldTest
[ RUN      ] OverworldTest.OverworldMapInitialization
[       OK ] OverworldTest.OverworldMapInitialization (2 ms)
[ RUN      ] OverworldTest.AreaSizeEnumValues
[       OK ] OverworldTest.AreaSizeEnumValues (1 ms)
[ RUN      ] OverworldTest.OverworldMapDestroy
[       OK ] OverworldTest.OverworldMapDestroy (1 ms)
[----------] 3 tests from OverworldTest (4 ms total)
[----------] Global test environment tear-down.
[==========] 3 tests from 1 test suite ran. (4 ms total)
[  PASSED  ] 3 tests.
```

### Success Indicators

- `[       OK ]`: Test passed
- `[  PASSED  ]`: All tests in suite passed
- No error messages or stack traces

### Failure Indicators

- `[  FAILED  ]`: Test failed
- Error messages with expected vs actual values
- Stack traces showing failure location

### Example Failure Output

```
[ RUN      ] OverworldTest.OverworldMapInitialization
test/zelda3/overworld_test.cc:45: Failure
Expected equality of these values:
  map.area_graphics()
    Which is: 5
  0
[  FAILED  ] OverworldTest.OverworldMapInitialization (2 ms)
```

## Adding New Tests

### 1. Unit Test Example

```cpp
// Add to overworld_test.cc
TEST_F(OverworldTest, VanillaOverlayLoading) {
  OverworldMap map(0, rom_.get());
  
  // Test vanilla overlay loading
  RETURN_IF_ERROR(map.LoadVanillaOverlay());
  
  // Verify overlay data
  EXPECT_TRUE(map.has_vanilla_overlay());
  EXPECT_GT(map.vanilla_overlay_data().size(), 0);
}
```

### 2. Integration Test Example

```cpp
// Add to overworld_integration_test.cc
TEST_F(OverworldIntegrationTest, ZSCustomOverworldV3Features) {
  // Load ZSCustomOverworld v3 ROM
  auto rom = LoadZSCustomOverworldV3ROM();
  Overworld overworld(rom.get());
  
  // Test v3 features
  auto* map = overworld.overworld_map(0);
  EXPECT_GT(map->subscreen_overlay(), 0);
  EXPECT_GT(map->animated_gfx(), 0);
  
  // Test custom tile graphics
  for (int i = 0; i < 8; i++) {
    EXPECT_GE(map->custom_tileset(i), 0);
  }
}
```

### 3. Performance Test Example

```cpp
TEST_F(OverworldPerformanceTest, LargeMapLoading) {
  auto start = std::chrono::high_resolution_clock::now();
  
  // Load large number of maps
  for (int i = 0; i < 100; i++) {
    OverworldMap map(i % 160, rom_.get());
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Ensure loading is reasonably fast
  EXPECT_LT(duration.count(), 1000); // Less than 1 second
}
```

## Debugging Failed Tests

### 1. Enable Debug Output

```cpp
// Add debug output to tests
TEST_F(OverworldTest, DebugTest) {
  OverworldMap map(0, rom_.get());
  
  // Print debug information
  std::cout << "Map index: " << map.index() << std::endl;
  std::cout << "Area graphics: " << static_cast<int>(map.area_graphics()) << std::endl;
  std::cout << "Area palette: " << static_cast<int>(map.area_palette()) << std::endl;
  
  // Your assertions here
}
```

### 2. Use GDB for Debugging

```bash
# Run test with GDB
gdb --args ./bin/yaze_test --gtest_filter="OverworldTest.DebugTest"

# Set breakpoints
(gdb) break overworld_test.cc:45
(gdb) run

# Inspect variables
(gdb) print map.area_graphics()
(gdb) print map.area_palette()
```

### 3. Memory Debugging

```bash
# Run with Valgrind (Linux)
valgrind --leak-check=full ./bin/yaze_test --gtest_filter="OverworldTest.*"

# Run with AddressSanitizer
export ASAN_OPTIONS=detect_leaks=1
./bin/yaze_test --gtest_filter="OverworldTest.*"
```

### 4. Common Debugging Scenarios

#### ROM Loading Issues

```cpp
// Check ROM version detection
uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
std::cout << "ASM Version: 0x" << std::hex << static_cast<int>(asm_version) << std::endl;

// Verify ROM size
std::cout << "ROM Size: " << rom_->size() << " bytes" << std::endl;
```

#### Map Property Issues

```cpp
// Check map loading
auto* map = overworld.overworld_map(current_map_);
std::cout << "Map " << current_map_ << " properties:" << std::endl;
std::cout << "  Area Graphics: 0x" << std::hex << static_cast<int>(map->area_graphics()) << std::endl;
std::cout << "  Area Palette: 0x" << std::hex << static_cast<int>(map->area_palette()) << std::endl;
std::cout << "  Main Palette: 0x" << std::hex << static_cast<int>(map->main_palette()) << std::endl;
```

#### Sprite Issues

```cpp
// Check sprite loading
for (int game_state = 0; game_state < 3; game_state++) {
  auto& sprites = *overworld.mutable_sprites(game_state);
  std::cout << "Game State " << game_state << ": " << sprites.size() << " sprites" << std::endl;
  
  for (size_t i = 0; i < std::min(sprites.size(), size_t(5)); i++) {
    auto& sprite = sprites[i];
    std::cout << "  Sprite " << i << ": Map=" << sprite.map_id() 
              << ", X=" << sprite.x_ << ", Y=" << sprite.y_ << std::endl;
  }
}
```

## Best Practices

### 1. Test Organization

- **Group related tests**: Use descriptive test suite names
- **One concept per test**: Each test should verify one specific behavior
- **Descriptive names**: Test names should clearly indicate what they're testing

```cpp
// Good
TEST_F(OverworldTest, VanillaOverlayLoadingForMap00) {
  // Test specific map's overlay loading
}

// Bad
TEST_F(OverworldTest, Test1) {
  // Unclear what this tests
}
```

### 2. Test Data Management

```cpp
class OverworldTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize test data once
    rom_ = std::make_unique<Rom>();
    // ... setup code
  }
  
  void TearDown() override {
    // Clean up after each test
    rom_.reset();
  }
  
  std::unique_ptr<Rom> rom_;
};
```

### 3. Error Handling in Tests

```cpp
TEST_F(OverworldTest, ErrorHandling) {
  // Test error conditions
  OverworldMap map(999, rom_.get()); // Invalid map index
  
  // Verify error handling
  EXPECT_FALSE(map.is_initialized());
}
```

### 4. Performance Considerations

```cpp
// Use fixtures for expensive setup
class ExpensiveTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Expensive setup here
    LoadLargeROM();
    ProcessAllMaps();
  }
};

// Run expensive tests separately
TEST_F(ExpensiveTest, FullOverworldProcessing) {
  // Test that requires expensive setup
}
```

### 5. Continuous Integration

Add tests to CI pipeline:

```yaml
# .github/workflows/tests.yml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build and Test
        run: |
          cd build
          make -j4 yaze_test
          ./bin/yaze_test --gtest_output=xml:test_results.xml
      - name: Upload Test Results
        uses: actions/upload-artifact@v2
        with:
          name: test-results
          path: build/test_results.xml
```

## Test Results Interpretation

### Viewing Results

Test results are typically displayed in the terminal, but you can also generate XML reports:

```bash
# Generate XML report
./bin/yaze_test --gtest_output=xml:test_results.xml

# View in browser (if you have a test result viewer)
open test_results.xml
```

### Key Metrics

- **Test Count**: Number of tests run
- **Pass Rate**: Percentage of tests that passed
- **Execution Time**: How long tests took to run
- **Memory Usage**: Peak memory consumption during tests

### Performance Benchmarks

Track performance over time:

```bash
# Run with timing
time ./bin/yaze_test --gtest_filter="OverworldPerformanceTest.*"

# Profile with gprof
gprof ./bin/yaze_test gmon.out > profile.txt
```

## Conclusion

The integration test suite is essential for maintaining yaze's reliability and ensuring compatibility with different ROM types. By following this guide, you can effectively run tests, debug issues, and add new test cases to improve the overall quality of the codebase.

Remember to:
- Run tests regularly during development
- Add tests for new features
- Debug failures promptly
- Keep tests fast and focused
- Use appropriate test data and fixtures
