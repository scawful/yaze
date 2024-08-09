#ifndef YAZE_APP_CORE_PLATFORM_RENDERER_H
#define YAZE_APP_CORE_PLATFORM_RENDERER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/utils/sdl_deleter.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {
namespace core {

/**
 * @class Renderer
 * @brief The Renderer class represents the renderer for the Yaze application.
 *
 * This class is a singleton that provides functionality for creating and
 * rendering bitmaps to the screen. It also includes methods for updating
 * bitmaps on the screen.
 *
 * The Renderer class uses the ExperimentFlags class to access the application's
 * flags.
 */
class Renderer : public ExperimentFlags {
 public:
  static Renderer& GetInstance() {
    static Renderer instance;
    return instance;
  }

  absl::Status CreateRenderer(SDL_Window* window) {
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

  auto renderer() -> SDL_Renderer* { return renderer_.get(); }

  /**
   * @brief Used to render a bitmap to the screen.
   */
  void RenderBitmap(gfx::Bitmap* bitmap) {
    if (flags()->kLoadTexturesAsStreaming) {
      bitmap->CreateTexture(renderer_.get());
    } else {
      // bitmap->CreateTexture(renderer_);
    }
  }

  /**
   * @brief Used to update a bitmap on the screen.
   */
  void UpdateBitmap(gfx::Bitmap* bitmap, bool use_sdl_update = false) {
    if (flags()->kLoadTexturesAsStreaming) {
      bitmap->UpdateTexture(renderer_.get(), use_sdl_update);
    } else {
      // bitmap->UpdateTexture(renderer_);
    }
  }

  absl::Status CreateAndRenderBitmap(int width, int height, int depth,
                                     const Bytes& data, gfx::Bitmap& bitmap,
                                     gfx::SnesPalette& palette) {
    bitmap.Create(width, height, depth, data);
    RETURN_IF_ERROR(bitmap.ApplyPalette(palette));
    RenderBitmap(&bitmap);
    return absl::OkStatus();
  }

 private:
  Renderer() = default;

  std::unique_ptr<SDL_Renderer, SDL_Deleter> renderer_;

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif