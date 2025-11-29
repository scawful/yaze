# SDL2 to SDL3 Migration and Rendering Abstraction Plan

## 1. Introduction

This document outlines a strategic plan to refactor the rendering architecture of the `yaze` application. The primary goals are:

1.  **Decouple the application from the SDL2 rendering API.**
2.  **Create a clear and straightforward path for migrating to SDL3.**
3.  **Enable support for multiple rendering backends** (e.g., OpenGL, Metal, DirectX) to improve cross-platform performance and leverage modern graphics APIs.

## 2. Current State Analysis

The current architecture exhibits tight coupling with the SDL2 rendering API.

-   **Direct Dependency:** Components like `gfx::Bitmap`, `gfx::Arena`, and `gfx::AtlasRenderer` directly accept or call functions using an `SDL_Renderer*`.
-   **Singleton Pattern:** The `core::Renderer` singleton in `src/app/core/window.h` provides global access to the `SDL_Renderer`, making it difficult to manage, replace, or mock.
-   **Dual Rendering Pipelines:** The main application (`yaze.cc`, `app_delegate.mm`) and the standalone emulator (`app/emu/emu.cc`) both perform their own separate, direct SDL initialization and rendering loops. This code duplication makes maintenance and migration efforts more complex.

This tight coupling makes it brittle, difficult to maintain, and nearly impossible to adapt to newer rendering APIs like SDL3 or other backends without a major, project-wide rewrite.

## 3. Proposed Architecture: The `Renderer` Abstraction

The core of this plan is to introduce a `Renderer` interface (an abstract base class) that defines a set of rendering primitives. The application will be refactored to program against this interface, not a concrete SDL2 implementation.

### 3.1. The `IRenderer` Interface

A new interface, `IRenderer`, will be created. It will define the contract for all rendering operations.

**File:** `src/app/gfx/irenderer.h`

```cpp
#pragma once

#include <SDL.h> // For SDL_Rect, SDL_Color, etc.
#include <memory>
#include <vector>

#include "app/gfx/bitmap.h"

namespace yaze {
namespace gfx {

// Forward declarations
class Bitmap;

// A handle to a texture, abstracting away the underlying implementation
using TextureHandle = void*;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // --- Initialization and Lifecycle ---
    virtual bool Initialize(SDL_Window* window) = 0;
    virtual void Shutdown() = 0;

    // --- Texture Management ---
    virtual TextureHandle CreateTexture(int width, int height) = 0;
    virtual void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) = 0;
    virtual void DestroyTexture(TextureHandle texture) = 0;

    // --- Rendering Primitives ---
    virtual void Clear() = 0;
    virtual void Present() = 0;
    virtual void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect) = 0;
    virtual void SetRenderTarget(TextureHandle texture) = 0;
    virtual void SetDrawColor(SDL_Color color) = 0;

    // --- Backend-specific Access ---
    // Provides an escape hatch for libraries like ImGui that need the concrete renderer.
    virtual void* GetBackendRenderer() = 0;
};

} // namespace gfx
} // namespace yaze
```

### 3.2. The `SDL2Renderer` Implementation

A concrete class, `SDL2Renderer`, will be the first implementation of the `IRenderer` interface. It will encapsulate all the existing SDL2-specific rendering logic.

**File:** `src/app/gfx/sdl2_renderer.h` & `src/app/gfx/sdl2_renderer.cc`

```cpp
// sdl2_renderer.h
#include "app/gfx/irenderer.h"
#include "util/sdl_deleter.h"

namespace yaze {
namespace gfx {

class SDL2Renderer : public IRenderer {
public:
    SDL2Renderer();
    ~SDL2Renderer() override;

    bool Initialize(SDL_Window* window) override;
    void Shutdown() override;

    TextureHandle CreateTexture(int width, int height) override;
    void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) override;
    void DestroyTexture(TextureHandle texture) override;

    void Clear() override;
    void Present() override;
    void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect) override;
    void SetRenderTarget(TextureHandle texture) override;
    void SetDrawColor(SDL_Color color) override;

    void* GetBackendRenderer() override { return renderer_.get(); }

private:
    std::unique_ptr<SDL_Renderer, util::SDL_Deleter> renderer_;
};

} // namespace gfx
} // namespace yaze
```

## 4. Migration Plan

The migration will be executed in phases to ensure stability and minimize disruption.

