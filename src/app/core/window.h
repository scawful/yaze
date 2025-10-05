#ifndef YAZE_CORE_WINDOW_H_
#define YAZE_CORE_WINDOW_H_

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/sdl_deleter.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace core {

struct Window {
  std::shared_ptr<SDL_Window> window_;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<int16_t> audio_buffer_;
  bool active_ = true;
};

absl::Status CreateWindow(Window &window, int flags);
absl::Status HandleEvents(Window &window);
absl::Status ShutdownWindow(Window &window);

/**
 * @class Renderer
 * @brief The Renderer class represents the renderer for the Yaze application.
 *
 * This class is a singleton that provides functionality for creating and
 * rendering bitmaps to the screen. It also includes methods for updating
 * bitmaps on the screen.
 * 
 * IMPORTANT: This class MUST be used only on the main thread because:
 * 1. SDL_Renderer operations are not thread-safe
 * 2. OpenGL/DirectX contexts are bound to the creating thread
 * 3. Texture creation and rendering must happen on the main UI thread
 * 
 * For performance optimization during ROM loading:
 * - Use deferred texture creation (CreateBitmapWithoutTexture) for bulk operations
 * - Batch texture creation operations when possible
 * - Consider background processing of bitmap data before texture creation
 */
class Renderer {
 public:
  static Renderer &Get() {
    static Renderer instance;
    return instance;
  }

  /**
   * @brief Initialize the SDL renderer on the main thread
   * 
   * This MUST be called from the main thread as SDL renderer operations
   * are not thread-safe and require the OpenGL/DirectX context to be bound
   * to the calling thread.
   */
  absl::Status CreateRenderer(SDL_Window *window) {
    renderer_ = std::unique_ptr<SDL_Renderer, SDL_Deleter>(
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED));
    if (renderer_ == nullptr) {
      return absl::InternalError(
          absl::StrFormat("SDL_CreateRenderer: %s\n", SDL_GetError()));
    }
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_.get(), 0x00, 0x00, 0x00, 0x00);
    return absl::OkStatus();
  }

  auto renderer() -> SDL_Renderer * { return renderer_.get(); }

  /**
   * @brief Create texture for bitmap on main thread
   * 
   * This operation blocks the main thread and should be used sparingly
   * during bulk loading operations. Consider using CreateBitmapWithoutTexture
   * followed by batch texture creation.
   */
  void RenderBitmap(gfx::Bitmap *bitmap) {
    bitmap->CreateTexture(renderer_.get());
  }

  /**
   * @brief Update existing texture on main thread
   */
  void UpdateBitmap(gfx::Bitmap *bitmap) {
    bitmap->UpdateTexture(renderer_.get());
  }

  /**
   * @brief Create bitmap and immediately create texture (blocking operation)
   * 
   * This is the original method that blocks during texture creation.
   * For performance during ROM loading, consider using CreateBitmapWithoutTexture
   * and deferring texture creation until needed.
   */
  void CreateAndRenderBitmap(int width, int height, int depth,
                             const std::vector<uint8_t> &data,
                             gfx::Bitmap &bitmap, gfx::SnesPalette &palette) {
    bitmap.Create(width, height, depth, data);
    bitmap.SetPalette(palette);
    RenderBitmap(&bitmap);
  }

  /**
   * @brief Create bitmap without creating texture (non-blocking)
   * 
   * This method prepares the bitmap data and surface but doesn't create
   * the GPU texture, allowing for faster bulk operations during ROM loading.
   * Texture creation can be deferred until the bitmap is actually needed
   * for rendering.
   */
  void CreateBitmapWithoutTexture(int width, int height, int depth,
                                  const std::vector<uint8_t> &data,
                                  gfx::Bitmap &bitmap, gfx::SnesPalette &palette) {
    bitmap.Create(width, height, depth, data);
    bitmap.SetPalette(palette);
    // Note: No RenderBitmap call - texture creation is deferred
  }

  /**
   * @brief Batch create textures for multiple bitmaps
   * 
   * This method can be used to efficiently create textures for multiple
   * bitmaps that have already been prepared with CreateBitmapWithoutTexture.
   * Useful for deferred texture creation during ROM loading.
   */
  void BatchCreateTextures(std::vector<gfx::Bitmap*> &bitmaps) {
    for (auto* bitmap : bitmaps) {
      if (bitmap && !bitmap->texture()) {
        bitmap->CreateTexture(renderer_.get());
      }
    }
  }

  void Clear() {
    SDL_SetRenderDrawColor(renderer_.get(), 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer_.get());
  }

  void Present() { SDL_RenderPresent(renderer_.get()); }

 private:
  Renderer() = default;

  std::unique_ptr<SDL_Renderer, SDL_Deleter> renderer_;

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;
};

}  // namespace core
}  // namespace yaze
#endif  // YAZE_CORE_WINDOW_H_