# AI Infrastructure Improvements Plan

**Status:** Active  
**Owner (Agent ID):** ai-infra-architect  
**Branch:** `feature/ai-infra-improvements`  
**Created:** 2025-11-21  
**Last Updated:** 2025-11-25  
**Next Review:** 2025-12-02  
**Coordination Board Entry:** link when claimed

## Overview

This document outlines the gaps in yaze's AI infrastructure (gRPC services, MCP integration) and the planned improvements to make yaze-mcp fully functional for AI-assisted SNES ROM hacking.

## Current State Analysis

### yaze-mcp Tools (28 Emulator + 10 ROM = 38 total)

| Category | Tools | Status |
|----------|-------|--------|
| Emulator Lifecycle | pause, resume, reset | Working |
| Execution Control | step_instruction, run_to_breakpoint | Working |
| Execution Control | step_over, step_out | Partial (step_over falls back to single-step) |
| Memory | read_memory, write_memory, get_game_state | Working |
| Breakpoints | add/remove/list/toggle | Working (execute only during emulation) |
| Watchpoints | add/remove/list/history | NOT WIRED (only called from RPC handlers) |
| Disassembly | get_disassembly | Partial (returns OPCODE_XX placeholders) |
| Tracing | get_execution_trace | BROKEN (returns FAILED_PRECONDITION) |
| Symbols | load/resolve/get_symbol_at | DISABLED (ASAR integration disabled) |
| ROM Basic | get_rom_info, read_rom_bytes | Working |
| ROM Advanced | overworld/dungeon/sprite reads | NOT IMPLEMENTED (stubs return errors) |
| ROM Versioning | create/list snapshots | Working |
| Input | press_buttons | Working |

### Critical Gaps

1. **gRPC Server Not Started** - `AgentControlServer` exists but is never instantiated
2. **Watchpoints Bypass Emulation** - Only triggered by RPC read/write, not CPU bus activity
3. **Disassembly Uses Placeholders** - No proper 65816 disassembler integration
4. **Execution Trace Not Buffered** - No ring buffer for instruction history
5. **Symbols Disabled** - ASAR integration commented out
6. **ROM Domain RPCs Stubbed** - Overworld/dungeon/sprite return "not yet implemented"

## Implementation Plan

### Phase 1: gRPC Server Hosting (Priority: Critical)

**Goal:** Stand up a unified gRPC server that registers EmulatorService + RomService

**Files to Modify:**
- `src/app/editor/editor_manager.cc` - Start AgentControlServer when YAZE_ENABLE_REMOTE_AUTOMATION
- `src/cli/service/agent/agent_control_server.cc` - Register both EmulatorService and RomService
- `src/app/service/unified_grpc_server.cc` - Consider merging with AgentControlServer

**Tasks:**
- [ ] Add `StartAgentServer()` method to EditorManager
- [ ] Wire startup to `YAZE_ENABLE_REMOTE_AUTOMATION` flag or `--enable-grpc` CLI flag
- [ ] Register EmulatorService and RomService on same server
- [ ] Add configurable port (default 50051)
- [ ] Test with yaze-mcp `check_status`

### Phase 2: Emulator Debug RPCs (Priority: High)

**Goal:** Flesh out disassembly, execution trace, and stepping

**2a. Proper Disassembly**
- Use DisassemblyViewer's existing instruction recording
- Or integrate a standalone 65816 disassembler (bsnes style)
- File: `src/cli/service/agent/emulator_service_impl.cc` lines 661-705

**2b. Execution Trace Buffer**
- Add ring buffer (1000-10000 entries) to DisassemblyViewer
- Record: address, opcode, operands, cycle count, register snapshot
- File: `src/app/emu/debug/disassembly_viewer.h`

**2c. StepOver Implementation**
- Detect JSR (0x20) and JSL (0x22) opcodes
- Set temporary breakpoint at return address (PC + instruction length)
- Run until breakpoint hit, then remove temporary BP
- File: `src/cli/service/agent/emulator_service_impl.cc` lines 598-607

**Tasks:**
- [ ] Integrate real 65816 disassembly into GetDisassembly RPC
- [ ] Add ExecutionTraceBuffer class with ring buffer
- [ ] Implement GetExecutionTrace from buffer
- [ ] Implement proper StepOver with JSR/JSL detection

### Phase 3: Breakpoint/Watchpoint Memory Integration (Priority: High)

**Goal:** Wire memory breakpoints and watchpoints into emulator memory bus

**Current State:**
- `BreakpointManager::ShouldBreakOnExecute()` IS called via CPU callback
- `BreakpointManager::ShouldBreakOnMemoryAccess()` IS NOT called during emulation
- `WatchpointManager::OnMemoryAccess()` IS NOT called during emulation

**Files to Modify:**
- `src/app/emu/snes.h` - Add read/write callbacks
- `src/app/emu/snes.cc` - Invoke breakpoint/watchpoint managers in CpuRead/CpuWrite
- `src/app/emu/emulator.cc` - Wire managers to callbacks

**Implementation:**
```cpp
// In Snes::CpuRead() or via callback:
if (debugging_enabled_) {
  if (breakpoint_manager_.ShouldBreakOnMemoryAccess(addr, BreakpointManager::AccessType::READ)) {
    running_ = false;
  }
  watchpoint_manager_.OnMemoryAccess(addr, /*is_write=*/false, value);
}

// In Snes::CpuWrite() or via callback:
if (debugging_enabled_) {
  if (breakpoint_manager_.ShouldBreakOnMemoryAccess(addr, BreakpointManager::AccessType::WRITE)) {
    running_ = false;
  }
  watchpoint_manager_.OnMemoryAccess(addr, /*is_write=*/true, value);
}
```

