# Audio System Handoff & Status Report

**Date:** November 30, 2025
**Status:** Functional but Imperfect (Audio artifacts, speed/pitch accuracy issues)
**Context:** Integration of `MusicPlayer` (Audio-only mode) with `Emulator` (Full system)

## 1. Executive Summary

The audio system currently suffers from synchronization issues ("static/crackling", "fast playback") caused by drift between the emulated SNES clock (~32040 Hz) and the host audio device (48000 Hz). Recent attempts to implement Dynamic Rate Control (DRC) and fix Varispeed (playback speed) introduced regressions due to logic errors in rate calculation.

**Current Symptoms:**
*   **Static/Crackling:** Buffer underruns. The emulator isn't generating samples fast enough, or the host is consuming them too fast.
*   **Fast Playback:** At 1.0x speed, audio may drift faster than real-time to catch up with buffer underruns.
*   **Broken Varispeed:** At <1.0x speeds, audio is pitched down doubly (slower tempo + lower pitch) due to a math error in `RunAudioFrame`.

## 2. Technical Context

### 2.1. The "32040 Hz" Reality
*   **Nominal:** SNES APU documents often cite 32000 Hz.
*   **Actual:** Hardware measurements confirm the DSP output is ~32040 Hz.
*   **Implementation:** We updated `kNativeSampleRate` to `32040` in `emulator.cc`. This is correct and should remain.

### 2.2. Audio Pipeline
1.  **SPC700/DSP:** Generates 16-bit stereo samples at ~32040 Hz into a ring buffer (`dsp.cc`).
2.  **Emulator Loop:** `RunAudioFrame` (or `Run`) executes CPU/APU cycles until ~1 frame of time has passed.
3.  **Extraction:** `GetSampleCount` / `ReadRawSamples` drains the DSP ring buffer.
4.  **Resampling:** `SDL_AudioStream` (SDL2) handles 32040 -> 48000 Hz conversion.
5.  **Output:** `QueueSamples` pushes data to the OS driver.

### 2.3. The Logic Errors

#### A. Double-Applied Varispeed
In `Emulator::RunAudioFrame` (used by Music Editor):
```cpp
// ERROR: playback_speed_ is used twice!
// 1. To determine how much source data to generate (Correct for tempo)
int samples_to_generate = wanted_samples_ / playback_speed_; 

// 2. To determine the playback rate (Incorrect - Double Pitch Shift)
int effective_rate = kNativeSampleRate * playback_speed_; 
```
*   **Effect:** If speed is 0.5x:
    *   We generate 2x data (correct to fill time).
    *   We tell SDL "This data is 16020 Hz" (instead of 32040 Hz).
    *   SDL resamples 16k->48k (3x stretch) ON TOP of the 2x data generation.
    *   Result: 0.25x speed / pitch drop.

#### B. Flawed DRC
The current DRC implementation adjusts `effective_rate` based on buffer depth. While the *idea* is correct (buffer full -> play faster), it interacts poorly with the Varispeed bug above, leading to wild oscillations or "static" as it fights the double-speed factor.

## 3. Proposed Solutions

### Phase 1: The Quick Fix (Recommended First)
Correct the Varispeed math in `src/app/emu/emulator.cc`.

**Logic:**
*   **Source Generation:** Continue scaling `samples_to_generate` by `1/speed` (to fill the time buffer).
*   **Playback Rate:** The `effective_rate` sent to SDL should **ALWAYS** be `kNativeSampleRate` (32040), regardless of playback speed. We are stretching the *content*, not changing the *clock*.
    *   *Exception:* DRC adjustments (+/- 100 Hz) are applied to this 32040 base.

**Pseudocode Fix:**
```cpp
// Generate enough samples to fill the frame time at this speed
snes_.SetSamples(native_buffer, samples_available);

// BASE rate is always native. Speed change happens because we generated 
// MORE/LESS data for the same real-time interval.
int output_rate = kNativeSampleRate; 

// Apply subtle DRC only for synchronization
if (buffer_full) output_rate += 100;
if (buffer_empty) output_rate -= 100;

queue_samples(native_buffer, output_rate);
```

### Phase 2: Robust DRC (Mid-Term)
Implement a PID controller or smoothed average for the DRC adjustment instead of the current +/- 100 Hz "bang-bang" control, which causes pitch wobble.

### Phase 3: Callback-Driven Audio (Long-Term)
Switch from `SDL_QueueAudio` (Push) to `SDL_AudioCallback` (Pull).
*   **Mechanism:** SDL calls *us* when it needs data.
*   **Action:** We run the Emulator core *inside* the callback (or wait for a thread to produce it) until the buffer is full.
*   **Benefit:** Guaranteed synchronization with the audio clock. Impossible to have underruns if the emulation is fast enough.
*   **Cost:** Major refactor of the main loop.

## 4. Investigation References

### Key Files
*   `src/app/emu/emulator.cc`: Main audio loop, DRC logic, Varispeed math.
*   `src/app/emu/audio/dsp.cc`: Sample generation, interpolation (Gaussian).
*   `src/app/emu/audio/audio_backend.cc`: SDL2 stream management.

### External References
*   **bsnes/higan:** Uses "Dynamic Rate Control" (micro-resampling) to sync video (60.09Hz) and audio (32040Hz) to PC (60Hz/48000Hz).
*   **Snes9x:** Uses a similar buffer-based feedback loop.

## 5. Action Plan for Next Dev
1.  **Open `src/app/emu/emulator.cc`**.
2.  **Locate `RunAudioFrame` and `Run`**.
3.  **Fix Varispeed:** Change `int effective_rate = kNativeSampleRate * playback_speed_` to `int effective_rate = kNativeSampleRate`.
4.  **Retain DRC:** Keep the `if (queued > high) rate += delta` logic, but apply it to the fixed 32040 base.
5.  **Test:** Verify 1.0x speed is static-free, and 0.5x speed is actually half-speed, not quarter-speed.
