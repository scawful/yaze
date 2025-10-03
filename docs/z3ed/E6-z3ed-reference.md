# z3ed CLI Technical Reference

**Version**: 0.1.0-alpha  
**Last Updated**: [Current Date]  
**Status**: Production Ready (macOS), Windows Testing Pending

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Command Reference](#command-reference)
3. [Implementation Guide](#implementation-guide)
4. [Testing & Validation](#testing--validation)
5. [Development Workflows](#development-workflows)
6. [Troubleshooting](#troubleshooting)
7. [API Reference](#api-reference)
8. [Platform Notes](#platform-notes)

---

## Architecture Overview

### System Components

```
┌─────────────────────────────────────────────────────────┐
│ AI Agent Layer (LLM)                                     │
│  └─ Natural language prompts                            │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ z3ed CLI (Command-Line Interface)                       │
│  ├─ agent run --prompt "..." --sandbox                  │
│  ├─ agent test --prompt "..." (IT-02)                   │
│  ├─ agent list                                          │
│  ├─ agent diff --proposal-id <id>                       │
│  ├─ agent describe [--resource <name>]                  │
│  ├─ rom info/validate/diff/generate-golden             │
│  ├─ palette export/import/list                          │
│  ├─ overworld get-tile/find-tile/set-tile               │
│  └─ dungeon list-rooms/add-object                       │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ Service Layer (Singleton Services)                      │
│  ├─ ProposalRegistry (proposal tracking)                │
│  ├─ RomSandboxManager (isolated ROM copies)             │
│  ├─ ResourceCatalog (machine-readable API specs)        │
│  ├─ GuiAutomationClient (gRPC wrapper)                  │
│  ├─ TestWorkflowGenerator (NL → test steps)             │
│  └─ PolicyEvaluator (YAML constraints) [Planned]        │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ ImGuiTestHarness (gRPC Server)                          │
│  ├─ Ping (health check)                                 │
│  ├─ Click (button, menu, tab)                           │
│  ├─ Type (text input)                                   │
│  ├─ Wait (condition polling)                            │
│  ├─ Assert (state validation)                           │
│  ├─ Screenshot (capture) [Stub → IT-08]                 │
│  ├─ GetTestStatus (query test execution) [IT-05]        │
│  ├─ ListTests (enumerate tests) [IT-05]                 │
│  ├─ GetTestResults (detailed results) [IT-05]           │
│  ├─ DiscoverWidgets (widget enumeration) [IT-06]        │
│  ├─ StartRecording (test recording) [IT-07]             │
│  ├─ StopRecording (finish recording) [IT-07]            │
│  └─ ReplayTest (execute test script) [IT-07]            │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ YAZE GUI (ImGui Application)                            │
│  ├─ ProposalDrawer (Debug → Agent Proposals)            │
│  │   ├─ List/detail views                               │
│  │   ├─ Accept/Reject/Delete                            │
│  │   └─ ROM merging                                     │
│  └─ Editor Windows                                      │
│      ├─ Overworld Editor                                │
│      ├─ Dungeon Editor                                  │
│      ├─ Palette Editor                                  │
│      └─ Graphics Editor                                 │
└─────────────────────────────────────────────────────────┘
```

### Data Flow: Proposal Lifecycle

```
User: z3ed agent run "Make soldiers red" --sandbox
         │
         ▼
┌────────────────────────┐
│ MockAIService          │ → ["palette export sprites_aux1 4 soldier.col"]
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ RomSandboxManager      │ → Creates: /tmp/.../sandboxes/20251002T100000/zelda3.sfc
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ Execute Commands       │ → Runs: palette export on sandbox ROM
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ ProposalRegistry       │ → Creates: proposal-20251002T100000/
│                        │   • execution.log
│                        │   • diff.txt (if generated)
└────────┬───────────────┘
         │
         ▼ (User opens YAZE GUI)
┌────────────────────────┐
│ ProposalDrawer         │ → Displays: List of proposals
└────────┬───────────────┘
         │
         ▼ (User clicks "Accept")
┌────────────────────────┐
│ AcceptProposal()       │ → 1. Load sandbox ROM
│                        │   2. rom_->WriteVector(0, sandbox_rom.vector())
│                        │   3. ROM marked dirty
│                        │   4. User saves ROM
└────────────────────────┘
```

---

## Command Reference

### Agent Commands

#### `agent run` - Execute AI-driven ROM modifications
```bash
z3ed agent run --prompt "<description>" --rom <file> [--sandbox]

Options:
  --prompt <text>    Natural language description of desired changes
  --rom <file>       Path to ROM file (default: current ROM)
  --sandbox          Create isolated copy for testing (recommended)

Example:
  z3ed agent run --prompt "Change soldier armor to red" \
    --rom=zelda3.sfc --sandbox
```

**Output**:
- Proposal ID
- Sandbox path
- Command execution log
- Next steps guidance

#### `agent list` - Show all proposals
```bash
z3ed agent list

Example Output:
=== Agent Proposals ===

ID: proposal-20251002T100000-1
  Status: Pending
  Created: 2025-10-02 10:00:00
  Prompt: Change soldier armor to red
  Commands: 3
  Bytes Changed: 128

Total: 1 proposal(s)
```

#### `agent diff` - Show proposal changes
```bash
z3ed agent diff [--proposal-id <id>]

Options:
  --proposal-id <id>  View specific proposal (default: latest pending)

Example:
  z3ed agent diff --proposal-id proposal-20251002T100000-1
```

**Output**:
- Proposal metadata
- Execution log
- Diff content
- Next steps

#### `agent describe` - Export machine-readable API specs
```bash
z3ed agent describe [--format <yaml|json>] [--resource <name>] [--output <file>]

Options:
  --format <type>     Output format: yaml or json (default: yaml)
  --resource <name>   Filter to specific resource (rom, palette, etc.)
  --output <file>     Write to file instead of stdout

Examples:
  z3ed agent describe --format yaml
  z3ed agent describe --format json --resource rom
  z3ed agent describe --output docs/api/z3ed-resources.yaml
```

**Resources Available**:
- `rom` - ROM file operations
- `patch` - Patch application
- `palette` - Palette manipulation
- `overworld` - Overworld editing
- `dungeon` - Dungeon editing
- `agent` - Agent commands

#### `agent resource-list` - Enumerate labeled resources for the AI
```bash
z3ed agent resource-list --type <resource> [--format <table|json>]

Options:
  --type <resource>   Required label family (dungeon, overworld, sprite, palette, etc.)
  --format <mode>     Output format, defaults to `table`. Use `json` for LLM tooling.

Examples:
  # Show dungeon labels in a table
  z3ed agent resource-list --type dungeon

  # Emit JSON for the conversation agent to consume
  z3ed agent resource-list --type overworld --format json
```

**Notes**:
- When the conversation agent invokes this tool, JSON output is requested automatically.
- Labels are loaded from `ResourceContextBuilder`, so the command reflects project-specific metadata.

#### `agent dungeon-list-sprites` - Inspect sprites in a dungeon room
```bash
z3ed agent dungeon-list-sprites --room <hex_id> [--format <table|json>]

Options:
  --room <hex_id>   Dungeon room ID (hexadecimal). Accepts `0x` prefixes or decimal.
  --format <mode>   Output format, defaults to `table`.

Examples:
  z3ed agent dungeon-list-sprites --room 0x012
  z3ed agent dungeon-list-sprites --room 18 --format json
```

**Output**:
- Table view prints sprite id/x/y in hex+decimal for quick inspection.
- JSON view is tailored for the LLM toolchain and is returned automatically during tool calls.

#### `agent chat` - Interactive terminal chat (TUI prototype)
```bash
z3ed agent chat
```

- Opens an FTXUI-based interface with scrolling history and input box.
- Uses the shared `ConversationalAgentService`, so the same backend powers the GUI widget.
- Useful for manual testing of tool dispatching and new prompting strategies.

#### `agent test` - Automated GUI testing (IT-02)
```bash
z3ed agent test --prompt "<test_description>" [--host <hostname>] [--port <port>]

Options:
  --prompt <text>     Natural language test description
  --host <hostname>   Test harness hostname (default: localhost)
  --port <port>       Test harness port (default: 50052)
  --timeout <seconds> Maximum test duration (default: 30)

Supported Prompt Patterns:
  - "Open <Editor> editor"
  - "Open <Editor> and verify it loads"
  - "Click <Button>"
  - "Type '<text>' in <input>"

Examples:
  z3ed agent test --prompt "Open Overworld editor"
  z3ed agent test --prompt "Open Dungeon editor and verify it loads"
  z3ed agent test --prompt "Click Save button"
  z3ed agent test --prompt "Type 'zelda3.sfc' in filename input"
```

**Prerequisites**:
1. YAZE running with test harness:
   ```bash
   ./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
     --enable_test_harness \
     --test_harness_port=50052 \
     --rom_file=assets/zelda3.sfc &
   ```
2. z3ed built with gRPC support:
   ```bash
   cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
   cmake --build build-grpc-test --target z3ed -j$(sysctl -n hw.ncpu)
   ```

#### `agent gui` - GUI Introspection & Control (IT-05/IT-06)

##### `agent gui discover` - Enumerate available widgets
```bash
z3ed agent gui discover \
  [--host <name>] [--port <port>] \
  [--window <name>] [--path-prefix <path>] \
  [--type <widget_type>] [--include-invisible] [--include-disabled] \
  [--format <table|json>] [--limit <n>]

Options:
  --host <name>          Harness host (default: localhost)
  --port <port>          Harness port (default: 50052)
  --window <name>        Filter by window name (case-insensitive substring)
  --path-prefix <path>   Require widget path to start with prefix
  --type <type>          Filter widget type: button, input, menu, tab,
                         checkbox, slider, canvas, selectable, other
  --include-invisible    Include widgets whose parent window is hidden
  --include-disabled     Include widgets flagged as disabled
  --format <mode>        Output as `table` (default) or `json`
  --limit <n>            Maximum widgets to display (useful for large UIs)

Each discovered widget now reports:
- Current visibility/enablement and bounding box (when available)
- Last observed frame number and UTC timestamp
- A `stale` flag when the widget hasn't appeared in the current frame
- Its underlying ImGui ID for low-level automation

Examples:
  # Discover all widgets currently registered
  z3ed agent gui discover

  # Focus on buttons inside the Overworld editor window
  z3ed agent gui discover --window "Overworld" --type button

  # Export a JSON snapshot for an automation agent (showing first 50 widgets)
  z3ed agent gui discover --format json --limit 50 > widgets.json
```

**Table Output Example**:
```
=== Widget Discovery ===
Server: localhost:50052
Window filter: Overworld
Type filter: button
Include invisible: no
Include disabled: no

Window: Overworld (visible)
  • [button] Save
    Path: Overworld/Toolbar/button:Save
    Suggested: Click button:Save
    State: visible, enabled
    Bounds: (24.0, 64.0) → (112.0, 92.0)
  Last seen: frame 18432 @ 2025-01-16 19:42:05
    Widget ID: 0x13fc41a2

Widgets shown: 3 of 18 (truncated)
Snapshot: 2025-01-16 19:42:05

_Widgets that have not appeared in the current frame are marked `[STALE]` in the table output._
```

**Use Cases**:
- AI agents discover available GUI interactions dynamically
- Test scripts validate expected widgets are present
- Documentation generation for GUI features

##### `agent test status` - Query test execution state
```bash
z3ed agent test status --test-id <id> [--follow]

Options:
  --test-id <id>  Test ID from test command output
  --follow        Continuously poll until test completes (blocking)

Example:
  z3ed agent test status --test-id grpc_click_12345678 --follow
```

**Output**:
```yaml
test_id: grpc_click_12345678
status: PASSED
execution_time_ms: 1234
started_at: 2025-10-02T14:23:45Z
completed_at: 2025-10-02T14:23:46Z
assertions_passed: 3
assertions_failed: 0
```

**Usage Example**:
```bash
$ z3ed agent test status --test-id grpc_wait_20251002T182455 --follow
=== Test Status ===
Test ID: grpc_wait_20251002T182455
Server: localhost:50052
Follow mode: polling every 1000ms

Status: RUNNING
Queued At: 2025-10-02T18:24:55Z
Started At: 2025-10-02T18:24:55Z
Completed At: n/a
Execution Time (ms): 432
Assertion Failures: 0
---
Status: PASSED
Queued At: 2025-10-02T18:24:55Z
Started At: 2025-10-02T18:24:55Z
Completed At: 2025-10-02T18:24:55Z
Execution Time (ms): 612
Assertion Failures: 0
```

##### `agent test results` - Get detailed test results
```bash
z3ed agent test results --test-id <id> [--format <json|yaml>] [--include-logs]

Options:
  --test-id <id>      Test ID to retrieve results for
  --format <format>   Output format (default: yaml)
  --include-logs      Include full execution logs

Example:
  z3ed agent test results --test-id grpc_click_12345678 --include-logs
```

**Usage Example (YAML)**:
```bash
$ z3ed agent test results --test-id grpc_assert_20251002T182500 --include-logs
test_id: grpc_assert_20251002T182500
success: true
name: "grpc assert Overworld"
category: "grpc"
executed_at: 2025-10-02T18:25:00Z
duration_ms: 118
assertions:
  - description: "Overworld window visible"
    passed: true
logs:
  - "[2025-10-02T18:25:00Z] Queued assertion"
  - "[2025-10-02T18:25:00Z] Assertion passed"
metrics:
  execution_frames: 2
```

**Usage Example (JSON)**:
```bash
$ z3ed agent test results --test-id grpc_assert_20251002T182500 --format json
{
  "test_id": "grpc_assert_20251002T182500",
  "success": true,
  "name": "grpc assert Overworld",
  "category": "grpc",
  "executed_at": "2025-10-02T18:25:00Z",
  "duration_ms": 118,
  "assertions": [
    {"description": "Overworld window visible", "passed": true}
  ],
  "logs": [],
  "metrics": {
    "execution_frames": 2
  }
}
```

##### `agent test list` - List all tests
```bash
z3ed agent test list [--category <name>] [--status <filter>]

Options:
  --category <name>  Filter by category: grpc, unit, integration, e2e
  --status <filter>  Filter by status: passed, failed, running, queued

Example:
  z3ed agent test list --category grpc --status failed
```

**Usage Example**:
```bash
$ z3ed agent test list --category grpc --limit 3
=== Harness Test Catalog ===
Server: localhost:50052
Category filter: grpc

Test ID: grpc_click_20251002T182440
  Name: grpc click Open Overworld
  Category: grpc
  Last Run: 2025-10-02T18:24:41Z
  Runs: 5 (5 pass / 0 fail)
  Average Duration (ms): 327

Test ID: grpc_wait_20251002T182455
  Name: grpc wait Overworld visible
  Category: grpc
  Last Run: 2025-10-02T18:24:55Z
  Runs: 5 (5 pass / 0 fail)
  Average Duration (ms): 614

Displayed 2 test(s) (catalog size: 42).
```

#### `agent test record` - Record test sessions (IT-07)

##### `agent test record start` - Begin recording
```bash
z3ed agent test record start --output <file> [--description "..."]

Options:
  --output <file>         Output file for test script (JSON)
  --description <text>    Human-readable test description

Example:
  z3ed agent test record start --output tests/overworld_load.json \
    --description "Test Overworld editor loading"
```

##### `agent test record stop` - Finish recording
```bash
z3ed agent test record stop [--validate]

Options:
  --validate  Run recorded test immediately to verify it works

Example:
  z3ed agent test record stop --validate
```

#### `agent test replay` - Execute recorded tests
```bash
z3ed agent test replay <test_script> [--ci-mode] [--output-dir <dir>]

Options:
  --ci-mode           Exit with code 1 on failure, generate JUnit XML
  --output-dir <dir>  Directory for test results (default: test-results/)

Examples:
  # Run single test
  z3ed agent test replay tests/overworld_load.json

  # Run test suite in CI
  z3ed agent test replay tests/suite.yaml --ci-mode
```

#### `agent test suite` - Manage test suites (IT-09)
```bash
z3ed agent test suite <action> [options]

Actions:
  run <suite>     Run test suite (YAML/JSON)
  create <name>   Create new test suite interactively (writes tests/<name>.yaml)
  validate <file> Validate test suite format

Options for `create`:
  --force         Overwrite the target file without prompting

The create workflow walks you through suite metadata, groups, and tests. If
you pass a bare name (e.g., `smoke`), the suite is written to
`tests/smoke.yaml`; supplying a path like `ci/regression.yaml` preserves the
directory. Use the prompts to add groups, associate JSON replay scripts, tags,
and key=value parameters. The CLI warns if referenced scripts are missing but
still records them so you can author them later.

Examples:
  z3ed agent test suite run tests/smoke.yaml
  z3ed agent test suite validate tests/regression.yaml
  z3ed agent test suite create smoke
```

### ROM Commands

#### `rom info` - Display ROM metadata
```bash
z3ed rom info --rom <file>

Example:
  z3ed rom info --rom=zelda3.sfc
```

#### `rom validate` - Verify ROM integrity
```bash
z3ed rom validate --rom <file>

Example:
  z3ed rom validate --rom=zelda3.sfc
```

#### `rom diff` - Compare two ROMs
```bash
z3ed rom diff --rom1 <file1> --rom2 <file2>

Example:
  z3ed rom diff --rom1=zelda3.sfc --rom2=zelda3_modified.sfc
```

#### `rom generate-golden` - Create reference checksums
```bash
z3ed rom generate-golden --rom <file> --output <json_file>

Example:
  z3ed rom generate-golden --rom=zelda3.sfc --output=golden.json
```

### Palette Commands

#### `palette export` - Export palette to file
```bash
z3ed palette export <group_name> <palette_id> <output_file>

Example:
  z3ed palette export sprites_aux1 4 soldier.col
```

#### `palette import` - Import palette from file
```bash
z3ed palette import <group_name> <palette_id> <input_file>

Example:
  z3ed palette import sprites_aux1 4 soldier_red.col
```

#### `palette list` - Show available palettes
```bash
z3ed palette list [--group <name>]

Example:
  z3ed palette list --group sprites_aux1
```

### Overworld Commands

#### `overworld get-tile` - Get tile at coordinates
```bash
z3ed overworld get-tile --map <id> --x <x> --y <y>

Example:
  z3ed overworld get-tile --map=0 --x=100 --y=50
```

#### `overworld find-tile` - Locate tile instances across maps
```bash
z3ed overworld find-tile --tile <id> [--map <map_id>] [--world light|dark|special] [--format json|text]

Examples:
  # Scan entire overworld for tile 0x02E and emit JSON
  z3ed overworld find-tile --tile 0x02E --format json

  # Limit search to Light World map 0x05
  z3ed overworld find-tile --tile 0x02E --map 0x05
```

#### `overworld set-tile` - Set tile at coordinates
```bash
z3ed overworld set-tile --map <id> --x <x> --y <y> --tile <id>

Example:
  z3ed overworld set-tile --map=0 --x=100 --y=50 --tile=0x1234
```

### Dungeon Commands

#### `dungeon list-rooms` - List all dungeon rooms
```bash
z3ed dungeon list-rooms --dungeon <id>

Example:
  z3ed dungeon list-rooms --dungeon=0
```

#### `dungeon add-object` - Add object to room
```bash
z3ed dungeon add-object --dungeon <id> --room <id> --object <type>

Example:
  z3ed dungeon add-object --dungeon=0 --room=5 --object=chest
```

---

## Implementation Guide

### Building with gRPC Support

#### macOS (Recommended)

```bash
# Install dependencies (via vcpkg or system)
# vcpkg is handled automatically by CMake

# Configure with gRPC enabled
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON

# Build YAZE and z3ed
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)
cmake --build build-grpc-test --target z3ed -j$(sysctl -n hw.ncpu)

# First build takes 15-20 minutes (gRPC compilation)
# Incremental builds: 5-10 seconds
```

#### Windows (Experimental)

```powershell
# Install vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install

# Install dependencies
C:\vcpkg\vcpkg install grpc:x64-windows abseil:x64-windows sdl2:x64-windows

# Configure and build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DYAZE_WITH_GRPC=ON -A x64
cmake --build build --config Release --target yaze
cmake --build build --config Release --target z3ed
```

**Windows Notes**:
- Test harness not yet validated on Windows
- Use static linking to avoid DLL conflicts
- See `docs/02-build-instructions.md` for details

### Starting Test Harness

#### Basic Usage

```bash
# Start YAZE with test harness enabled
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Verify server is running
lsof -i :50052
# Should show yaze process listening
```

#### Configuration Options

```bash
--enable_test_harness       Enable gRPC test harness (default: false)
--test_harness_port=<port>  Port number (default: 50051)
--rom_file=<file>           ROM to load on startup (optional)
```

### Testing RPCs with grpcurl

```bash
# Install grpcurl
brew install grpcurl

# Health check
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"message":"test"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping

# Click button
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Type text
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"target":"input:Search","text":"tile16","clear_first":true}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Type

# Wait for condition
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Assert state
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Main Window"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

---

## Testing & Validation

### Automated E2E Test Script

```bash
# Run comprehensive test suite
./scripts/test_harness_e2e.sh

# Expected: All 6 tests pass
# - Ping (health check)
# - Click (button interaction)
# - Type (text input)
# - Wait (condition polling)
# - Assert (state validation)
# - Screenshot (stub - returns not implemented)
```

### Manual Testing Workflow

#### 1. Create Proposal

```bash
./build/bin/z3ed agent run \
  --rom=assets/zelda3.sfc \
  --prompt "Test proposal" \
  --sandbox
```

#### 2. List Proposals

```bash
./build/bin/z3ed agent list
```

#### 3. View Diff

```bash
./build/bin/z3ed agent diff
```

#### 4. Review in GUI

```bash
# Start YAZE
./build/bin/yaze.app/Contents/MacOS/yaze

# Navigate: Debug → Agent Proposals
# Select proposal → Review → Accept/Reject/Delete
```

### Performance Benchmarks

| Operation | Typical Time | Notes |
|-----------|-------------|-------|
| Ping RPC | < 10ms | Health check overhead |
| Click RPC | 50-200ms | Widget lookup + event |
| Type RPC | 100-300ms | Focus + clear + input |
| Wait RPC | 100-5000ms | Depends on condition |
| Assert RPC | 10-100ms | State query |
| Full Workflow | 1-2s | Click + Wait + Assert |
| Proposal Creation | < 1s | Mock AI service |
| ROM Merge | < 100ms | Memory copy |

---

## Development Workflows

### Adding New Agent Commands

1. **Create Handler** (`src/cli/handlers/<resource>.cc`)
```cpp
absl::Status HandleNewCommand(const CommandOptions& options) {
  // Implementation
  return absl::OkStatus();
}
```

2. **Register Command** (`src/cli/modern_cli.cc`)
```cpp
if (absl::GetFlag(FLAGS_new_resource) == "new-action") {
  return HandleNewCommand(options);
}
```

3. **Add to Resource Catalog** (`src/cli/service/resource_catalog.cc`)
```cpp
catalog.resources.push_back({
  .name = "new_resource",
  .description = "Description",
  .actions = {{
    .name = "new-action",
    .description = "Action description",
    .arguments = {/* ... */},
    .effects = {/* ... */},
    .returns = {/* ... */}
  }}
});
```

4. **Update Documentation**
- Add to `docs/api/z3ed-resources.yaml` (regenerate via `agent describe`)
- Add examples to relevant guides

### Adding New Test Harness RPCs

1. **Update Proto** (`src/app/core/proto/imgui_test_harness.proto`)
```protobuf
service ImGuiTestHarness {
  rpc NewOperation(NewRequest) returns (NewResponse);
}

message NewRequest {
  string parameter = 1;
}

message NewResponse {
  bool success = 1;
  string message = 2;
}
```

2. **Implement Handler** (`src/app/core/service/imgui_test_harness_service.cc`)
```cpp
grpc::Status ImGuiTestHarnessServiceImpl::NewOperation(
    grpc::ServerContext* context,
    const NewRequest* request,
    NewResponse* response) {
  // Implementation
  response->set_success(true);
  response->set_message("Operation completed");
  return grpc::Status::OK;
}
```

3. **Rebuild**
```bash
cmake --build build-grpc-test --target yaze
```

4. **Test**
```bash
grpcurl -plaintext -import-path src/app/core/proto \
  -proto imgui_test_harness.proto \
  -d '{"parameter":"value"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/NewOperation
```

### Adding Test Workflow Patterns

1. **Add Pattern Matcher** (`src/cli/service/test_workflow_generator.cc`)
```cpp
bool MatchesNewPattern(const std::string& prompt, YourParams* params) {
  std::regex pattern(R"(your regex pattern)");
  std::smatch matches;
  if (std::regex_search(prompt, matches, pattern)) {
    // Extract parameters
    return true;
  }
  return false;
}
```

2. **Add Workflow Builder**
```cpp
TestWorkflow BuildNewPatternWorkflow(const YourParams& params) {
  TestWorkflow workflow;
  workflow.description = "Your workflow description";
  
  // Add steps
  workflow.steps.push_back({
    .type = TestStep::kClick,
    .target = "button:Name",
    /* ... */
  });
  
  return workflow;
}
```

3. **Integrate into Generator**
```cpp
absl::StatusOr<TestWorkflow> TestWorkflowGenerator::GenerateFromPrompt(
    const std::string& prompt) {
  YourParams params;
  if (MatchesNewPattern(prompt, &params)) {
    return BuildNewPatternWorkflow(params);
  }
  // ... other patterns
}
```

---

## Troubleshooting

### Common Issues

#### Port Already in Use

**Problem**: `Failed to start gRPC server: Address already in use`

**Solutions**:
```bash
# Find and kill existing instance
lsof -i :50052
kill <PID>

# Or use different port
./yaze --enable_test_harness --test_harness_port=50053
```

#### Connection Refused

**Problem**: `Error connecting to server: Connection refused`

**Solutions**:
1. Verify server is running: `lsof -i :50052`
2. Check firewall settings
3. Ensure correct port number

#### Widget Not Found

**Problem**: `Button 'XYZ' not found`

**Solutions**:
1. Verify widget label is correct (case-sensitive)
2. Check if widget is in active window
3. Wait for window to be visible first
4. Use Assert to check widget exists

##### Widget Not Found or Stale State

**Problem**: A `Wait` or `Assert` RPC fails with a "Widget Not Found" or "Window Not Found" error, even though a preceding `Click` action should have made it appear. This is a common issue in the e2e test script.

**Root Cause**: This is an `ImGui` frame timing issue. The `Click` RPC may return *before* the ImGui frame containing the new window or widget has been rendered. The subsequent `Wait` or `Assert` call then executes on a stale frame, failing to find the element.

**Solution**:
- **In the Test Harness Code**: The RPC handler that performs the action (e.g., `Click`) must call `ctx->Yield()` on the `ImGuiTestContext` *after* performing the click. This pauses the RPC and allows the test engine to render the next frame, ensuring the UI state is up-to-date before the RPC returns and the next test step begins.

#### Crashes in `Wait` or `Assert` RPCs

**Problem**: The application crashes with a `SIGSEGV` (segmentation fault) when running `Wait` or `Assert` RPCs.

**Root Cause**: This is a thread-safety issue. The gRPC server runs its handlers on a separate thread pool from the main thread where the ImGui Test Engine runs. Sharing state (like the test context or condition parameters) directly between these threads without synchronization is unsafe.

**Solution**:
- **In the Test Harness Code**: Implement a thread-safe pattern for these RPCs.
    1.  Define a state structure (e.g., `struct WaitState`) to hold all necessary data for the operation.
    2.  In the RPC handler, allocate this structure as a `std::shared_ptr`.
    3.  Register a dynamic test with the ImGui Test Engine and pass the `shared_ptr` to the test's lambda function.
    4.  The test function can then safely access the state data on the main thread to perform its checks. The `shared_ptr` manages the lifetime of the state across threads.

### Build Errors - Boolean Flag

**Problem**: `std::stringstream >> bool` doesn't parse "true"/"false"

**Solution**: Already fixed in `src/util/flag.h` with template specialization

#### Build Errors - Incomplete Type

**Problem**: `error: delete called on 'grpc::Server' that is incomplete`

**Solution**: Ensure destructor implementation is in `.cc` file, not header

### Debug Mode

Enable verbose logging:

```bash
# In z3ed
export YAZE_LOG_LEVEL=debug
./z3ed agent test --prompt "..."

# In test harness
./yaze --enable_test_harness --log_level=debug
```

### Test Harness Diagnostics

```bash
# Check server status
grpcurl -plaintext 127.0.0.1:50052 list

# Check available services
grpcurl -plaintext 127.0.0.1:50052 list yaze.test.ImGuiTestHarness

# Describe service
grpcurl -plaintext 127.0.0.1:50052 describe yaze.test.ImGuiTestHarness
```

---

## API Reference

### RPC Service Definition

```protobuf
syntax = "proto3";
package yaze.test;

service ImGuiTestHarness {
  rpc Ping(PingRequest) returns (PingResponse);
  rpc Click(ClickRequest) returns (ClickResponse);
  rpc Type(TypeRequest) returns (TypeResponse);
  rpc Wait(WaitRequest) returns (WaitResponse);
  rpc Assert(AssertRequest) returns (AssertResponse);
  rpc Screenshot(ScreenshotRequest) returns (ScreenshotResponse);
}
```

### Request/Response Schemas

#### Ping

**Request**:
```json
{
  "message": "string"
}
```

**Response**:
```json
{
  "message": "string",
  "timestampMs": "int64",
  "yazeVersion": "string"
}
```

#### Click

**Request**:
```json
{
  "target": "button:Name | menu:File→Open | tab:Editor",
  "type": "LEFT | RIGHT | MIDDLE | DOUBLE"
}
```

**Response**:
```json
{
  "success": true,
  "message": "Clicked button 'Name'",
  "executionTimeMs": "int32"
}
```

#### Type

**Request**:
```json
{
  "target": "input:FieldName",
  "text": "text to type",
  "clear_first": true
}
```

**Response**:
```json
{
  "success": true,
  "message": "Typed 'text' into input 'FieldName'"
}
```

#### Wait

**Request**:
```json
{
  "condition": "window_visible:WindowName | element_visible:Label | element_enabled:Label",
  "timeout_ms": 5000,
  "poll_interval_ms": 100
}
```

**Response**:
```json
{
  "success": true,
  "message": "Condition met after X ms",
  "elapsedMs": "int32"
}
```

#### Assert

**Request**:
```json
{
  "condition": "visible:Window | enabled:Button | exists:Element | text_contains:Input:ExpectedText"
}
```

**Response**:
```json
{
  "success": true,
  "message": "Assertion passed",
  "actualValue": "string",
  "expectedValue": "string"
}
```

#### Screenshot

**Request**:
```json
{
  "region": "full | window:Name",
  "format": "PNG | JPEG"
}
```

**Response**:
```json
{
  "success": false,
  "message": "Screenshot not yet implemented",
  "filePath": "",
  "fileSizeBytes": 0
}
```

### Resource Catalog Schema

See `docs/api/z3ed-resources.yaml` for complete machine-readable API specification.

**Example Resource**:
```yaml
resources:
  - name: rom
    description: ROM file operations
    actions:
      - name: info
        description: Display ROM metadata
        arguments:
          - name: rom
            type: string
            required: true
            description: Path to ROM file
        effects:
          - Reads ROM file from disk
          - Parses ROM header
        returns:
          - type: object
            fields:
              - title: string
              - size: integer
              - checksum: string
```

---

## Platform Notes

### macOS (ARM64) - Production Ready ✅

**Status**: Fully tested and operational  
**Build Time**: 15-20 minutes (first build), 5-10 seconds (incremental)  
**Binary Size**: ~74 MB (with gRPC)  
**Known Issues**: None

**Recommended Setup**:
```bash
# Use Homebrew for dependencies
brew install cmake grpcurl

# Build with vcpkg (automatic via CMake)
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)
```

### macOS (Intel) - Should Work ⚠️

**Status**: Not explicitly tested, but should work  
**Expected Issues**: None (same toolchain as ARM64)

### Linux - Should Work ⚠️

**Status**: Not explicitly tested  
**Expected Issues**: None (gRPC has excellent Linux support)

**Setup**:
```bash
# Install dependencies
sudo apt-get install cmake build-essential

# Build (same as macOS)
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(nproc)
```

### Windows - Experimental 🔬

**Status**: Build system ready, test harness not validated  
**Known Limitations**:
- Test harness not yet tested on Windows
- May require static linking to avoid DLL conflicts
- vcpkg setup more complex than macOS

**Recommended Approach**: Use Windows Subsystem for Linux (WSL2) for now

---

## Appendix

### File Structure

```
docs/z3ed/
├── E6-z3ed-cli-design.md             # Architecture & design (source of truth)
├── E6-z3ed-implementation-plan.md    # Implementation tracker & roadmap
├── E6-z3ed-reference.md              # This document (technical reference)
├── README.md                         # Quick overview & links
├── IT-01-QUICKSTART.md               # Test harness quick start
├── AGENT_TEST_QUICKREF.md            # CLI agent test command reference
├── E2E_VALIDATION_GUIDE.md           # Complete validation checklist
├── PROJECT_STATUS_OCT2.md            # Current project status
└── archive/                          # Historical documentation
```

### Related Documentation

- **Build Instructions**: `docs/02-build-instructions.md`
- **API Reference**: `docs/api/z3ed-resources.yaml`
- **Testing Guide**: `docs/A1-testing-guide.md`
- **Contributing**: `docs/B1-contributing.md`

### Version History

- **0.1.0-alpha** (Oct 2, 2025) - Initial release
  - Resource catalogue complete
  - Acceptance workflow operational
  - ImGuiTestHarness (IT-01) complete
  - CLI agent test (IT-02) complete
  - E2E validation 80% complete

### Contributors

- @scawful (Project lead, implementation)
- GitHub Copilot (Development assistance, documentation)

### License

Same as YAZE - see `LICENSE` in repository root.

---

**Document Status**: Living document, updated as features are added  
**Last Review**: October 2, 2025  
**Next Review**: After Windows testing completion
