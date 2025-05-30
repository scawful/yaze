#ifndef YAZE_APP_CORE_PLATFORM_RENDERER_H
#define YAZE_APP_CORE_PLATFORM_RENDERER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/sdl_deleter.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace core {

/**
 * @class Renderer
 * @brief The Renderer class represents the renderer for the Yaze application.
 *
 * This class is a singleton that provides functionality for creating and
 * rendering bitmaps to the screen. It also includes methods for updating
 * bitmaps on the screen.
 */
class Renderer {
 public:
  static Renderer &Get() {
    static Renderer instance;
    return instance;
  }

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

  void RenderBitmap(gfx::Bitmap *bitmap) {
    bitmap->CreateTexture(renderer_.get());
  }

  void UpdateBitmap(gfx::Bitmap *bitmap) {
    bitmap->UpdateTexture(renderer_.get());
  }

  void CreateAndRenderBitmap(int width, int height, int depth,
                             const std::vector<uint8_t> &data,
                             gfx::Bitmap &bitmap, gfx::SnesPalette &palette) {
    bitmap.Create(width, height, depth, data);
    bitmap.SetPalette(palette);
    RenderBitmap(&bitmap);
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

#endif
