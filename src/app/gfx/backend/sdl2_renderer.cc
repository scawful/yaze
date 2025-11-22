#include "app/gfx/backend/sdl2_renderer.h"

#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"

namespace yaze {
namespace gfx {

SDL2Renderer::SDL2Renderer() = default;

SDL2Renderer::~SDL2Renderer() {
  Shutdown();
}

/**
 * @brief Initializes the SDL2 renderer.
 * This function creates an accelerated SDL2 renderer and attaches it to the
 * given window.
 */
bool SDL2Renderer::Initialize(SDL_Window* window) {
  // Create an SDL2 renderer with hardware acceleration.
  renderer_ = std::unique_ptr<SDL_Renderer, util::SDL_Deleter>(
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));

  if (renderer_ == nullptr) {
    // Log an error if renderer creation fails.
    printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
    return false;
  }

  // Set the blend mode to allow for transparency.
  SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
  return true;
}

/**
 * @brief Shuts down the renderer.
 * The underlying SDL_Renderer is managed by a unique_ptr, so its destruction is
 * handled automatically.
 */
void SDL2Renderer::Shutdown() {
  renderer_.reset();
}

/**
 * @brief Creates an SDL_Texture.
 * The texture is created with streaming access, which is suitable for textures
 * that are updated frequently.
 */
TextureHandle SDL2Renderer::CreateTexture(int width, int height) {
  // The TextureHandle is a void*, so we cast the SDL_Texture* to it.
  return static_cast<TextureHandle>(
      SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height));
}

/**
 * @brief Creates an SDL_Texture with a specific pixel format and access
 * pattern. This is useful for specialized textures like emulator PPU output.
 */
TextureHandle SDL2Renderer::CreateTextureWithFormat(int width, int height,
                                                    uint32_t format,
                                                    int access) {
  return static_cast<TextureHandle>(
      SDL_CreateTexture(renderer_.get(), format, access, width, height));
}

/**
 * @brief Updates an SDL_Texture with data from a Bitmap.
 * This involves converting the bitmap's surface to the correct format and
 * updating the texture.
 */
void SDL2Renderer::UpdateTexture(TextureHandle texture, const Bitmap& bitmap) {
  SDL_Surface* surface = bitmap.surface();

  // Validate texture, surface, and surface format
  if (!texture || !surface || !surface->format) {
    return;
  }

  // Validate surface has pixels
  if (!surface->pixels || surface->w <= 0 || surface->h <= 0) {
    return;
  }

  // Convert the bitmap's surface to RGBA8888 format for compatibility with the
  // texture.
  auto converted_surface =
      std::unique_ptr<SDL_Surface, util::SDL_Surface_Deleter>(
          SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0));

  if (!converted_surface || !converted_surface->pixels) {
    return;
  }

  // Update the texture with the pixels from the converted surface.
  SDL_UpdateTexture(static_cast<SDL_Texture*>(texture), nullptr,
                    converted_surface->pixels, converted_surface->pitch);
}

/**
 * @brief Destroys an SDL_Texture.
 */
void SDL2Renderer::DestroyTexture(TextureHandle texture) {
  if (texture) {
    SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
  }
}

bool SDL2Renderer::LockTexture(TextureHandle texture, SDL_Rect* rect,
                               void** pixels, int* pitch) {
  return SDL_LockTexture(static_cast<SDL_Texture*>(texture), rect, pixels,
                         pitch) == 0;
}

void SDL2Renderer::UnlockTexture(TextureHandle texture) {
  SDL_UnlockTexture(static_cast<SDL_Texture*>(texture));
}

/**
 * @brief Clears the screen with the current draw color.
 */
void SDL2Renderer::Clear() {
  SDL_RenderClear(renderer_.get());
}

/**
 * @brief Presents the rendered frame to the screen.
 */
void SDL2Renderer::Present() {
  SDL_RenderPresent(renderer_.get());
}

/**
 * @brief Copies a texture to the render target.
 */
void SDL2Renderer::RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                              const SDL_Rect* dstrect) {
  SDL_RenderCopy(renderer_.get(), static_cast<SDL_Texture*>(texture), srcrect,
                 dstrect);
}

/**
 * @brief Sets the render target.
 */
void SDL2Renderer::SetRenderTarget(TextureHandle texture) {
  SDL_SetRenderTarget(renderer_.get(), static_cast<SDL_Texture*>(texture));
}

/**
 * @brief Sets the draw color.
 */
void SDL2Renderer::SetDrawColor(SDL_Color color) {
  SDL_SetRenderDrawColor(renderer_.get(), color.r, color.g, color.b, color.a);
}

}  // namespace gfx
}  // namespace yaze
