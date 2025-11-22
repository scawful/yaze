# E5 - Debugging and Testing Guide

**Last Updated**: October 9, 2025
**Status**: Active

This document provides a comprehensive guide to debugging and testing the `yaze` application. It covers strategies for developers and provides the necessary information for AI agents to interact with, test, and validate the application.

---

## 1. Standardized Logging for Print Debugging

For all print-based debugging, `yaze` uses a structured logging system defined in `util/log.h`. This is the **only** approved method for logging; direct use of `printf` or `std::cout` should be avoided and replaced with the appropriate `LOG_*` macro.

### Log Levels and Usage

-   `LOG_DEBUG(category, "message", ...)`: For verbose, development-only information.
-   `LOG_INFO(category, "message", ...)`: For general, informational messages.
-   `LOG_WARN(category, "message", ...)`: For potential issues that don't break functionality.
-   `LOG_ERROR(category, "message", ...)`: For errors that cause a specific operation to fail.

### Log Categories

Categories allow you to filter logs to focus on a specific subsystem. Common categories include:
- `"Main"`
- `"TestManager"`
- `"EditorManager"`
- `"APU"`, `"CPU"`, `"SNES"` (for the emulator)

### Enabling and Configuring Logs via CLI

You can control logging behavior using command-line flags when launching `yaze` or `yaze_test`.

-   **Enable Verbose Debug Logging**:
    ```bash
    ./build/bin/yaze --debug
    ```

-   **Log to a File**:
    ```bash
    ./build/bin/yaze --log_file=yaze_debug.log
    ```

-   **Filter by Category**:
    ```bash
    # Only show logs from the APU and CPU emulator components
    ./build/bin/yaze_emu --emu_debug_apu=true --emu_debug_cpu=true
    ```

**Best Practice**: When debugging a specific component, add detailed `LOG_DEBUG` statements with a unique category. Then, run `yaze` with the appropriate flags to isolate the output.

---

## 2. Command-Line Workflows for Testing

The `yaze` ecosystem provides several executables and flags to streamline testing and debugging.

### Launching the GUI for Specific Tasks

-   **Load a ROM on Startup**: To immediately test a specific ROM, use the `--rom_file` flag. This bypasses the welcome screen.
    ```bash
    ./build/bin/yaze --rom_file /path/to/your/zelda3.sfc
    ```

-   **Enable the GUI Test Harness**: To allow the `z3ed` CLI to automate the GUI, you must start `yaze` with the gRPC server enabled.
    ```bash
    ./build/bin/yaze --rom_file zelda3.sfc --enable_test_harness
    ```

-   **Open a Specific Editor and Cards**: To quickly test a specific editor and its components, use the `--editor` and `--cards` flags. This is especially useful for debugging complex UIs like the Dungeon Editor.
    ```bash
    # Open the Dungeon Editor with the Room Matrix and two specific room cards
    ./build/bin/yaze --rom_file zelda3.sfc --editor=Dungeon --cards="Room Matrix,Room 0,Room 105"
    
    # Available editors: Assembly, Dungeon, Graphics, Music, Overworld, Palette, 
    #                    Screen, Sprite, Message, Hex, Agent, Settings
    
    # Dungeon editor cards: Rooms List, Room Matrix, Entrances List, Room Graphics,
    #                       Object Editor, Palette Editor, Room N (where N is room ID)
    ```
    
    **Quick Examples**:
    ```bash
    # Fast dungeon room testing
    ./build/bin/yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0"
    
    # Compare multiple rooms side-by-side
    ./build/bin/yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0,Room 1,Room 105"
    
    # Full dungeon workspace with all tools
    ./build/bin/yaze --rom_file=zelda3.sfc --editor=Dungeon \
      --cards="Rooms List,Room Matrix,Object Editor,Palette Editor"
    
    # Jump straight to overworld editing
    ./build/bin/yaze --rom_file=zelda3.sfc --editor=Overworld
    ```
    
    For a complete reference, see [docs/debugging-startup-flags.md](debugging-startup-flags.md).

### Running Automated C++ Tests

The `yaze_test` executable is used to run the project's suite of unit, integration, and E2E tests.

-   **Run All Tests**:
    ```bash
    ./build_ai/bin/yaze_test
    ```

-   **Run Specific Categories**:
    ```bash
    # Run only fast, dependency-free unit tests
    ./build_ai/bin/yaze_test --unit

    # Run tests that require a ROM file
    ./build_ai/bin/yaze_test --rom-dependent --rom-path /path/to/zelda3.sfc
    ```

-   **Run GUI-based E2E Tests**:
    ```bash
    # Run E2E tests and watch the GUI interactions
    ./build_ai/bin/yaze_test --e2e --show-gui
    ```

### Inspecting ROMs with `z3ed`

The `z3ed` CLI is a powerful tool for inspecting ROM data without launching the full GUI. This is ideal for quick checks and scripting.

-   **Get ROM Information**:
    ```bash
    z3ed rom info --rom zelda3.sfc
    ```

-   **Inspect Dungeon Sprites**:
    ```bash
    z3ed dungeon list-sprites --rom zelda3.sfc --dungeon 2
    ```

---

## 3. GUI Automation for AI Agents

The primary way for an AI agent to test its changes and interact with `yaze` is through the GUI automation framework. This system consists of the `yaze` gRPC server (Test Harness) and the `z3ed` CLI client.

### Architecture Overview

1.  **`yaze` (Server)**: When launched with `--enable_test_harness`, it starts a gRPC server that exposes the UI for automation.
2.  **`z3ed` (Client)**: The `z3ed agent test` commands connect to the gRPC server to send commands and receive information.
3.  **AI Agent**: The agent generates `z3ed` commands to drive the UI and verify its actions.

### Step-by-Step Workflow for AI

#### Step 1: Launch `yaze` with the Test Harness

The AI must first ensure the `yaze` GUI is running and ready for automation.

```bash
./build/bin/yaze --rom_file zelda3.sfc --enable_test_harness --test_harness_port 50051
```

#### Step 2: Discover UI Elements

Before interacting with the UI, the agent needs to know the stable IDs of the widgets.

```bash
# Discover all widgets in the Dungeon editor window
z3ed agent test discover --window "Dungeon" --grpc localhost:50051
```
This will return a list of widget IDs (e.g., `Dungeon/Canvas/Map`) that can be used in scripts.

**Tip**: You can also launch `yaze` with the `--editor` flag to automatically open a specific editor:
```bash
./build/bin/yaze --rom_file zelda3.sfc --enable_test_harness --editor=Dungeon --cards="Room 0"
```

#### Step 3: Record or Write a Test Script

An agent can either generate a test script from scratch or use a pre-recorded one.

-   **Recording a human interaction**:
    ```bash
    z3ed agent test record --suite my_test.jsonl
    ```
-   **A generated script might look like this**:
    ```json
    // my_test.jsonl
    {"action": "click", "target": "Dungeon/Toolbar/Open Room"}
    {"action": "wait", "duration_ms": 500}
    {"action": "type", "target": "Room Selector/Filter", "text": "Room 105"}
    {"action": "click", "target": "Room Selector/List/Room 105"}
    {"action": "assert_visible", "target": "Room Card 105"}
    ```
    
-   **Or use startup flags to prepare the environment**:
    ```bash
    # Start yaze with the room already open
    ./build/bin/yaze --rom_file zelda3.sfc --enable_test_harness \
      --editor=Dungeon --cards="Room 105"
    
    # Then your test script just needs to validate the state
    {"action": "assert_visible", "target": "Room Card 105"}
    {"action": "assert_visible", "target": "Dungeon/Canvas"}
    ```

#### Step 4: Replay the Test and Verify

The agent executes the script to perform the actions and validate the outcome.

```bash
z3ed agent test replay my_test.jsonl --watch
```
The `--watch` flag streams results back to the CLI in real-time. The agent can parse this output to confirm its actions were successful.

---

## 4. Advanced Debugging Tools

For more complex issues, especially within the emulator, `yaze` provides several advanced debugging windows. These are covered in detail in the [Emulator Development Guide](emulator-development-guide.md).

-   **Disassembly Viewer**: A live, interactive view of the 65816 and SPC700 CPU execution.
-   **Breakpoint Manager**: Set breakpoints on code execution, memory reads, or memory writes.
-   **Memory Viewer**: Inspect WRAM, SRAM, VRAM, and ROM.
-   **APU Inspector**: A dedicated debugger for the audio subsystem.
-   **Event Viewer**: A timeline of all hardware events (NMI, IRQ, DMA).

These tools are accessible from the **Debug** menu in the main application.
