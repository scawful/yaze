# Emulator Debug API Design for AI Agent Integration

## Executive Summary

This document outlines the design for a comprehensive debugging API that enables AI agents to debug Zelda ROM hacks through the yaze emulator. The API provides execution control, memory inspection, disassembly, and analysis capabilities specifically tailored for 65816 and SPC700 debugging.

## Architecture Overview

```
┌──────────────────┐      ┌─────────────────┐      ┌──────────────────┐
│   AI Agent       │◄────►│  Tool Dispatcher │◄────►│   gRPC Service   │
│  (Claude/GPT)    │      │                 │      │                  │
└──────────────────┘      └─────────────────┘      └──────────────────┘
                                   │                         │
                                   ▼                         ▼
                          ┌─────────────────┐      ┌──────────────────┐
                          │  Tool Handlers  │      │ Emulator Service │
                          │                 │      │   Implementation │
                          └─────────────────┘      └──────────────────┘
                                   │                         │
                                   ▼                         ▼
                          ┌─────────────────────────────────────────┐
                          │         Debug Infrastructure           │
                          ├─────────────────┬───────────────────────┤
                          │ Disassembler    │  Step Controller      │
                          │ Symbol Provider │  Breakpoint Manager   │
                          │ Memory Tracker  │  State Snapshots      │
                          └─────────────────┴───────────────────────┘
```

## Phase 1 (MVP) Features

### 1. Execution Control API

```cpp
// Tool Dispatcher Commands
enum class EmulatorDebugTool {
  // Basic execution
  kDebugRun,           // Run until breakpoint or pause
  kDebugStep,          // Single instruction step
  kDebugStepOver,      // Step over JSR/JSL calls
  kDebugStepOut,       // Step out of current subroutine
  kDebugPause,         // Pause execution
  kDebugReset,         // Reset to power-on state

  // Breakpoint management
  kDebugSetBreak,      // Set execution breakpoint
  kDebugClearBreak,    // Clear breakpoint by ID
  kDebugListBreaks,    // List all breakpoints
  kDebugToggleBreak,   // Enable/disable breakpoint
};
```

#### Example Tool Call Structure:
```cpp
struct DebugStepRequest {
  enum StepType {
    SINGLE,      // One instruction
    OVER,        // Step over calls
    OUT,         // Step out of routine
    TO_ADDRESS   // Run to specific address
  };

  StepType type;
  uint32_t target_address;  // For TO_ADDRESS
  uint32_t max_steps;       // Timeout protection
};

struct DebugStepResponse {
  bool success;
  uint32_t pc;              // New program counter
  uint32_t instruction_count;
  DisassembledInstruction current_instruction;
  std::vector<std::string> call_stack;
  std::string message;
};
```

### 2. Memory Inspection API

```cpp
enum class MemoryDebugTool {
  kMemoryRead,         // Read memory region
  kMemoryWrite,        // Write memory (for patching)
  kMemoryWatch,        // Set memory watchpoint
  kMemoryCompare,      // Compare two memory regions
  kMemorySearch,       // Search for byte pattern
  kMemorySnapshot,     // Save memory state
  kMemoryDiff,         // Diff against snapshot
};
```

#### Memory Region Types:
```cpp
enum class MemoryRegion {
  WRAM,      // Work RAM ($7E0000-$7FFFFF)
  SRAM,      // Save RAM ($700000-$77FFFF)
  ROM,       // ROM banks ($008000-$FFFFFF)
  VRAM,      // Video RAM (PPU)
  OAM,       // Sprite data
  CGRAM,     // Palette data
  APU_RAM,   // Audio RAM ($0000-$FFFF in SPC700 space)
};

struct MemoryReadRequest {
  MemoryRegion region;
  uint32_t address;      // 24-bit SNES address
  uint32_t size;         // Bytes to read
  bool as_hex;           // Format as hex dump
  bool with_symbols;     // Include symbol annotations
};

struct MemoryReadResponse {
  std::vector<uint8_t> data;
  std::string hex_dump;
  std::map<uint32_t, std::string> symbols;  // Address -> symbol name
  std::string interpretation;  // AI-friendly interpretation
};
```

### 3. Disassembly API

```cpp
enum class DisassemblyTool {
  kDisassemble,        // Disassemble at address
  kDisassembleRange,   // Disassemble N instructions
  kDisassembleContext, // Show surrounding code
  kFindInstruction,    // Search for instruction pattern
  kGetCallStack,       // Get current call stack
  kTraceExecution,     // Get execution history
};
```

#### Disassembly Request/Response:
```cpp
struct DisassemblyRequest {
  uint32_t address;
  uint32_t instruction_count;
  uint32_t context_before;    // Instructions before target
  uint32_t context_after;     // Instructions after target
  bool include_symbols;
  bool include_execution_counts;
  bool track_branches;        // Show branch targets
};

struct DisassemblyResponse {
  struct Line {
    uint32_t address;
    std::string hex_bytes;      // "20 34 80"
    std::string mnemonic;       // "JSR"
    std::string operands;       // "$8034"
    std::string symbol;         // "MainGameLoop"
    std::string comment;        // "; Initialize game state"
    bool is_breakpoint;
    bool is_current_pc;
    uint32_t execution_count;
    uint32_t branch_target;     // For jumps/branches
  };

  std::vector<Line> lines;
  std::string formatted_text;   // Human-readable disassembly
  std::vector<std::string> referenced_symbols;
};
```

