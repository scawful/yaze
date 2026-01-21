# Platform Backend Architecture

**Status:** Complete (infrastructure)
**Last Updated:** 2026-01-20

This document describes the platform abstraction layer that enables yaze to support multiple window/audio/input backends (SDL2, SDL3, and potentially GLFW).

## Overview

The platform layer provides runtime-switchable backends through interface abstractions:

```
src/app/platform/
├── iwindow.h              # IWindowBackend interface + factory
├── iaudio.h               # IAudioBackend interface
├── iinput.h               # IInputBackend interface
├── irenderer.h            # IRenderer interface
├── sdl2_window_backend.cc # SDL2 implementation
├── sdl3_window_backend.cc # SDL3 implementation
├── sdl3_audio_backend.cc  # SDL3 audio
├── window_backend_factory.cc # Runtime backend selection
└── window.cc              # High-level window management
```

## Backend Selection

Backend selection is compile-time via CMake:

```cmake
# In cmake/options-simple.cmake
option(YAZE_USE_SDL3 "Use SDL3 instead of SDL2 (experimental)" OFF)
```

The `WindowBackendFactory` creates the appropriate backend:

```cpp
// window_backend_factory.cc
std::unique_ptr<IWindowBackend> WindowBackendFactory::Create() {
#ifdef YAZE_USE_SDL3
  return std::make_unique<Sdl3WindowBackend>();
#else
  return std::make_unique<Sdl2WindowBackend>();
#endif
}
```

## IWindowBackend Interface

```cpp
class IWindowBackend {
 public:
  virtual ~IWindowBackend() = default;

  // Lifecycle
  virtual absl::Status Initialize(const WindowConfig& config) = 0;
  virtual void Shutdown() = 0;

  // Frame handling
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;

  // Event handling
  virtual bool PollEvents() = 0;  // Returns false on quit
  virtual WindowEventType GetLastEventType() const = 0;

  // Window properties
  virtual void* GetNativeHandle() const = 0;
  virtual ImVec2 GetWindowSize() const = 0;
  virtual float GetDpiScale() const = 0;
};
```

## SDL2 vs SDL3 Differences

### Viewport Support

**SDL3** enables ImGui viewports (multi-window) by default:
```cpp
// sdl3_window_backend.cc
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
```

**SDL2** only renders viewports if already enabled elsewhere:
```cpp
// sdl2_window_backend.cc
if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}
```

### GPU Rendering

SDL3 uses a GPU-based renderer (`SDL_GPU`) which provides:
- Better performance for texture-heavy workloads
- Native Vulkan/Metal/D3D12 support
- More efficient texture uploads

SDL2 uses `SDL_Renderer` with software fallbacks.

### Audio Backend

SDL3 audio backend (`sdl3_audio_backend.cc`) provides:
- Modern audio device enumeration
- Better latency control
- Improved format negotiation

## GLFW Status

GLFW is **not currently integrated**. ImGui includes GLFW backends in `ext/imgui/backends/`:
- `imgui_impl_glfw.cpp`
- `imgui_impl_opengl3.cpp`

Integrating GLFW would require:
1. New `GlfwWindowBackend` class implementing `IWindowBackend`
2. CMake option `YAZE_USE_GLFW`
3. GLFW dependency management
4. Testing on all platforms

**Use case:** GLFW may provide better OpenGL compatibility on some Linux configurations.

## Current Status

| Backend | Window | Audio | Input | Viewports | Status |
|---------|--------|-------|-------|-----------|--------|
| SDL2 | ✅ | ✅ | ✅ | Optional | **Default** |
| SDL3 | ✅ | ✅ | ✅ | Default | Experimental |
| GLFW | ❌ | ❌ | ❌ | N/A | Not integrated |

## Migration to SDL3

To test SDL3:

```bash
cmake --preset mac-dbg -DYAZE_USE_SDL3=ON
cmake --build build -j8
```

Known issues:
- Some texture format differences
- Event handling API changes
- Audio device enumeration differences

## Testing

Platform backend tests are in `test/platform/`:
- `window_backend_test.cc` - Factory and basic window tests
- `sdl3_audio_backend_test.cc` - SDL3 audio tests (skips if unavailable)

Run with:
```bash
ctest --test-dir build -R platform -j4
```

## Related Documents

- [graphics_system_architecture.md](graphics_system_architecture.md) - Graphics rendering pipeline
- [editor_card_layout_system.md](editor_card_layout_system.md) - UI layout system
- [../../CLAUDE.md](../../CLAUDE.md) - Project overview
