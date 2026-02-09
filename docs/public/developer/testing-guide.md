# Testing Guide

This guide covers the testing framework for YAZE, including test organization, execution, and the GUI automation system.

---

## Test Organization

Tests are organized by purpose and dependencies:

```
test/
├── unit/                    # Isolated component tests
│   ├── core/                # Core functionality (asar, hex utils)
│   ├── cli/                 # Command-line interface tests
│   ├── emu/                 # Emulator component tests
│   ├── gfx/                 # Graphics system (tiles, palettes)
│   ├── gui/                 # GUI widget tests
│   ├── rom/                 # ROM data structure tests
│   └── zelda3/              # Game-specific logic tests
├── integration/             # Component interaction tests
│   ├── ai/                  # AI agent tests
│   ├── editor/              # Editor integration tests
│   └── zelda3/              # ROM-dependent integration tests
├── e2e/                     # End-to-end GUI workflow tests
│   ├── rom_dependent/       # E2E tests requiring a ROM
│   └── zscustomoverworld/   # ZSCustomOverworld upgrade tests
├── benchmarks/              # Performance benchmarks
├── mocks/                   # Mock objects
└── assets/                  # Test data and patches
```

---

## Test Categories

| Category | Purpose | Dependencies | Speed |
|----------|---------|--------------|-------|
| **Unit** | Test individual classes/functions | None | Fast |
| **Integration** | Test component interactions | May require ROM | Medium |
| **E2E** | Simulate user workflows | GUI + ROM | Slow |
| **Benchmarks** | Measure performance | None | Variable |

**ROM policy**: GitHub CI does not run ROM-dependent tests. Run them locally with `YAZE_ENABLE_ROM_TESTS=ON` and `YAZE_TEST_ROM_*` paths. Use `YAZE_SKIP_ROM_TESTS=1` to force-skip ROM suites.

### Unit Tests

Fast, isolated tests with no external dependencies. Run in CI on every commit.

### Integration Tests

Test module interactions (e.g., `asar` wrapper with `Rom` class). Some require a ROM file.

### E2E Tests

GUI-driven tests using ImGui Test Engine. Validate complete user workflows.

### Benchmarks

Performance measurement for critical paths. Run manually or in specialized CI jobs.

---

## Running Tests

> See the [Build and Test Quick Reference](../build/quick-reference.md) for full command reference.

### Using yaze_test Executable

```bash
# Build
cmake --build build_ai --target yaze_test

# Run all tests
./scripts/yaze_test

# Run by category
./scripts/yaze_test --unit
./scripts/yaze_test --integration
./scripts/yaze_test --e2e --show-gui

# Run ROM-dependent tests
./scripts/yaze_test --rom-dependent --rom-vanilla /path/to/alttp_vanilla.sfc

# Run by pattern
./scripts/yaze_test "*Asar*"

# Show all options
./scripts/yaze_test --help
```

### Using CTest

```bash
# Fast local loop (preferred)
./scripts/test_fast.sh --quick

# Stable suites (label filtered)
ctest --test-dir build_ai -C Debug -L unit
ctest --test-dir build_ai -C Debug -L integration

# Preset-based runs (see CMakePresets.json testPresets)
ctest --preset mac-ai-unit
ctest --preset mac-ai-integration
ctest --preset mac-ai-quick-unit
ctest --preset mac-ai-quick-integration
```

On Linux (single-config builds), omit `-C Debug`.

ROM-dependent CTest suites are local-only and will be skipped when ROMs are not configured.

---

## Writing Tests

Place tests based on their purpose:

| Test Type | File Location |
|-----------|---------------|
| Unit test for `MyClass` | `test/unit/my_class_test.cc` |
| Integration with ROM | `test/integration/my_class_rom_test.cc` |
| UI workflow test | `test/e2e/my_class_workflow_test.cc` |

---

## E2E GUI Testing

The E2E framework uses ImGui Test Engine for UI automation.

### Key Files

| File | Purpose |
|------|---------|
| `test/yaze_test.cc` | Main test runner with GUI initialization |
| `test/e2e/framework_smoke_test.cc` | Infrastructure verification |
| `test/e2e/canvas_selection_test.cc` | Canvas interaction tests |
| `test/e2e/dungeon_editor_tests.cc` | Dungeon editor UI tests |
| `test/test_utils.h` | Helper functions (LoadRomInTest, OpenEditorInTest) |

### Running GUI Tests

```bash
# Run all E2E tests with visible GUI
./scripts/yaze_test --e2e --show-gui

# Run specific test
./scripts/yaze_test --show-gui --gtest_filter="*DungeonEditorSmokeTest"
```

### AI Integration

UI elements are registered with stable IDs for programmatic access via `z3ed`:
- `z3ed gui discover` - List available widgets
- `z3ed gui click` - Interact with widgets
- `z3ed agent test replay` - Replay recorded tests

See the [z3ed CLI Guide](../usage/z3ed-cli.md) for more details.

---

## Web/WASM Smoke Tests

The web build uses a Playwright smoke test to validate the debug API and basic
terminal/filesystem flows. Run it locally against a served `build-wasm/dist`:

```bash
python3 -m http.server 8000 --directory build-wasm/dist
cd scripts/agents
YAZE_WASM_URL=http://127.0.0.1:8000 npx playwright test playwright-wasm-smoke.spec.js
```
