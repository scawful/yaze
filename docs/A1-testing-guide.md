# A1 - Testing Guide

This guide provides a comprehensive overview of the testing framework for the yaze project, including the test organization, execution methods, and the end-to-end GUI automation system.

## 1. Test Organization

The test suite is organized into a clear directory structure that separates tests by their purpose and dependencies. This is the primary way to understand the nature of a test.

```
test/
├── unit/                    # Unit tests for individual components
│   ├── core/                # Core functionality (asar, hex utils)
│   ├── cli/                 # Command-line interface tests
│   ├── emu/                 # Emulator component tests
│   ├── gfx/                 # Graphics system (tiles, palettes)
│   ├── gui/                 # GUI widget tests
│   ├── rom/                 # ROM data structure tests
│   └── zelda3/              # Game-specific logic tests
├── integration/             # Tests for interactions between components
│   ├── ai/                  # AI agent and vision tests
│   ├── editor/              # Editor integration tests
│   └── zelda3/              # Game-specific integration tests (ROM-dependent)
├── e2e/                     # End-to-end user workflow tests (GUI-driven)
│   ├── rom_dependent/       # E2E tests requiring a ROM
│   └── zscustomoverworld/   # ZSCustomOverworld upgrade E2E tests
├── benchmarks/              # Performance benchmarks
├── mocks/                   # Mock objects for isolating tests
└── assets/                  # Test assets (patches, data)
```

## 2. Test Categories

Based on the directory structure, tests fall into the following categories:

### Unit Tests (`unit/`)
- **Purpose**: To test individual classes or functions in isolation.
- **Characteristics**:
    - Fast, self-contained, and reliable.
    - No external dependencies (e.g., ROM files, running GUI).
    - Form the core of the CI/CD validation pipeline.

### Integration Tests (`integration/`)
- **Purpose**: To verify that different components of the application work together correctly.
- **Characteristics**:
    - May require a real ROM file (especially those in `integration/zelda3/`). These are considered "ROM-dependent".
    - Test interactions between modules, such as the `asar` wrapper and the `Rom` class, or AI services with the GUI controller.
    - Slower than unit tests but crucial for catching bugs at module boundaries.

### End-to-End (E2E) Tests (`e2e/`)
- **Purpose**: To simulate a full user workflow from start to finish.
- **Characteristics**:
    - Driven by the **ImGui Test Engine**.
    - Almost always require a running GUI and often a real ROM.
    - The slowest but most comprehensive tests, validating the user experience.
    - Includes smoke tests, canvas interactions, and complex workflows like ZSCustomOverworld upgrades.

### Benchmarks (`benchmarks/`)
- **Purpose**: To measure and track the performance of critical code paths, particularly in the graphics system.
- **Characteristics**:
    - Not focused on correctness but on speed and efficiency.
    - Run manually or in specialized CI jobs to prevent performance regressions.

## 3. Running Tests

### Using the Enhanced Test Runner (`yaze_test`)

The most flexible way to run tests is by using the `yaze_test` executable directly. It provides flags to filter tests by category, which is ideal for development and AI agent workflows.

```bash
# First, build the test executable
cmake --build build_ai --target yaze_test

# Run all tests
./build_ai/bin/yaze_test

# Run only unit tests
./build_ai/bin/yaze_test --unit

# Run only integration tests
./build_ai/bin/yaze_test --integration

# Run E2E tests (requires a GUI)
./build_ai/bin/yaze_test --e2e --show-gui

# Run ROM-dependent tests with a specific ROM
./build_ai/bin/yaze_test --rom-dependent --rom-path /path/to/zelda3.sfc

# Run tests matching a specific pattern (e.g., all Asar tests)
./build_ai/bin/yaze_test "*Asar*"

# Get a full list of options
./build_ai/bin/yaze_test --help
```

### Using CTest and CMake Presets

For CI/CD or a more traditional workflow, you can use `ctest` with CMake presets.

```bash
# Configure a development build (enables ROM-dependent tests)
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=/path/to/your/zelda3.sfc

# Build the tests
cmake --build --preset mac-dev --target yaze_test

# Run stable tests (fast, primarily unit tests)
ctest --preset dev

# Run all tests, including ROM-dependent and E2E
ctest --preset all
```

## 4. Writing Tests

When adding new tests, place them in the appropriate directory based on their purpose and dependencies.

- **New class `MyClass`?** Add `test/unit/my_class_test.cc`.
- **Testing `MyClass` with a real ROM?** Add `test/integration/my_class_rom_test.cc`.
- **Testing a full UI workflow involving `MyClass`?** Add `test/e2e/my_class_workflow_test.cc`.

## 5. E2E GUI Testing Framework

The E2E framework uses `ImGuiTestEngine` to automate UI interactions.

### Architecture

- **`test/yaze_test.cc`**: The main test runner that can initialize a GUI for E2E tests.
- **`test/e2e/`**: Contains all E2E test files, such as:
    - `framework_smoke_test.cc`: Basic infrastructure verification.
    - `canvas_selection_test.cc`: Canvas interaction tests.
    - `dungeon_editor_tests.cc`: UI tests for the dungeon editor.
- **`test/test_utils.h`**: Provides high-level helper functions for common actions like loading a ROM (`LoadRomInTest`) or opening an editor (`OpenEditorInTest`).

### Running GUI Tests

To run E2E tests and see the GUI interactions, use the `--show-gui` flag.

```bash
# Run all E2E tests with the GUI visible
./build_ai/bin/yaze_test --e2e --show-gui

# Run a specific E2E test by name
./build_ai/bin/yaze_test --show-gui --gtest_filter="*DungeonEditorSmokeTest"
```

### Widget Discovery and AI Integration

The GUI testing framework is designed for AI agent automation. All major UI elements are registered with stable IDs, allowing an agent to "discover" and interact with them programmatically via the `z3ed` CLI.

Refer to the `z3ed` agent guide for details on using commands like `z3ed gui discover`, `z3ed gui click`, and `z3ed agent test replay`.