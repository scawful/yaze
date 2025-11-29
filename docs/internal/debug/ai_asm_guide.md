# AI-Assisted 65816 Assembly Debugging Guide

This guide documents how AI agents (Claude, Gemini, etc.) can use the yaze EmulatorService gRPC API to debug 65816 assembly code in SNES ROM hacks like Oracle of Secrets.

## Overview

The EmulatorService provides comprehensive debugging capabilities:
- **Disassembly**: Convert raw bytes to human-readable 65816 assembly
- **Symbol Resolution**: Map addresses to labels from Asar ASM files
- **Breakpoints/Watchpoints**: Pause execution on conditions
- **Stepping**: StepInto, StepOver, StepOut with call stack tracking
- **Memory Inspection**: Read/write SNES memory regions

## Getting Started

### 1. Start the Emulator Server

```bash
# Launch z3ed with ROM and start gRPC server
z3ed emu start --rom oracle_of_secrets.sfc --grpc-port 50051
```

### 2. Load Symbols (Optional but Recommended)

Load symbols from your ASM source directory for meaningful labels:

```protobuf
rpc LoadSymbols(SymbolFileRequest) returns (CommandResponse)

// Request:
// - path: Directory containing .asm files (e.g., "assets/asm/usdasm/bank00/")
// - format: ASAR_ASM | WLA_DX | MESEN | BSNES
```

### 3. Set Breakpoints

```protobuf
rpc AddBreakpoint(BreakpointRequest) returns (BreakpointResponse)

// Request:
// - address: 24-bit address (e.g., 0x008000 for bank 00, offset $8000)
// - type: EXECUTE | READ | WRITE
// - enabled: true/false
// - condition: Optional expression (e.g., "A == 0x10")
```

### 4. Run Until Breakpoint

```protobuf
rpc RunToBreakpoint(Empty) returns (BreakpointHitResponse)

// Response includes:
// - address: Where execution stopped
// - breakpoint_id: Which breakpoint triggered
// - registers: Current CPU state (A, X, Y, PC, SP, P, DBR, PBR, DP)
```

## Debugging Workflow

### Disassembling Code

```protobuf
rpc GetDisassembly(DisassemblyRequest) returns (DisassemblyResponse)

// Request:
// - address: Starting 24-bit address
// - count: Number of instructions to disassemble
// - m_flag: Accumulator size (true = 8-bit, false = 16-bit)
// - x_flag: Index register size (true = 8-bit, false = 16-bit)
```

Example response with symbols loaded:
```
$008000: SEI           ; Disable interrupts
$008001: CLC           ; Clear carry for native mode
$008002: XCE           ; Switch to native mode
$008003: REP #$30      ; 16-bit A, X, Y
$008005: LDA #$8000    ; Load screen buffer address
$008008: STA $2100     ; PPU_BRIGHTNESS
$00800B: JSR Reset     ; Call Reset subroutine
```

### Stepping Through Code

**StepInto** - Execute one instruction:
```protobuf
rpc StepInstruction(Empty) returns (StepResponse)
```

**StepOver** - Execute subroutine as single step:
```protobuf
rpc StepOver(Empty) returns (StepResponse)
// If current instruction is JSR/JSL, runs until it returns
// Otherwise equivalent to StepInto
```

**StepOut** - Run until current subroutine returns:
```protobuf
rpc StepOut(Empty) returns (StepResponse)
// Continues execution until RTS/RTL decreases call depth
```

### Reading Memory

```protobuf
rpc ReadMemory(MemoryRequest) returns (MemoryResponse)

// Request:
// - address: Starting address
// - length: Number of bytes to read

// Response:
// - data: Bytes as hex string or raw bytes
```

Common SNES memory regions:
- `$7E0000-$7FFFFF`: WRAM (128KB)
- `$000000-$FFFFFF`: ROM (varies by mapper)
- `$2100-$213F`: PPU registers
- `$4200-$421F`: CPU registers
- `$4300-$437F`: DMA registers

### Symbol Lookup

