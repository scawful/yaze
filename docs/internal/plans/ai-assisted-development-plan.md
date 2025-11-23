# AI-Assisted Development Workflow Plan

## Executive Summary

This document outlines a practical AI-assisted development workflow for the yaze project, enabling AI agents to help developers during both yaze development and ROM hack debugging. The system leverages existing infrastructure (gRPC services, tool dispatcher, emulator integration) to deliver immediate value with minimal new development.

## Architecture Overview

### Core Components

```
┌─────────────────────────────────────────────────┐
│                   z3ed CLI                      │
│  ┌──────────────────────────────────────────┐  │
│  │         AI Service Factory               │  │
│  │  (Ollama/Gemini/Mock Providers)         │  │
│  └──────────────────────────────────────────┘  │
│                      │                          │
│  ┌──────────────────────────────────────────┐  │
│  │         Agent Orchestrator               │  │
│  │  (Conversational + Tool Dispatcher)      │  │
│  └──────────────────────────────────────────┘  │
│                      │                          │
│         ┌────────────┴────────────┐             │
│         ▼                          ▼             │
│  ┌──────────────┐          ┌──────────────┐    │
│  │ Dev Mode     │          │ Debug Mode   │    │
│  │ Agent        │          │ Agent        │    │
│  └──────────────┘          └──────────────┘    │
│         │                          │             │
│         ▼                          ▼             │
│  ┌──────────────────────────────────────────┐  │
│  │         Tool Dispatcher                  │  │
│  │  • FileSystemTool  • EmulatorTool       │  │
│  │  • BuildTool       • DisassemblyTool    │  │
│  │  • TestRunner      • MemoryInspector    │  │
│  └──────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
                          │
         ┌────────────────┼────────────────┐
         ▼                ▼                ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│Build System  │ │ Emulator     │ │ ROM Editor   │
│(CMake/Ninja) │ │ (via gRPC)   │ │ (via Tools)  │
└──────────────┘ └──────────────┘ └──────────────┘
```

### Existing Infrastructure (Ready to Use)

1. **EmulatorServiceImpl** (`src/cli/service/agent/emulator_service_impl.cc`)
   - Full debugger control via gRPC
   - Breakpoints, watchpoints, memory inspection
   - Execution control (step, run, pause)
   - Disassembly and trace capabilities

2. **ToolDispatcher** (`src/cli/service/agent/tool_dispatcher.h`)
   - Extensible tool system
   - Already supports ROM operations, GUI automation
   - Easy to add new tools (FileSystem, Build, etc.)

3. **Disassembler65816** (`src/cli/service/agent/disassembler_65816.h`)
   - Full 65816 instruction decoding
   - Execution trace buffer
   - CPU state snapshots

4. **AI Service Integration**
   - Ollama and Gemini providers implemented
   - Conversational agent with tool calling
   - Prompt builder with context management

5. **FileSystemTool** (Just implemented by CLAUDE_AIINF)
   - Safe read-only filesystem exploration
   - Project directory restriction
   - Binary file detection

## Mode 1: App Development Agent

### Purpose
Help developers while coding yaze itself - catch errors, run tests, analyze crashes, suggest improvements.

### Key Features

#### 1.1 Build Monitoring & Error Resolution
```yaml
Triggers:
  - Compilation error detected
  - Link failure
  - CMake configuration issue

Agent Actions:
  - Parse error messages
  - Analyze include paths and dependencies
  - Suggest fixes with code snippets
  - Check for common pitfalls (circular deps, missing headers)

Tools Used:
  - BuildTool (configure, compile, status)
  - FileSystemTool (read source files)
  - TestRunner (verify fixes)
```

#### 1.2 Crash Analysis
```yaml
Triggers:
  - Segmentation fault
  - Assertion failure
  - Stack overflow

Agent Actions:
  - Parse stack trace
  - Read relevant source files
  - Analyze call chain
  - Suggest root cause and fix
  - Check for similar patterns in codebase

Tools Used:
  - FileSystemTool (read crash context)
  - BuildTool (recompile with debug symbols)
  - TestRunner (reproduce crash)
```

