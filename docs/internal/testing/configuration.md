# Test Suite Configuration Guide

## Overview

The yaze test suite has been reorganized to improve CI performance and developer experience. Optional test suites (ROM-dependent, AI experimental, benchmarks) are now gated OFF by default and only run in nightly CI or when explicitly enabled.

## CMake Options for Test Suites

| Option | Default | Description | Required For |
|--------|---------|-------------|--------------|
| `YAZE_BUILD_TESTS` | ON | Build test executables | All tests |
| `YAZE_ENABLE_ROM_TESTS` | **OFF** | Enable tests requiring ROM files | ROM-dependent tests |
| `YAZE_ENABLE_AI_RUNTIME` | **OFF** | Enable AI runtime integration tests | Experimental AI tests |
| `YAZE_ENABLE_BENCHMARK_TESTS` | **OFF** | Enable performance benchmarks | Benchmark suite |
| `YAZE_TEST_ROM_VANILLA_PATH` | *(empty)* | Path to vanilla test ROM file | ROM-dependent tests |
| `YAZE_TEST_ROM_EXPANDED_PATH` | *(empty)* | Path to expanded test ROM file | Expanded ROM tests |
| `YAZE_TEST_ROM_PATH` | *(legacy)* | Legacy vanilla ROM path | ROM-dependent tests |

## Test Categories and Labels

### Default Test Suites (Always Enabled)

- **stable**: Core functionality tests that should always pass
  - Unit tests for core components
  - Integration tests without external dependencies
  - Basic smoke tests
  - Label: `stable`

- **gui**: GUI framework tests using ImGuiTestEngine
  - Canvas automation tests
  - Editor smoke tests
  - Can run headlessly with `-nogui` flag
  - Labels: `gui`, `experimental`

### Optional Test Suites (Off by Default)

- **rom_dependent**: Tests requiring Zelda3 ROM file
  - ROM loading and manipulation tests
  - Version upgrade tests
  - Full editor workflow tests
  - Label: `rom_dependent`
  - Enable with: `-DYAZE_ENABLE_ROM_TESTS=ON`

- **experimental**: AI and experimental feature tests
  - Gemini vision API tests
  - AI-powered test generation
  - Agent automation tests
  - Label: `experimental`
  - Enable with: `-DYAZE_ENABLE_AI_RUNTIME=ON`

- **benchmark**: Performance benchmarks
  - Graphics optimization benchmarks
  - Memory pool performance tests
  - Label: `benchmark`
  - Enable with: `-DYAZE_ENABLE_BENCHMARK_TESTS=ON`

## Running Tests

### Quick Start (Stable Tests Only)
```bash
# Configure with default settings (optional suites OFF)
cmake --preset mac-dbg
cmake --build build --target yaze_test
ctest --test-dir build -L stable
```

### With ROM-Dependent Tests
```bash
# Configure with ROM tests enabled
cmake --preset mac-dbg -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_VANILLA_PATH=~/roms/alttp_vanilla.sfc
cmake --build build
ctest --test-dir build -L rom_dependent
```

### With AI/Experimental Tests
```bash
# Configure with AI runtime enabled
cmake --preset mac-ai  # or lin-ai, win-ai
cmake --build build
ctest --test-dir build -L experimental

# Note: AI tests require:
# - GEMINI_API_KEY environment variable for Gemini tests
# - Ollama running locally for Ollama tests
```

### With Benchmark Tests
```bash
# Configure with benchmarks enabled
cmake --preset mac-dbg -DYAZE_ENABLE_BENCHMARK_TESTS=ON
cmake --build build
ctest --test-dir build -L benchmark
```

### Run All Tests (Nightly Configuration)
```bash
# Enable all optional suites
cmake --preset mac-dbg \
  -DYAZE_ENABLE_ROM_TESTS=ON \
  -DYAZE_ENABLE_AI_RUNTIME=ON \
  -DYAZE_ENABLE_BENCHMARK_TESTS=ON \
  -DYAZE_TEST_ROM_VANILLA_PATH=~/roms/alttp_vanilla.sfc \
  -DYAZE_TEST_ROM_EXPANDED_PATH=~/roms/oos168.sfc
cmake --build build
ctest --test-dir build
```

## CI/CD Configuration

### PR/Push Workflow (Fast Feedback)
- Runs on: push to master/develop, pull requests
- Test suites: `stable` only
- Approximate runtime: 5-10 minutes
- Purpose: Quick regression detection

### Nightly Workflow (Comprehensive Coverage)
- Runs on: Schedule (3 AM UTC daily)
- Test suites: All (stable, rom_dependent, experimental, benchmark)
- Approximate runtime: 30-45 minutes
- Purpose: Deep validation, performance tracking

## Graceful Test Skipping

Tests automatically skip when prerequisites are missing:

### AI Tests
- Check for `GEMINI_API_KEY` environment variable
- Skip with `GTEST_SKIP()` if not present
- Example:
```cpp
void SetUp() override {
  const char* api_key = std::getenv("GEMINI_API_KEY");
  if (!api_key || std::string(api_key).empty()) {
    GTEST_SKIP() << "GEMINI_API_KEY not set. Skipping multimodal tests.";
  }
}
```

### ROM-Dependent Tests
- Check for ROM file at configured path
- Skip if file doesn't exist
- Controlled by `YAZE_ENABLE_ROM_TESTS` CMake option

## Backward Compatibility

The changes maintain backward compatibility:
- Existing developer workflows continue to work
- Default `cmake --build build --target yaze_test` still builds core tests
- Optional suites only built when explicitly enabled
- CI presets unchanged for existing workflows

## Preset Configurations

### Development Presets (Optional Tests OFF)
- `mac-dbg`, `lin-dbg`, `win-dbg`: Debug builds, core tests only
- `mac-rel`, `lin-rel`, `win-rel`: Release builds, core tests only

### AI Development Presets (AI Tests ON, Others OFF)
- `mac-ai`, `lin-ai`, `win-ai`: AI runtime enabled
- Includes `YAZE_ENABLE_AI_RUNTIME=ON`
- For AI/agent development and testing

### CI Presets
- `ci-linux`, `ci-macos`, `ci-windows`: Minimal CI builds
- `ci-windows-ai`: Windows with AI runtime for agent testing

## Migration Guide for Developers

### If you were running all tests:
Before: `./build/bin/yaze_test`
After: Same command still works, but only runs stable tests by default

### To run the same comprehensive suite as before:
```bash
cmake --preset your-preset \
  -DYAZE_ENABLE_ROM_TESTS=ON \
  -DYAZE_ENABLE_BENCHMARK_TESTS=ON
./build/bin/yaze_test
```

### For AI developers:
Use the AI-specific presets: `mac-ai`, `lin-ai`, or `win-ai`

## Troubleshooting

### Tests Not Found
If expected tests are missing, check:
1. CMake option is enabled (e.g., `-DYAZE_ENABLE_ROM_TESTS=ON`)
2. Dependencies are available (ROM file, API keys)
3. Correct test label used with ctest (e.g., `-L rom_dependent`)

### AI Tests Failing
Ensure:
- `GEMINI_API_KEY` is set in environment
- Ollama is running (for Ollama tests): `ollama serve`
- Network connectivity is available

### ROM Tests Failing
Verify:
- ROM file exists at `YAZE_TEST_ROM_VANILLA` (or `YAZE_TEST_ROM_EXPANDED` for v3 tests)
- ROM is valid Zelda3 US version
- Path is absolute, not relative
