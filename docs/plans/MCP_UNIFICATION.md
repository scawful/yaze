# Yaze MCP Unification Plan

**Status:** Draft
**Goal:** Create a high-quality, unified toolset for AI-assisted debugging in Zelda3 ROM hacking.

## 1. Backend Consolidation (C++)

### A. Move `EmulatorServiceImpl` to `yaze_grpc_support`
-   Relocate `src/cli/service/agent/emulator_service_impl.{cc,h}` to `src/app/service/`.
-   Update `src/app/service/grpc_support.cmake` to include these files.
-   This allows the GUI application to link against the emulator control logic.

### B. Update `YazeGRPCServer`
-   Modify `YazeGRPCServer::Initialize` to accept a `yaze::emu::Emulator*`.
-   In `BuildServer()`, register the `EmulatorServiceImpl`.
-   Result: A single gRPC port (default 50052) handles ROM I/O, GUI automation, AND Emulator control.

## 2. MCP Server Enhancements (Python)

### A. Semantic Translation (Knowledge Graph)
-   Integrate `afs` knowledge base.
-   Add a lookup layer: if a tool returns an address (e.g., `$028000`), the MCP server should look up the label in `symbols.json` and append it: `$02:8000 [Module07_Underworld]`.
-   Allow tools to accept labels directly: `read_memory("Link_X_Coord")` -> `read_memory("$7EF36D")`.

### B. Visualization Support
-   Implement `capture_screenshot` using the `CanvasAutomationService`.
-   Return high-resolution PNGs to the AI agent for visual debugging of sprite positions and menu states.

### C. Watchpoint History Analysis
-   Improve `get_watchpoint_history` to provide a summary of *why* memory changed (e.g., "Address $7EF36D changed from $00 to $01 during `Link_MoveX`").

## 3. Sandboxed Test Suites

### A. Headless Validation
-   Use `z3ed` (the CLI) to run the unified gRPC server in a headless mode.
-   Create a suite of Python scripts that use the MCP tools to verify common scenarios:
    -   Can I set a breakpoint and have it hit?
    -   Can I read Link's health and modify it?
    -   Can I step through a routine and verify the CPU flags change?

### B. Integration with ALTTP ROMs
-   Provide a set of "Golden State" snapshots for vanilla ALTTP.
-   Test routines against these snapshots to ensure the MCP tools provide consistent and accurate data across different game regions.