#### 1.3 Test Automation
```yaml
Triggers:
  - Code changes detected
  - Manual test request
  - Pre-commit hook

Agent Actions:
  - Identify affected test suites
  - Run relevant tests
  - Parse test output
  - Suggest test additions for uncovered code
  - Generate test cases for new functions

Tools Used:
  - TestRunner (execute tests)
  - FileSystemTool (analyze coverage)
  - BuildTool (compile test targets)
```

#### 1.4 Performance Analysis
```yaml
Triggers:
  - Performance regression detected
  - Manual profiling request
  - CI benchmark failure

Agent Actions:
  - Run performance benchmarks
  - Compare with baseline
  - Identify hot spots
  - Suggest optimization strategies
  - Check for common inefficiencies

Tools Used:
  - TestRunner (run benchmarks)
  - FileSystemTool (analyze code)
  - BuildTool (compile with profiling)
```

### Example Workflow: Compilation Error

```bash
# Developer encounters error
$ cmake --build build_ai
[ERROR] snes_color.cc:45: 'gfx::Arena' has not been declared

# Agent automatically kicks in
$ z3ed agent dev-assist --monitor-build

AI: I detected a compilation error. Let me analyze...

[Reading src/app/gfx/snes_color.cc]
[Checking include paths]
[Searching for gfx::Arena definition]

Found the issue! The file is missing:
  #include "app/gfx/arena.h"

The Arena class is defined in app/gfx/arena.h but not included.
Here's the fix:

--- a/src/app/gfx/snes_color.cc
+++ b/src/app/gfx/snes_color.cc
@@ -3,6 +3,7 @@
 #include "app/gfx/bitmap.h"
+#include "app/gfx/arena.h"

Would you like me to:
1. Apply this fix and rebuild?
2. Check for similar missing includes?
3. Run tests after fixing?
```

## Mode 2: Zelda Hack Debugging Agent

### Purpose
Help ROM hackers debug ASM patches, understand crashes, analyze game state during development.

### Key Features

#### 2.1 Breakpoint Analysis
```yaml
Triggers:
  - Breakpoint hit in emulator
  - Manual disassembly request
  - Crash during ROM execution

Agent Actions:
  - Disassemble current location
  - Explain instruction sequence
  - Show call stack
  - Analyze register values
  - Suggest what the code is doing

Tools Used:
  - EmulatorTool (control execution)
  - DisassemblyTool (decode instructions)
  - MemoryInspector (read RAM/ROM)
```

#### 2.2 Memory State Analysis
```yaml
Triggers:
  - Watchpoint triggered
  - Manual memory inspection
  - Corruption detected

Agent Actions:
  - Read memory regions
  - Compare with known structures
  - Identify data types (sprites, tiles, etc.)
  - Track memory modifications
  - Suggest corruption sources

Tools Used:
  - MemoryInspector (read/monitor memory)
  - EmulatorTool (set watchpoints)
  - ResourceTool (correlate with ROM data)
```

#### 2.3 ASM Patch Debugging
```yaml
Triggers:
  - Patch causes crash
  - Unexpected behavior after patch
  - Hook not executing

Agent Actions:
  - Compare patched vs original code
  - Trace execution flow
  - Verify jump targets
  - Check for stack imbalance
  - Validate addressing modes

Tools Used:
  - DisassemblyTool (analyze patches)
  - EmulatorTool (trace execution)
  - DiffTool (compare ROM regions)
```

#### 2.4 Routine Tracing
```yaml
Triggers:
  - Need to understand game routine
  - Reverse engineering request
  - Finding hook points

Agent Actions:
  - Trace routine execution
  - Document register usage
  - Map memory accesses
  - Identify subroutine calls
  - Generate routine documentation

Tools Used:
  - EmulatorTool (step through code)
  - DisassemblyTool (decode instructions)
  - SymbolProvider (resolve addresses)
```

### Example Workflow: Debugging ROM Crash

