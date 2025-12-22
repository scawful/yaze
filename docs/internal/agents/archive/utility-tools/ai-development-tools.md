# AI Development Tools - Technical Reference

This document provides technical details on the tools available to AI agents for development assistance and ROM debugging. It covers the tool architecture, API reference, and patterns for extending the system.

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│         z3ed Agent Service                      │
│  ┌──────────────────────────────────────────┐  │
│  │  Conversation Handler                    │  │
│  │  (Prompt Builder + AI Service)           │  │
│  └──────────────────────────────────────────┘  │
│                     │                           │
│         ┌───────────┴───────────┐               │
│         ▼                       ▼               │
│  ┌────────────────────┐  ┌────────────────┐   │
│  │ Tool Dispatcher    │  │ Device Manager │   │
│  └────────────────────┘  └────────────────┘   │
│         │                                       │
│    ┌────┼────┬──────┬──────┬─────┐            │
│    ▼    ▼    ▼      ▼      ▼     ▼            │
│  ┌──────────────────────────────────────────┐ │
│  │          Tool Implementations            │ │
│  │                                          │ │
│  │ • FileSystemTool    • BuildTool          │ │
│  │ • EmulatorTool      • TestRunner         │ │
│  │ • MemoryInspector   • DisassemblyTool    │ │
│  │ • ResourceTool      • SymbolProvider     │ │
│  └──────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

## ToolDispatcher System

The `ToolDispatcher` class in `src/cli/service/agent/tool_dispatcher.h` is the central hub for tool management.

### Core Concept

Tools are extensible modules that perform specific operations. The dispatcher:
1. Receives tool calls from the AI model
2. Validates arguments
3. Executes the tool
4. Returns results to the AI model

### Tool Types

```cpp
enum class ToolCallType {
  // FileSystem Tools
  kFilesystemList,
  kFilesystemRead,
  kFilesystemExists,
  kFilesystemInfo,

  // Build Tools
  kBuildConfigure,
  kBuildCompile,
  kBuildTest,
  kBuildStatus,

  // Test Tools
  kTestRun,
  kTestList,
  kTestCoverage,

  // ROM Operations
  kRomInfo,
  kRomLoadGraphics,
  kRomExportData,

  // Emulator Tools
  kEmulatorConnect,
  kEmulatorReadMemory,
  kEmulatorWriteMemory,
  kEmulatorSetBreakpoint,
  kEmulatorStep,
  kEmulatorRun,
  kEmulatorPause,

  // Disassembly Tools
  kDisassemble,
  kDisassembleRange,
  kTraceExecution,

  // Symbol/Debug Info
  kLookupSymbol,
  kGetStackTrace,
};
```

## Tool Implementations

### 1. FileSystemTool

Read-only filesystem access for agents. Fully documented in `filesystem-tool.md`.

**Tools**:
- `filesystem-list`: List directory contents
- `filesystem-read`: Read text files
- `filesystem-exists`: Check path existence
- `filesystem-info`: Get file metadata

**Example Usage**:
```cpp
ToolDispatcher dispatcher(rom, ai_service);
auto result = dispatcher.DispatchTool({
  .tool_type = ToolCallType::kFilesystemRead,
  .args = {
    {"path", "src/app/gfx/arena.h"},
    {"lines", "50"}
  }
});
```

### 2. BuildTool (Phase 1)

CMake/Ninja integration for build management.

**Tools**:
- `kBuildConfigure`: Run CMake configuration
- `kBuildCompile`: Compile specific targets
- `kBuildTest`: Build test targets
- `kBuildStatus`: Check build status

**API**:
```cpp
struct BuildRequest {
  std::string preset;              // cmake preset (mac-dbg, lin-ai, etc)
  std::string target;              // target to build (yaze, z3ed, etc)
  std::vector<std::string> flags;  // additional cmake/ninja flags
  bool verbose = false;
};

struct BuildResult {
  bool success;
  std::string output;
  std::vector<CompileError> errors;
  std::vector<std::string> warnings;
  int exit_code;
};
```

**Example**:
```cpp
BuildResult result = tool_dispatcher.Build({
  .preset = "mac-dbg",
  .target = "yaze",
  .verbose = true
});

for (const auto& error : result.errors) {
  LOG_ERROR("Build", "{}:{}: {}",
    error.file, error.line, error.message);
}
```

**Implementation Notes**:
- Parses CMake/Ninja output for error extraction
- Detects common error patterns (missing includes, undefined symbols, etc.)
- Maps error positions to source files for FileSystemTool integration
- Supports incremental builds (only rebuild changed targets)

### 3. TestRunner (Phase 1)

CTest integration for test automation.

**Tools**:
- `kTestRun`: Execute specific tests
- `kTestList`: List available tests
- `kTestCoverage`: Analyze coverage

**API**:
```cpp
struct TestRequest {
  std::string preset;              // cmake preset
  std::vector<std::string> filters; // test name patterns
  std::string label;               // ctest label (stable, unit, etc)
  bool verbose = false;
};

struct TestResult {
  bool all_passed;
  int passed_count;
  int failed_count;
  std::vector<TestFailure> failures;
  std::string summary;
};
```

**Example**:
```cpp
TestResult result = tool_dispatcher.RunTests({
  .preset = "mac-dbg",
  .label = "stable",
  .filters = {"OverworldTest*"}
});

for (const auto& failure : result.failures) {
  LOG_ERROR("Test", "{}: {}",
    failure.test_name, failure.error_message);
}
```

**Implementation Notes**:
- Integrates with ctest for test execution
- Parses Google Test output format
- Detects assertion types (EXPECT_EQ, EXPECT_TRUE, etc.)
- Provides failure context (actual vs expected values)
- Supports test filtering by name or label

### 4. MemoryInspector (Phase 2)

Emulator memory access and analysis.

**Tools**:
- `kEmulatorReadMemory`: Read memory regions
- `kEmulatorWriteMemory`: Write memory (for debugging)
- `kEmulatorSetBreakpoint`: Set conditional breakpoints
- `kEmulatorReadWatchpoint`: Monitor memory locations

**API**:
```cpp
struct MemoryReadRequest {
  uint32_t address;          // SNES address (e.g., $7E:0000)
  uint32_t length;           // bytes to read
  bool interpret = false;    // try to decode as data structure
};

struct MemoryReadResult {
  std::vector<uint8_t> data;
  std::string hex_dump;
  std::string interpretation;  // e.g., "Sprite data: entity=3, x=120"
};
```

**Example**:
```cpp
MemoryReadResult result = tool_dispatcher.ReadMemory({
  .address = 0x7E0000,
  .length = 256,
  .interpret = true
});

// Result includes:
// hex_dump: "00 01 02 03 04 05 06 07..."
// interpretation: "WRAM header region"
```

**Implementation Notes**:
- Integrates with emulator's gRPC service
- Detects common data structures (sprite tables, tile data, etc.)
- Supports structured memory reads (tagged as "player RAM", "sprite data")
- Provides memory corruption detection

### 5. DisassemblyTool (Phase 2)

65816 instruction decoding and execution analysis.

**Tools**:
- `kDisassemble`: Disassemble single instruction
- `kDisassembleRange`: Disassemble code region
- `kTraceExecution`: Step through code with trace

**API**:
```cpp
struct DisassemblyRequest {
  uint32_t address;           // ROM/RAM address
  uint32_t length;            // bytes to disassemble
  bool with_trace = false;    // include CPU state at each step
};

struct DisassemblyResult {
  std::vector<Instruction> instructions;
  std::string assembly_text;
  std::vector<CpuState> trace_states;  // if with_trace=true
};

struct Instruction {
  uint32_t address;
  std::string opcode;
  std::string operand;
  std::string mnemonic;
  std::vector<std::string> explanation;
};
```

**Example**:
```cpp
DisassemblyResult result = tool_dispatcher.Disassemble({
  .address = 0x0A8000,
  .length = 32,
  .with_trace = true
});

for (const auto& insn : result.instructions) {
  LOG_INFO("Disasm", "{:06X} {} {}",
    insn.address, insn.mnemonic, insn.operand);
}
```

**Implementation Notes**:
- Uses `Disassembler65816` for instruction decoding
- Explains each instruction's effect in plain English
- Tracks register/flag changes in execution trace
- Detects jump targets and resolves addresses
- Identifies likely subroutine boundaries

### 6. ResourceTool (Phase 2)

ROM resource access and interpretation.

**Tools**:
- Query ROM data structures (sprites, tiles, palettes)
- Cross-reference memory addresses to ROM resources
- Export resource data

