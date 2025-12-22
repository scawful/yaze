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
cmake --build build --target yaze_test

# Run all tests
./build/bin/yaze_test

# Run by category
./build/bin/yaze_test --unit
./build/bin/yaze_test --integration
./build/bin/yaze_test --e2e --show-gui

# Run ROM-dependent tests
./build/bin/yaze_test --rom-dependent --rom-path /path/to/zelda3.sfc

# Run by pattern
./build/bin/yaze_test "*Asar*"

# Show all options
./build/bin/yaze_test --help
```

### Using CTest

```bash
# Configure with ROM tests
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc

# Build
cmake --build --preset mac-dev --target yaze_test

# Run tests
ctest --preset dev          # Stable tests
ctest --preset all          # All tests
```

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
./build/bin/yaze_test --e2e --show-gui

# Run specific test
./build/bin/yaze_test --show-gui --gtest_filter="*DungeonEditorSmokeTest"
```

### AI Integration

UI elements are registered with stable IDs for programmatic access via `z3ed`:
- `z3ed gui discover` - List available widgets
- `z3ed gui click` - Interact with widgets
- `z3ed agent test replay` - Replay recorded tests

See the [z3ed CLI Guide](../usage/z3ed-cli.md) for more details.
