# ALTTP Naming Screen Input Debug Log

## Problem Statement
On the ALTTP naming screen:
- **D-pad works** - cursor moves correctly
- **A and B buttons do NOT work** - cannot select letters or delete

## What We've Confirmed Working

### 1. SDL Input Polling ✓
- `SDL_PumpEvents()` and `SDL_GetKeyboardState()` correctly detect keypresses
- Keyboard input is captured and converted to button state
- Logs show: `SDL2 Poll: buttons=0x0100 (keyboard detected)` when A pressed

### 2. Internal Button State (`current_state_`) ✓
- `SetButtonState()` correctly sets bits in `input1.current_state_`
- A button = bit 8 (0x0100), B button = bit 0 (0x0001)
- State changes are logged and verified

### 3. Per-Frame Input Polling ✓
- Fixed: `Poll()` now called before each `snes_.RunFrame()` (not just once per GUI frame)
- This ensures fresh keyboard state for each SNES frame
- Critical for edge detection when multiple SNES frames run per GUI update

### 4. HandleInput() / Auto-Joypad Read ✓
- `HandleInput()` is called at VBlank when `auto_joy_read_` is enabled
- `port_auto_read_[]` is correctly populated via serial read simulation
- Logs confirm state changes:
  ```
  HandleInput #909: current_state CHANGED 0x0000 -> 0x0100
  HandleInput #909 RESULT: port_auto_read CHANGED 0x0000 -> 0x0080
  HandleInput #912: current_state CHANGED 0x0100 -> 0x0000
  HandleInput #912 RESULT: port_auto_read CHANGED 0x0080 -> 0x0000
  ```

### 5. Button Serialization ✓
- Internal bit 8 (A) correctly maps to port_auto_read bit 7 (0x0080)
- This matches SNES hardware: A is bit 7 of $4218 (JOY1L)
- Verified mappings:
  - A (0x0100) → port_auto_read 0x0080 ✓
  - B (0x0001) → port_auto_read 0x8000 ✓
  - Start (0x0008) → port_auto_read 0x1000 ✓
  - Down (0x0020) → port_auto_read 0x0400 ✓

### 6. Register Reads ($4218/$4219) ✓
- Game reads both registers in NMI handler at PC=$00:83D7 and $00:83DC
- $4218 returns low byte of port_auto_read (contains A, X, L, R)
- $4219 returns high byte of port_auto_read (contains B, Y, Select, Start, D-pad)
- Logs confirm: `Game read $4218 = $80` when A pressed

### 7. Edge Transitions Exist ✓
- port_auto_read transitions: 0x0000 → 0x0080 → 0x0000
- The hardware-level "edge" (button press/release) IS being created
- Game should see: $4218 = 0x00, then 0x80, then 0x00

## ALTTP Input System (from usdasm analysis)

### Memory Layout
| Address | Name | Source | Contents |
|---------|------|--------|----------|
| $F0 | cur_hi | $4219 | B, Y, Select, Start, U, D, L, R |
| $F2 | cur_lo | $4218 | A, X, L, R, 0, 0, 0, 0 |
| $F4 | new_hi | edge($F0) | Newly pressed from high byte |
| $F6 | new_lo | edge($F2) | Newly pressed from low byte |
| $F8 | prv_hi | prev $F0 | Previous frame high byte |
| $FA | prv_lo | prev $F2 | Previous frame low byte |

### Edge Detection Formula (NMI_ReadJoypads at $00:83D1)
```asm
; For low byte (contains A button):
LDA $4218       ; Read current
STA $F2         ; Store current
EOR $FA         ; XOR with previous (bits that changed)
AND $F2         ; AND with current (only newly pressed)
STA $F6         ; Store newly pressed
STY $FA         ; Update previous
```

### Key Difference: D-pad vs Face Buttons
- **D-pad**: Uses `$F0` (CURRENT state) - no edge detection needed
  ```asm
  LDA.b $F0      ; Load current high byte
  AND.b #$0F     ; Mask D-pad bits
  ```
- **A/B buttons**: Uses `$F6` (NEWLY PRESSED) - requires edge detection
  ```asm
  LDA.b $F6      ; Load newly pressed low byte
  AND.b #$C0    ; Mask A ($80) and X ($40)
  BNE .select   ; Branch if newly pressed
  ```

