# SDL3 Audio Backend Implementation

**Date**: 2025-11-23
**Author**: snes-emulator-expert agent
**Status**: Implementation Complete

## Overview

This document describes the SDL3 audio backend implementation for the YAZE SNES emulator. The SDL3 backend provides a modern, stream-based audio interface that replaces the SDL2 queue-based approach.

## Architecture

### Key Components

1. **SDL3AudioBackend Class** (`src/app/emu/audio/sdl3_audio_backend.h/.cc`)
   - Implements the `IAudioBackend` interface
   - Uses SDL3's stream-based audio API
   - Provides volume control, resampling, and playback management

2. **SDL Compatibility Layer** (`src/app/platform/sdl_compat.h`)
   - Provides cross-version compatibility macros
   - Abstracts differences between SDL2 and SDL3 APIs
   - Enables conditional compilation based on `YAZE_USE_SDL3`

3. **Factory Integration** (`src/app/emu/audio/audio_backend.cc`)
   - Updated `AudioBackendFactory::Create()` to support SDL3
   - Conditional compilation ensures SDL3 backend only available when built with SDL3

## SDL3 Audio API Changes

### Major Differences from SDL2

| SDL2 API | SDL3 API | Purpose |
|----------|----------|---------|
| `SDL_OpenAudioDevice()` | `SDL_OpenAudioDeviceStream()` | Device initialization |
| `SDL_QueueAudio()` | `SDL_PutAudioStreamData()` | Queue audio samples |
| `SDL_GetQueuedAudioSize()` | `SDL_GetAudioStreamQueued()` | Get queued data size |
| `SDL_ClearQueuedAudio()` | `SDL_ClearAudioStream()` | Clear audio buffer |
| `SDL_PauseAudioDevice(id, 0/1)` | `SDL_ResumeAudioDevice()` / `SDL_PauseAudioDevice()` | Control playback |
| `SDL_GetAudioDeviceStatus()` | `SDL_IsAudioDevicePaused()` | Check playback state |

### Stream-Based Architecture

SDL3 introduces `SDL_AudioStream` as the primary interface for audio:

```cpp
// Create stream with device
SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,  // Use default device
    &spec,                               // Desired format
    nullptr,                             // No callback
    nullptr                              // No user data
);

// Queue audio data
SDL_PutAudioStreamData(stream, samples, size_in_bytes);

// Get device from stream
SDL_AudioDeviceID device = SDL_GetAudioStreamDevice(stream);

// Control playback through device
SDL_ResumeAudioDevice(device);
SDL_PauseAudioDevice(device);
```

## Implementation Details

### Initialization

The `Initialize()` method:
1. Creates an audio stream using `SDL_OpenAudioDeviceStream()`
2. Extracts the device ID from the stream
3. Queries actual device format (may differ from requested)
4. Starts playback immediately with `SDL_ResumeAudioDevice()`

### Audio Data Flow

```
Application → QueueSamples() → Volume Scaling → SDL_PutAudioStreamData() → SDL3 → Audio Device
```

### Volume Control

Volume is applied during sample queueing:
- Fast path: When volume = 1.0, samples pass through unchanged
- Slow path: Samples are scaled by volume factor with clamping

### Resampling Support

The backend supports native rate resampling for SPC700 emulation:

1. **Setup**: Create separate resampling stream with `SDL_CreateAudioStream()`
2. **Input**: Native rate samples (e.g., 32kHz from SPC700)
3. **Process**: SDL3 handles resampling internally
4. **Output**: Resampled data at device rate (e.g., 48kHz)

### Thread Safety

- Volume control uses `std::atomic<float>` for thread-safe access
- Initialization state tracked with `std::atomic<bool>`
- SDL3 handles internal thread safety for audio streams

## Build Configuration

### CMake Integration

The SDL3 backend is conditionally compiled based on the `YAZE_USE_SDL3` flag:

```cmake
# In src/CMakeLists.txt
if(YAZE_USE_SDL3)
  list(APPEND YAZE_APP_EMU_SRC app/emu/audio/sdl3_audio_backend.cc)
endif()
```

### Compilation Flags

- Define `YAZE_USE_SDL3` to enable SDL3 support
- Include paths must contain SDL3 headers
- Link against SDL3 library (not SDL2)

## Testing

### Unit Tests

Located in `test/unit/sdl3_audio_backend_test.cc`:
- Basic initialization and shutdown
- Volume control
- Sample queueing (int16 and float)
- Playback control (play/pause/stop)
- Queue clearing
- Resampling support
- Double initialization handling

### Integration Testing

To test the SDL3 audio backend in the emulator:

1. Build with SDL3 support:
   ```bash
   cmake -DYAZE_USE_SDL3=ON ..
   make
   ```

2. Run the emulator with a ROM:
   ```bash
   ./yaze --rom_file=zelda3.sfc
   ```

3. Verify audio playback in the emulator

## Performance Considerations

### Optimizations

1. **Volume Scaling Fast Path**
   - Skip processing when volume = 1.0 (common case)
   - Use thread-local buffers to avoid allocations

2. **Buffer Management**
   - Reuse buffers for resampling operations
   - Pre-allocate based on expected sizes

3. **Minimal Locking**
   - Rely on SDL3's internal thread safety
   - Use lock-free atomics for shared state

### Latency

SDL3's stream-based approach can provide lower latency than SDL2's queue:
- Smaller buffer sizes possible
- More direct path to audio hardware
- Better synchronization with video

## Known Issues and Limitations

1. **Platform Support**
   - SDL3 is newer and may not be available on all platforms
   - Fallback to SDL2 backend when SDL3 unavailable

2. **API Stability**
   - SDL3 API may still evolve
   - Monitor SDL3 releases for breaking changes

3. **Device Enumeration**
   - Current implementation uses default device only
   - Could be extended to support device selection

## Future Enhancements

1. **Device Selection**
   - Add support for choosing specific audio devices
   - Implement device change notifications

2. **Advanced Resampling**
   - Expose resampling quality settings
   - Support for multiple resampling streams

3. **Spatial Audio**
   - Leverage SDL3's potential spatial audio capabilities
   - Support for surround sound configurations

4. **Performance Monitoring**
   - Add metrics for buffer underruns
   - Track actual vs requested latency

## Migration from SDL2

To migrate from SDL2 to SDL3 backend:

1. Install SDL3 development libraries
2. Set `YAZE_USE_SDL3=ON` in CMake
3. Rebuild the project
4. Audio backend factory automatically selects SDL3

No code changes required in the emulator - the `IAudioBackend` interface abstracts the differences.

## References

- [SDL3 Migration Guide](https://wiki.libsdl.org/SDL3/README-migration)
- [SDL3 Audio API Documentation](https://wiki.libsdl.org/SDL3/CategoryAudio)
- [SDL_AudioStream Documentation](https://wiki.libsdl.org/SDL3/SDL_AudioStream)