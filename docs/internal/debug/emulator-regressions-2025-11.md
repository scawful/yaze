# Emulator Regressions - November 2025

## Status: UNRESOLVED

Two regressions have been identified in the SNES emulator that affect:
1. Input handling (A button not working on file naming screen)
2. PPU rendering (title screen BG layer not showing)

**Note**: Keybindings system is currently being modified by another agent. Changes may interact.

---

## Issue 1: Input Button Mapping Bug

### Symptoms
- A button does not work on the ALTTP file naming screen
- D-pad works correctly
- A button works on title screen (different code path?)

### Root Cause Analysis

**Bug Location**: `src/app/emu/snes.cc:763`

```cpp
void Snes::SetButtonState(int player, int button, bool pressed) {
  // BUG: This logic is inverted!
  Input* input = (player == 1) ? &input1 : &input2;
  // ...
}
```

When calling `SetButtonState(0, button, true)` (player 0 = player 1 in SNES terms), it incorrectly selects `input2` instead of `input1`.

**Introduced in**: Commit `9ffb7803f5` (Oct 11, 2025)
- "refactor(emulator): enhance input handling and audio resampling features"

### Attempted Fix

In this session, we updated the button constants in `save_state_manager.h` from bitmasks to bit indices:
```cpp
// Before (incorrect for SetButtonState API):
constexpr uint16_t kA = 0x0080;  // Bitmask

// After (correct bit index):
constexpr int kA = 8;  // Bit index
```

However, this fix alone doesn't resolve the issue because `SetButtonState` itself has the player mapping inverted.

### Proposed Fix

Change line 763 in `snes.cc`:
```cpp
// Current (wrong):
Input* input = (player == 1) ? &input1 : &input2;

// Should be:
Input* input = (player == 0) ? &input1 : &input2;
```

Or alternatively, to match common conventions (player 1 = first player):
```cpp
Input* input = (player <= 1) ? &input1 : &input2;
```

### Additional Notes

The title screen may work because it uses a different input reading path or auto-joypad read timing that happens to work despite the bug.

---

## Issue 2: PPU Title Screen BG Layer Not Rendering

### Symptoms
- Title screen background layer(s) not showing
- Timing unclear - may have been introduced in recent commits

### Potential Root Cause

**Commit**: `e37497e9ef` (Nov 23, 2025)
- "feat(emu): add PPU JIT catch-up for mid-scanline raster effects"

This commit refactored PPU rendering from a simple `RunLine()` call to a progressive JIT system:

```cpp
// Old approach:
ppu_.RunLine(line);  // Render entire line at once

// New approach:
ppu_.StartLine(line);  // Setup for line
ppu_.CatchUp(512);     // Render first half
ppu_.CatchUp(1104);    // Render second half
```

### Key Changes to Investigate

1. **StartLine() timing**: Now called at H=0 instead of H=512
   - `StartLine()` does sprite evaluation and mode 7 setup
   - May need to be called earlier or with different conditions

2. **CatchUp() vs RunLine()**: The new progressive rendering may have edge cases
   - `CatchUp(512)` renders pixels 0-127
   - `CatchUp(1104)` should render pixels 128-255
   - But 1104/4 = 276, so it tries to render up to 256 (clamped)

3. **WriteBBus PPU catch-up**: Added mid-scanline PPU register write handling
   - May interfere with normal rendering sequence

### Files Changed in PPU Refactor

- `src/app/emu/video/ppu.cc`: Added `StartLine()`, `CatchUp()`, `last_rendered_x_`
- `src/app/emu/video/ppu.h`: Added new method declarations
- `src/app/emu/snes.cc`: Changed `RunLine()` calls to `StartLine()`/`CatchUp()`

### Key Timing Difference

**Before PPU JIT (commit e37497e9ef~1)**:
```cpp
case 512: {
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.RunLine(memory_.v_pos());  // Everything at H=512
}
```

**After PPU JIT**:
```cpp
case 16: {
  ppu_.StartLine(memory_.v_pos());  // Sprite eval at H=16
}
case 512: {
  ppu_.CatchUp(512);  // Pixels 0-127 at H=512
}
case 1104: {
  ppu_.CatchUp(1104);  // Pixels 128-255 at H=1104
}
```

