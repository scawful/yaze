# Emulator Regression Trace

Tracking git history to find root cause of title screen BG being black.

## Issue Description
- **Symptom**: Title screen background is black (after sword comes down)
- **Working**: Triforce animation, Nintendo logo, cutscene after title screen, file select
- **Broken**: Title screen BG layer specifically

---

## Commit Analysis

### Commit: e37497e9ef - "feat(emu): add PPU JIT catch-up for mid-scanline raster effects"
**Date**: Sun Nov 23 00:40:58 2025
**Author**: scawful + Claude

This is the commit that introduced the JIT progressive rendering system.

#### Changes Made

**ppu.h additions:**
```cpp
void StartLine(int line);
void CatchUp(int h_pos);
// New members:
int last_rendered_x_ = 0;
int current_scanline_;  // (implicit, used in CatchUp)
```

**ppu.cc changes:**
```cpp
// OLD RunLine - rendered entire line at once:
void Ppu::RunLine(int line) {
  obj_pixel_buffer_.fill(0);
  if (!forced_blank_) EvaluateSprites(line - 1);
  if (mode == 7) CalculateMode7Starts(line);
  for (int x = 0; x < 256; x++) {
    HandlePixel(x, line);
  }
}

// NEW - Split into StartLine + CatchUp:
void Ppu::StartLine(int line) {
  current_scanline_ = line;
  last_rendered_x_ = 0;
  obj_pixel_buffer_.fill(0);
  if (!forced_blank_) EvaluateSprites(line - 1);
  if (mode == 7) CalculateMode7Starts(line);
}

void Ppu::CatchUp(int h_pos) {
  int target_x = h_pos / 4;  // 1 pixel = 4 master cycles
  if (target_x > 256) target_x = 256;
  if (target_x <= last_rendered_x_) return;

  for (int x = last_rendered_x_; x < target_x; x++) {
    HandlePixel(x, current_scanline_);
  }
  last_rendered_x_ = target_x;
}

void Ppu::RunLine(int line) {
  // Legacy wrapper
  StartLine(line);
  CatchUp(2000);  // Force full line render
}
```

**snes.cc changes:**
```cpp
// Timing calls in RunCycle():
case 16: {  // Was: case 0 in some versions
  // ... init_hdma_request ...
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.StartLine(memory_.v_pos());  // NEW: Initialize scanline
}
case 512: {
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.CatchUp(512);  // CHANGED: Was ppu_.RunLine(memory_.v_pos())
}
case 1104: {
  if (!in_vblank_ && memory_.v_pos() > 0)
    ppu_.CatchUp(1104);  // NEW: Finish line
  // Then run HDMA...
}

// WriteBBus addition:
void Snes::WriteBBus(uint8_t adr, uint8_t val) {
  if (adr < 0x40) {
    // NEW: Catch up before PPU register write for mid-scanline effects
    if (!in_vblank_ && memory_.v_pos() > 0 && memory_.h_pos() < 1100) {
      ppu_.CatchUp(memory_.h_pos());
    }
    ppu_.Write(adr, val);
    return;
  }
  // ...
}
```

#### Potential Issues Identified

1. **`current_scanline_` not declared in ppu.h** - The variable is used but may not be properly declared as a member. Need to verify.

2. **h_pos timing at case 16 vs case 0** - The code shows `case 16:` but original may have been different. Need to verify StartLine is called at correct time.

3. **CatchUp(512) only renders pixels 0-127** - With `target_x = 512/4 = 128`, this only renders half the line. The second `CatchUp(1104)` renders `1104/4 = 276` → clamped to 256, so pixels 128-255.

4. **WriteBBus CatchUp may cause double-rendering** - If HDMA writes to PPU registers, CatchUp is called, but then CatchUp(1104) is also called. This should be fine since `last_rendered_x_` prevents re-rendering.

5. **HDMA runs AFTER CatchUp(1104)** - HDMA modifications to PPU registers happen after the scanline is fully rendered. This is correct for HDMA (affects next line), but need to verify.

---

### Commit: 9d788fe6b0 - "perf: Implement lazy SNES emulator initialization..."
**Date**: Tue Nov 25 14:58:52 2025

**Changes to emulator:**
- Only changed `Snes::Init` signature from `std::vector<uint8_t>&` to `const std::vector<uint8_t>&`
- No changes to PPU or rendering logic

**Verdict**: NOT RELATED to rendering bug.

---

### Commit: a0ab5a5eee - "perf(wasm): optimize emulator performance and audio system"
**Date**: Tue Nov 25 19:02:21 2025

**Changes to emulator:**
- emulator.cc: Frame timing and progressive frame skip (no PPU changes)
- wasm_audio.cc: AudioWorklet implementation (no PPU changes)

**Verdict**: NOT RELATED to rendering bug.

---

## Commits Between Pre-JIT and Now

Only 2 commits modified PPU/snes.cc:
1. `e37497e9ef` - JIT introduction (SUSPECT)
2. `9d788fe6b0` - Init signature change (NOT RELATED)

---

## Hypothesis

The JIT commit `e37497e9ef` is the root cause. Possible bugs:

