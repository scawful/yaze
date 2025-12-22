# yaze Test Suite

This directory contains the comprehensive test suite for YAZE, organized into **default (stable) tests** and **optional test suites**. Tests are managed through CMake presets and run via `ctest` labels.

## Testing Strategy: Tiered Approach

YAZE uses a **tiered testing strategy** to balance CI speed with comprehensive coverage:

1. **PR/Push CI (Fast Feedback)** - Runs stable tests + GUI smoke tests (~5 minutes)
   - Label: `stable` (unit + integration tests)
   - Label: `gui` (framework validation, headless mode)
   - Must pass before merging

2. **Nightly CI (Comprehensive)** - Full suite including heavy/flaky tests (~30-60 minutes)
   - All of the above PLUS
   - Label: `rom_dependent` (requires Zelda3 ROM)
   - Label: `experimental` (AI features, unstable)
   - Label: `benchmark` (performance tests)
   - Non-blocking but alerts on failure

3. **Local Development** - Mix and match based on your changes
   - Stable tests for quick iteration
   - Add ROM tests when modifying editors
   - Add AI tests when touching agent features

For detailed CI configuration, see the CI/CD section below.

## Quick Start

```bash
# Run default tests (what PR CI runs - ~5 minutes)
ctest --test-dir build -L stable

# Run all available tests (respects your preset configuration)
ctest --test-dir build --output-on-failure

# Run with ROM path for full coverage
cmake --preset mac-dbg -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=~/zelda3.sfc
ctest --test-dir build
```

## Fast Test Builds (Recommended for Development)

For rapid test iteration, use the dedicated fast test presets. These use `RelWithDebInfo` with optimized flags (`-O2 -g1`) which builds **2-3x faster** than Debug while retaining enough debug info for test failures.

### Quick Commands (macOS)

```bash
# Configure with fast test preset
cmake --preset mac-test

# Build tests
cmake --build --preset mac-test

# Run stable tests with fast preset
ctest --preset fast
```

### Platform-Specific Fast Test Presets

| Platform | Configure Preset | Build Preset | Test Preset |
|----------|-----------------|--------------|-------------|
| macOS    | `mac-test`      | `mac-test`   | `fast`      |
| Windows  | `win-test`      | `win-test`   | `fast-win`  |
| Linux    | `lin-test`      | `lin-test`   | `fast-lin`  |

### Example Workflow

```bash
# One-liner: Configure, build, and test (macOS)
cmake --preset mac-test && cmake --build --preset mac-test && ctest --preset fast

# Windows equivalent
cmake --preset win-test && cmake --build --preset win-test && ctest --preset fast-win

# Linux equivalent
cmake --preset lin-test && cmake --build --preset lin-test && ctest --preset fast-lin
```

### z3ed CLI Self-Test

The z3ed CLI includes a built-in self-test for quick verification:

```bash
# Run z3ed self-test diagnostics
./build_test/bin/z3ed --self-test

# The self-test is also included in ctest with label "z3ed"
ctest --test-dir build_test -L z3ed
```

## Test Structure

### Default Test Suite (Always Enabled)

The **default/stable test suite** runs automatically in CI and when you build without special flags. It includes:

- **Unit Tests (Fast)**: Core, ROM, Graphics, Zelda3 functionality tests
- **Integration Tests (Medium)**: Editor, Asar, Dungeon integration
- **GUI Smoke Tests (Experimental)**: Basic framework, dungeon editor, canvas workflows

When built with `YAZE_BUILD_TESTS=ON` (default in debug presets), the stable suite is always available:
```
cmake --preset mac-dbg      # Includes stable tests
cmake --preset lin-dbg      # Includes stable tests
cmake --preset win-dbg      # Includes stable tests
```

### Optional Test Suites

#### 1. ROM-Dependent Tests
Tests that require an actual Zelda3 ROM file. Disabled by default to avoid distribution issues.

**Enable with**:
```bash
cmake --preset mac-dbg -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc
cmake --build --preset mac-dbg --target yaze_test_rom_dependent
```

