# MusicEditor 1.5x Audio Speed Bug - Handoff Document

**Date:** 2025-12-05
**Status:** Unresolved
**Priority:** High

## Problem Statement

The MusicEditor plays audio at approximately 1.5x speed. The exact ratio (48000/32040 = 1.498) indicates that **samples generated at 32040 Hz are being played at 48000 Hz without proper resampling**.

Additionally, there's a "first play" issue where clicking Play produces no audio the first time, but stopping and playing again works (at 1.5x speed).

## Audio Pipeline Overview

```
MusicPlayer::Update() [called at ~60 Hz]
    │
    ▼
Emulator::RunAudioFrame()
    │
    ├─► Snes::RunAudioFrame()
    │       │
    │       ├─► cpu_.RunOpcode() loop until vblank
    │       │       └─► RunCycle() → CatchUpApu() → apu_.RunCycles()
    │       │                           └─► DSP generates ~533 samples at 32040 Hz
    │       │
    │       └─► [At vblank] dsp.NewFrame() sets lastFrameBoundary
    │
    └─► snes_.SetSamples() → dsp.GetSamples()
            │
            └─► Reads ~533 samples from DSP ring buffer
                    │
                    ▼
audio_backend->QueueSamplesNative(samples, 533, 2, 32040)
    │
    ├─► SDL_AudioStreamPut(samples) at 32040 Hz
    │
    └─► SDL_AudioStreamGet(resampled) → SDL_QueueAudio()
            └─► Output at 48000 Hz (resampled by SDL)
```

## What Has Been Verified Working

### 1. APU Timing (VERIFIED CORRECT)
- APU runs at ~1,024,000 Hz (tests pass)
- DSP generates samples at ~32040 Hz (tests pass)
- ~533 samples generated per NTSC frame

### 2. SDL_AudioStream Resampling (VERIFIED CORRECT)
Diagnostic logs confirm correct resampling ratio:
```
QueueSamplesNative: In=2132 bytes (32040Hz) → Out=3192 bytes (48000Hz)
Resampling ratio: 1.497 (expected: 1.498)
```

### 3. Audio Backend Configuration (VERIFIED CORRECT)
- SDL audio device opens at 48000 Hz
- SDL_AudioStream created: 32040 Hz stereo → 48000 Hz stereo
- `audio_stream_enabled_ = true` confirmed in logs

### 4. Shared Audio Backend (IMPLEMENTED)
- MusicPlayer's `audio_emulator_` now uses external backend from main emulator
- `Emulator::RunAudioFrame()` uses `audio_backend()` accessor (not direct member)
- Single SDL device shared between main emulator and MusicPlayer

## What Has Been Tried and Ruled Out

### 1. Duplicate NewFrame() Calls - REMOVED
Preview methods had explicit `dsp.NewFrame()` calls that conflicted with the internal call in `RunAudioFrame()`. These were removed but didn't fix the issue.

### 2. Audio Backend Member vs Accessor - FIXED
`Emulator::RunAudioFrame()` was using `audio_backend_` directly instead of `audio_backend()` accessor. When external backend was set, `audio_backend_` was null, so no audio was queued. Fixed to use accessor.

### 3. Two SDL Audio Devices - FIXED
Main emulator and MusicPlayer were creating separate SDL audio devices. Implemented `SetExternalAudioBackend()` to share a single device. Verified in logs that same device ID is used.

### 4. Initialization Order - VERIFIED CORRECT
- `SetSharedAudioBackend()` called in `MusicEditor::Initialize()`
- `EnsureAudioReady()` sets external backend before `EnsureInitialized()`
- Resampling configured before playback starts

### 5. First Play Silence - PARTIALLY UNDERSTOOD
Logs show the device is already "playing" with stale audio from main emulator when MusicPlayer starts. The exclusivity callback sets `running=false` on main emulator, but this may not immediately stop audio generation.

## Current Code State

