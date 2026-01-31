#pragma once

#include "app/platform/sdl_compat.h"

// Undef C math macros that can be defined by /usr/local/include/math.h (e.g.
// Homebrew on macOS). They conflict with libc++ __math/traits.h when <memory>
// or other C++ headers use these identifiers. Must run after sdl_compat (SDL
// can pull in C math) and before any C++ stdlib includes.
#ifdef isfinite
#undef isfinite
#endif
#ifdef isinf
#undef isinf
#endif
#ifdef isnan
#undef isnan
#endif
#ifdef isnormal
#undef isnormal
#endif
#ifdef signbit
#undef signbit
#endif

#include <memory>
#include <vector>

// Forward declarations prevent circular dependencies and speed up compilation.
// Instead of including the full header, we just tell the compiler that these
// types exist.
namespace yaze {
namespace gfx {
class Bitmap;
}
}  // namespace yaze

namespace yaze {
namespace gfx {

/**
 * @brief An abstract handle representing a texture.
 *
 * This typedef allows the underlying texture implementation (e.g.,
 * SDL_Texture*, an OpenGL texture ID, etc.) to be hidden from the application
 * logic.
 */
using TextureHandle = void*;

/**
 * @interface IRenderer
 * @brief Defines an abstract interface for all rendering operations.
 *
 * This interface decouples the application from any specific rendering API
 * (like SDL2, SDL3, OpenGL, etc.). It provides a contract for creating
 * textures, managing their lifecycle, and performing primitive drawing
 * operations. The goal is to program against this interface, allowing the
 * concrete rendering backend to be swapped out with minimal changes to the
 * application code.
 */
class IRenderer {
 public:
  virtual ~IRenderer() = default;

  // --- Initialization and Lifecycle ---

  /**
   * @brief Initializes the renderer with a given window.
   * @param window A pointer to the SDL_Window to render into.
   * @return True if initialization was successful, false otherwise.
   */
  virtual bool Initialize(SDL_Window* window) = 0;

  /**
   * @brief Shuts down the renderer and releases all associated resources.
   */
  virtual void Shutdown() = 0;

  // --- Texture Management ---

  /**
   * @brief Creates a new, empty texture.
   * @param width The width of the texture in pixels.
   * @param height The height of the texture in pixels.
   * @return An abstract TextureHandle to the newly created texture, or nullptr
   * on failure.
   */
  virtual TextureHandle CreateTexture(int width, int height) = 0;

  /**
   * @brief Creates a new texture with a specific pixel format.
   * @param width The width of the texture in pixels.
   * @param height The height of the texture in pixels.
   * @param format The SDL pixel format (e.g., SDL_PIXELFORMAT_ARGB8888).
   * @param access The texture access pattern (e.g.,
   * SDL_TEXTUREACCESS_STREAMING).
   * @return An abstract TextureHandle to the newly created texture, or nullptr
   * on failure.
   */
  virtual TextureHandle CreateTextureWithFormat(int width, int height,
                                                uint32_t format,
                                                int access) = 0;

  /**
   * @brief Updates a texture with the pixel data from a Bitmap.
   * @param texture The handle of the texture to update.
   * @param bitmap The Bitmap containing the new pixel data.
   */
  virtual void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) = 0;

  /**
   * @brief Destroys a texture and frees its associated resources.
   * @param texture The handle of the texture to destroy.
   */
  virtual void DestroyTexture(TextureHandle texture) = 0;

  // --- Direct Pixel Access ---
  virtual bool LockTexture(TextureHandle texture, SDL_Rect* rect, void** pixels,
                           int* pitch) = 0;
  virtual void UnlockTexture(TextureHandle texture) = 0;

  // --- Rendering Primitives ---

  /**
   * @brief Clears the entire render target with the current draw color.
   */
  virtual void Clear() = 0;

  /**
   * @brief Presents the back buffer to the screen, making the rendered content
   * visible.
   */
  virtual void Present() = 0;

  /**
   * @brief Copies a portion of a texture to the current render target.
   * @param texture The source texture handle.
   * @param srcrect A pointer to the source rectangle, or nullptr for the entire
   * texture.
   * @param dstrect A pointer to the destination rectangle, or nullptr for the
   * entire render target.
   */
  virtual void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                          const SDL_Rect* dstrect) = 0;

  /**
   * @brief Sets the render target for subsequent drawing operations.
   * @param texture The texture to set as the render target, or nullptr to set
   * it back to the default (the window).
   */
  virtual void SetRenderTarget(TextureHandle texture) = 0;

  /**
   * @brief Sets the color used for drawing operations (e.g., Clear).
   * @param color The SDL_Color to use.
   */
  virtual void SetDrawColor(SDL_Color color) = 0;

  // --- Backend-specific Access ---

  /**
   * @brief Provides an escape hatch to get the underlying, concrete renderer
   * object.
   *
   * This is necessary for integrating with third-party libraries like ImGui
   * that are tied to a specific rendering backend (e.g., SDL_Renderer*,
   * ID3D11Device*).
   *
   * @return A void pointer to the backend-specific renderer object. The caller
   * is responsible for casting it to the correct type.
   */
  virtual void* GetBackendRenderer() = 0;
};

}  // namespace gfx
}  // namespace yaze