**Includes**:
- ASAR ROM patching tests (`integration/asar_rom_test.cc`)
- Complete ROM editing workflows (`e2e/rom_dependent/e2e_rom_test.cc`)
- ZSCustomOverworld upgrade validation (`e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc`)

**Run with**:
```bash
ctest --test-dir build -L rom_dependent
```

#### 2. Experimental AI Tests
Tests for AI-powered features and runtime. Requires `YAZE_ENABLE_AI_RUNTIME=ON`.

**Enable with**:
```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze_test_experimental
```

**Includes**:
- AI tile placement tests
- Gemini Vision integration tests
- AI GUI controller workflows

**Run with**:
```bash
ctest --test-dir build -L experimental
```

#### 3. Benchmark Tests
Performance and optimization benchmarks.

**Enable with** (always enabled when tests are built):
```bash
ctest --test-dir build -L benchmark
```

## Directory Structure

```
test/
├── unit/                           # Fast unit tests (no ROM required)
│   ├── core/                      # Core utilities, ASAR, hex
│   ├── rom/                       # ROM loading/saving
│   ├── gfx/                       # Graphics system
│   ├── zelda3/                    # Zelda3 data structures
│   └── gui/                       # GUI components
├── integration/                    # Integration tests (may require ROM)
│   ├── editor/                    # Editor integration
│   ├── ai/                        # AI runtime integration
│   ├── zelda3/                    # Zelda3 system integration
│   ├── asar_integration_test.cc
│   ├── asar_rom_test.cc          # ROM-dependent ASAR tests
│   └── dungeon_editor_test.cc
├── e2e/                            # End-to-end UI tests
│   ├── framework_smoke_test.cc     # Basic framework validation
│   ├── dungeon_editor_smoke_test.cc
│   ├── canvas_selection_test.cc
│   ├── rom_dependent/             # ROM-dependent E2E tests
│   └── zscustomoverworld/         # Version upgrade tests
├── benchmarks/                     # Performance tests
├── mocks/                          # Test helpers and mocks
├── assets/                         # Test data and patches
├── test_utils.cc/h                 # Shared test utilities
└── CMakeLists.txt                  # Test configuration
```

## Test Categories & Labels

Tests are organized by ctest labels for flexible execution. Labels determine which tests run in PR/push CI vs nightly builds:

| Label | Description | PR/Push CI? | Nightly? | Requirements |
|-------|-------------|-----------|----------|--------------|
| `stable` | Core unit and integration tests (fast, reliable) | Yes | Yes | None |
| `gui` | GUI smoke tests (ImGui framework validation) | Yes | Yes | SDL display or headless |
| `z3ed` | z3ed CLI self-test and smoke tests | Yes | Yes | z3ed target built |
| `rom_dependent` | Tests requiring actual Zelda3 ROM | No | Yes | `YAZE_ENABLE_ROM_TESTS=ON` + ROM path |
| `experimental` | AI runtime features and experiments | No | Yes | `YAZE_ENABLE_AI_RUNTIME=ON` |
| `benchmark` | Performance and optimization tests | No | Yes | None |
| `headless_gui` | GUI tests in headless mode (CI-safe) | Yes | Yes | None |

## Running Tests

### Stable Tests Only (Default)
```bash
ctest --preset default -L stable
# or
ctest --test-dir build -L stable
```

### GUI Smoke Tests
```bash
# Run all GUI tests
ctest --preset default -L gui

# Run headlessly (CI mode)
ctest --test-dir build -L headless_gui
```

### ROM-Dependent Tests
```bash
# Must configure with ROM path first
cmake --preset mac-dbg \
  -DYAZE_ENABLE_ROM_TESTS=ON \
  -DYAZE_TEST_ROM_PATH=~/zelda3.sfc

# Build ROM-dependent test suite
cmake --build --preset mac-dbg --target yaze_test_rom_dependent

# Run ROM tests
ctest --test-dir build -L rom_dependent
```

### Experimental AI Tests
```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze_test_experimental
ctest --test-dir build -L experimental
```

