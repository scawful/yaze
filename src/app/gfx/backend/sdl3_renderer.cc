#ifdef YAZE_USE_SDL3

#include "app/gfx/backend/sdl3_renderer.h"

#include <SDL3/SDL.h>

#include "app/gfx/core/bitmap.h"

namespace yaze {
namespace gfx {

SDL3Renderer::SDL3Renderer() = default;

SDL3Renderer::~SDL3Renderer() { Shutdown(); }

/**
 * @brief Initializes the SDL3 renderer.
 *
 * This function creates an SDL3 renderer and attaches it to the given window.
 * SDL3 simplified renderer creation - no driver index or flags parameter.
 * Use SDL_SetRenderVSync() separately for vsync control.
 */
bool SDL3Renderer::Initialize(SDL_Window* window) {
  // Create an SDL3 renderer.
  // SDL3 API: SDL_CreateRenderer(window, driver_name)
  // Pass nullptr to let SDL choose the best available driver.
  renderer_ = SDL_CreateRenderer(window, nullptr);

  if (renderer_ == nullptr) {
    SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
    return false;
  }

  // Set blend mode for transparency support.
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  // Enable vsync for smoother rendering.
  SDL_SetRenderVSync(renderer_, 1);

  return true;
}

/**
 * @brief Shuts down the renderer.
 */
void SDL3Renderer::Shutdown() {
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
}

/**
 * @brief Creates an SDL_Texture with default streaming access.
 *
 * The texture is created with streaming access, which is suitable for textures
 * that are updated frequently.
 */
TextureHandle SDL3Renderer::CreateTexture(int width, int height) {
  // SDL3 texture creation is largely unchanged from SDL2.
  return static_cast<TextureHandle>(SDL_CreateTexture(
      renderer_,
      static_cast<SDL_PixelFormat>(SDL_PIXELFORMAT_RGBA8888),
      static_cast<SDL_TextureAccess>(SDL_TEXTUREACCESS_STREAMING), width,
      height));
}

/**
 * @brief Creates an SDL_Texture with a specific pixel format and access
 * pattern.
 *
 * This is useful for specialized textures like emulator PPU output.
 */
TextureHandle SDL3Renderer::CreateTextureWithFormat(int width, int height,
                                                    uint32_t format,
                                                    int access) {
  return static_cast<TextureHandle>(
      SDL_CreateTexture(renderer_,
                        static_cast<SDL_PixelFormat>(format),
                        static_cast<SDL_TextureAccess>(access), width, height));
}

/**
 * @brief Updates an SDL_Texture with data from a Bitmap.
 *
 * This involves converting the bitmap's surface to the correct format and
 * updating the texture. SDL3 renamed SDL_ConvertSurfaceFormat to
 * SDL_ConvertSurface and removed the flags parameter.
 */
void SDL3Renderer::UpdateTexture(TextureHandle texture, const Bitmap& bitmap) {
  SDL_Surface* surface = bitmap.surface();

  // Validate texture, surface, and surface format
  if (!texture || !surface || surface->format == SDL_PIXELFORMAT_UNKNOWN) {
    return;
  }

  // Validate surface has pixels
  if (!surface->pixels || surface->w <= 0 || surface->h <= 0) {
    return;
  }

  // Convert the bitmap's surface to RGBA8888 format for compatibility with the
  // texture.
  // SDL3 API: SDL_ConvertSurface(surface, format) - no flags parameter
  SDL_Surface* converted_surface =
      SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);

  if (!converted_surface || !converted_surface->pixels) {
    if (converted_surface) {
      SDL_DestroySurface(converted_surface);
    }
    return;
  }

  // Update the texture with the pixels from the converted surface.
  SDL_UpdateTexture(static_cast<SDL_Texture*>(texture), nullptr,
                    converted_surface->pixels, converted_surface->pitch);

  // SDL3 uses SDL_DestroySurface instead of SDL_FreeSurface
  SDL_DestroySurface(converted_surface);
}

/**
 * @brief Destroys an SDL_Texture.
 */
void SDL3Renderer::DestroyTexture(TextureHandle texture) {
  if (texture) {
    SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
  }
}

/**
 * @brief Locks a texture for direct pixel access.
 */
bool SDL3Renderer::LockTexture(TextureHandle texture, SDL_Rect* rect,
                               void** pixels, int* pitch) {
  // SDL3 LockTexture now takes SDL_FRect*, but for simplicity we use the
  // integer version when available. In SDL3, LockTexture still accepts
  // SDL_Rect* for the region.
  return SDL_LockTexture(static_cast<SDL_Texture*>(texture), rect, pixels,
                         pitch);
}

/**
 * @brief Unlocks a previously locked texture.
 */
void SDL3Renderer::UnlockTexture(TextureHandle texture) {
  SDL_UnlockTexture(static_cast<SDL_Texture*>(texture));
}

/**
 * @brief Clears the screen with the current draw color.
 */
void SDL3Renderer::Clear() { SDL_RenderClear(renderer_); }

/**
 * @brief Presents the rendered frame to the screen.
 */
void SDL3Renderer::Present() { SDL_RenderPresent(renderer_); }

/**
 * @brief Copies a texture to the render target.
 *
 * SDL3 renamed SDL_RenderCopy to SDL_RenderTexture and uses SDL_FRect
 * for the destination rectangle.
 */
void SDL3Renderer::RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                              const SDL_Rect* dstrect) {
  SDL_FRect src_frect, dst_frect;
  SDL_FRect* src_ptr = ToFRect(srcrect, &src_frect);
  SDL_FRect* dst_ptr = ToFRect(dstrect, &dst_frect);

  // SDL3 API: SDL_RenderTexture(renderer, texture, srcrect, dstrect)
  // Both rectangles use SDL_FRect (float) in SDL3.
  SDL_RenderTexture(renderer_, static_cast<SDL_Texture*>(texture), src_ptr,
                    dst_ptr);
}

/**
 * @brief Sets the render target.
 */
void SDL3Renderer::SetRenderTarget(TextureHandle texture) {
  SDL_SetRenderTarget(renderer_, static_cast<SDL_Texture*>(texture));
}

/**
 * @brief Sets the draw color.
 */
void SDL3Renderer::SetDrawColor(SDL_Color color) {
  SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}

/**
 * @brief Convert SDL_Rect (int) to SDL_FRect (float).
 *
 * SDL3 uses floating-point rectangles for many rendering operations.
 * This helper converts integer rectangles to float rectangles.
 *
 * @param rect Input integer rectangle (may be nullptr)
 * @param frect Output float rectangle
 * @return Pointer to frect if rect was valid, nullptr otherwise
 */
SDL_FRect* SDL3Renderer::ToFRect(const SDL_Rect* rect, SDL_FRect* frect) {
  if (!rect || !frect) {
    return nullptr;
  }

  frect->x = static_cast<float>(rect->x);
  frect->y = static_cast<float>(rect->y);
  frect->w = static_cast<float>(rect->w);
  frect->h = static_cast<float>(rect->h);

  return frect;
}

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_USE_SDL3