### 4. Analysis API

```cpp
enum class AnalysisTool {
  kAnalyzeRoutine,     // Analyze subroutine behavior
  kFindReferences,     // Find references to address
  kDetectPattern,      // Detect common bug patterns
  kCompareRom,         // Compare with original ROM
  kProfileExecution,   // Performance profiling
  kTrackDataFlow,      // Track value propagation
};
```

## Tool Dispatcher Integration

### New Tool Definitions

```cpp
// In tool_dispatcher.h
enum class ToolCallType {
  // ... existing tools ...

  // Debugger - Execution Control
  kDebugRun,
  kDebugStep,
  kDebugStepOver,
  kDebugStepOut,
  kDebugRunToAddress,

  // Debugger - Breakpoints
  kDebugSetBreakpoint,
  kDebugSetWatchpoint,
  kDebugClearBreakpoint,
  kDebugListBreakpoints,
  kDebugEnableBreakpoint,

  // Debugger - Memory
  kDebugReadMemory,
  kDebugWriteMemory,
  kDebugSearchMemory,
  kDebugCompareMemory,
  kDebugSnapshotMemory,

  // Debugger - Disassembly
  kDebugDisassemble,
  kDebugGetCallStack,
  kDebugGetExecutionTrace,
  kDebugFindInstruction,

  // Debugger - Analysis
  kDebugAnalyzeRoutine,
  kDebugFindReferences,
  kDebugDetectBugs,
  kDebugProfileCode,
};
```

### Tool Handler Implementation

```cpp
class DebuggerToolHandler {
 public:
  explicit DebuggerToolHandler(EmulatorServiceImpl* service);

  absl::StatusOr<ToolResponse> HandleDebugStep(const nlohmann::json& params);
  absl::StatusOr<ToolResponse> HandleDebugSetBreakpoint(const nlohmann::json& params);
  absl::StatusOr<ToolResponse> HandleDebugDisassemble(const nlohmann::json& params);
  absl::StatusOr<ToolResponse> HandleDebugReadMemory(const nlohmann::json& params);
  absl::StatusOr<ToolResponse> HandleDebugAnalyze(const nlohmann::json& params);

 private:
  EmulatorServiceImpl* emulator_service_;
  Disassembler65816 disassembler_;
  SymbolProvider symbol_provider_;
  MemoryAnalyzer analyzer_;
};
```

## Example AI Agent Interactions

### Example 1: Debugging a Crash

**Agent Query:**
```json
{
  "tool": "debug_analyze",
  "params": {
    "problem": "Game crashes when entering dungeon room 15",
    "context": {
      "room_id": 15,
      "entrance_id": 3
    }
  }
}
```

**System Response:**
```json
{
  "analysis": {
    "crash_location": {
      "address": "0x008A45",
      "instruction": "LDA [$00],Y",
      "symbol": "LoadRoomHeader+0x15"
    },
    "cause": "Null pointer dereference",
    "explanation": "Direct page location $00 contains $0000, causing invalid long pointer access",
    "registers_at_crash": {
      "Y": "0x0010",
      "DP": "0x1E00",
      "$1E00": "0x0000"
    },
    "suggested_fix": "Check room header pointer before dereferencing",
    "call_stack": [
      "Module_Dungeon",
      "Dungeon_LoadRoom",
      "LoadRoomHeader"
    ]
  }
}
```

### Example 2: Step-by-Step Debugging

**Agent Query:**
```json
{
  "tool": "debug_step",
  "params": {
    "type": "over",
    "with_context": true,
    "show_memory": ["$7E0010", "$7E0012"]
  }
}
```

**Response:**
```json
{
  "pc_before": "0x008034",
  "pc_after": "0x008037",
  "instruction_executed": {
    "address": "0x008034",
    "mnemonic": "JSR",
    "operand": "$8520",
    "symbol": "UpdateSprites",
    "cycles": 6
  },
  "context": {
    "before": [
      {"address": "0x008031", "instruction": "LDA $10", "value": "0x07"}
    ],
    "after": [
      {"address": "0x008037", "instruction": "BEQ $8045"}
    ]
  },
  "memory_values": {
    "$7E0010": "0x07",
    "$7E0012": "0x00"
  },
  "call_depth": 2
}
```

### Example 3: Finding Bug Patterns

**Agent Query:**
```json
{
  "tool": "debug_detect_bugs",
  "params": {
    "patterns": ["stack_overflow", "invalid_bank", "dma_collision"],
    "range": {
      "start": "0x008000",
      "end": "0x00FFFF"
    }
  }
}
```

