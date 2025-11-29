# Handoff: Dungeon Object Emulator Preview

**Date:** 2025-11-26
**Status:** Root Cause Identified - Emulator Mode Requires Redesign
**Priority:** Medium

## Summary

Implemented a dual-mode object preview system for the dungeon editor. The **Static mode** (ObjectDrawer-based) works and renders objects. The **Emulator mode** has been significantly improved with proper game state initialization based on expert analysis of ALTTP's drawing handlers.

## CRITICAL DISCOVERY: Handler Execution Root Cause (Session 2, Final)

**The emulator mode cannot work with cold-start execution.**

### Test Suite Investigation

A comprehensive test suite was created at `test/integration/emulator_object_preview_test.cc` to trace handler execution. The `TraceObject00Handler` test revealed the root cause:

```
[TEST] Object 0x00 handler: $8B89
[TRACE] Starting execution trace from $01:8B89
[   0] $01:8B89: 20 -> $00:8000 (A=$0000 X=$00D8 Y=$0020)
[   1] $00:8000: 78 [CPU_AUDIO] === ENTERED BANK $00 at PC=$8000 ===
```

**Finding:** Object 0x00's handler at `$8B89` immediately executes `JSR $8000`, which is the **game's RESET vector**. This runs full game initialization including:
- Hardware register setup
- APU initialization (the $00:8891 handshake loop)
- WRAM clearing
- NMI/interrupt setup

### Why Handlers Cannot Run in Isolation

ALTTP's object handlers are designed to run **within an already-running game context**:

1. **Shared Subroutines**: Handlers call common routines that assume game state is initialized
2. **Bank Switching**: Code frequently jumps between banks, requiring proper stack/return state
3. **Zero-Page Dependencies**: Dozens of zero-page variables must be pre-set by the game
4. **Interrupt Context**: Some operations depend on NMI/HDMA being active

### Implications

The current approach of "cold start emulation" (reset SNES → jump to handler) is fundamentally flawed for ALTTP. Object handlers are **not self-contained functions** - they're subroutines within a complex runtime environment.

### Recommended Future Approaches

1. **Save State Injection**: Load a save state from a running game, modify WRAM to set up object parameters, then execute handler
2. **Full Game Boot**: Run the game to a known "drawing ready" state (room loaded), then call handlers
3. **Static Mode**: Continue using ObjectDrawer for reliable rendering (current default)
4. **Hybrid Tracing**: Use emulator for debugging/analysis only, not rendering

---

## Recent Improvements (2025-11-26)

### CRITICAL FIX: SNES-to-PC Address Conversion (Session 2)
- **Issue:** All ROM addresses were SNES addresses (e.g., `$01:8000`) but code used them as PC file offsets
- **Root Cause:** ALTTP uses LoROM mapping where SNES addresses must be converted to PC offsets
- **Fix:** Added `SnesToPc()` helper function and converted all ROM address accesses
- **Conversion Formula:** `PC = (bank & 0x7F) * 0x8000 + (addr - 0x8000)`
- **Examples:**
  - `$01:8000` → PC `$8000` (handler table)
  - `$01:8200` → PC `$8200` (handler routine table)
  - `$0D:D308` → PC `$6D308` (sprite aux palette)
- **Result:** Correct handler addresses from ROM

### Tilemap Pointer Fix (Session 2, Update 2)
- **Issue:** Tilemap pointers read from ROM were garbage - they're NOT stored in ROM
- **Root Cause:** Game initializes these pointers dynamically at runtime, not from ROM data
- **Fix:** Manually initialize tilemap pointers to point to WRAM buffer rows
- **Pointers:** `$BF`, `$C2`, `$C5`, ... → `$7E2000`, `$7E2080`, `$7E2100`, ... (each row +$80)
- **Result:** Valid WRAM pointers for indirect long addressing (`STA [$BF],Y`)

### APU Mock Fix (Session 2, Update 3)
- **Issue:** APU handshake at `$00:8891` still hanging despite writing mock values
- **Root Cause:** APU I/O ports have **separate read/write latches**:
  - `Write($2140)` goes to `in_ports_[]` (CPU→SPC direction)
  - `Read($2140)` returns from `out_ports_[]` (SPC→CPU direction)
- **Fix:** Set `out_ports_[]` directly instead of using Write():
  ```cpp
  apu.out_ports_[0] = 0xAA;  // CPU reads $AA from $2140
  apu.out_ports_[1] = 0xBB;  // CPU reads $BB from $2141
  ```