```protobuf
rpc ResolveSymbol(SymbolLookupRequest) returns (SymbolLookupResponse)
// name: "Player_X" -> address: 0x7E0010

rpc GetSymbolAt(AddressRequest) returns (SymbolLookupResponse)
// address: 0x7E0010 -> name: "Player_X", type: RAM
```

## 65816 Debugging Tips for AI Agents

### Understanding M/X Flags

The 65816 has variable-width registers controlled by status flags:
- **M flag** (bit 5 of P): Controls accumulator/memory width
  - M=1: 8-bit accumulator, 8-bit memory operations
  - M=0: 16-bit accumulator, 16-bit memory operations
- **X flag** (bit 4 of P): Controls index register width
  - X=1: 8-bit X and Y registers
  - X=0: 16-bit X and Y registers

Track flag changes from `REP` and `SEP` instructions:
```asm
REP #$30    ; M=0, X=0 (16-bit A, X, Y)
SEP #$20    ; M=1 (8-bit A, X and Y unchanged)
```

### Call Stack Tracking

The StepController automatically tracks:
- `JSR $addr` - 16-bit call within current bank
- `JSL $addr` - 24-bit long call across banks
- `RTS` - Return from JSR
- `RTL` - Return from JSL
- `RTI` - Return from interrupt

Use `GetDebugStatus` to view the current call stack.

### Common Debugging Scenarios

**1. Finding where a value is modified:**
```
1. Add a WRITE watchpoint on the memory address
2. Run emulation
3. When watchpoint triggers, examine call stack and code
```

**2. Tracing execution flow:**
```
1. Add EXECUTE breakpoint at entry point
2. Use StepOver to execute subroutines as single steps
3. Use StepInto when you want to enter a subroutine
4. Use StepOut to return from deep call stacks
```

**3. Understanding unknown code:**
```
1. Load symbols from source ASM files
2. Disassemble the region of interest
3. Cross-reference labels with source code
```

## Example: Debugging Player Movement

```python
# Pseudo-code for AI agent debugging workflow

# 1. Load symbols from Oracle of Secrets source
client.LoadSymbols(path="oracle_of_secrets/src/", format=ASAR_ASM)

# 2. Find the player update routine
result = client.ResolveSymbol(name="Player_Update")
player_update_addr = result.address

# 3. Set breakpoint at player update
bp = client.AddBreakpoint(address=player_update_addr, type=EXECUTE)

# 4. Run until we hit the player update
hit = client.RunToBreakpoint()

# 5. Step through and inspect state
while True:
    step = client.StepOver()
    print(f"PC: ${step.new_pc:06X} - {step.message}")

    # Read player position after each step
    player_x = client.ReadMemory(address=0x7E0010, length=2)
    player_y = client.ReadMemory(address=0x7E0012, length=2)
    print(f"Player: ({player_x}, {player_y})")

    if input("Continue? (y/n): ") != "y":
        break
```

## Proto Definitions Reference

Key message types from `protos/emulator_service.proto`:

```protobuf
message DisassemblyRequest {
  uint32 address = 1;
  uint32 count = 2;
  bool m_flag = 3;
  bool x_flag = 4;
}

message BreakpointRequest {
  uint32 address = 1;
  BreakpointType type = 2;
  bool enabled = 3;
  string condition = 4;
}

message StepResponse {
  bool success = 1;
  uint32 new_pc = 2;
  uint32 instructions_executed = 3;
  string message = 4;
}

message SymbolLookupRequest {
  string name = 1;
}

message SymbolLookupResponse {
  string name = 1;
  uint32 address = 2;
  string type = 3;  // RAM, ROM, CONST
}
```

## Troubleshooting

**Q: Disassembly shows wrong operand sizes**
A: The M/X flags might not match. Use `GetGameState` to check current P register, then pass correct `m_flag` and `x_flag` values.

**Q: Symbols not resolving**
A: Ensure you loaded symbols with `LoadSymbols` before calling `ResolveSymbol`. Check that the path points to valid ASM files.

**Q: StepOut not working**
A: The call stack might be empty (program is at top level). Check `GetDebugStatus` for current call depth.

**Q: Breakpoint not triggering**
A: Verify the address is correct (24-bit, bank:offset format). Check that the code actually executes that path.