**Response:**
```json
{
  "bugs_found": [
    {
      "type": "potential_stack_overflow",
      "location": "0x009A23",
      "description": "Recursive JSR without stack check",
      "severity": "high",
      "suggestion": "Add stack pointer validation before recursive call"
    },
    {
      "type": "invalid_bank_switch",
      "location": "0x00B456",
      "description": "PHB without corresponding PLB",
      "severity": "medium",
      "suggestion": "Ensure data bank is restored after operation"
    }
  ]
}
```

### Example 4: Memory Watchpoint

**Agent Query:**
```json
{
  "tool": "debug_set_watchpoint",
  "params": {
    "address": "0x7E0020",
    "size": 2,
    "type": "write",
    "condition": "value > 0x00FF",
    "description": "Monitor game state overflow"
  }
}
```

**Response:**
```json
{
  "watchpoint_id": 5,
  "status": "active",
  "message": "Watchpoint set on $7E0020-$7E0021 for writes > 0x00FF"
}
```

## Phase 2 (Full) Features

### Advanced Analysis
- **Control Flow Graphs**: Generate CFG for routines
- **Data Flow Analysis**: Track value propagation through code
- **Symbolic Execution**: Analyze possible execution paths
- **Pattern Matching**: Detect specific code patterns (e.g., DMA setup, HDMA tables)
- **Performance Profiling**: Cycle-accurate performance analysis

### Enhanced Debugging
- **Conditional Breakpoints**: Complex expressions (e.g., "A > 0x10 && X == 0")
- **Trace Recording**: Record full execution traces to file
- **Reverse Debugging**: Step backwards through recorded execution
- **Memory Diffing**: Visual diff between memory states
- **SPC700 Debugging**: Full audio processor debugging support

### AI-Specific Features
- **Semantic Analysis**: Understanding game logic from assembly
- **Bug Pattern Database**: ML-trained bug detection
- **Automated Fix Suggestions**: Propose assembly patches for bugs
- **Test Case Generation**: Generate test scenarios for ROM hacks
- **Documentation Generation**: Auto-document assembly routines

## Implementation Priority

### Phase 1A (Immediate - Week 1-2)
1. Basic step control (single, over, out)
2. Simple breakpoints (address-based)
3. Memory read/write operations
4. Basic disassembly at address

### Phase 1B (Short-term - Week 3-4)
1. Call stack tracking
2. Symbol resolution
3. Memory watchpoints
4. Execution trace (last N instructions)

### Phase 1C (Medium-term - Week 5-6)
1. Pattern-based bug detection
2. Memory snapshots and comparison
3. Advanced breakpoint conditions
4. Performance metrics

### Phase 2 (Long-term - Month 2+)
1. Full analysis suite
2. SPC700 debugging
3. Reverse debugging
4. AI-specific enhancements

## Success Metrics

### Technical Metrics
- Response time < 100ms for step operations
- Support for 100+ simultaneous breakpoints without performance impact
- Accurate disassembly for 100% of valid 65816 opcodes
- Symbol resolution for all loaded ASM files

### User Experience Metrics
- AI agents can identify crash causes in < 5 interactions
- Step debugging provides sufficient context without overwhelming
- Memory inspection clearly shows relevant game state
- Bug detection has < 10% false positive rate

## Integration Points

### With Existing yaze Components
- **Rom Class**: Read-only access to ROM data
- **Emulator Core**: Direct CPU/PPU/APU state access
- **Symbol Files**: Integration with usdasm output
- **Canvas System**: Visual debugging overlays (Phase 2)

### With AI Infrastructure
- **Tool Dispatcher**: Seamless tool call routing
- **Prompt Builder**: Context-aware debugging prompts
- **Agent Memory**: Persistent debugging session state
- **Response Formatter**: Human-readable debug output

## Security Considerations

1. **Read-Only by Default**: Prevent accidental ROM corruption
2. **Sandboxed Execution**: Limit memory access to emulated space
3. **Rate Limiting**: Prevent runaway debugging loops
4. **Audit Logging**: Track all debugging operations
5. **Session Isolation**: Separate debug sessions per agent

## Testing Strategy

### Unit Tests
- Disassembler accuracy for all opcodes
- Step controller call stack tracking
- Breakpoint manager hit detection
- Symbol provider resolution

### Integration Tests
- Full debugging session workflows
- gRPC service communication
- Tool dispatcher routing
- Memory state consistency

### End-to-End Tests
- AI agent debugging scenarios
- Bug detection accuracy
- Performance under load
- Error recovery paths

## Documentation Requirements

1. **API Reference**: Complete gRPC service documentation
2. **Tool Guide**: How to use each debugging tool
3. **Assembly Primer**: 65816 basics for AI agents
4. **Common Patterns**: Debugging patterns for Zelda3
5. **Troubleshooting**: Common issues and solutions

## Conclusion

This debugging API design provides a comprehensive foundation for AI agents to effectively debug SNES ROM hacks. The phased approach ensures quick delivery of core features while building toward advanced analysis capabilities. The integration with existing yaze infrastructure and focus on 65816-specific debugging makes this a powerful tool for ROM hacking assistance.

The API balances technical depth with usability, providing both low-level control for precise debugging and high-level analysis for pattern recognition. This enables AI agents to assist with everything from simple crash debugging to complex performance optimization.