**This explains why D-pad works but A/B don't** - D-pad bypasses edge detection!

## Current Hypothesis

The edge detection computation in the game's RAM is failing. Specifically:
- $F2 gets correct value (0x80 when A pressed)
- $F6 should get 0x80 on the first frame A is pressed
- But $F6 might be staying 0x00

### Possible Causes
1. **$FA (previous) already has A bit set** - Would cause XOR to cancel out
2. **CPU emulation bug** - EOR or AND instruction not working correctly
3. **RAM write issue** - Values not being stored correctly
4. **Timing issue** - Previous frame's value not being saved properly

## Debug Logging Added

### 1. HandleInput State Changes
```cpp
if (input1.current_state_ != last_current) {
  LOG_DEBUG("HandleInput #%d: current_state CHANGED 0x%04X -> 0x%04X", ...);
}
if (port_auto_read_[0] != last_port) {
  LOG_DEBUG("HandleInput #%d RESULT: port_auto_read CHANGED 0x%04X -> 0x%04X", ...);
}
```

### 2. RAM Writes to Joypad Variables
```cpp
// Log writes to $F2, $F6, $FA when A bit is set
if (adr == 0x00F2 || adr == 0x00F6 || adr == 0x00FA) {
  if (val & 0x80) {  // A button bit
    LOG_DEBUG("RAM WRITE %s = $%02X (A bit SET)", ...);
  }
}
```

## Key Findings (Nov 26, 2025)

### Discovery: Two Separate Joypad RAM Areas

ALTTP maintains TWO sets of joypad RAM:

| Address Range | Written By | PC Range | Value | Status |
|--------------|------------|----------|-------|--------|
| $01F0-$01FA | Game loop code | $8141/$8144 | $81 (correct!) | **WORKS** |
| $00F0-$00FA | NMI_ReadJoypads | $83E2 | $00 (wrong!) | **BROKEN** |

The naming screen reads from the $00Fx range (via Direct Page addressing with D=$0000),
which always has $00, instead of the $01Fx range which has correct values.

### Register Analysis at Write Points

| PC | Address | Value | A reg | X reg | Y reg | Analysis |
|----|---------|-------|-------|-------|-------|----------|
| $83E2 | $00F2 | $00 | $0200 | $0000 | $0000 | A.low = $00, matches! |
| $8141 | $01F2 | $81 | $0000 | $0000 | $002E | None match $81 (?!) |
| $8144 | $01F2 | $81 | $0200 | $0000 | $0000 | None match $81 (?!) |

### Root Cause Hypothesis

The NMI_ReadJoypads routine at PC=$83D7 reads $4218 into A register, but by the time
it stores at PC=$83E2, the A register contains $00xx instead of $80xx.

Possible causes:
1. **Instruction sequence bug**: Something clears A between $83D7 and $83E2
2. **Auto-joypad timing**: $4218 returns $00 because auto-read hasn't completed
3. **Mode flag issue**: 8/16-bit mode switching affecting register contents

### Sequence Per Frame

1. Game loop ($8141/$8144) writes $81 to $01F2 (correct!)
2. NMI handler ($83E2) writes $00 to $00F2 (wrong!)

The game loop runs FIRST, then NMI fires and overwrites with wrong data.

## Next Steps

1. **Trace instruction sequence** from $83D7 to $83E2 to find what clears A
2. **Check auto-joypad timing** - is the read happening before auto-read completes?
3. **Compare with other emulators** - how does bsnes/snes9x handle this?
4. **Check $4212 HVBJOY status** - is the game checking if auto-read is done?

## Filter Commands

```bash
# Show HandleInput state changes
grep -E "HandleInput.*CHANGED"

# Show RAM writes to joypad variables
grep -E "RAM WRITE"

# Combined
grep -E "RAM WRITE|HandleInput.*CHANGED"
```

## Files Modified for Debugging

- `src/app/emu/snes.cc` - HandleInput logging, RAM write logging
- `src/app/emu/emulator.cc` - Per-frame Poll() calls
- `src/app/emu/ui/emulator_ui.cc` - Virtual controller debug display