```bash
# ROM hacker experiences crash
$ z3ed agent debug-rom --rom=my_hack.sfc --emulator-port=50051

AI: Connected to emulator. Setting up debugging session...

[Emulator paused at crash point]
Address: $0A:8234
Instruction: STA $7E2000,X
Registers: A=$FFFF X=$8000 Y=$0000

AI: I see the crash! You're writing to $7E:A000 (WRAM mirror + $8000).
This is out of bounds for WRAM ($7E0000-$7FFFFF).

Let me trace back to see how X got this value...

[Analyzing execution trace]
5 instructions ago at $0A:8220:
  LDX $1A     ; Loaded X from $1A (value was $8000)

The issue is $1A contains $8000 when it should be < $2000.
This variable appears to be an index into the sprite table.

Possible causes:
1. Sprite index overflow (too many sprites)
2. Uninitialized memory at $1A
3. Calculation error in previous routine

Would you like me to:
1. Set a watchpoint on $1A to find what's setting it?
2. Analyze the routine that calculates sprite indices?
3. Check for similar boundary issues in your patches?
```

## Required New Components

### Phase 1: Core Tools (1-2 days)
1. **BuildTool** - CMake/Ninja integration
   - Configure, compile, test commands
   - Parse build output for errors
   - Status monitoring

2. **TestRunner** - CTest integration
   - Run specific test suites
   - Parse test results
   - Coverage analysis

3. **MemoryInspector** - Enhanced memory tools
   - Structured memory reads
   - Pattern matching
   - Corruption detection

### Phase 2: Agent Modes (2-3 days)
1. **DevAssistAgent** - Development helper
   - Build monitoring loop
   - Error pattern matching
   - Solution suggestion engine

2. **RomDebugAgent** - ROM hacking assistant
   - Emulator connection manager
   - Crash analysis engine
   - Patch verification system

### Phase 3: Enhanced Integration (3-5 days)
1. **Continuous Monitoring**
   - File watcher for auto-rebuild
   - Test runner on file changes
   - Performance regression detection

2. **Context Management**
   - Project state tracking
   - History of issues and fixes
   - Learning from past solutions

## Implementation Phases

### Phase 1: Foundation (Week 1)
**Goal**: Basic tool infrastructure
**Deliverables**:
- BuildTool implementation
- TestRunner implementation
- Basic DevAssistAgent with build monitoring
- Command: `z3ed agent dev-assist --monitor`

### Phase 2: Debugging (Week 2)
**Goal**: ROM debugging capabilities
**Deliverables**:
- MemoryInspector enhancements
- RomDebugAgent implementation
- Emulator integration improvements
- Command: `z3ed agent debug-rom --rom=<file>`

### Phase 3: Intelligence (Week 3)
**Goal**: Smart analysis and suggestions
**Deliverables**:
- Error pattern database
- Solution suggestion engine
- Context-aware responses
- Test generation capabilities

### Phase 4: Polish (Week 4)
**Goal**: Production readiness
**Deliverables**:
- Performance optimization
- Documentation and tutorials
- Example workflows
- Integration tests

## Integration Points

### With Existing Code

1. **ToolDispatcher** (`tool_dispatcher.h`)
   ```cpp
   // Add new tool types
   enum class ToolCallType {
     // ... existing ...
     kBuildConfigure,
     kBuildCompile,
     kBuildTest,
     kMemoryRead,
     kMemoryWatch,
     kTestRun,
     kTestCoverage,
   };
   ```

2. **ConversationalAgentService**
   ```cpp
   // Add agent modes
   class DevAssistAgent : public AgentMode {
     void MonitorBuild();
     void AnalyzeCrash(const StackTrace& trace);
     void SuggestFix(const CompileError& error);
   };
   ```

3. **EmulatorService**
   ```cpp
   // Enhance debugging APIs
   class DebugSession {
     void SetAutomaticAnalysis(bool enabled);
     void RegisterCrashHandler(CrashCallback cb);
     void EnableInstructionTrace(size_t buffer_size);
   };
   ```

### With CI/CD Pipeline

1. **GitHub Actions Integration**
   ```yaml
   - name: AI-Assisted Build Check
     run: |
       z3ed agent dev-assist --ci-mode \
         --analyze-errors \
         --suggest-fixes \
         --output=ai-analysis.md
   ```

