# ASM Debug Prompt Engineering System

This document defines a comprehensive prompt engineering system for AI-assisted 65816 assembly debugging in the context of ALTTP (The Legend of Zelda: A Link to the Past) ROM hacking. It is designed to integrate with the existing `prompt_builder.cc` infrastructure and the EmulatorService gRPC API.

## Table of Contents

1. [System Prompt Templates](#1-system-prompt-templates)
2. [Context Extraction Functions](#2-context-extraction-functions)
3. [Example Q&A Pairs](#3-example-qa-pairs)
4. [Integration with prompt_builder.cc](#4-integration-with-prompt_buildercc)

---

## 1. System Prompt Templates

### 1.1 Base 65816 Instruction Reference

```markdown
# 65816 Processor Reference for ALTTP Debugging

## Processor Modes

The 65816 operates in two main modes:
- **Emulation Mode**: 6502 compatibility (8-bit registers, 64KB address space)
- **Native Mode**: Full 65816 features (variable register widths, 24-bit addressing)

ALTTP always runs in **Native Mode** after initial boot.

## Register Width Flags (CRITICAL)

**M Flag (Bit 5 of Status Register P)**
- M=1: 8-bit Accumulator/Memory operations
- M=0: 16-bit Accumulator/Memory operations
- Changed by: `SEP #$20` (set M=1), `REP #$20` (clear M=0)

**X Flag (Bit 4 of Status Register P)**
- X=1: 8-bit Index Registers (X, Y)
- X=0: 16-bit Index Registers (X, Y)
- Changed by: `SEP #$10` (set X=1), `REP #$10` (clear X=0)

**Common Patterns:**
```asm
REP #$30    ; M=0, X=0 -> 16-bit A, X, Y (accumulator AND index)
SEP #$30    ; M=1, X=1 -> 8-bit A, X, Y
REP #$20    ; M=0 only -> 16-bit A, index unchanged
SEP #$20    ; M=1 only -> 8-bit A, index unchanged
```

## Status Register Flags (P)

| Bit | Flag | Name      | Meaning When Set |
|-----|------|-----------|------------------|
| 7   | N    | Negative  | Result is negative (bit 7/15 set) |
| 6   | V    | Overflow  | Signed overflow occurred |
| 5   | M    | Memory    | 8-bit accumulator/memory |
| 4   | X    | Index     | 8-bit index registers |
| 3   | D    | Decimal   | BCD arithmetic mode |
| 2   | I    | Interrupt | IRQ disabled |
| 1   | Z    | Zero      | Result is zero |
| 0   | C    | Carry     | Carry/borrow for arithmetic |

## Addressing Modes

| Mode | Syntax | Example | Description |
|------|--------|---------|-------------|
| Immediate | #$nn | `LDA #$10` | Load literal value |
| Direct Page | $nn | `LDA $0F` | Zero page (DP+offset) |
| Absolute | $nnnn | `LDA $0020` | 16-bit address in current bank |
| Absolute Long | $nnnnnn | `LDA $7E0020` | Full 24-bit address |
| Indexed X | $nnnn,X | `LDA $0D00,X` | Base + X register |
| Indexed Y | $nnnn,Y | `LDA $0D00,Y` | Base + Y register |
| Indirect | ($nn) | `LDA ($00)` | Pointer at DP address |
| Indirect Long | [$nn] | `LDA [$00]` | 24-bit pointer at DP |
| Indirect,Y | ($nn),Y | `LDA ($00),Y` | Pointer + Y offset |
| Stack Relative | $nn,S | `LDA $01,S` | Stack-relative addressing |

## Common Instruction Classes

### Data Transfer
- `LDA/LDX/LDY`: Load register from memory
- `STA/STX/STY/STZ`: Store register to memory
- `TAX/TAY/TXA/TYA`: Transfer between registers
- `PHA/PLA/PHX/PLX/PHY/PLY`: Push/pull registers to/from stack
- `PHP/PLP`: Push/pull processor status

### Arithmetic
- `ADC`: Add with carry (use `CLC` first for addition)
- `SBC`: Subtract with carry (use `SEC` first for subtraction)
- `INC/DEC`: Increment/decrement memory or register
- `INX/INY/DEX/DEY`: Increment/decrement index registers
- `CMP/CPX/CPY`: Compare (sets flags without storing result)

### Logic
- `AND/ORA/EOR`: Bitwise operations
- `ASL/LSR`: Arithmetic/logical shift
- `ROL/ROR`: Rotate through carry
- `BIT`: Test bits (sets N, V, Z flags)
- `TSB/TRB`: Test and set/reset bits

### Control Flow
- `JMP`: Unconditional jump
- `JSR/JSL`: Jump to subroutine (16-bit/24-bit)
- `RTS/RTL`: Return from subroutine (16-bit/24-bit)
- `RTI`: Return from interrupt
- `BEQ/BNE/BCC/BCS/BMI/BPL/BVS/BVC/BRA`: Conditional branches

### 65816-Specific
- `REP/SEP`: Reset/set processor flags
- `XCE`: Exchange carry and emulation flags
- `XBA`: Exchange A high/low bytes
- `MVN/MVP`: Block memory move
- `PHB/PLB`: Push/pull data bank register
- `PHK`: Push program bank register
- `PEA/PEI/PER`: Push effective address
```

### 1.2 ALTTP-Specific Memory Map

```markdown
# ALTTP Memory Map Reference

## WRAM Layout ($7E0000-$7FFFFF)

### Core Game State ($7E0000-$7E00FF)
| Address | Name | Size | Description |
|---------|------|------|-------------|
| $7E0010 | GameMode | 1 | Main game module (0x07=Underworld, 0x09=Overworld) |
| $7E0011 | GameSubmode | 1 | Sub-state within current module |
| $7E0012 | NMIFlag | 1 | NMI processing flag |
| $7E001A | FrameCounter | 1 | Increments every frame |
| $7E001B | IndoorsFlag | 1 | 0x00=outdoors, 0x01=indoors |

### Link's State ($7E0020-$7E009F)
| Address | Name | Size | Description |
|---------|------|------|-------------|
| $7E0020 | LinkPosY | 2 | Link's Y coordinate (low + high) |
| $7E0022 | LinkPosX | 2 | Link's X coordinate (low + high) |
| $7E002E | LinkLayer | 1 | Current layer (0=BG1, 1=BG2) |
| $7E002F | LinkDirection | 1 | Facing direction (0=up, 2=down, 4=left, 6=right) |
| $7E003C | LinkSpeed | 1 | Current movement speed |
| $7E005D | LinkAction | 1 | State machine state |
| $7E0069 | LinkHealth | 1 | Current hearts (x4 for quarter hearts) |

### Overworld State ($7E008A-$7E009F)
| Address | Name | Size | Description |
|---------|------|------|-------------|
| $7E008A | OverworldScreen | 1 | Current OW screen ID (0x00-0x3F LW, 0x40-0x7F DW) |
| $7E008C | OverworldMapX | 2 | Absolute X position on overworld |
| $7E008E | OverworldMapY | 2 | Absolute Y position on overworld |

### Dungeon State ($7E00A0-$7E00CF)
| Address | Name | Size | Description |
|---------|------|------|-------------|
| $7E00A0 | CurrentRoom | 2 | Current dungeon room ID ($000-$127) |
| $7E00A2 | LastRoom | 2 | Previous room ID |
| $7E00A4 | RoomBG2Property | 1 | BG2 layer property |

### Sprite Arrays ($7E0D00-$7E0FFF)
| Base | Offset | Size | Description |
|------|--------|------|-------------|
| $7E0D00 | +X | 16 | Sprite Y position (low) |
| $7E0D10 | +X | 16 | Sprite X position (low) |
| $7E0D20 | +X | 16 | Sprite Y position (high) |
| $7E0D30 | +X | 16 | Sprite X position (high) |
| $7E0D40 | +X | 16 | Sprite Y velocity |
| $7E0D50 | +X | 16 | Sprite X velocity |
| $7E0DD0 | +X | 16 | Sprite state (0=inactive) |
| $7E0E20 | +X | 16 | Sprite type/ID |
| $7E0E50 | +X | 16 | Sprite health |
| $7E0E90 | +X | 16 | Sprite subtype/flags |
| $7E0EB0 | +X | 16 | Sprite AI timer 1 |
| $7E0ED0 | +X | 16 | Sprite AI state |
| $7E0F10 | +X | 16 | Sprite direction |

### Ancilla Arrays ($7E0BF0-$7E0CFF)
| Base | Description |
|------|-------------|
| $7E0BFA | Ancilla Y position |
| $7E0C04 | Ancilla X position |
| $7E0C0E | Ancilla Y position (high) |
| $7E0C18 | Ancilla X position (high) |

### SRAM Mirror ($7EF000-$7EF4FF)
| Address | Name | Description |
|---------|------|-------------|
| $7EF340 | CurrentSword | Sword level (0-4) |
| $7EF341 | CurrentShield | Shield level (0-3) |
| $7EF342 | CurrentArmor | Armor level (0-2) |
| $7EF354 | BottleContents[4] | Bottle contents array |
| $7EF35A | BottleContents2[4] | Additional bottle data |
| $7EF36D | MaxHealth | Maximum hearts (x8) |
| $7EF36E | CurrentMagic | Current magic |
| $7EF370 | Rupees | Rupee count (2 bytes) |
| $7EF374 | PendantsBosses | Pendant/Crystal flags |
| $7EF37A | Abilities | Special abilities (dash, swim, etc.) |
| $7EF3C5-$7EF3CA | DungeonKeys | Keys per dungeon |

## ROM Data Addresses

### Sprite Data
| Address | Description |
|---------|-------------|
| $00DB97 | Sprite property table 1 |
| $00DC97 | Sprite property table 2 |
| $00DD97 | Sprite property table 3 |
| $0ED4C0 | Sprite graphics index table |
| $0ED6D0 | Sprite palette table |

### Dungeon Data
| Address | Description |
|---------|-------------|
| $028000 | Room header pointer table (low) |
| $02808F | Room header pointer table (mid) |
| $04F1E0 | Room object data starts |
| $058000 | Sprite data for rooms |

### Overworld Data
| Address | Description |
|---------|-------------|
| $0A8000 | Map16 tile definitions |
| $0F8000 | Overworld map data |
| $1BC2A9 | Entrance data |
| $1BB96C | Exit data |
```

### 1.3 Common Bug Patterns and Fixes

```markdown
# Common 65816 Bug Patterns in ALTTP Hacking

## 1. Register Width Mismatches

### Problem: Forgetting to set M/X flags
```asm
; BUG: Assuming 8-bit A when it might be 16-bit
LDA $0020
AND #$0F        ; If A is 16-bit, high byte not masked!
STA $0020
```

### Fix: Always explicitly set flags
```asm
SEP #$20        ; Force 8-bit A
LDA $0020
AND #$0F
STA $0020
REP #$20        ; Restore 16-bit if needed
```

### Detection: Look for immediate values that should be 16-bit but are 8-bit, or vice versa

## 2. Bank Boundary Issues

### Problem: Code jumps across bank without updating DBR
```asm
; BUG: JSR can't reach code in another bank
JSR FarFunction ; Only works if FarFunction is in same bank!
```

### Fix: Use JSL for cross-bank calls
```asm
JSL FarFunction ; 24-bit call crosses banks correctly
```

### Detection: Check if target address is >$FFFF from current PC

## 3. Stack Imbalance

### Problem: Push without matching pull
```asm
; BUG: PHP without PLP
MyRoutine:
    PHP
    SEP #$30
    ; ... code ...
    RTS         ; Stack still has status byte!
```

### Fix: Always pair push/pull operations
```asm
MyRoutine:
    PHP
    SEP #$30
    ; ... code ...
    PLP         ; Restore status
    RTS
```

### Detection: Count push/pull operations in subroutine

## 4. Sprite Index Corruption

### Problem: Not preserving X register (sprite index)
```asm
; BUG: X is sprite index, but gets overwritten
Sprite_DoSomething:
    LDX #$10        ; Overwrites sprite index!
    LDA Table,X
    ; ...
```

### Fix: Push/pull X or use Y instead
```asm
Sprite_DoSomething:
    PHX
    LDX #$10
    LDA Table,X
    PLX             ; Restore sprite index
```

### Detection: Check if sprite routines modify X without preservation

## 5. Zero-Page Collision

### Problem: Using same DP addresses as engine
```asm
; BUG: $00-$0F are scratch registers used by many routines
STA $00         ; May conflict with subroutine calls!
JSR SomeRoutine ; Routine might use $00!
LDA $00         ; No longer your value
```

### Fix: Use safe scratch areas or stack
```asm
PHA             ; Push value to stack
JSR SomeRoutine
PLA             ; Retrieve value
```

### Detection: Check DP address usage before and after JSR/JSL

## 6. Carry Flag Not Set/Cleared

### Problem: ADC without CLC or SBC without SEC
```asm
; BUG: Previous carry affects result
LDA $0020
ADC #$10        ; If C=1, adds 17 instead of 16!
STA $0020
```

### Fix: Always clear/set carry for arithmetic
```asm
LDA $0020
CLC             ; Clear carry before addition
ADC #$10
STA $0020
```

### Detection: Look for ADC/SBC without preceding CLC/SEC

## 7. Comparing Signed vs Unsigned

### Problem: Using wrong branch after comparison
```asm
; BUG: BCS is unsigned, but data is signed
LDA PlayerVelocity  ; Can be negative (signed)
CMP #$10
BCS .too_fast       ; Wrong! This is unsigned comparison
```

### Fix: Use signed branches (BMI/BPL) for signed data
```asm
LDA PlayerVelocity
SEC
SBC #$10            ; Subtract to check sign
BMI .not_too_fast   ; Negative = less than (signed)
```

### Detection: Check if memory location stores signed values

## 8. Off-by-One in Loops

### Problem: Loop counter starts/ends incorrectly
```asm
; BUG: Processes 17 sprites instead of 16
LDX #$10            ; X = 16
.loop
    LDA $0DD0,X     ; Process sprite X
    DEX
    BPL .loop       ; Loops while X >= 0 (X=16,15,...,0 = 17 iterations)
```

### Fix: Start at correct value
```asm
LDX #$0F            ; X = 15 (last sprite index)
.loop
    LDA $0DD0,X
    DEX
    BPL .loop       ; X=15,14,...,0 = 16 iterations
```

### Detection: Trace loop counter values

## 9. Using Wrong Relative Branch Distance

### Problem: Branch target too far
```asm
; BUG: BRA only reaches +/-127 bytes
    BRA FarTarget   ; Assembler error if target > 127 bytes away
```

### Fix: Use BRL (16-bit relative) or JMP
```asm
    BRL FarTarget   ; 16-bit relative branch
    ; or
    JMP FarTarget   ; Absolute jump
```

### Detection: Check branch offsets during assembly
```

### 1.4 Sprite/Object System Overview

```markdown
# ALTTP Sprite System Architecture

## Sprite Slot Management

ALTTP supports up to 16 active sprites (indices $00-$0F). Each sprite is defined by multiple arrays at different WRAM addresses.

### Sprite Lifecycle

1. **Spawn**: `Sprite_SpawnDynamically` ($09:A300) finds free slot, sets type
2. **Initialize**: Sprite-specific init routine sets position, health, AI state
3. **Update**: Main sprite loop calls AI routine every frame
4. **Death**: `Sprite_PrepOamCoord` handles death animation, slot freed

### Key State Variables

```
$0DD0,X (SpriteState):
  $00 = Dead/Inactive (slot available)
  $01-$07 = Dying states
  $08 = Active
  $09 = Carried by Link
  $0A = Stunned
  $0B = Recoiling

$0E20,X (SpriteType):
  Sprite ID from sprite table (0x00-0xFF)
  Examples: $09=Green Soldier, $D2=Fish, $4A=Bomb

$0ED0,X (SpriteAIState):
  Sprite-specific AI state machine value
  Meaning varies per sprite type

$0E50,X (SpriteHealth):
  Hit points remaining
  0 triggers death sequence
```

## Sprite Main Loop

Located in Bank $06, the main sprite loop:

```asm
; Simplified main sprite loop (Bank $06)
MainSpriteLoop:
    LDX #$0F            ; Start with sprite 15
.loop
    LDA $0DD0,X         ; Get sprite state
    BEQ .inactive       ; Skip if dead

    JSR Sprite_CheckActive  ; Visibility/activity check
    BCC .skip_ai        ; Skip if off-screen

    JSL Sprite_Main     ; Call sprite's AI routine

.skip_ai
.inactive
    DEX
    BPL .loop           ; Process all 16 sprites
```

## Sprite AI Routine Structure

Each sprite type has an AI routine indexed by sprite type:

```asm
; Sprite AI dispatch table
Sprite_Main:
    LDA $0E20,X         ; Get sprite type
    ASL A               ; x2 for pointer table
    TAY
    LDA SpriteAI_Low,Y  ; Get routine address
    STA $00
    LDA SpriteAI_High,Y
    STA $01
    JMP ($0000)         ; Jump to AI routine
```

## Common Sprite Subroutines (Bank $06)

### Position and Movement
| Routine | Address | Description |
|---------|---------|-------------|
| Sprite_ApplySpeedTowardsLink | $06:90E3 | Move toward Link |
| Sprite_Move | $06:F0D4 | Apply X/Y velocities |
| Sprite_BounceFromWall | $06:F2E0 | Wall collision handling |
| Sprite_CheckTileCollision | $06:E8A7 | Tile collision check |

### Combat and Interaction
| Routine | Address | Description |
|---------|---------|-------------|
| Sprite_CheckDamageToLink | $06:D600 | Check if hurting Link |
| Sprite_CheckDamageFromLink | $06:D61D | Check if Link hurting sprite |
| Sprite_AttemptZapDamage | $06:D66A | Apply damage to sprite |
| Sprite_SetupHitBox | $06:D580 | Configure collision box |

### Graphics and OAM
| Routine | Address | Description |
|---------|---------|-------------|
| Sprite_PrepOamCoord | $06:D3A8 | Prepare OAM coordinates |
| Sprite_DrawShadow | $06:D4B0 | Draw sprite shadow |
| Sprite_OAM_AllocateDef | $06:D508 | Allocate OAM slots |

### Spawning
| Routine | Address | Description |
|---------|---------|-------------|
| Sprite_SpawnDynamically | $09:A300 | Spawn new sprite |
| Sprite_SpawnThrowableTerrain | $09:A4A0 | Spawn throwable object |
| Sprite_SetSpawnedCoordinates | $09:A380 | Set spawn position |

## Debugging Sprite Issues

### "Sprite Not Appearing"
1. Check if slot available (all $0DD0,X values = 0?)
2. Verify sprite ID is valid (0x00-0xFF)
3. Check position is on-screen
4. Verify spriteset supports this sprite type

### "Sprite Stuck/Not Moving"
1. Check AI state ($0ED0,X)
2. Verify velocity values ($0D40,X and $0D50,X)
3. Check for collision issues (tile/wall)
4. Verify main loop is being called

### "Sprite Dies Instantly"
1. Check initial health ($0E50,X)
2. Verify damage immunity flags
3. Check if spawning inside collision

### Useful Breakpoints for Sprite Debugging
| Address | Trigger On | Purpose |
|---------|------------|---------|
| $06:8000 | Execute | Start of sprite bank |
| $09:A300 | Execute | Sprite spawn |
| $06:D600 | Execute | Damage to Link |
| $0DD0 | Write | Sprite state change |
| $0E50 | Write | Health change |
```

---

## 2. Context Extraction Functions

These C++ functions should be added to the codebase to extract debugging context for AI prompts.

### 2.1 Header File: `src/cli/service/ai/asm_debug_context.h`

```cpp
#ifndef YAZE_CLI_SERVICE_AI_ASM_DEBUG_CONTEXT_H_
#define YAZE_CLI_SERVICE_AI_ASM_DEBUG_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace yaze {
namespace cli {
namespace ai {

// CPU state snapshot for context injection
struct CpuStateContext {
  uint16_t a;        // Accumulator
  uint16_t x;        // X index register
  uint16_t y;        // Y index register
  uint16_t sp;       // Stack pointer
  uint16_t pc;       // Program counter
  uint8_t pb;        // Program bank
  uint8_t db;        // Data bank
  uint16_t dp;       // Direct page register
  uint8_t status;    // Processor status (P)
  uint64_t cycles;   // Cycle count

  // Decoded flags
  bool flag_n() const { return status & 0x80; }  // Negative
  bool flag_v() const { return status & 0x40; }  // Overflow
  bool flag_m() const { return status & 0x20; }  // Memory/Accumulator size
  bool flag_x() const { return status & 0x10; }  // Index register size
  bool flag_d() const { return status & 0x08; }  // Decimal mode
  bool flag_i() const { return status & 0x04; }  // IRQ disable
  bool flag_z() const { return status & 0x02; }  // Zero
  bool flag_c() const { return status & 0x01; }  // Carry

  std::string FormatForPrompt() const;
};

// Disassembly line for context
struct DisassemblyLine {
  uint32_t address;
  uint8_t opcode;
  std::vector<uint8_t> operands;
  std::string mnemonic;
  std::string operand_str;
  std::string symbol;      // Resolved symbol name if available
  std::string comment;     // From source if available
  bool is_current_pc;      // True if this is current PC
  bool has_breakpoint;

  std::string FormatForPrompt() const;
};

// Memory region snapshot
struct MemorySnapshot {
  uint32_t start_address;
  std::vector<uint8_t> data;
  std::string region_name;  // e.g., "Stack", "Sprite Arrays", "Link State"

  std::string FormatHexDump() const;
  std::string FormatWithLabels(
      const std::map<uint32_t, std::string>& labels) const;
};

// Stack frame info
struct CallStackEntry {
  uint32_t return_address;
  uint32_t call_site;
  std::string symbol_name;  // Resolved if available
  bool is_long_call;        // JSL vs JSR
};

// Breakpoint hit context
struct BreakpointHitContext {
  uint32_t address;
  std::string breakpoint_type;  // "EXECUTE", "READ", "WRITE"
  std::string trigger_reason;
  CpuStateContext cpu_state;
  std::vector<DisassemblyLine> surrounding_code;  // Before and after
  std::vector<CallStackEntry> call_stack;
  std::map<std::string, MemorySnapshot> relevant_memory;

  std::string BuildPromptContext() const;
};

// Memory comparison for before/after analysis
struct MemoryDiff {
  uint32_t address;
  uint8_t old_value;
  uint8_t new_value;
  std::string label;  // Symbol if available
};

struct MemoryComparisonContext {
  std::string description;
  std::vector<MemoryDiff> diffs;
  CpuStateContext state_before;
  CpuStateContext state_after;
  std::vector<DisassemblyLine> executed_code;

  std::string BuildPromptContext() const;
};

// Crash analysis context
struct CrashAnalysisContext {
  std::string crash_type;  // "INVALID_OPCODE", "BRK", "STACK_OVERFLOW", etc.
  CpuStateContext final_state;
  std::vector<DisassemblyLine> code_at_crash;
  std::vector<CallStackEntry> call_stack;
  MemorySnapshot stack_memory;
  std::vector<std::string> potential_causes;

  std::string BuildPromptContext() const;
};

// Execution trace context
struct ExecutionTraceContext {
  std::vector<DisassemblyLine> trace_entries;
  std::string filter_description;  // What was being traced
  CpuStateContext start_state;
  CpuStateContext end_state;

  std::string BuildPromptContext() const;
};

// Builder class for constructing debug contexts
class AsmDebugContextBuilder {
 public:
  AsmDebugContextBuilder() = default;

  // Set memory reader callback
  using MemoryReader = std::function<uint8_t(uint32_t)>;
  void SetMemoryReader(MemoryReader reader) { memory_reader_ = reader; }

  // Set symbol provider
  using SymbolLookup = std::function<std::string(uint32_t)>;
  void SetSymbolLookup(SymbolLookup lookup) { symbol_lookup_ = lookup; }

  // Build breakpoint hit context
  BreakpointHitContext BuildBreakpointContext(
      uint32_t address,
      const CpuStateContext& cpu_state,
      int surrounding_lines = 10);

  // Build memory comparison context
  MemoryComparisonContext BuildMemoryComparison(
      const std::vector<MemorySnapshot>& before,
      const std::vector<MemorySnapshot>& after,
      const CpuStateContext& state_before,
      const CpuStateContext& state_after);

  // Build crash analysis context
  CrashAnalysisContext BuildCrashContext(
      const std::string& crash_type,
      const CpuStateContext& final_state);

  // Build execution trace context
  ExecutionTraceContext BuildTraceContext(
      const std::vector<DisassemblyLine>& trace,
      const CpuStateContext& start_state,
      const CpuStateContext& end_state);

  // Get ALTTP-specific memory regions
  std::map<std::string, MemorySnapshot> GetALTTPRelevantMemory();

 private:
  MemoryReader memory_reader_;
  SymbolLookup symbol_lookup_;

  std::vector<DisassemblyLine> DisassembleRegion(
      uint32_t start, uint32_t end);
  std::string LookupSymbol(uint32_t address);
};

}  // namespace ai
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_ASM_DEBUG_CONTEXT_H_
```

### 2.2 Context Formatting Implementation

```cpp
// src/cli/service/ai/asm_debug_context.cc

#include "cli/service/ai/asm_debug_context.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace yaze {
namespace cli {
namespace ai {

std::string CpuStateContext::FormatForPrompt() const {
  return absl::StrFormat(R"(## CPU State
- **PC**: $%02X:%04X (Bank $%02X, Offset $%04X)
- **A**: $%04X (%d)%s
- **X**: $%04X (%d)%s
- **Y**: $%04X (%d)%s
- **SP**: $%04X
- **DP**: $%04X
- **DB**: $%02X

### Status Flags (P = $%02X)
| N | V | M | X | D | I | Z | C |
|---|---|---|---|---|---|---|---|
| %c | %c | %c | %c | %c | %c | %c | %c |

%s: %s accumulator/memory
%s: %s index registers
)",
    pb, pc, pb, pc,
    a, a, flag_m() ? "" : " (16-bit)",
    x, x, flag_x() ? "" : " (16-bit)",
    y, y, flag_x() ? "" : " (16-bit)",
    sp, dp, db,
    status,
    flag_n() ? '1' : '0', flag_v() ? '1' : '0',
    flag_m() ? '1' : '0', flag_x() ? '1' : '0',
    flag_d() ? '1' : '0', flag_i() ? '1' : '0',
    flag_z() ? '1' : '0', flag_c() ? '1' : '0',
    flag_m() ? "M=1" : "M=0", flag_m() ? "8-bit" : "16-bit",
    flag_x() ? "X=1" : "X=0", flag_x() ? "8-bit" : "16-bit"
  );
}

std::string DisassemblyLine::FormatForPrompt() const {
  std::string marker = is_current_pc ? ">>>" : "   ";
  std::string bp_marker = has_breakpoint ? "*" : " ";
  std::string sym = symbol.empty() ? "" : absl::StrFormat(" ; %s", symbol);
  std::string cmt = comment.empty() ? "" : absl::StrFormat("  // %s", comment);

  return absl::StrFormat("%s%s $%06X: %-4s %-12s%s%s",
    marker, bp_marker, address, mnemonic, operand_str, sym, cmt);
}

std::string BreakpointHitContext::BuildPromptContext() const {
  std::ostringstream oss;

  oss << "# Breakpoint Hit Context\n\n";
  oss << absl::StrFormat("**Breakpoint Type**: %s\n", breakpoint_type);
  oss << absl::StrFormat("**Address**: $%06X\n", address);
  oss << absl::StrFormat("**Reason**: %s\n\n", trigger_reason);

  oss << cpu_state.FormatForPrompt() << "\n";

  oss << "## Disassembly (current instruction marked with >>>)\n```asm\n";
  for (const auto& line : surrounding_code) {
    oss << line.FormatForPrompt() << "\n";
  }
  oss << "```\n\n";

  if (!call_stack.empty()) {
    oss << "## Call Stack\n";
    for (size_t i = 0; i < call_stack.size(); ++i) {
      const auto& entry = call_stack[i];
      oss << absl::StrFormat("%zu. $%06X <- $%06X (%s) [%s]\n",
        i, entry.return_address, entry.call_site,
        entry.symbol_name.empty() ? "???" : entry.symbol_name,
        entry.is_long_call ? "JSL" : "JSR");
    }
    oss << "\n";
  }

  if (!relevant_memory.empty()) {
    oss << "## Relevant Memory Regions\n";
    for (const auto& [name, snapshot] : relevant_memory) {
      oss << absl::StrFormat("### %s ($%06X)\n```\n%s```\n\n",
        name, snapshot.start_address, snapshot.FormatHexDump());
    }
  }

  return oss.str();
}

std::string CrashAnalysisContext::BuildPromptContext() const {
  std::ostringstream oss;

  oss << "# Crash Analysis Context\n\n";
  oss << absl::StrFormat("**Crash Type**: %s\n\n", crash_type);

  oss << final_state.FormatForPrompt() << "\n";

  oss << "## Code at Crash\n```asm\n";
  for (const auto& line : code_at_crash) {
    oss << line.FormatForPrompt() << "\n";
  }
  oss << "```\n\n";

  if (!call_stack.empty()) {
    oss << "## Call Stack at Crash\n";
    for (size_t i = 0; i < call_stack.size(); ++i) {
      const auto& entry = call_stack[i];
      oss << absl::StrFormat("%zu. $%06X (%s)\n",
        i, entry.return_address,
        entry.symbol_name.empty() ? "unknown" : entry.symbol_name);
    }
    oss << "\n";
  }

  oss << "## Stack Memory\n```\n" << stack_memory.FormatHexDump() << "```\n\n";

  if (!potential_causes.empty()) {
    oss << "## Potential Causes (Pre-Analysis)\n";
    for (const auto& cause : potential_causes) {
      oss << "- " << cause << "\n";
    }
    oss << "\n";
  }

  return oss.str();
}

std::map<std::string, MemorySnapshot>
AsmDebugContextBuilder::GetALTTPRelevantMemory() {
  std::map<std::string, MemorySnapshot> regions;

  if (!memory_reader_) return regions;

  // Link's state
  {
    MemorySnapshot snapshot;
    snapshot.start_address = 0x7E0020;
    snapshot.region_name = "Link State";
    for (uint32_t i = 0; i < 64; ++i) {
      snapshot.data.push_back(memory_reader_(0x7E0020 + i));
    }
    regions["Link State"] = snapshot;
  }

  // Current sprite arrays (first sprite)
  {
    MemorySnapshot snapshot;
    snapshot.start_address = 0x7E0D00;
    snapshot.region_name = "Sprite 0 Data";
    // Sample key sprite addresses for sprite 0
    uint32_t sprite_addrs[] = {
      0x7E0D00, 0x7E0D10, 0x7E0D20, 0x7E0D30,  // Position
      0x7E0D40, 0x7E0D50,  // Velocity
      0x7E0DD0, 0x7E0E20, 0x7E0E50, 0x7E0E90, 0x7E0ED0  // State
    };
    for (uint32_t addr : sprite_addrs) {
      snapshot.data.push_back(memory_reader_(addr));
    }
    regions["Sprite 0 Data"] = snapshot;
  }

  // Game state
  {
    MemorySnapshot snapshot;
    snapshot.start_address = 0x7E0010;
    snapshot.region_name = "Game State";
    for (uint32_t i = 0; i < 16; ++i) {
      snapshot.data.push_back(memory_reader_(0x7E0010 + i));
    }
    regions["Game State"] = snapshot;
  }

  return regions;
}

}  // namespace ai
}  // namespace cli
}  // namespace yaze
```

---

## 3. Example Q&A Pairs

These examples can be used for few-shot prompting and as test cases for the system.

### 3.1 "Why is this sprite not showing?"

**User Query:**
```
I added a sprite to room 0x45 but it's not appearing. The sprite ID is 0x09 (green soldier) at position (0x48, 0x58). Here's the debug context:

[BREAKPOINT HIT at $068000]
CPU: A=$0000 X=$05 Y=$0F PC=$068000 P=$35 (M=1 X=1)
Sprite slot 5 state: $0DD0+5 = $00 (inactive)
```

**Expected AI Response:**
```json
{
  "text_response": "The sprite is not showing because its state byte ($0DD0,X) is $00, indicating the slot is inactive. This typically happens when:\n\n1. **Spriteset Mismatch**: Room 0x45's spriteset may not include sprite ID 0x09. Each room uses a specific spriteset (defined in the room header at byte 10), and only sprites in that set can spawn.\n\n2. **Sprite Limit Reached**: If all 16 sprite slots were full when the room loaded, this sprite couldn't spawn. Check $0DD0+0 through $0DD0+F for active sprites.\n\n3. **Spawn Coordinates Invalid**: Position (0x48, 0x58) in sprite data format means tile (4,5) with sub-position (8,8). Verify this is a valid floor tile, not solid.\n\n**Debugging Steps:**\n1. Set breakpoint at `$09:A300` (Sprite_SpawnDynamically) to see spawn attempts\n2. Check room header spriteset at `$028000 + (room_id * 14) + 10`\n3. Monitor $0DD0-$0DDF to see which slots are in use\n\n**Quick Fix:** If spriteset is wrong, either change the room's spriteset or use a sprite that exists in the current set.",
  "tool_calls": [
    {"tool_name": "read-memory", "args": {"address": "0x7E0DD0", "size": "16"}},
    {"tool_name": "get-room-header", "args": {"room_id": "0x45"}}
  ],
  "reasoning": "Sprite slot 5 shows $00 state (inactive). Need to verify spriteset compatibility and slot availability. Requesting memory reads to confirm diagnosis."
}
```

### 3.2 "What does this code do?"

**User Query:**
```
Can you explain what this routine does?

$068003: LDY #$0F
$068005: LDA $0DD0,Y
$068008: BEQ .inactive
$06800A: LDA $0E20,Y
$06800D: CMP #$D2
$06800F: BEQ .is_fish
.inactive:
$068011: DEY
$068012: BPL .next
```

**Expected AI Response:**
```json
{
  "text_response": "This routine searches through all 16 sprite slots looking for a specific sprite type (0xD2 = Fish).\n\n**Line-by-line analysis:**\n\n```asm\n$068003: LDY #$0F        ; Start with sprite slot 15 (last slot)\n$068005: LDA $0DD0,Y     ; Load sprite state for slot Y\n$068008: BEQ .inactive   ; If state=0 (dead), skip this slot\n$06800A: LDA $0E20,Y     ; Load sprite type/ID for slot Y\n$06800D: CMP #$D2        ; Is it sprite 0xD2 (Fish)?\n$06800F: BEQ .is_fish    ; Yes - branch to fish handling\n.inactive:\n$068011: DEY             ; Move to previous sprite slot\n$068012: BPL .next       ; Loop while Y >= 0 (checks slots 15 to 0)\n```\n\n**Purpose:** This is likely the `BottleMerchant_DetectFish` routine from the bottle merchant's AI. It searches for a live fish sprite near the merchant so he can offer to buy it.\n\n**Key Memory Addresses:**\n- `$0DD0,Y`: Sprite state array (0=inactive, $08=active)\n- `$0E20,Y`: Sprite type/ID array\n- Sprite 0xD2: Fish (catchable with bottle)\n\n**Pattern:** This is a standard 'find sprite by type' loop used throughout ALTTP's sprite code. The descending loop (15 to 0) is common because BPL naturally terminates when Y goes negative.",
  "reasoning": "Recognized this as a sprite search loop from Bank 06. The sprite ID 0xD2 and the loop pattern match BottleMerchant_DetectFish from usdasm documentation."
}
```

### 3.3 "How do I fix this crash?"

**User Query:**
```
My ROM hack crashes when entering room 0x7B. Here's the crash context:

[CRASH: INVALID_OPCODE at $06FF00]
CPU State: A=$4C00 X=$FF Y=$08 PC=$FF00 PB=$06 P=$34
Stack: $01F8: 12 80 06 00 A3 09 FF FF ...
Last instructions before crash:
$0681F0: JSR $8200
$0681F3: LDA $0D00,X
$0681F6: JSR $FF00  <- INVALID TARGET
```

**Expected AI Response:**
```json
{
  "text_response": "This crash is caused by **corrupted jump table data** or **index out of bounds**. The JSR to $FF00 is invalid - that address contains garbage, not code.\n\n**Root Cause Analysis:**\n\n1. **X Register = $FF**: This is suspicious. X should be a sprite index (0-15), but it's 255. This suggests:\n   - A loop underflowed (DEX when X was already 0)\n   - Corrupted sprite index from stack\n   - Missing bounds check before indexed JSR\n\n2. **Stack Analysis** ($01F8):\n   - `12 80 06` = Return to $06:8012 (likely MainSpriteLoop)\n   - `00 A3 09` = Return to $09:A300 (Sprite_SpawnDynamically)\n   - The call chain suggests a sprite spawn triggered this\n\n3. **The JSR $FF00**: Looking at $0681F6, this appears to be:\n   ```asm\n   JSR ($0000,X)  ; Jump table with X index\n   ```\n   With X=$FF, it reads the address from $00FF/$0100, which crosses into stack!\n\n**Fix Steps:**\n\n1. Add bounds checking before the jump table:\n   ```asm\n   CPX #$10        ; Check X < 16\n   BCS .bail_out   ; Branch if X >= 16\n   JSR ($0000,X)   ; Safe jump\n   ```\n\n2. Find where X gets corrupted - set watchpoint on X register changes\n\n3. Check room 0x7B sprite data for invalid sprite IDs that could cause index overflow\n\n**Breakpoints to Set:**\n- `$068000` (EXECUTE): Start of sprite bank\n- `$0DD0` (WRITE): Sprite state changes\n- `$0681F0` (EXECUTE): Just before the crash",
  "tool_calls": [
    {"tool_name": "get-disassembly", "args": {"address": "0x0681E0", "count": "20"}},
    {"tool_name": "read-memory", "args": {"address": "0x7E0100", "size": "32"}}
  ],
  "reasoning": "X=$FF is clearly invalid for sprite code. The JSR target $FF00 comes from reading wrong addresses. This is an index bounds issue, likely from DEX underflow or bad sprite data."
}
```

### 3.4 "Where is X stored in memory?"

**User Query:**
```
Where is Link's X position stored? I want to set a watchpoint to track when it changes.
```

**Expected AI Response:**
```json
{
  "text_response": "Link's X position is stored at **$7E0022** (2 bytes, little-endian).\n\n**Complete Position Variables:**\n\n| Address | Size | Description |\n|---------|------|-------------|\n| $7E0020 | 2 | Link Y position (low + high) |\n| $7E0022 | 2 | Link X position (low + high) |\n| $7E0024 | 2 | Link Y sub-pixel |\n| $7E0026 | 2 | Link X sub-pixel |\n\n**Watchpoint Recommendations:**\n\n```\n// Track all X position changes\nAddWatchpoint(0x7E0022, WRITE, 2 bytes)\n\n// Track sub-pixel for smooth movement analysis  \nAddWatchpoint(0x7E0026, WRITE, 2 bytes)\n```\n\n**Common Writers:**\n- `$07:F500` region: Link's movement code\n- `$07:F1A0` region: Collision response\n- `$07:F800` region: Knockback handling\n- `$02:A9C0` region: Screen transition\n\n**Usage in Code:**\n```asm\n; Reading Link's X\nLDA $22         ; Direct page alias (when DP=$0000)\nLDA $0022       ; Absolute addressing\nLDA $7E0022     ; Long addressing (cross-bank safe)\n\n; Common pattern in sprite AI\nLDA $22         ; Link X\nSEC\nSBC $0D10,X     ; Subtract sprite X\n; Result = distance from sprite to Link\n```\n\n**Note:** $22 and $0022 are the same location when Direct Page register is $0000 (default in ALTTP gameplay code).",
  "reasoning": "User wants to track Link's X position. Provided the canonical address, related addresses, and practical watchpoint advice with code examples."
}
```

### 3.5 Stepping Through Sprite AI

**User Query:**
```
I'm stepping through the green soldier AI. Currently at:

$1E8412: LDA $0ED0,X   ; A=$02 X=$05
$1E8415: ASL A
$1E8416: TAY
$1E8417: LDA .jump_table,Y
$1E841A: STA $00
$1E841C: LDA .jump_table+1,Y
$1E841F: STA $01
$1E8421: JMP ($0000)

What will happen next?
```

**Expected AI Response:**
```json
{
  "text_response": "This is a **state machine dispatch** for the green soldier AI. Let me trace the execution:\n\n**Current State:**\n- `$0ED0,X` (AI state) = $02\n- Sprite slot X = $05\n\n**Execution Trace:**\n\n```asm\n$1E8412: LDA $0ED0,X   ; A = $02 (state 2)\n$1E8415: ASL A          ; A = $04 (state * 2 for word index)\n$1E8416: TAY            ; Y = $04\n$1E8417: LDA .jump_table,Y    ; Load low byte of handler address\n$1E841A: STA $00        ; Store to DP $00\n$1E841C: LDA .jump_table+1,Y  ; Load high byte\n$1E841F: STA $01        ; Store to DP $01\n$1E8421: JMP ($0000)    ; Jump to handler for state 2\n```\n\n**What This Means:**\n- The soldier is in **AI state 2**\n- State 2 typically means: `Chasing Player` or `Moving Toward Target`\n- The code will jump to the 3rd entry (index 4) in the jump table\n\n**Green Soldier AI States (typical):**\n| State | Meaning |\n|-------|--------|\n| 0 | Initialize/spawn |\n| 1 | Idle/patrol |\n| 2 | Chase player |\n| 3 | Attack wind-up |\n| 4 | Attack/lunge |\n| 5 | Recoil/stunned |\n\n**To See the Target:**\n1. Read `.jump_table+4` and `.jump_table+5` to get the handler address\n2. Or set breakpoint at `$0000` after the STA instructions\n\n**Useful Follow-up:**\n```\nread-memory --address=$1E8440 --size=16  // Dump jump table\n```",
  "tool_calls": [
    {"tool_name": "read-memory", "args": {"address": "0x1E8440", "size": "16"}}
  ],
  "reasoning": "User is at a state machine dispatch. State 2 ($0ED0,X) means chase mode for soldiers. The ASL A converts state to word offset for the jump table lookup. Offering to read the jump table to show exact target."
}
```

---

## 4. Integration with prompt_builder.cc

### 4.1 New Methods for PromptBuilder Class

Add these methods to `src/cli/service/ai/prompt_builder.h`:

```cpp
class PromptBuilder {
 public:
  // ... existing methods ...

  // ASM Debug System Prompts
  std::string BuildAsmDebugSystemPrompt();
  std::string Build65816InstructionReference();
  std::string BuildALTTPMemoryMapReference();
  std::string BuildCommonBugPatternsReference();
  std::string BuildSpriteSystemReference();

  // Context-Aware Debug Prompts
  std::string BuildBreakpointHitPrompt(
      const ai::BreakpointHitContext& context);
  std::string BuildCrashAnalysisPrompt(
      const ai::CrashAnalysisContext& context);
  std::string BuildCodeExplanationPrompt(
      const std::vector<ai::DisassemblyLine>& code,
      const std::string& user_question);
  std::string BuildMemorySearchPrompt(
      const std::string& what_to_find);

  // Load external reference files
  absl::Status LoadAsmDebugReferences(const std::string& reference_dir);

 private:
  // Reference content storage
  std::string instruction_reference_;
  std::string memory_map_reference_;
  std::string bug_patterns_reference_;
  std::string sprite_system_reference_;
};
```

### 4.2 Implementation Skeleton

```cpp
// src/cli/service/ai/prompt_builder.cc additions

std::string PromptBuilder::BuildAsmDebugSystemPrompt() {
  std::ostringstream oss;

  oss << R"(You are an expert 65816 assembly debugger specializing in ALTTP (The Legend of Zelda: A Link to the Past) ROM hacking. You have comprehensive knowledge of:

- 65816 processor architecture, instruction set, and addressing modes
- SNES hardware: PPU, DMA, HDMA, memory mapping
- ALTTP game engine: sprite system, overworld/underworld, Link's state machine
- Common ROM hacking patterns and bug fixes

When analyzing code or debugging issues:
1. Always consider the M/X flag states for register widths
2. Check bank boundaries for cross-bank calls
3. Verify sprite indices are within 0-15 range
4. Watch for stack imbalances (push/pull pairs)
5. Consider ALTTP-specific memory layouts and routines

)";

  // Add references if loaded
  if (!instruction_reference_.empty()) {
    oss << instruction_reference_ << "\n\n";
  }
  if (!memory_map_reference_.empty()) {
    oss << memory_map_reference_ << "\n\n";
  }

  return oss.str();
}

std::string PromptBuilder::BuildBreakpointHitPrompt(
    const ai::BreakpointHitContext& context) {
  std::ostringstream oss;

  oss << BuildAsmDebugSystemPrompt();
  oss << "\n---\n\n";
  oss << context.BuildPromptContext();
  oss << "\n---\n\n";
  oss << R"(Based on this breakpoint hit, please:
1. Explain what the code at the current PC is doing
2. Describe the current program state and what led here
3. Identify any potential issues or bugs
4. Suggest next debugging steps (breakpoints, watchpoints, memory to inspect)

Respond in JSON format with text_response and optional tool_calls.)";

  return oss.str();
}

std::string PromptBuilder::BuildCrashAnalysisPrompt(
    const ai::CrashAnalysisContext& context) {
  std::ostringstream oss;

  oss << BuildAsmDebugSystemPrompt();
  oss << "\n" << BuildCommonBugPatternsReference();
  oss << "\n---\n\n";
  oss << context.BuildPromptContext();
  oss << "\n---\n\n";
  oss << R"(Analyze this crash and provide:
1. **Root Cause**: What caused the crash (be specific about the code path)
2. **Bug Pattern**: Which common bug pattern this matches
3. **Fix Suggestion**: Assembly code to fix the issue
4. **Prevention**: How to avoid this in the future

Respond in JSON format with detailed text_response.)";

  return oss.str();
}