The sprite evaluation (`EvaluateSprites`) now happens at H=16 instead of H=512. This timing change could affect games that modify OAM or PPU registers via HDMA between H=16 and H=512.

### Quick Test: Revert to Old PPU Timing

To test if the PPU JIT is causing the issue, temporarily revert to `RunLine()`:

In `src/app/emu/snes.cc`, change the case 16 and 512 blocks:

```cpp
case 16: {
  next_horiz_event = 512;
  if (memory_.v_pos() == 0)
    memory_.init_hdma_request();
  // Remove StartLine call
} break;
case 512: {
  next_horiz_event = 1104;
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.RunLine(memory_.v_pos());  // Back to old method
} break;
case 1104: {
  // Remove CatchUp call
  if (!in_vblank_)
    memory_.run_hdma_request();
  // ... rest unchanged
```

### Debugging Steps

1. Add logging to PPU to verify:
   - Is `StartLine()` being called for each visible scanline?
   - Is `CatchUp()` rendering all 256 pixels?
   - Are any BG enable flags being cleared unexpectedly?

2. Test reverting PPU changes:
   ```bash
   git checkout e37497e9ef~1 -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h src/app/emu/snes.cc
   ```

3. Compare title screen behavior before and after commit `e37497e9ef`

---

## Git History Reference

### Key Commits (Chronological)

| Date | Commit | Description |
|------|--------|-------------|
| Oct 11, 2025 | `9ffb7803f5` | Input handling refactor - introduced player mapping bug |
| Nov 23, 2025 | `e37497e9ef` | PPU JIT catch-up - potential BG rendering regression |
| Nov 25, 2025 | `9d788fe6b0` | Lazy SNES init - may affect startup timing |
| Nov 26, 2025 | (this session) | SaveStateManager button constant fix |

### Commands to Investigate

```bash
# View input handling changes
git show 9ffb7803f5 -- src/app/emu/snes.cc

# View PPU changes
git show e37497e9ef -- src/app/emu/video/ppu.cc src/app/emu/snes.cc

# Diff current vs before PPU JIT
git diff e37497e9ef~1..HEAD -- src/app/emu/video/ppu.cc

# Test with old PPU code
git stash
git checkout e37497e9ef~1 -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h
cmake --build build --target yaze
# Test emulator, then restore:
git checkout HEAD -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h
git stash pop
```

---

## Attempted Fixes (Did Not Resolve)

### Session 2025-11-26

1. **Button constants fix** (`save_state_manager.h`)
   - Changed from bitmasks to bit indices
   - Status: Applied, did not fix input issue

2. **SetButtonState player mapping** (`snes.cc:763`)
   - Changed `player == 1` to `player <= 1`
   - Status: Applied, did not fix input issue

3. **PPU JIT revert** (`snes.cc`)
   - Reverted StartLine/CatchUp back to RunLine
   - Status: Applied, did not fix BG layer issue

## Investigation Session 2025-11-26 (New Findings)

### Input Bug Analysis

**SetButtonState is now correct** (`snes.cc:750`):
```cpp
Input* input = (player <= 1) ? &input1 : &input2;
```

**Debug logging already exists** in HandleInput():
- Logs when A button is active in `current_state_`
- Logs `port_auto_read_[0]` value after auto-joypad read

**CRITICAL SUSPECT: ImGui WantTextInput blocking**

In `src/app/emu/input/sdl3_input_backend.cc:67-73`:
```cpp
if (io.WantTextInput) {
  static int text_input_log_count = 0;
  if (text_input_log_count++ < 5) {
    LOG_DEBUG("InputBackend", "Blocking game input - WantTextInput=true");
  }
  return ControllerState{};  // <-- ALL input blocked!
}
```

If ANY ImGui text input widget is active, ALL game input is blocked. This could explain:
- Why D-pad works but A doesn't → unlikely, would block both
- Why title screen works but naming screen doesn't → possible if yaze UI has text field active

**Diagnostic**: Check if "Blocking game input - WantTextInput=true" appears in logs when on naming screen.

### PPU Bug Analysis

**CRITICAL FINDING: "Revert" was incomplete**

Current `snes.cc:214` calls `ppu_.RunLine()`:
```cpp
case 512: {
  next_horiz_event = 1104;
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.RunLine(memory_.v_pos());  // Looks like old code
}
```

