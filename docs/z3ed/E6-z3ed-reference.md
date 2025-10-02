# z3ed CLI Technical Reference

**Version**: 0.1.0-alpha  
**Last Updated**: October 2, 2025  
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
│  ├─ overworld get-tile/set-tile                         │
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
│  └─ Screenshot (capture) [Stub]                         │
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
   ./yaze --enable_test_harness --test_harness_port=50052 --rom_file=zelda3.sfc &
   ```
2. z3ed built with gRPC support:
   ```bash
   cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
   cmake --build build-grpc-test --target z3ed
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

#### `overworld set-tile` - Set tile at coordinates
```bash
z3ed overworld set-tile --map <id> --x <x> --y <y> --tile-id <id>

Example:
  z3ed overworld set-tile --map=0 --x=100 --y=50 --tile-id=0x1234
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

2. **Implement Handler** (`src/app/core/imgui_test_harness_service.cc`)
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

#### Build Errors - Boolean Flag

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
