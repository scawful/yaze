# SDL3 Migration Plan

**Version**: 0.4.0 Target
**Author**: imgui-frontend-engineer agent
**Date**: 2025-11-23
**Status**: Planning Phase

## Executive Summary

This document outlines the migration strategy from SDL2 (v2.30.0) to SDL3 for the YAZE project. SDL3 was released as stable in January 2025 and brings significant architectural improvements, particularly in audio handling and event processing. The YAZE codebase is well-positioned for this migration due to existing abstraction layers for audio, input, and rendering.

## Current SDL2 Usage Inventory

### Core Application Files

| Category | Files | SDL2 APIs Used |
|----------|-------|----------------|
| **Window Management** | `src/app/platform/window.h`, `window.cc` | `SDL_Window`, `SDL_CreateWindow`, `SDL_DestroyWindow`, `SDL_GetCurrentDisplayMode`, `SDL_PollEvent`, `SDL_GetMouseState`, `SDL_GetModState` |
| **Main Controller** | `src/app/controller.h`, `controller.cc` | `SDL_Delay`, `SDL_WINDOW_RESIZABLE` |
| **Timing** | `src/app/platform/timing.h` | `SDL_GetPerformanceCounter`, `SDL_GetPerformanceFrequency` |

### Graphics Subsystem

| Category | Files | SDL2 APIs Used |
|----------|-------|----------------|
| **Renderer Interface** | `src/app/gfx/backend/irenderer.h` | `SDL_Window*`, `SDL_Rect`, `SDL_Color` |
| **SDL2 Renderer** | `src/app/gfx/backend/sdl2_renderer.h`, `sdl2_renderer.cc` | `SDL_Renderer`, `SDL_CreateRenderer`, `SDL_CreateTexture`, `SDL_UpdateTexture`, `SDL_RenderCopy`, `SDL_RenderPresent`, `SDL_RenderClear`, `SDL_SetRenderTarget`, `SDL_LockTexture`, `SDL_UnlockTexture` |
| **Bitmap** | `src/app/gfx/core/bitmap.h`, `bitmap.cc` | `SDL_Surface`, `SDL_CreateRGBSurfaceWithFormat`, `SDL_FreeSurface`, `SDL_SetSurfacePalette`, `SDL_DEFINE_PIXELFORMAT` |
| **Palette** | `src/app/gfx/types/snes_palette.cc` | `SDL_Color` |
| **Resource Arena** | `src/app/gfx/resource/arena.cc` | `SDL_Surface`, texture management |
| **Utilities** | `src/util/sdl_deleter.h` | `SDL_DestroyWindow`, `SDL_DestroyRenderer`, `SDL_FreeSurface`, `SDL_DestroyTexture` |

### Emulator Subsystem

| Category | Files | SDL2 APIs Used |
|----------|-------|----------------|
| **Audio Backend** | `src/app/emu/audio/audio_backend.h`, `audio_backend.cc` | `SDL_AudioSpec`, `SDL_OpenAudioDevice`, `SDL_CloseAudioDevice`, `SDL_PauseAudioDevice`, `SDL_QueueAudio`, `SDL_ClearQueuedAudio`, `SDL_GetQueuedAudioSize`, `SDL_GetAudioDeviceStatus`, `SDL_AudioStream`, `SDL_NewAudioStream`, `SDL_AudioStreamPut`, `SDL_AudioStreamGet`, `SDL_FreeAudioStream` |
| **Input Backend** | `src/app/emu/input/input_backend.h`, `input_backend.cc` | `SDL_GetKeyboardState`, `SDL_GetScancodeFromKey`, `SDLK_*` keycodes, `SDL_Event`, `SDL_KEYDOWN`, `SDL_KEYUP` |
| **Input Handler UI** | `src/app/emu/ui/input_handler.cc` | `SDL_GetKeyName`, `SDL_PollEvent` |
| **Standalone Emulator** | `src/app/emu/emu.cc` | Full SDL2 initialization, window, renderer, audio, events |

### ImGui Integration

| Category | Files | Notes |
|----------|-------|-------|
| **Platform Backend** | `ext/imgui/backends/imgui_impl_sdl2.cpp`, `imgui_impl_sdl2.h` | Used for platform/input integration |
| **Renderer Backend** | `ext/imgui/backends/imgui_impl_sdlrenderer2.cpp`, `imgui_impl_sdlrenderer2.h` | Used for rendering |
| **SDL3 Backends (Available)** | `ext/imgui/backends/imgui_impl_sdl3.cpp`, `imgui_impl_sdl3.h`, `imgui_impl_sdlrenderer3.cpp`, `imgui_impl_sdlrenderer3.h` | Ready to use |