### Key Files Modified
- `src/app/emu/emulator.h` - Added `SetExternalAudioBackend()`, `audio_backend()` accessor
- `src/app/emu/emulator.cc` - `RunAudioFrame()` and `ResetFrameTiming()` use accessor
- `src/app/editor/music/music_player.h` - Added `SetSharedAudioBackend()`
- `src/app/editor/music/music_player.cc` - Uses shared backend, removed duplicate NewFrame() calls
- `src/app/editor/music/music_editor.cc` - Shares main emulator's backend with MusicPlayer
- `src/app/emu/audio/audio_backend.cc` - Added diagnostic logging

### Diagnostic Logging Added
- `QueueSamplesNative()` logs input/output byte counts and resampling ratio
- `GetStatus()` logs device ID and queue state
- `Clear()` logs device ID and queue before/after
- `Play()` logs device status transitions
- `RunAudioFrame()` logs which backend is being used (external vs owned)

## Remaining Hypotheses

### 1. SDL_AudioStream Not Actually Being Used
**Theory:** Despite logs showing resampling, audio might be taking a different path.
**Investigation:** Add logging at every audio queue call site to trace actual execution path.

### 2. Frame Timing Issue
**Theory:** `MusicPlayer::Update()` might not be called at the expected rate, or `RunAudioFrame()` might be called multiple times per frame.
**Investigation:** Add frame timing logs to verify Update() is called at ~60 Hz and RunAudioFrame() once per call.

### 3. DSP Sample Extraction Bug
**Theory:** `dsp.GetSamples()` might return wrong number of samples or from wrong position.
**Investigation:** Log actual sample counts returned by GetSamples() vs expected (~533).

### 4. Main Emulator Still Generating Audio
**Theory:** Even with `running=false`, main emulator's Update() might still be called and generating audio.
**Investigation:** Add logging to main emulator's audio generation path to verify it stops when MusicPlayer is active.

### 5. Audio Stream Bypass Path
**Theory:** There might be a code path that calls `QueueSamples()` (direct, non-resampled) instead of `QueueSamplesNative()`.
**Investigation:** Search for all `QueueSamples` calls and verify none are being hit during music playback.

### 6. Resampling Disabled Mid-Playback
**Theory:** `audio_stream_config_dirty_` or another flag might disable resampling during playback.
**Investigation:** Add logging to `SetAudioStreamResampling()` to catch any disable calls.

## Suggested Next Steps

1. **Add comprehensive tracing** to follow a single frame of audio from DSP generation through to SDL queue
2. **Verify frame timing** - confirm Update() runs at expected rate
3. **Check for bypass paths** - ensure all audio goes through QueueSamplesNative()
4. **Monitor resampling state** - ensure it stays enabled throughout playback
5. **Test with simpler case** - generate known test tone and verify output rate

## Test Commands

```bash
# Build with debug
cmake --preset mac-dbg && cmake --build build --target yaze -j8

# Run with logging visible
./build/bin/Debug/yaze.app/Contents/MacOS/yaze 2>&1 | grep -E "(AudioBackend|MusicPlayer|Emulator)"

# Run audio timing tests
ctest --test-dir build -R "apu_timing|dsp_sample" -V
```

## Key Constants

| Value | Meaning |
|-------|---------|
| 32040 Hz | Native SNES DSP sample rate |
| 48000 Hz | SDL audio device sample rate |
| 1.498 | Correct resampling ratio (48000/32040) |
| 533 | Samples per NTSC frame at 32040 Hz |
| ~60.0988 Hz | NTSC frame rate |
| 1,024,000 Hz | APU clock rate |

## Files to Investigate

| File | Relevance |
|------|-----------|
| `src/app/editor/music/music_player.cc` | Main playback logic, Update() loop |
| `src/app/emu/emulator.cc` | RunAudioFrame(), audio queuing |
| `src/app/emu/audio/audio_backend.cc` | SDL audio, resampling |
| `src/app/emu/audio/dsp.cc` | Sample generation, GetSamples() |
| `src/app/emu/snes.cc` | RunAudioFrame(), SetSamples() |

## Contact

Previous investigation done by Claude Code agents. See git history for detailed changes.
