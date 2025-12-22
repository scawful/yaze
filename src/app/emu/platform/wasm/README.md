# WASM Platform-Specific Code

This directory contains WebAssembly/Emscripten-specific implementations for the yaze emulator.

## Files

### Audio Backend (`wasm_audio.h` / `wasm_audio.cc`)

The WASM audio backend provides WebAudio API integration for playing SNES audio in web browsers.

#### Features
- WebAudio context management with browser autoplay policy handling
- ScriptProcessorNode-based audio playback (compatible with all browsers)
- Sample format conversion (16-bit PCM to Float32)
- Volume control via GainNode
- Audio queue management with buffer limiting
- Automatic context suspension/resumption

#### Technical Details
- **Sample Rate**: Configurable, defaults to browser's native rate
- **Input Format**: 16-bit stereo PCM (SNES native format)
- **Output Format**: Float32 (WebAudio requirement)
- **Buffer Management**: Queue-based with automatic overflow protection
- **Latency**: Interactive mode for lower latency

#### Browser Compatibility
- Handles browser autoplay restrictions
- Call `HandleUserInteraction()` on user events to resume suspended contexts
- Check `IsContextSuspended()` to determine if user interaction is needed

#### Usage
```cpp
// Create backend via factory
auto backend = AudioBackendFactory::Create(BackendType::WASM);

// Or create directly
WasmAudioBackend audio;

// Initialize with config
AudioConfig config;
config.sample_rate = 48000;
config.channels = 2;
config.buffer_frames = 2048;
audio.Initialize(config);

// Queue audio samples
int16_t samples[4096];
audio.QueueSamples(samples, 4096);

// Start playback
audio.Play();

// Handle browser autoplay policy
if (audio.IsContextSuspended()) {
    // On user click/interaction:
    audio.HandleUserInteraction();
}
```

#### Build Configuration
The WASM audio backend is automatically included when building for Emscripten:
```cmake
if(EMSCRIPTEN)
  list(APPEND YAZE_APP_EMU_SRC app/emu/platform/wasm/wasm_audio.cc)
endif()
```

#### Implementation Notes
- Uses `EM_JS` macros for JavaScript integration
- `clang-format off` is used to prevent formatting issues with `EM_JS` blocks
- Audio context and processor are managed via global JavaScript objects
- Sample data is copied from WASM memory to JavaScript arrays

#### Future Improvements
- AudioWorklet support (when browser support is more widespread)
- Dynamic resampling for native SNES rate (32kHz)
- Latency optimization
- WebAudio analyzer node for visualization