### All Available Tests
```bash
ctest --test-dir build
```

## Test Suites by Build Preset

| Preset | Stable | GUI | ROM-Dep | Experimental | Benchmark | Use Case |
|--------|--------|-----|---------|--------------|-----------|----------|
| `mac-test`, `lin-test`, `win-test` | ✓ | ✓ | ✗ | ✗ | ✓ | **Fast iteration** (2-3x faster than debug) |
| `mac-dbg`, `lin-dbg`, `win-dbg` | ✓ | ✓ | ✗ | ✗ | ✓ | Default development builds |
| `mac-rel`, `lin-rel`, `win-rel` | ✗ | ✗ | ✗ | ✗ | ✗ | Release binaries (no tests) |
| `mac-ai`, `lin-ai`, `win-ai` | ✓ | ✓ | ✗ | ✓ | ✓ | AI/agent development with experiments |
| `mac-dev`, `lin-dev`, `win-dev` | ✓ | ✓ | ✓ | ✗ | ✓ | Full development with ROM testing |
| `ci-*` | ✓ | ✓ | ✗ | ✗ | ✓ | GitHub Actions CI builds |

## Environment Variables

These variables control test behavior:

```bash
# Specify ROM for tests (if YAZE_ENABLE_ROM_TESTS=ON)
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Skip ROM tests (useful for CI without ROM)
export YAZE_SKIP_ROM_TESTS=1

# Enable GUI tests (default if display available)
export YAZE_ENABLE_UI_TESTS=1
```

## ROM Auto-Discovery

The test framework automatically discovers ROMs without requiring environment variables. It searches for common ROM filenames in these locations (relative to working directory):

**Search Paths:** `.`, `roms/`, `../roms/`, `../../roms/`

**ROM Filenames:** `zelda3.sfc`, `alttp_vanilla.sfc`, `vanilla.sfc`, `Legend of Zelda, The - A Link to the Past (USA).sfc`

This means you can simply place your ROM in the `roms/` directory and run tests without setting `YAZE_TEST_ROM_PATH`:

```bash
# Just works if you have roms/zelda3.sfc
./build/bin/Debug/yaze_test_stable
```

The environment variable still takes precedence if set.

## Running Tests from Command Line

### Traditional Approach (Single Binary)
```bash
# Build single unified test binary
cmake --build build --target yaze_test

# Run with gtest filters
./build/bin/yaze_test --gtest_filter="*Rom*"
./build/bin/yaze_test --gtest_filter="*GUI*" --show-gui
```

### CMake/CTest Approach (Recommended)
```bash
# Run all tests
ctest --test-dir build

# Run specific label
ctest --test-dir build -L stable

# Run with output
ctest --test-dir build --output-on-failure

# Run tests matching pattern
ctest --test-dir build -R "RomTest"

# Run verbose
ctest --test-dir build --verbose
```

## AI Agent Testing Notes

The test suite is optimized for AI agent automation:

### Key Guidelines for AI Agents

1. **Always use ctest labels** for filtering (more robust than gtest filters)
   - `ctest --test-dir build -L stable` (recommended for most tasks)
   - `ctest --test-dir build -L "stable|gui"` (if GUI validation needed)

2. **Check CMake configuration before assuming optional tests**
   ```bash
   cmake . -DYAZE_ENABLE_ROM_TESTS=ON  # Check if this flag was used
   ```

3. **Use build presets to control test suites automatically**
   - `mac-dbg` / `lin-dbg` / `win-dbg` - Includes stable tests only
   - `mac-ai` / `lin-ai` / `win-ai` - Includes stable + experimental tests
   - `mac-dev` / `lin-dev` / `win-dev` - Includes stable + ROM-dependent tests

4. **Provide ROM path explicitly when needed**
   ```bash
   cmake . -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=/path/to/rom
   ```

5. **Use headless mode for CI-safe GUI tests**
   ```bash
   ctest --test-dir build -L headless_gui  # GUI tests without display
   ```