- **Result:** APU handshake check passes, handler execution continues

### Palette Fix (Both Modes)
- **Issue:** Tiles specifying palette indices 6-7 showed magenta (out-of-bounds)
- **Fix:** Now loads sprite auxiliary palettes from ROM `$0D:D308` (PC: `$6D308`) into indices 90-119
- **Result:** Full 120-color palette support (palettes 0-7)

### Emulator Mode Fixes (Session 1)
Based on analysis from zelda3-hacking-expert and snes-emulator-expert agents:

1. **Zero-Page Tilemap Pointers** - Initialized $BF-$DD from `RoomData_TilemapPointers` at `$01:86F8`
2. **APU Mock** - Set `$2140-$2143` to "ready" values (`$AA`, `$BB`) to prevent infinite APU handshake loop at `$00:8891`
3. **Two-Table Handler Lookup** - Now uses both data offset table and handler address table
4. **Object Parameters** - Properly initializes zero-page variables ($04, $08, $B2, $B4, etc.)
5. **CPU State** - Correct register setup (X=data_offset, Y=tilemap_pos, PB=$01, DB=$7E)
6. **STP Trap** - Uses STP opcode at `$01:FF00` for reliable return detection

## What Was Built

### DungeonObjectEmulatorPreview Widget
Location: `src/app/gui/widgets/dungeon_object_emulator_preview.cc`

A preview tool that renders individual dungeon objects using two methods:

1. **Static Mode (Default, Working)**
   - Uses `zelda3::ObjectDrawer` to render objects
   - Same rendering path as the main dungeon canvas
   - Reliable and fast
   - Now supports full 120-color palette (palettes 0-7)

2. **Emulator Mode (Enhanced)**
   - Runs game's native drawing handlers via CPU emulation
   - Full room context initialization
   - Proper WRAM state setup
   - APU mock to prevent infinite loops

### Key Features
- Object ID input with hex display and name lookup
- Quick-select presets for common objects
- Object browser with all Type 1/2/3 objects
- Position (X/Y) and size controls
- Room ID for graphics/palette context
- Render mode toggle (Static vs Emulator)

## Technical Details

### Palette Handling (Updated)
- Dungeon main palette: 6 sub-palettes × 15 colors = 90 colors (indices 0-89)
- Sprite auxiliary palette: 2 sub-palettes × 15 colors = 30 colors (indices 90-119)
- Total: 120 colors (palettes 0-7)
- Source: Main from palette group, Aux from ROM `$0D:D308`

### Emulator State Initialization
```
1. Reset SNES, load room context
2. Load full 120-color palette into CGRAM
3. Convert 8BPP graphics to 4BPP planar, load to VRAM
4. Clear tilemap buffers ($7E:2000, $7E:4000)
5. Initialize zero-page tilemap pointers from $01:86F8
6. Mock APU I/O ($2140-$2143 = $AA/$BB)
7. Set object parameters in zero-page
8. Two-table handler lookup (data offset + handler address)
9. Setup CPU: X=data_offset, Y=tilemap_pos, PB=$01, DB=$7E
10. Push STP trap address, jump to handler
11. Execute until STP or timeout
12. Copy WRAM buffers to VRAM, render PPU
```

### Files Modified
- `src/app/gui/widgets/dungeon_object_emulator_preview.h` - Static rendering members
- `src/app/gui/widgets/dungeon_object_emulator_preview.cc` - All emulator fixes
- `src/app/editor/ui/right_panel_manager.cc` - Fixed deprecated ImGui flags

### Tests
- BPP Conversion Tests: 12/12 PASS
- Dungeon Object Rendering Tests: 8/8 PASS
- **Emulator State Injection Tests**: `test/integration/emulator_object_preview_test.cc`
  - LoROM Conversion Tests: Validates `SnesToPc()` formula
  - APU Mock Tests: Verifies `out_ports_[]` read behavior
  - Tilemap Pointer Setup Tests: Confirms WRAM pointer initialization
  - Handler Table Reading Tests: Validates two-table lookup
  - Handler Execution Trace Tests: Traces handler execution flow

## ROM Addresses Reference

**IMPORTANT:** ALTTP uses LoROM mapping. Always use `SnesToPc()` to convert SNES addresses to PC file offsets!