std::string PromptBuilder::BuildCodeExplanationPrompt(
    const std::vector<ai::DisassemblyLine>& code,
    const std::string& user_question) {
  std::ostringstream oss;

  oss << BuildAsmDebugSystemPrompt();
  oss << "\n---\n\n## Code to Analyze\n```asm\n";

  for (const auto& line : code) {
    oss << line.FormatForPrompt() << "\n";
  }

  oss << "```\n\n";
  oss << "## User Question\n" << user_question << "\n\n";
  oss << R"(Provide a detailed explanation including:
1. Line-by-line analysis
2. Overall purpose of the routine
3. Relevant ALTTP context (what game feature this relates to)
4. Any potential issues or improvements

Respond in JSON format with text_response.)";

  return oss.str();
}
```

### 4.3 YAML Catalogue Additions

Add to `assets/agent/prompt_catalogue.yaml`:

```yaml
# ASM Debug Tools
tools:
  - name: get-disassembly
    description: Disassemble code at a given address
    arguments:
      - name: address
        description: 24-bit address to start disassembly (hex, e.g., 0x068000)
        required: true
        example: "0x068000"
      - name: count
        description: Number of instructions to disassemble
        required: false
        example: "20"
      - name: m_flag
        description: Accumulator size flag (true=8-bit, false=16-bit)
        required: false
        example: "true"
      - name: x_flag
        description: Index register size flag (true=8-bit, false=16-bit)
        required: false
        example: "true"
    usage_notes: Use the M/X flags from the current CPU state for accurate operand sizes

  - name: add-breakpoint
    description: Set a breakpoint at an address
    arguments:
      - name: address
        description: 24-bit address for breakpoint
        required: true
      - name: type
        description: Breakpoint type (EXECUTE, READ, WRITE)
        required: false
        example: "EXECUTE"
      - name: condition
        description: Optional condition expression
        required: false
        example: "X == 5"
    usage_notes: EXECUTE breaks when PC reaches address; READ/WRITE break on memory access

  - name: step-instruction
    description: Execute a single instruction
    arguments: []
    usage_notes: Returns new CPU state after stepping

  - name: step-over
    description: Execute instruction, treating JSR/JSL as single step
    arguments: []
    usage_notes: Useful for skipping subroutine internals

  - name: step-out
    description: Run until current subroutine returns
    arguments: []
    usage_notes: Only works if call stack is not empty

  - name: get-cpu-state
    description: Get current CPU register values and flags
    arguments: []
    usage_notes: Returns A, X, Y, PC, SP, P, DBR, PBR, DP

  - name: resolve-symbol
    description: Look up a symbol name to get its address
    arguments:
      - name: name
        description: Symbol name to look up
        required: true
        example: "Sprite_SpawnDynamically"
    usage_notes: Requires symbols to be loaded first with load-symbols