### Test Files

| Files | Notes |
|-------|-------|
| `test/yaze_test.cc` | SDL initialization for tests |
| `test/test_editor.cc` | SDL window for editor tests |
| `test/integration/editor/editor_integration_test.cc` | Integration tests with SDL |

## SDL3 Breaking Changes Affecting YAZE

### Critical Changes (Must Address)

#### 1. Audio API Overhaul
**SDL2 Code**:
```cpp
SDL_AudioSpec want, have;
want.callback = nullptr;  // Queue-based
device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
SDL_QueueAudio(device_id_, samples, size);
SDL_PauseAudioDevice(device_id_, 0);
```

**SDL3 Equivalent**:
```cpp
SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, 48000 };
SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
    SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
SDL_PutAudioStreamData(stream, samples, size);
SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
```

**Impact**: `SDL2AudioBackend` class needs complete rewrite. The existing `IAudioBackend` interface isolates this change.

#### 2. Window Event Restructuring
**SDL2 Code**:
```cpp
case SDL_WINDOWEVENT:
    switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE: ...
        case SDL_WINDOWEVENT_RESIZED: ...
    }
```

**SDL3 Equivalent**:
```cpp
case SDL_EVENT_WINDOW_CLOSE_REQUESTED: ...
case SDL_EVENT_WINDOW_RESIZED: ...
```

**Impact**: `window.cc` HandleEvents() needs event type updates.

#### 3. Keyboard Event Changes
**SDL2 Code**:
```cpp
event.key.keysym.sym  // SDL_Keycode
SDL_GetKeyboardState(nullptr)  // Returns Uint8*
```

**SDL3 Equivalent**:
```cpp
event.key.key  // SDL_Keycode (keysym removed)
SDL_GetKeyboardState(nullptr)  // Returns bool*
```

**Impact**: `SDL2InputBackend` keyboard handling needs updates.

#### 4. Surface Format Changes
**SDL2 Code**:
```cpp
surface->format->BitsPerPixel
```

**SDL3 Equivalent**:
```cpp
SDL_GetPixelFormatDetails(surface->format)->bits_per_pixel
```

**Impact**: `Bitmap` class surface handling needs updates.

### Moderate Changes

#### 5. Event Type Renaming
| SDL2 | SDL3 |
|------|------|
| `SDL_KEYDOWN` | `SDL_EVENT_KEY_DOWN` |
| `SDL_KEYUP` | `SDL_EVENT_KEY_UP` |
| `SDL_MOUSEMOTION` | `SDL_EVENT_MOUSE_MOTION` |
| `SDL_MOUSEWHEEL` | `SDL_EVENT_MOUSE_WHEEL` |
| `SDL_DROPFILE` | `SDL_EVENT_DROP_FILE` |
| `SDL_QUIT` | `SDL_EVENT_QUIT` |

#### 6. Function Renames
| SDL2 | SDL3 |
|------|------|
| `SDL_GetTicks()` | `SDL_GetTicks()` (now returns Uint64) |
| `SDL_GetTicks64()` | Removed (use `SDL_GetTicks()`) |
| N/A | `SDL_GetTicksNS()` (new, nanoseconds) |

#### 7. Audio Device Functions
| SDL2 | SDL3 |
|------|------|
| `SDL_OpenAudioDevice()` | `SDL_OpenAudioDeviceStream()` |
| `SDL_QueueAudio()` | `SDL_PutAudioStreamData()` |
| `SDL_GetQueuedAudioSize()` | `SDL_GetAudioStreamQueued()` |
| `SDL_ClearQueuedAudio()` | `SDL_ClearAudioStream()` |
| `SDL_PauseAudioDevice(id, 0/1)` | `SDL_ResumeAudioDevice(id)` / `SDL_PauseAudioDevice(id)` |

### Low Impact Changes

#### 8. Initialization
```cpp
// SDL2
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)

// SDL3 - largely unchanged
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)
```

#### 9. Renderer Creation
```cpp
// SDL2
SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)

// SDL3
SDL_CreateRenderer(window, nullptr)  // Name string instead of index
```

## Existing Abstraction Layers

### Strengths - Ready for Migration