**API**:
```cpp
struct ResourceQuery {
  std::string resource_type;  // "sprite", "tile", "palette", etc
  uint32_t resource_id;
  bool with_metadata = true;
};

struct ResourceResult {
  std::string type;
  std::string description;
  std::vector<uint8_t> data;
  std::map<std::string, std::string> metadata;
};
```

**Example**:
```cpp
ResourceResult result = tool_dispatcher.QueryResource({
  .resource_type = "sprite",
  .resource_id = 0x13,
  .with_metadata = true
});

// Returns sprite data, graphics, palette info
```

## Tool Integration Patterns

### Pattern 1: Error-Driven Tool Chaining

When a tool produces an error, chain to informational tools:

```cpp
// 1. Attempt to compile
auto build_result = tool_dispatcher.Build({...});

// 2. If failed, analyze error
if (!build_result.success) {
  for (const auto& error : build_result.errors) {
    // 3. Read the source file at error location
    auto file_result = tool_dispatcher.ReadFile({
      .path = error.file,
      .offset = error.line - 5,
      .lines = 15
    });

    // 4. AI analyzes context and suggests fix
    // "You're missing #include 'app/gfx/arena.h'"
  }
}
```

### Pattern 2: Memory Analysis Workflow

Debug memory corruption by reading and interpreting:

```cpp
// 1. Read suspect memory region
auto mem_result = tool_dispatcher.ReadMemory({
  .address = 0x7E7000,
  .length = 256,
  .interpret = true
});

// 2. Set watchpoint if available
if (needs_monitoring) {
  tool_dispatcher.SetWatchpoint({
    .address = 0x7E7000,
    .on_write = true
  });
}

// 3. Continue execution and capture who writes
// AI analyzes the execution trace to find the culprit
```

### Pattern 3: Instruction-by-Instruction Analysis

Understand complex routines:

```cpp
// 1. Disassemble the routine
auto disasm = tool_dispatcher.Disassemble({
  .address = 0x0A8000,
  .length = 128,
  .with_trace = true
});

// 2. Analyze each instruction
for (const auto& insn : disasm.instructions) {
  // - What registers are affected?
  // - What memory locations accessed?
  // - Is this a jump/call?
}

// 3. Build understanding of routine's purpose
// AI synthesizes into "This routine initializes sprite table"
```

## Adding New Tools

### Step 1: Define Tool Type

Add to `enum class ToolCallType` in `tool_dispatcher.h`:

```cpp
enum class ToolCallType {
  // ... existing ...
  kMyCustomTool,
};
```

### Step 2: Define Tool Interface

Create base class in `tool_dispatcher.h`:

```cpp
class MyCustomTool : public ToolBase {
public:
  std::string GetName() const override {
    return "my-custom-tool";
  }

  std::string GetDescription() const override {
    return "Does something useful";
  }

  absl::StatusOr<ToolResult> Execute(
    const ToolArgs& args) override;

  bool RequiresLabels() const override {
    return false;
  }
};
```

### Step 3: Implement Tool

In `tool_dispatcher.cc`:

```cpp
absl::StatusOr<ToolResult> MyCustomTool::Execute(
    const ToolArgs& args) {

  // Validate arguments
  if (!args.count("required_arg")) {
    return absl::InvalidArgumentError(
      "Missing required_arg parameter");
  }

  std::string required_arg = args.at("required_arg");

  // Perform operation
  auto result = DoSomethingUseful(required_arg);

  // Return structured result
  return ToolResult{
    .success = true,
    .output = result.ToString(),
    .data = result.AsJson()
  };
}
```

### Step 4: Register Tool

In `ToolDispatcher::DispatchTool()`:

```cpp
case ToolCallType::kMyCustomTool: {
  MyCustomTool tool;
  return tool.Execute(args);
}
```

### Step 5: Add to AI Prompt

Update the prompt builder to inform AI about the new tool:

```cpp
// In prompt_builder.cc
tools_description += R"(
- my-custom-tool: Does something useful
  Args: required_arg (string)
  Example: {"tool_name": "my-custom-tool",
            "args": {"required_arg": "value"}}
)";
```

## Error Handling Patterns

### Pattern 1: Graceful Degradation

When a tool fails, provide fallback behavior:

```cpp
// Try to use emulator tool
auto mem_result = tool_dispatcher.ReadMemory({...});

if (!mem_result.ok()) {
  // Fallback: Use ROM data instead
  auto rom_result = tool_dispatcher.QueryResource({...});
  return rom_result;
}
```

### Pattern 2: Error Context

Always include context in errors:

```cpp
if (!file_exists(path)) {
  return absl::NotFoundError(
    absl::StrFormat(
      "File not found: %s (checked in project dir: %s)",
      path, project_root));
}
```

### Pattern 3: Timeout Handling

Long operations should timeout gracefully:

```cpp
// In BuildTool
const auto timeout = std::chrono::minutes(5);
auto result = RunBuildWithTimeout(preset, target, timeout);

if (result.timed_out) {
  return absl::DeadlineExceededError(
    "Build took too long (> 5 minutes). "
    "Try building specific target instead of all.");
}
```

## Tool State Management

### Session State

Tools operate within a session context:

```cpp
struct ToolSession {
  std::string session_id;
  std::string rom_path;
  std::string build_preset;
  std::string workspace_dir;
  std::map<std::string, std::string> environment;
};
```

### Tool Preferences

Users can configure tool behavior:

```cpp
struct ToolPreferences {
  bool filesystem = true;      // Enable filesystem tools
  bool build = true;           // Enable build tools
  bool test = true;            // Enable test tools
  bool emulator = true;        // Enable emulator tools
  bool experimental = false;   // Enable experimental tools

  int timeout_seconds = 300;   // Default timeout
  bool verbose = false;        // Verbose output
};
```

## Performance Considerations

### Caching

Cache expensive operations:

```cpp
// Cache file reads
std::unordered_map<std::string, FileContent> file_cache;

// Cache test results
std::unordered_map<std::string, TestResult> test_cache;
```

### Async Execution

Long operations should be async:

```cpp
// In BuildTool
auto future = std::async(std::launch::async,
  [this] { return RunBuild(); });

auto result = future.get();  // Wait for completion
```

### Resource Limits

Enforce limits on resource usage:

```cpp
// Limit memory reads
constexpr size_t MAX_MEMORY_READ = 64 * 1024;  // 64KB

// Limit disassembly length
constexpr size_t MAX_DISASM_BYTES = 16 * 1024; // 16KB

// Limit files listed
constexpr size_t MAX_FILES_LISTED = 1000;
```

## Debugging Tools

### Tool Logging

Enable verbose logging for tool execution:

```cpp
export Z3ED_TOOL_DEBUG=1
z3ed agent chat --debug --log-file tools.log
```

### Tool Testing

Unit tests for each tool in `test/unit/`:

```cpp
TEST(FileSystemToolTest, ListsDirectoryRecursively) {
  FileSystemTool tool;
  auto result = tool.Execute({
    {"path", "src"},
    {"recursive", "true"}
  });
  EXPECT_TRUE(result.ok());
}
```

### Tool Profiling

Profile tool execution:

```bash
z3ed agent chat --profile-tools
# Output: Tool timings and performance metrics
```

## Security Considerations

### Input Validation

All tool inputs must be validated:

```cpp
// FileSystemTool validates paths against project root
if (!IsPathInProject(path)) {
  return absl::PermissionDeniedError(
    "Path outside project directory");
}

// BuildTool validates preset names
if (!IsValidPreset(preset)) {
  return absl::InvalidArgumentError(
    "Unknown preset: " + preset);
}
```

### Sandboxing

Operations should be sandboxed:

```cpp
// BuildTool uses dedicated build directories
const auto build_dir = workspace / "build_ai";

// FileSystemTool restricts to project directory
// EmulatorTool only connects to local ports
```

### Access Control

Sensitive operations may require approval:

```cpp
// Emulator write operations log for audit
LOG_WARNING("Emulator",
  "Writing to memory at {:06X} (value: {:02X})",
  address, value);

// ROM modifications require confirmation
// Not implemented in agent, but planned for future
```

## Related Documentation

- **FileSystemTool**: `filesystem-tool.md`
- **AI Infrastructure (archived)**: `archive/legacy-2025-11/ai-infrastructure-initiative-archived-2025-11-25.md`
- **Agent Architecture**: `agent-architecture.md`
- **Development Plan**: `../plans/ai-assisted-development-plan.md`