# ASM Debug Examples
examples:
  - user_prompt: "Why is my sprite not showing?"
    text_response: "Let me check the sprite's state and the room's spriteset configuration."
    tool_calls:
      - tool_name: read-memory
        args:
          address: "0x7E0DD0"
          size: "16"
    reasoning: "Sprite visibility issues are usually due to state=0 (inactive) or spriteset mismatch. Checking sprite state array first."

  - user_prompt: "What does the code at $068000 do?"
    tool_calls:
      - tool_name: get-disassembly
        args:
          address: "0x068000"
          count: "30"
    reasoning: "User wants code explanation. Disassembling 30 instructions to get full routine context."

  - user_prompt: "Set a breakpoint when Link's health changes"
    tool_calls:
      - tool_name: add-breakpoint
        args:
          address: "0x7E0069"
          type: "WRITE"
    text_response: "I've set a write breakpoint on $7E0069 (Link's health). The emulator will pause whenever Link's health value is modified."
    reasoning: "$7E0069 is Link's current health. A WRITE breakpoint will trigger when any code modifies it."

  - user_prompt: "Where is the sprite spawn routine?"
    tool_calls:
      - tool_name: resolve-symbol
        args:
          name: "Sprite_SpawnDynamically"
    text_response: "The main sprite spawn routine is Sprite_SpawnDynamically. Let me look up its address."
    reasoning: "Using symbol resolution to find the canonical spawn routine address from loaded symbols."
```

### 4.4 Asset Files to Create

Create these reference files in `assets/agent/`:

1. **`65816_instruction_reference.md`** - Full instruction set documentation
2. **`alttp_memory_map.md`** - Complete ALTTP memory reference
3. **`common_asm_bugs.md`** - Bug pattern library
4. **`sprite_system_reference.md`** - Sprite engine documentation

These should be loaded by `LoadAsmDebugReferences()` and cached for prompt injection.

---

## Summary

This prompt engineering system provides:

1. **Comprehensive System Prompts**: Base knowledge for 65816 architecture and ALTTP specifics
2. **Context Extraction**: C++ structures and builders to capture debugging state
3. **Example Q&A**: Training/testing pairs covering common debugging scenarios
4. **Integration Points**: Extensions to existing `prompt_builder.cc` infrastructure

The system is designed to:
- Inject relevant context automatically based on debugging situation
- Provide few-shot examples for each query type
- Support both conversational queries and tool-based interactions
- Scale from simple questions to complex crash analysis