### Theory 1: `current_scanline_` initialization issue
If `current_scanline_` is not properly initialized or declared, `HandlePixel(x, current_scanline_)` could be passing garbage values.

**To verify**: Check if `current_scanline_` is declared in ppu.h

### Theory 2: h_pos timing mismatch
The title screen may rely on specific timing that the JIT system breaks. If PPU registers are read/written at different h_pos values than expected, rendering could be affected.

### Theory 3: WriteBBus CatchUp interference with HDMA
The title screen uses HDMA for wavy cloud scroll. If the WriteBBus CatchUp is being called for HDMA writes and causing state issues, BG rendering could be affected.

**HDMA flow:**
1. h_pos=1104: CatchUp(1104) renders pixels 128-255
2. h_pos=1104: run_hdma_request() executes HDMA
3. HDMA writes to scroll registers via WriteBBus
4. WriteBBus calls CatchUp(h_pos) - but line is already fully rendered!

This should be harmless since `last_rendered_x_ = 256` means CatchUp returns early. But need to verify.

### Theory 4: Title screen uses unusual PPU setup
The title screen may have a specific PPU configuration that triggers a bug in the JIT system that other screens don't trigger.

---

## Verification Results

### Theory 1: VERIFIED - `current_scanline_` is declared
Found at ppu.h:335: `int current_scanline_ = 0;` - properly declared and initialized.

### Theory 2: Timing Analysis
- h_pos=16: StartLine(v_pos) called, resets `last_rendered_x_=0`
- h_pos=512: CatchUp(512) renders pixels 0-127
- h_pos=1104: CatchUp(1104) renders pixels 128-255, then HDMA runs

Math check:
- 512/4 = 128 pixels (0-127)
- 1104/4 = 276 → clamped to 256 (128-255)
- Total: 256 pixels ✓

### Theory 3: WriteBBus CatchUp Analysis
```cpp
if (!in_vblank_ && memory_.v_pos() > 0 && memory_.h_pos() < 1100) {
  ppu_.CatchUp(memory_.h_pos());
}
```
- HDMA runs at h_pos=1104, which is > 1100, so NO catchup for HDMA writes ✓
- This is correct - HDMA changes affect next scanline

### New Theory 5: HandleFrameStart doesn't reset JIT state
`HandleFrameStart()` doesn't reset `last_rendered_x_` or `current_scanline_`. These are only reset in `StartLine()` which is called for scanlines 1-224.

**Potential issue**: If WriteBBus CatchUp is called during vblank or on scanline 0, `current_scanline_` might have stale value from previous frame.

Check: WriteBBus condition is `!in_vblank_ && memory_.v_pos() > 0`, so this should be safe.

### New Theory 6: Title screen specific PPU configuration
The title screen might use a specific PPU configuration that triggers a rendering bug. Need to compare $212C (layer enables), $2105 (mode), $210B-$210C (tile addresses) between title screen and working screens.

---

## Next Steps

1. ✅ Verify `current_scanline_` is properly declared - DONE, it's fine
2. **Test pre-JIT commit** to confirm title screen BG works without JIT
3. **Add targeted debug logging** for title screen (module 0x01) PPU state
4. **Compare PPU register state** between title screen and cutscene
5. **Check if forced_blank is stuck** during title screen BG rendering

---

## Conclusion

**Root Cause Commit**: `e37497e9ef` - "feat(emu): add PPU JIT catch-up for mid-scanline raster effects"

This is the ONLY commit that changed PPU rendering logic. The bug must be in this commit.

**What the JIT system changed:**
1. Split `RunLine()` into `StartLine()` + `CatchUp()`
2. Added progressive pixel rendering based on h_pos
3. Added WriteBBus CatchUp to handle mid-scanline PPU register writes

**Why title screen specifically might be affected:**
The title screen uses HDMA for the wavy cloud scroll effect on BG1. While basic HDMA timing appears correct (runs after CatchUp(1104)), there may be a subtle timing or state issue that affects only certain screen configurations.

**Recommended debugging approach:**
1. Test pre-JIT to confirm title screen works: `git checkout e37497e9ef~1 -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h src/app/emu/snes.cc`
2. If confirmed, binary search within the JIT changes to isolate the specific bug
3. Add logging to compare PPU state ($212C, $2105, etc.) between title screen and working screens

---

## Testing Commands

```bash
# Checkout pre-JIT to test (CONFIRMS if JIT is the culprit)
git checkout e37497e9ef~1 -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h src/app/emu/snes.cc

# Restore JIT version
git checkout HEAD -- src/app/emu/video/ppu.cc src/app/emu/video/ppu.h src/app/emu/snes.cc

# Test with just WriteBBus CatchUp removed (isolate that change)
# Edit snes.cc WriteBBus to comment out the CatchUp call

# Test with RunLine instead of StartLine/CatchUp (but keep WriteBBus CatchUp)
# Would require manual code changes
```

---

## Status

- [x] Identified root cause commit: `e37497e9ef`
- [x] Verified no other commits changed PPU rendering
- [x] Documented JIT system changes
- [ ] **PENDING**: Test pre-JIT to confirm
- [ ] **PENDING**: Isolate specific bug in JIT implementation