1. **`IAudioBackend` Interface** (`src/app/emu/audio/audio_backend.h`)
   - Complete abstraction for audio operations
   - Factory pattern with `BackendType::SDL3` placeholder already defined
   - Only `SDL2AudioBackend` implementation needs updating

2. **`IInputBackend` Interface** (`src/app/emu/input/input_backend.h`)
   - Platform-agnostic controller state management
   - Factory pattern with `BackendType::SDL3` placeholder already defined
   - Only `SDL2InputBackend` implementation needs updating

3. **`IRenderer` Interface** (`src/app/gfx/backend/irenderer.h`)
   - Abstract texture and rendering operations
   - `SDL2Renderer` implementation isolated
   - Ready for `SDL3Renderer` implementation

4. **`util::SDL_Deleter`** (`src/util/sdl_deleter.h`)
   - Centralized resource cleanup
   - Easy to add SDL3 variants

### Gaps - Need New Abstractions

1. **Window Management**
   - `core::Window` struct directly exposes `SDL_Window*`
   - `CreateWindow()` and `HandleEvents()` have inline SDL2 code
   - **Recommendation**: Create `IWindow` interface or wrapper class

2. **Event Handling**
   - Event processing embedded in `window.cc`
   - SDL2 event types used directly
   - **Recommendation**: Create event abstraction layer or adapter

3. **Timing**
   - `TimingManager` uses SDL2 functions directly
   - **Recommendation**: Create `ITimer` interface (low priority - minimal changes)

4. **Bitmap/Surface**
   - `Bitmap` class directly uses `SDL_Surface`
   - Tight coupling with SDL2 surface APIs
   - **Recommendation**: Create `ISurface` wrapper or use conditional compilation

## Migration Phases

### Phase 1: Preparation (Estimated: 1-2 days)

#### 1.1 Add SDL3 Build Configuration
```cmake
# cmake/dependencies/sdl3.cmake (new file)
option(YAZE_USE_SDL3 "Use SDL3 instead of SDL2" OFF)

if(YAZE_USE_SDL3)
  CPMAddPackage(
    NAME SDL3
    VERSION 3.2.0
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.2.0
    OPTIONS
      "SDL_SHARED OFF"
      "SDL_STATIC ON"
  )
endif()
```

#### 1.2 Create Abstraction Headers
- Create `src/app/platform/sdl_compat.h` for cross-version macros
- Define version-agnostic type aliases

```cpp
// src/app/platform/sdl_compat.h
#pragma once

#ifdef YAZE_USE_SDL3
#include <SDL3/SDL.h>
#define YAZE_SDL_KEYDOWN SDL_EVENT_KEY_DOWN
#define YAZE_SDL_KEYUP SDL_EVENT_KEY_UP
#define YAZE_SDL_WINDOW_CLOSE SDL_EVENT_WINDOW_CLOSE_REQUESTED
// ... etc
#else
#include <SDL.h>
#define YAZE_SDL_KEYDOWN SDL_KEYDOWN
#define YAZE_SDL_KEYUP SDL_KEYUP
#define YAZE_SDL_WINDOW_CLOSE SDL_WINDOWEVENT // (handle internally)
// ... etc
#endif
```

#### 1.3 Update ImGui CMake
```cmake
# cmake/dependencies/imgui.cmake
if(YAZE_USE_SDL3)
  set(IMGUI_SDL_BACKEND "imgui_impl_sdl3.cpp")
  set(IMGUI_RENDERER_BACKEND "imgui_impl_sdlrenderer3.cpp")
else()
  set(IMGUI_SDL_BACKEND "imgui_impl_sdl2.cpp")
  set(IMGUI_RENDERER_BACKEND "imgui_impl_sdlrenderer2.cpp")
endif()
```

### Phase 2: Core Subsystem Migration (Estimated: 3-5 days)

#### 2.1 Audio Backend (Priority: High)
1. Create `SDL3AudioBackend` class in `audio_backend.cc`
2. Implement using `SDL_AudioStream` API
3. Update `AudioBackendFactory::Create()` to handle SDL3

**Key changes**:
```cpp
class SDL3AudioBackend : public IAudioBackend {
  SDL_AudioStream* stream_ = nullptr;

  bool Initialize(const AudioConfig& config) override {
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = config.channels;
    spec.freq = config.sample_rate;

    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    return stream_ != nullptr;
  }

  bool QueueSamples(const int16_t* samples, int num_samples) override {
    return SDL_PutAudioStreamData(stream_, samples,
        num_samples * sizeof(int16_t));
  }
};
```

