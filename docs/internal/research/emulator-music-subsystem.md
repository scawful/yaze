# Emulator Music Subsystem Research - Audit & Verification

## 1. Naming Screen Logic Audit

**Finding:** The Naming Screen is **Module 0x04** (`Module04_NameFile`), located in `usdasm/bank_0C.asm`.
*   **Correction:** The initial hypothesis of Module 0x0E was incorrect (0x0E is the Interface module).
*   **Input Mechanism:** The Naming Screen does **not** read hardware registers (`$4218`) directly. It reads **WRAM variables** updated by the NMI handler:
    *   `$F0` (`JOY1A_ALL`): Current state of controller 1.
    *   `$F4` (`JOY1A_NEW`): New button presses (Edge Detected).
    *   `$F6` (`JOY1B_NEW`): New button presses for controller 2? (Usually filtered input).
*   **Edge Detection Source:** The NMI handler (`NMI_ReadJoypads` in `bank_00.asm`) performs the edge detection:
    ```asm
    LDA $4218       ; Read Auto-Joypad register
    STA $F0         ; Store current state
    EOR $F2         ; XOR with previous state
    AND $F0         ; AND with current state -> only bits that went 0->1 remain
    STA $F4         ; Store as "NEW" buttons
    LDA $F0
    STA $F2         ; Update previous state
    ```
*   **Implication for Emulator Issue:**
    *   If the 'A' button isn't working, it means `$F4` bit 7 (A button) isn't being set.
    *   This happens if `$F0` (current) and `$F2` (previous) are identical when `NMI_ReadJoypads` runs.
    *   If the emulator runs multiple frames in a catch-up loop, and `InputManager::Poll` reports the same "Pressed" state for all of them:
        *   Frame 1: Current=Pressed, Previous=Released -> NEW=Pressed (Correct)
        *   Frame 2: Current=Pressed, Previous=Pressed -> NEW=0 (Correct for "held", but user might expect repeat?)
    *   **The Issue:** If the game expects to see a "Released" frame to register a *subsequent* press (e.g., typing "A" then "A"), and the emulator's input polling misses the physical release (because it's polling at 60Hz but the user released and pressed faster, or due to frame skipping), the game sees continuous "Pressed".
    *   **Critical:** If the Naming Screen relies on `JOY1A_NEW` (`$F4`) for the *initial* selection, and that works, but fails for *subsequent* presses of the same character, it confirms the "Released" state is being missed.

## 2. Audio/SPC Emulation Audit

**Finding:**
*   **kNativeSampleRate:**
    *   `src/app/emu/emulator.cc`: `constexpr int kNativeSampleRate = 32040;`
    *   `src/app/editor/music/music_player.cc`: In `EnsureAudioReady`, it defines `constexpr int kNativeSampleRate = 32000;` locally!
    *   **Discrepancy:** This 40Hz difference (0.125%) is small but indicates inconsistency. If `QueueSamplesNative` is called with 32000 but the backend expects 32040 (or vice versa), it might cause subtle drift or resampling artifacting, though unlikely to cause the massive 1.5x speedup on its own.
*   **1.5x Speedup Confirmation:**
    *   48000 Hz (Host) / 32040 Hz (Emulator) = **1.498**
    *   The math confirms that playing 32k samples at 48k results in exactly the reported speedup.
*   **HMAGIC Reference:**
    *   `assets/hmagic/AudioLogic.c` uses `ws_freq = 22050`. This suggests the C-based tracker (hmagic) was optimized for lower quality/performance or older systems, and is *not* a 1:1 reference for the high-fidelity SNES emulation in `yaze`. It explains why "Music Only Mode" (which seemingly uses the `MusicPlayer` / `Emulator` core) behaves differently than the lightweight hmagic tracker.

## 3. Recommendations Update

1.  **Fix `kNativeSampleRate` Inconsistency:**
    *   Update `src/app/editor/music/music_player.cc` to use `32040` or include `emulator.h`'s constant to match `src/app/emu/emulator.cc`.

2.  **Fix Input Polling for Naming Screen:**
    *   The Naming Screen depends on `NMI_ReadJoypads` detecting a 0->1 transition.
    *   If the user taps 'A' quickly, or if `turbo_mode` is used, the "Released" state must be visible to `NMI_ReadJoypads` for at least one frame.
    *   **Action:** Verify `InputManager::Poll` logic. If it blindly polls SDL state, ensure it's not "sticky".
    *   **Action:** Ensure `auto_joy_timer_` is emulated correctly. If `NMI_ReadJoypads` reads `$4218` while the transfer is still "happening" (timer > 0), it might get garbage or old values. The emulator currently returns the *new* value immediately.

3.  **Resampling Fix:**
    *   As previously identified, the fallback in `Emulator::RunFrameOnly` that queues 32k samples to a 48k backend must be removed.
