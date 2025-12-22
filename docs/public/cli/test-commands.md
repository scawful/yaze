# z3ed Test Commands

The test command suite provides machine-readable test discovery and execution for CI/CD and agent automation.

## Available Test Commands

| Command | Description | Requires ROM |
|---------|-------------|--------------|
| `test-list` | List available test suites with labels and requirements | No |
| `test-run` | Run tests with structured output | No |
| `test-status` | Show test configuration status | No |

## test-list

Discover available test suites and their requirements.

```bash
# Human-readable list
z3ed test-list

# Machine-readable JSON for agents
z3ed test-list --format json

# Filter by label
z3ed test-list --label stable
```

### Sample Output (Text)
```
=== Available Test Suites ===

  stable          Core unit and integration tests (fast, reliable)
                  Requirements: None

  gui             GUI smoke tests (ImGui framework validation)
                  Requirements: SDL display or headless

  z3ed            z3ed CLI self-test and smoke tests
                  Requirements: z3ed target built

  headless_gui    GUI tests in headless mode (CI-safe)
                  Requirements: None

  rom_dependent   Tests requiring actual Zelda3 ROM
                  Requirements: YAZE_ENABLE_ROM_TESTS=ON + ROM path
                  ⚠ Requires ROM file

  experimental    AI runtime features and experiments
                  Requirements: YAZE_ENABLE_AI_RUNTIME=ON
                  ⚠ Requires AI runtime

  benchmark       Performance and optimization tests
                  Requirements: None
```

### Sample Output (JSON)
```json
{
  "suites": [
    {
      "label": "stable",
      "description": "Core unit and integration tests (fast, reliable)",
      "requirements": "None",
      "requires_rom": false,
      "requires_ai": false
    },
    {
      "label": "rom_dependent",
      "description": "Tests requiring actual Zelda3 ROM",
      "requirements": "YAZE_ENABLE_ROM_TESTS=ON + ROM path",
      "requires_rom": true,
      "requires_ai": false
    }
  ],
  "total_tests_discovered": 42,
  "build_directory": "build"
}
```

## test-run

Run tests and get structured results.

```bash
# Run stable tests (default)
z3ed test-run

# Run specific label
z3ed test-run --label gui

# Run with preset
z3ed test-run --label stable --preset mac-test

# Verbose output
z3ed test-run --label stable --verbose

# JSON output for CI
z3ed test-run --label stable --format json
```

### Sample Output (JSON)
```json
{
  "build_directory": "build",
  "label": "stable",
  "preset": "default",
  "tests_passed": 42,
  "tests_failed": 0,
  "tests_total": 42,
  "success": true
}
```

### Exit Codes
- `0` - All tests passed
- `1` - One or more tests failed or error occurred

## test-status

Show current test configuration.

```bash
# Human-readable status
z3ed test-status

# JSON for agents
z3ed test-status --format json
```

### Sample Output (Text)
```
╔═══════════════════════════════════════════════════════════════╗
║                    TEST CONFIGURATION                         ║
╠═══════════════════════════════════════════════════════════════╣
║  ROM Path: (not set)                                          ║
║  Skip ROM Tests: NO                                           ║
║  UI Tests Enabled: NO                                         ║
║  Active Preset: mac-test (fast)                               ║
╠═══════════════════════════════════════════════════════════════╣
║  Available Build Directories:                                 ║
║    ✓ build                                                    ║
╠═══════════════════════════════════════════════════════════════╣
║  Available Test Suites:                                       ║
║    ✓ stable                                                   ║
║    ✓ gui                                                      ║
║    ✓ z3ed                                                     ║
║    ✗ rom_dependent   (needs ROM)                              ║
║    ✓ experimental    (needs AI)                               ║
╚═══════════════════════════════════════════════════════════════╝
```

### Sample Output (JSON)
```json
{
  "rom_path": "not set",
  "skip_rom_tests": false,
  "ui_tests_enabled": false,
  "build_directories": ["build"],
  "active_preset": "mac-test (fast)",
  "available_suites": ["stable", "gui", "z3ed", "headless_gui", "experimental", "benchmark"]
}
```

## Environment Variables

The test commands respect these environment variables:

| Variable | Description |
|----------|-------------|
| `YAZE_TEST_ROM_PATH` | Path to Zelda3 ROM for ROM-dependent tests |
| `YAZE_SKIP_ROM_TESTS` | Set to `1` to skip ROM tests |
| `YAZE_ENABLE_UI_TESTS` | Set to `1` to enable UI tests |

## Quick Start

### For Developers
```bash
# Configure fast test build
cmake --preset mac-test

# Build test targets
cmake --build --preset mac-test

# Run stable tests
z3ed test-run --label stable
```

### For CI/Agents
```bash
# Check what's available
z3ed test-list --format json

# Run stable suite and capture results
z3ed test-run --label stable --format json > test-results.json

# Check exit code
if [ $? -eq 0 ]; then echo "All tests passed"; fi
```

### With ROM Tests
```bash
# Set ROM path
export YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Configure with ROM tests enabled
cmake --preset mac-dev -DYAZE_ENABLE_ROM_TESTS=ON

# Run ROM-dependent tests
z3ed test-run --label rom_dependent
```

## Integration with ctest

The test commands wrap `ctest` internally. You can also use ctest directly:

```bash
# Equivalent to z3ed test-run --label stable
ctest --test-dir build -L stable

# Run all tests
ctest --test-dir build --output-on-failure

# Run specific pattern
ctest --test-dir build -R "RomTest"
```

## Test Labels Reference

| Label | Description | CI Stage |
|-------|-------------|----------|
| `stable` | Core unit + integration tests | PR/Push |
| `gui` | GUI smoke tests | PR/Push |
| `z3ed` | CLI self-tests | PR/Push |
| `headless_gui` | CI-safe GUI tests | PR/Push |
| `rom_dependent` | Tests requiring ROM | Nightly |
| `experimental` | AI features | Nightly |
| `benchmark` | Performance tests | Nightly |