6. **Build separate test binaries for isolation**
   ```bash
   cmake --build build --target yaze_test_stable       # Stable tests only
   cmake --build build --target yaze_test_rom_dependent  # ROM tests
   cmake --build build --target yaze_test_experimental   # AI tests
   ```

## Understanding Test Organization

### Default Tests (Always Safe to Run)
These tests have no external dependencies and run fast. They're enabled by default in all debug presets.

**Stable Unit Tests**
- ROM loading/saving
- Graphics system (tiles, palettes, compression)
- Zelda3 data structures
- ASAR wrapper functionality
- CLI utilities

**Stable Integration Tests**
- Tile editor workflows
- Dungeon editor integration
- Overworld system integration
- Message system

**GUI Smoke Tests**
- Framework validation
- Canvas selection workflow
- Dungeon editor UI

### Optional Tests (Require Configuration)

**ROM-Dependent Suite**
- Full ROM patching with actual ROM data
- Complete edit workflows
- ZSCustomOverworld version upgrades
- Data integrity validation

Disabled by default because they require a Zelda3 ROM file. Enable only when needed:
```bash
cmake ... -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=/path/to/rom
```

**Experimental AI Suite**
- AI tile placement
- Vision model integration
- AI-powered automation

Requires `-DYAZE_ENABLE_AI_RUNTIME=ON` (included in `*-ai` presets).

## Common Test Workflows

### Local Development
```bash
# Fast iteration - stable tests only
ctest --test-dir build -L stable -j4

# With GUI validation
ctest --test-dir build -L "stable|gui" -j4
```

### Before Committing Code
```bash
# Stable tests must pass
ctest --test-dir build -L stable --output-on-failure

# Add ROM tests if modifying ROM/editor code
cmake . -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=~/zelda3.sfc
ctest --test-dir build -L rom_dependent --output-on-failure
```

### Full Test Coverage (With All Features)
```bash
# AI features + ROM tests
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=~/zelda3.sfc
cmake --build --preset mac-dev --target yaze_test_rom_dependent yaze_test_experimental
ctest --test-dir build --output-on-failure
```

### CI/CD Pipeline
The CI pipeline runs:
1. **Stable tests**: Always, must pass
2. **GUI tests**: Always (headless mode), must pass
3. **ROM tests**: Only on `develop` branch with ROM
4. **Experimental**: Only on feature branches if enabled

See `.github/workflows/ci.yml` for details.

## Troubleshooting

### ROM Tests Not Found
```bash
# Ensure ROM tests are enabled
cmake . -DYAZE_ENABLE_ROM_TESTS=ON -DYAZE_TEST_ROM_PATH=/path/to/rom
cmake --build . --target yaze_test_rom_dependent
```

### GUI Tests Crash
- Ensure SDL display available: `export DISPLAY=:0` on Linux
- Check `assets/zelda3.sfc` exists if no ROM path specified
- Run headlessly: `ctest -L headless_gui`

### Tests Not Discovered
```bash
# Rebuild test targets
cmake --build build --target yaze_test_stable
ctest --test-dir build --verbose
```

### Performance Issues
- Use `-j4` to parallelize: `ctest --test-dir build -j4`
- Skip benchmarks: `ctest --test-dir build -L "^(?!benchmark)"`

## Adding New Tests

1. **Unit test**: Add to `unit/` subdirectory, auto-included in stable suite
2. **Integration test**: Add to `integration/`, auto-included in stable suite
3. **GUI test**: Add to `e2e/`, auto-included in GUI suite
4. **ROM-dependent**: Add to `e2e/rom_dependent/`, requires `YAZE_ENABLE_ROM_TESTS=ON`
5. **Experimental**: Add to `integration/ai/`, requires `YAZE_ENABLE_AI_RUNTIME=ON`

All files are automatically discovered by CMake's `add_executable()` and `gtest_discover_tests()`.

## References

- **CMakePresets.json**: Build configurations and preset definitions
- **test/CMakeLists.txt**: Test suite setup and labels
- **docs/public/build/quick-reference.md**: Quick build command reference
- **docs/internal/ci-and-testing.md**: CI pipeline documentation