### Phase 1: Implement the Abstraction Layer

1.  **Create `IRenderer` and `SDL2Renderer`:** Implement the interface and concrete class as defined above.
2.  **Refactor `core::Renderer` Singleton:** The existing `core::Renderer` singleton will be deprecated and removed. A new central mechanism (e.g., a service locator or passing the `IRenderer` instance) will provide access to the active renderer.
3.  **Update Application Entry Points:**
    *   In `app/core/controller.cc` (for the main editor) and `app/emu/emu.cc` (for the emulator), instantiate `SDL2Renderer` during initialization. The `Controller` will own the `unique_ptr<IRenderer>`.
    *   This immediately unifies the rendering pipeline initialization for both application modes.
4.  **Refactor `gfx` Library:**
    *   **`gfx::Bitmap`:** Modify `CreateTexture` and `UpdateTexture` to accept an `IRenderer*` instead of an `SDL_Renderer*`. The `SDL_Texture*` will be replaced with the abstract `TextureHandle`.
    *   **`gfx::Arena`:** The `AllocateTexture` method will now call `renderer->CreateTexture()`. The internal pools will store `TextureHandle`s.
    *   **`gfx::AtlasRenderer`:** The `Initialize` method will take an `IRenderer*`. All calls to `SDL_RenderCopy`, `SDL_SetRenderTarget`, etc., will be replaced with calls to the corresponding methods on the `IRenderer` interface.
5.  **Update ImGui Integration:**
    *   The ImGui backend requires the concrete `SDL_Renderer*`. The `GetBackendRenderer()` method on the interface provides a type-erased `void*` for this purpose.
    *   The ImGui initialization code will be modified as follows:
        ```cpp
        // Before
        ImGui_ImplSDLRenderer2_Init(sdl_renderer_ptr);

        // After
        auto backend_renderer = renderer->GetBackendRenderer();
        ImGui_ImplSDLRenderer2_Init(static_cast<SDL_Renderer*>(backend_renderer));
        ```

### Phase 2: Migrate to SDL3

With the abstraction layer in place, migrating to SDL3 becomes significantly simpler.

1.  **Create `SDL3Renderer`:** A new class, `SDL3Renderer`, will be created that implements the `IRenderer` interface using SDL3's rendering functions.
    *   This class will handle the differences in the SDL3 API (e.g., `SDL_CreateRendererWithProperties`, float-based rendering functions, etc.) internally.
    *   The `TextureHandle` will now correspond to an `SDL_Texture*` from SDL3.
2.  **Update Build System:** The CMake files will be updated to link against SDL3 instead of SDL2.
3.  **Switch Implementation:** The application entry points (`controller.cc`, `emu.cc`) will be changed to instantiate `SDL3Renderer` instead of `SDL2Renderer`.

The rest of the application, which only knows about the `IRenderer` interface, will require **no changes**.

### Phase 3: Support for Multiple Rendering Backends

The `IRenderer` interface makes adding new backends a modular task.

1.  **Implement New Backends:** Create new classes like `OpenGLRenderer`, `MetalRenderer`, or `VulkanRenderer`. Each will implement the `IRenderer` interface using the corresponding graphics API.
2.  **Backend Selection:** Implement a factory function or a strategy in the main controller to select and create the desired renderer at startup, based on platform, user configuration, or command-line flags.
3.  **ImGui Backend Alignment:** When a specific backend is chosen for `yaze`, the corresponding ImGui backend implementation must also be used (e.g., `ImGui_ImplOpenGL3_Init`, `ImGui_ImplMetal_Init`). The `GetBackendRenderer()` method will provide the necessary context (e.g., `ID3D11Device*`, `MTLDevice*`) for each implementation.

## 5. Conclusion

This plan transforms the rendering system from a tightly coupled, monolithic design into a flexible, modular, and future-proof architecture.

**Benefits:**

-   **Maintainability:** Rendering logic is centralized and isolated, making it easier to debug and maintain.
-   **Extensibility:** Adding support for new rendering APIs (like SDL3, Vulkan, Metal) becomes a matter of implementing a new interface, not refactoring the entire application.
-   **Testability:** The rendering interface can be mocked, allowing for unit testing of graphics components without a live rendering context.
-   **Future-Proofing:** The application is no longer tied to a specific version of SDL, ensuring a smooth transition to future graphics technologies.