BUT `RunLine()` in `ppu.cc:174-178` now calls the JIT mechanism:
```cpp
void Ppu::RunLine(int line) {
  // Legacy wrapper - renders the whole line at once
  StartLine(line);      // <-- Uses new JIT setup
  CatchUp(2000);        // <-- Uses new JIT rendering
}
```

**Original `RunLine()` was a direct loop** (before e37497e9ef):
```cpp
void Ppu::RunLine(int line) {
  obj_pixel_buffer_.fill(0);
  if (!forced_blank_) EvaluateSprites(line - 1);
  if (mode == 7) CalculateMode7Starts(line);
  for (int x = 0; x < 256; x++) {
    HandlePixel(x, line);  // Direct loop, no JIT state
  }
}
```

**Key Difference**:
- Old: Uses `line` parameter directly in `HandlePixel(x, line)`
- New: Uses member variable `current_scanline_` set by `StartLine()`

**Potential Bug**: If `current_scanline_` or `last_rendered_x_` have stale/incorrect values, rendering breaks.

**TRUE REVERT Required**: To test if JIT is the cause, must restore the original `ppu.cc` implementation, not just the `snes.cc` call sites.

---

## Investigation Session 2025-11-27 (snes-emulator-expert)

### PPU State Check (current dirty tree)
- `ppu.cc` has already been changed back to the legacy full-line renderer inside `RunLine()` (StartLine/CatchUp still exist but are unused). The earlier suspicion that the wrapper itself was blanking the BG no longer applies.
- `snes.cc` only calls `RunLine()` once per scanline at H=512; there are no remaining PPU catch-up hooks in `WriteBBus`, so the JIT path is effectively dead code right now.

### Runtime Observation (yaze_emu_trace.log)
- Headless run shows the CPU stuck in the SPC handshake loop at `$00:88B6` (`CMP.w APUIO0` / `BNE .wait_for_zero`), with NMIs never enabled in the first 120 frames.
- If the SPC handshake never completes, the game never uploads title-screen VRAM/CGRAM or enables 212C/212D, so the blank BG may be a fallout of stalled boot rather than a renderer defect.

### Next Steps (PPU-focused)
- First, confirm the SPC handshake completes (APUIO0 transitions off zero) so the game can reach module `0x01`; otherwise any PPU checks are moot.
- After the handshake, instrument `RunLine` (e.g., when `line==100`) to log `forced_blank_`, `mode`, and `layer_[i].mainScreenEnabled` to ensure BGs are actually enabled on the title frame.
- If layers are enabled but BG still missing, capture VRAM around the title tilemap upload to ensure DMA is populating the expected addresses.

---

## Updated Next Steps

### Priority 1: Input Bug
- [ ] Check logs for "Blocking game input - WantTextInput=true" message
- [ ] Verify if any ImGui InputText widget is active during emulation
- [ ] Test with `WantTextInput` check temporarily removed
- [ ] Trace: SDL key state → Poll() → SetButtonState() → HandleInput()

### Priority 2: PPU Bug
- [ ] **TRUE revert test**: Restore original `ppu.cc` from `e37497e9ef~1`
  ```bash
  git show e37497e9ef~1:src/app/emu/video/ppu.cc > /tmp/old_ppu.cc
  # Compare and apply the old RunLine() implementation
  ```
- [ ] Add logging to verify `current_scanline_` and `last_rendered_x_` values
- [ ] Check layer enable flags (`layer_[i].mainScreenEnabled`) during title screen
- [ ] Verify VRAM contains tile data

### Priority 3: General
- [ ] **Git bisect** to find exact commit where emulator last worked
- [ ] Coordinate with keybindings agent work

## Potentially Relevant Commits

| Commit | Date | Description |
|--------|------|-------------|
| `0579fc2c65` | Earlier | Implement input management system with SDL2 |
| `9ffb7803f5` | Oct 11 | Enhance input handling (introduced SetButtonState) |
| `2f0006ac0b` | Later | SDL compatibility layer |
| `a5dc884612` | Later | SDL3 backend infrastructure |
| `e37497e9ef` | Nov 23 | PPU JIT catch-up (reverted) |