| SNES Address | PC Offset | Purpose |
|--------------|-----------|---------|
| `$01:8000` | `$8000` | Type 1 data offset table |
| `$01:8200` | `$8200` | Type 1 handler routine table |
| `$01:8370` | `$8370` | Type 2 data offset table |
| `$01:8470` | `$8470` | Type 2 handler routine table |
| `$01:84F0` | `$84F0` | Type 3 data offset table |
| `$01:85F0` | `$85F0` | Type 3 handler routine table |
| `$00:9B52` | `$1B52` | RoomDrawObjectData (tile definitions) |
| `$0D:D734` | `$6D734` | Dungeon main palettes (0-5) |
| `$0D:D308` | `$6D308` | Sprite auxiliary palettes (6-7) |
| `$7E:2000` | (WRAM) | BG1 tilemap buffer (8KB) |
| `$7E:4000` | (WRAM) | BG2 tilemap buffer (8KB) |

**Note:** Tilemap pointers at `$BF-$DD` are NOT in ROM - they're initialized dynamically to `$7E2000+` at runtime.

## Known Issues

### 1. ~~SNES-to-PC Address Conversion~~ (FIXED - Session 2)
~~ROM addresses were SNES addresses but used as PC offsets~~ - Fixed with corrected `SnesToPc()` helper.
- Formula: `PC = (bank & 0x7F) * 0x8000 + (addr - 0x8000)`

### 2. ~~Tilemap Pointers from ROM~~ (FIXED - Session 2)
~~Tried to read tilemap pointers from ROM at $01:86F8~~ - Pointers are NOT stored in ROM.
- Fixed by manually initializing pointers to WRAM buffer rows ($7E2000, $7E2080, etc.)

### 3. Emulator Mode Fundamentally Broken (ROOT CAUSE IDENTIFIED)

**Root Cause:** Object handlers are NOT self-contained. Test tracing revealed that handler `$8B89` (object 0x00) immediately calls `JSR $8000` - the game's RESET vector. This means:

- Handlers expect to run **within a fully initialized game**
- Cold-start emulation will **always** hit APU initialization at `$00:8891`
- The handler code shares subroutines with game initialization

**Test Evidence:**
```
[   0] $01:8B89: 20 -> $00:8000  (JSR to RESET vector)
[   1] $00:8000: 78              (SEI - start of game init)
```

**Current Status:** Emulator mode requires architectural redesign. See "Recommended Future Approaches" at top of document.

**Workaround:** Use static mode for reliable rendering (default).

### 4. BG Layer Transparency
The compositing uses `0xFF` as transparent marker, but edge cases with palette index 0 may exist.

## How to Test

### GUI Testing
```bash
# Build
cmake --build build --target yaze -j8

# Run with dungeon editor
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon

# Open Emulator Preview from View menu or right panel
# Test both Static and Emulator modes
# Try objects: Wall (0x01), Floor (0x80), Chest (0xF8)
```

### Emulator State Injection Tests
```bash
# Build tests
cmake --build build --target yaze_test_rom_dependent -j8

# Run with ROM path
YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc ./build/bin/Debug/yaze_test_rom_dependent \
  --gtest_filter="*EmulatorObjectPreviewTest*"

# Run specific test suites
YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc ./build/bin/Debug/yaze_test_rom_dependent \
  --gtest_filter="*EmulatorStateInjectionTest*"

YAZE_TEST_ROM_PATH=/path/to/zelda3.sfc ./build/bin/Debug/yaze_test_rom_dependent \
  --gtest_filter="*HandlerExecutionTraceTest*"
```

### Test Coverage
The test suite validates:
1. **LoROM Conversion** - `SnesToPc()` formula correctness
2. **APU Mock** - Proper `out_ports_[]` vs `in_ports_[]` behavior
3. **Handler Tables** - Two-table lookup for all object types
4. **Tilemap Pointers** - WRAM pointer initialization
5. **Execution Tracing** - Handler flow analysis (reveals root cause)

## Related Files

- `src/zelda3/dungeon/object_drawer.h` - ObjectDrawer class
- `src/app/gfx/render/background_buffer.h` - BackgroundBuffer for tile storage
- `src/zelda3/dungeon/room_object.h` - RoomObject data structure
- `docs/internal/architecture/dungeon-object-rendering-plan.md` - Overall rendering architecture
- `assets/asm/usdasm/bank_01.asm` - Handler disassembly reference
- `test/integration/emulator_object_preview_test.cc` - Emulator state injection test suite