2. **Pre-commit Hooks**
   ```bash
   #!/bin/bash
   # .git/hooks/pre-commit
   z3ed agent dev-assist --check-changes \
     --run-affected-tests \
     --verify-no-regressions
   ```

## Example Workflows

### Workflow 1: AI-Assisted TDD
```bash
# Developer writes test first
$ cat > test/unit/my_feature_test.cc << EOF
TEST(MyFeature, ShouldDoSomething) {
  MyClass obj;
  EXPECT_EQ(obj.Process(5), 10);
}
EOF

# AI generates implementation
$ z3ed agent generate --from-test=test/unit/my_feature_test.cc

AI: Based on your test, here's a suggested implementation:

class MyClass {
public:
  int Process(int input) {
    return input * 2;  // Doubles the input based on test expectation
  }
};

Shall I create src/app/my_class.h with this implementation?
```

### Workflow 2: Crash During ROM Testing
```bash
# Developer testing ROM modifications
$ z3ed agent debug-rom --rom=hack.sfc --auto-debug

AI: Monitoring emulator for issues...

[CRASH DETECTED at $15:A0F2]
Illegal instruction: $42 (WDM - disabled on SNES)

This appears to be data executed as code. Analyzing...

The jump at $15:A0E0 uses indirect addressing: JMP ($0234)
The value at $0234 is $A0F2, which points to sprite data, not code.

Likely cause: Corrupted pointer table at $0234.

Suggested fix:
1. Check your modifications to pointer tables
2. The original value at $0234 should be $8CF2
3. Look for code that writes to $0234-$0235

Would you like me to trace what modified this address?
```

### Workflow 3: Performance Regression
```bash
# CI detects performance issue
$ z3ed agent analyze-perf --baseline=main --current=HEAD

AI: Performance regression detected in gfx::Arena::LoadTile16

Baseline: 1.2ms average
Current:  4.5ms average (275% slower)

Analyzing changes... Found the issue:

commit abc123: "Add texture validation"
+ for (auto& pixel : tile.data()) {
+   ValidatePixel(pixel);  // This is called 256 times per tile
+ }

The validation is happening in the hot path. Suggestions:
1. Move validation to debug builds only
2. Batch validation instead of per-pixel
3. Use SIMD for validation if keeping it

Here's an optimized version:
#ifdef DEBUG
  ValidateTileData(tile.data());  // Batch validation
#endif
```

## Success Metrics

### Immediate (Week 1)
- Build error resolution time < 30 seconds
- Basic test automation working
- 5+ common error patterns recognized

### Short-term (Month 1)
- 50% reduction in debugging time
- 80% of build errors auto-resolved
- 10+ developers using the tool

### Long-term (Quarter 1)
- Comprehensive error pattern database
- Integration with all major workflows
- Measurable improvement in development velocity
- Community contributions to agent capabilities

## Risk Mitigation

### Technical Risks
1. **AI Model Limitations**
   - Mitigation: Fallback to pattern matching when AI unavailable
   - Use local models (Ollama) for offline capability

2. **Performance Impact**
   - Mitigation: Async processing, optional features
   - Configurable resource limits

3. **False Positives**
   - Mitigation: Confidence scoring, user confirmation
   - Learning from corrections

### Adoption Risks
1. **Learning Curve**
   - Mitigation: Progressive disclosure, good defaults
   - Comprehensive examples and documentation

2. **Trust Issues**
   - Mitigation: Explainable suggestions, show reasoning
   - Allow manual override always

## Conclusion

This AI-assisted development workflow leverages yaze's existing infrastructure to provide immediate value with minimal new development. The phased approach ensures quick wins while building toward comprehensive AI assistance for both yaze development and ROM hacking workflows.

The system is designed to be:
- **Practical**: Uses existing components, minimal new code
- **Incremental**: Each phase delivers working features
- **Extensible**: Easy to add new capabilities
- **Reliable**: Fallbacks for when AI is unavailable

With just 1-2 weeks of development, we can have a working system that significantly improves developer productivity and ROM hacking debugging capabilities.