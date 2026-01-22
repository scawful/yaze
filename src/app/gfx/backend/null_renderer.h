// null_renderer.h - Null Renderer Implementation (Headless)

#ifndef YAZE_APP_GFX_BACKEND_NULL_RENDERER_H_
#define YAZE_APP_GFX_BACKEND_NULL_RENDERER_H_

#include "app/gfx/backend/irenderer.h"
#include "util/log.h"

namespace yaze {
namespace gfx {

/**
 * @class NullRenderer
 * @brief Null implementation of the renderer for headless mode.
 */
class NullRenderer : public IRenderer {
 public:
  NullRenderer() = default;
  ~NullRenderer() override = default;

  bool Initialize(SDL_Window* window) override {
    LOG_INFO("NullRenderer", "Initialized headless renderer");
    return true;
  }

  void Shutdown() override {
    LOG_INFO("NullRenderer", "Shutdown headless renderer");
  }

  TextureHandle CreateTexture(int width, int height) override {
    return nullptr;
  }

  TextureHandle CreateTextureWithFormat(int width, int height,
                                        uint32_t format,
                                        int access) override {
    return nullptr;
  }

  void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) override {
    // No-op
  }

  void DestroyTexture(TextureHandle texture) override {
    // No-op
  }

  bool LockTexture(TextureHandle texture, SDL_Rect* rect, void** pixels,
                   int* pitch) override {
    return false;
  }

  void UnlockTexture(TextureHandle texture) override {
    // No-op
  }

  void Clear() override {
    // No-op
  }

  void Present() override {
    // No-op
  }

  void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                  const SDL_Rect* dstrect) override {
    // No-op
  }

  void SetRenderTarget(TextureHandle texture) override {
    // No-op
  }

  void SetDrawColor(SDL_Color color) override {
    // No-op
  }

  void* GetBackendRenderer() override {
    return nullptr;
  }
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_BACKEND_NULL_RENDERER_H_