#### 2.2 Input Backend (Priority: High)
1. Create `SDL3InputBackend` class in `input_backend.cc`
2. Update keyboard state handling for `bool*` return type
3. Update event processing for new event types

**Key changes**:
```cpp
class SDL3InputBackend : public IInputBackend {
  ControllerState Poll(int player) override {
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    // Note: SDL3 returns bool* instead of Uint8*
    state.SetButton(SnesButton::B, keyboard_state[SDL_SCANCODE_Z]);
    // ...
  }
};
```

#### 2.3 Window/Event Handling (Priority: Medium)
1. Update `HandleEvents()` in `window.cc`
2. Replace `SDL_WINDOWEVENT` with individual event types
3. Update keyboard modifier handling

**Before (SDL2)**:
```cpp
case SDL_WINDOWEVENT:
    switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            window.active_ = false;
```

**After (SDL3)**:
```cpp
case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    window.active_ = false;
```

### Phase 3: Graphics Migration (Estimated: 2-3 days)

#### 3.1 Renderer Backend
1. Create `SDL3Renderer` class implementing `IRenderer`
2. Update renderer creation (string name instead of index)
3. Handle coordinate system changes (float vs int)

**Key changes**:
```cpp
class SDL3Renderer : public IRenderer {
  bool Initialize(SDL_Window* window) override {
    renderer_ = SDL_CreateRenderer(window, nullptr);
    return renderer_ != nullptr;
  }
};
```

#### 3.2 Surface/Bitmap Handling
1. Update pixel format access in `Bitmap` class
2. Handle palette creation changes
3. Update `SDL_DEFINE_PIXELFORMAT` macros if needed

**Key changes**:
```cpp
// SDL2
int depth = surface->format->BitsPerPixel;

// SDL3
const SDL_PixelFormatDetails* details =
    SDL_GetPixelFormatDetails(surface->format);
int depth = details->bits_per_pixel;
```

#### 3.3 Texture Management
1. Update texture creation in `SDL3Renderer`
2. Handle any lock/unlock API changes

### Phase 4: ImGui Integration (Estimated: 1 day)

#### 4.1 Update Backend Initialization
```cpp
// SDL2
ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
ImGui_ImplSDLRenderer2_Init(renderer);

// SDL3
ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
ImGui_ImplSDLRenderer3_Init(renderer);
```

#### 4.2 Update Frame Processing
```cpp
// SDL2
ImGui_ImplSDLRenderer2_NewFrame();
ImGui_ImplSDL2_NewFrame();
ImGui_ImplSDL2_ProcessEvent(&event);

// SDL3
ImGui_ImplSDLRenderer3_NewFrame();
ImGui_ImplSDL3_NewFrame();
ImGui_ImplSDL3_ProcessEvent(&event);
```

### Phase 5: Cleanup and Testing (Estimated: 2-3 days)

#### 5.1 Remove SDL2 Fallback (Optional)
- Once stable, consider removing dual-support code
- Keep SDL2 code path for legacy support if needed

#### 5.2 Update Tests
- Update test initialization for SDL3
- Verify all test suites pass with SDL3

#### 5.3 Documentation Updates
- Update build instructions
- Update dependency documentation
- Add SDL3-specific notes to CLAUDE.md

## Effort Estimates

| Phase | Task | Estimated Time | Complexity |
|-------|------|----------------|------------|
| **Phase 1** | Build configuration | 4 hours | Low |
| | Abstraction headers | 4 hours | Low |
| | ImGui CMake updates | 2 hours | Low |
| **Phase 2** | Audio backend | 8 hours | High |
| | Input backend | 4 hours | Medium |
| | Window/Event handling | 6 hours | Medium |
| **Phase 3** | Renderer backend | 8 hours | Medium |
| | Surface/Bitmap handling | 6 hours | Medium |
| | Texture management | 4 hours | Low |
| **Phase 4** | ImGui integration | 4 hours | Low |
| **Phase 5** | Cleanup and testing | 8-12 hours | Medium |
| **Total** | | **~58-62 hours** | |

## Risk Assessment

### High Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Audio API complexity | Emulator audio may break | Start with audio migration; extensive testing |
| Cross-platform differences | Platform-specific bugs | Test on all platforms early |
| ImGui backend compatibility | UI rendering issues | Use official SDL3 backends from Dear ImGui |

### Medium Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Performance regression | Slower rendering/audio | Benchmark before and after |
| Build system complexity | Build failures | Maintain dual-build support initially |
| Event timing changes (ns vs ms) | Input lag or timing issues | Careful timestamp handling |

### Low Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Function rename compilation errors | Build failures | Mechanical fixes with search/replace |
| Minor API differences | Runtime bugs | Comprehensive test coverage |

## Testing Strategy

### Unit Tests
- Audio backend: Test initialization, queue, playback control
- Input backend: Test keyboard state, event processing
- Renderer: Test texture creation, rendering operations

### Integration Tests
- Full emulator loop with SDL3
- Editor UI responsiveness
- Graphics loading and display

### Manual Testing Checklist
- [ ] Application launches without errors
- [ ] ROM loading works correctly
- [ ] All editors render properly
- [ ] Emulator audio plays without glitches
- [ ] Keyboard input responsive in emulator
- [ ] Window resize works correctly
- [ ] Multi-monitor support (if applicable)
- [ ] Performance comparable to SDL2

## Dependencies

### Required
- SDL 3.2.0 or later
- Updated ImGui with SDL3 backends (already available in ext/imgui)

### Optional
- SDL3_gpu for modern GPU rendering (future enhancement)
- SDL3_mixer for enhanced audio (if needed)

## Rollback Plan

If SDL3 migration causes critical issues:

1. Keep SDL2 build option available (`-DYAZE_USE_SDL3=OFF`)
2. Document known SDL3 issues in issue tracker
3. Maintain SDL2 compatibility branch if needed

## References

- [SDL3 Migration Guide](https://wiki.libsdl.org/SDL3/README-migration)
- [SDL3 GitHub Repository](https://github.com/libsdl-org/SDL)
- [Dear ImGui SDL3 Backends](https://github.com/ocornut/imgui/tree/master/backends)
- [SDL3 API Documentation](https://wiki.libsdl.org/SDL3/CategoryAPI)

## Appendix A: Full File Impact List

### Files Requiring Modification

```
src/app/platform/window.h          - SDL_Window type, event constants
src/app/platform/window.cc         - Event handling, window creation
src/app/platform/timing.h          - Performance counter functions
src/app/controller.cc              - ImGui backend calls
src/app/controller.h               - SDL_Window reference
src/app/gfx/backend/irenderer.h    - SDL types in interface
src/app/gfx/backend/sdl2_renderer.h/.cc - Entire file (create SDL3 variant)
src/app/gfx/core/bitmap.h/.cc      - Surface handling, pixel formats
src/app/gfx/types/snes_palette.cc  - SDL_Color usage
src/app/gfx/resource/arena.cc      - Surface/texture management
src/app/emu/audio/audio_backend.h/.cc - Complete audio API rewrite
src/app/emu/input/input_backend.h/.cc - Keyboard state, events
src/app/emu/ui/input_handler.cc    - Key name functions, events
src/app/emu/emu.cc                 - Full SDL initialization
src/util/sdl_deleter.h             - Deleter function signatures
test/yaze_test.cc                  - Test initialization
test/test_editor.cc                - Test window handling
cmake/dependencies/sdl2.cmake      - Build configuration
cmake/dependencies/imgui.cmake     - Backend selection
```

### New Files to Create

```
src/app/platform/sdl_compat.h      - Cross-version compatibility macros
src/app/gfx/backend/sdl3_renderer.h/.cc - SDL3 renderer implementation
cmake/dependencies/sdl3.cmake      - SDL3 build configuration
```

## Appendix B: Quick Reference - API Mapping

| SDL2 | SDL3 | Notes |
|------|------|-------|
| `SDL_INIT_TIMER` | Removed | Timer always available |
| `SDL_GetTicks()` | `SDL_GetTicks()` | Returns Uint64 |
| `SDL_OpenAudioDevice()` | `SDL_OpenAudioDeviceStream()` | Stream-based |
| `SDL_QueueAudio()` | `SDL_PutAudioStreamData()` | |
| `SDL_PauseAudioDevice(id, 0)` | `SDL_ResumeAudioDevice(id)` | |
| `SDL_PauseAudioDevice(id, 1)` | `SDL_PauseAudioDevice(id)` | |
| `SDL_CreateRenderer(w, -1, f)` | `SDL_CreateRenderer(w, name)` | |
| `SDL_KEYDOWN` | `SDL_EVENT_KEY_DOWN` | |
| `SDL_WINDOWEVENT` | Individual events | |
| `event.key.keysym.sym` | `event.key.key` | |
| `SDL_GetKeyboardState()` | `SDL_GetKeyboardState()` | Returns bool* |