**Tasks:**
- [ ] Add `on_memory_read_` and `on_memory_write_` callbacks to CPU
- [ ] Invoke BreakpointManager from callbacks
- [ ] Invoke WatchpointManager from callbacks
- [ ] Add MCP tools for watchpoints: `add_watchpoint`, `list_watchpoints`, `get_watchpoint_history`
- [ ] Test memory breakpoints and watchpoints with yaze-mcp

### Phase 4: Symbol Loading & Resolution (Priority: Medium)

**Goal:** Load ASAR/WLA-DX/CA65 symbol files and enable label resolution

**Current State:**
- EmulatorServiceImpl has stubbed symbol methods returning "not available"
- ASAR wrapper exists in `src/core/asar_wrapper.h`

**Implementation Approach:**
1. Create `SymbolTable` class to store symbols (name -> address map)
2. Implement parsers for each format:
   - ASAR: `.sym` files with `label = $XXXXXX` format
   - WLA-DX: `.sym` files with different format
   - CA65: `.dbg` or `.map` files
   - Mesen: `.mlb` label files
3. Wire LoadSymbols RPC to parse and populate SymbolTable
4. Wire ResolveSymbol/GetSymbolAt to query SymbolTable

**Files to Create:**
- `src/app/emu/debug/symbol_table.h`
- `src/app/emu/debug/symbol_table.cc`
- `src/app/emu/debug/symbol_parser.h` - Format parsers

**Tasks:**
- [ ] Design SymbolTable class (bidirectional lookup)
- [ ] Implement ASAR .sym parser
- [ ] Implement WLA-DX parser
- [ ] Wire to EmulatorServiceImpl
- [ ] Test with Oracle of Secrets symbols

### Phase 5: ROM Domain RPCs (Priority: Medium)

**Goal:** Implement overworld/dungeon/sprite read/write RPCs

**Current State:**
- All domain RPCs return "not yet implemented"
- ROM class has raw access, but not structured zelda3 data

**Implementation:**
- Leverage `zelda3::Overworld`, `zelda3::Dungeon`, `zelda3::Sprite` classes
- Need to instantiate these in RomServiceImpl or get from shared state

**Files to Modify:**
- `src/app/net/rom_service_impl.cc` - Implement ReadOverworldMap, ReadDungeonRoom, ReadSprite
- Proto messages already defined in `rom_service.proto`

**Tasks:**
- [ ] Add zelda3::Overworld access to RomServiceImpl
- [ ] Implement ReadOverworldMap (tile16 data for 160 maps)
- [ ] Implement WriteOverworldTile
- [ ] Add zelda3::Dungeon access
- [ ] Implement ReadDungeonRoom (tile16 data for 296 rooms)
- [ ] Implement WriteDungeonTile
- [ ] Implement ReadSprite

### Phase 6: yaze-mcp Enhancements (Priority: Low)

**Goal:** Improve MCP error handling and add missing tools

**Tasks:**
- [ ] Add timeout/retry logic based on gRPC status codes
- [ ] Add clearer error messages for unimplemented RPCs
- [ ] Add watchpoint tools to server.py
- [ ] Document required build preset and port
- [ ] Add connection health monitoring

## File Reference

### Emulator Service
- **Header:** `src/cli/service/agent/emulator_service_impl.h`
- **Implementation:** `src/cli/service/agent/emulator_service_impl.cc` (822 lines)
- **Server:** `src/cli/service/agent/agent_control_server.cc`

### ROM Service
- **Header:** `src/app/net/rom_service_impl.h`
- **Implementation:** `src/app/net/rom_service_impl.cc`
- **Version Manager:** `src/app/net/rom_version_manager.h`

### Debug Managers
- **Breakpoints:** `src/app/emu/debug/breakpoint_manager.h|cc`
- **Watchpoints:** `src/app/emu/debug/watchpoint_manager.h|cc`
- **Disassembly:** `src/app/emu/debug/disassembly_viewer.h`

### Emulator Core
- **Emulator:** `src/app/emu/emulator.h|cc`
- **SNES:** `src/app/emu/snes.h|cc`
- **CPU:** `src/app/emu/cpu/cpu.h`

### MCP Server
- **Location:** `/Users/scawful/Code/yaze-mcp/server.py`
- **Proto Stubs:** `/Users/scawful/Code/yaze-mcp/protos/`

## Success Criteria

1. **yaze-mcp `check_status`** connects and returns full emulator state
2. **Memory breakpoints** pause emulation on WRAM/SRAM access
3. **Watchpoints** track and log all memory accesses in specified ranges
4. **`get_disassembly`** returns proper 65816 mnemonics
5. **`get_execution_trace`** returns last N instructions executed
6. **Symbol loading** works with ASAR output from Oracle of Secrets
7. **ROM domain RPCs** return structured overworld/dungeon/sprite data

## Notes

- Consider performance impact of memory access callbacks (may need optimization)
- May want debug mode toggle to enable/disable expensive instrumentation
- Future: Canvas automation service for GUI automation via MCP
