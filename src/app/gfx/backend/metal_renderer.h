#pragma once

#include "app/gfx/backend/irenderer.h"

#include <unordered_map>
#include <vector>

namespace yaze {
namespace gfx {

class MetalRenderer final : public IRenderer {
 public:
  MetalRenderer() = default;
  ~MetalRenderer() override;

  bool Initialize(SDL_Window* window) override;
  void Shutdown() override;

  TextureHandle CreateTexture(int width, int height) override;
  TextureHandle CreateTextureWithFormat(int width, int height, uint32_t format,
                                        int access) override;
  void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) override;
  void DestroyTexture(TextureHandle texture) override;
  bool LockTexture(TextureHandle texture, SDL_Rect* rect, void** pixels,
                   int* pitch) override;
  void UnlockTexture(TextureHandle texture) override;

  void Clear() override;
  void Present() override;
  void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                  const SDL_Rect* dstrect) override;
  void SetRenderTarget(TextureHandle texture) override;
  void SetDrawColor(SDL_Color color) override;
  void* GetBackendRenderer() override;

  void SetMetalView(void* view);

 private:
  struct StagingBuffer {
    std::vector<uint8_t> data;
    int pitch = 0;
    int width = 0;
    int height = 0;
  };

  void* metal_view_ = nullptr;
  void* command_queue_ = nullptr;
  TextureHandle render_target_ = nullptr;
  std::unordered_map<TextureHandle, StagingBuffer> staging_buffers_;
};

}  // namespace gfx
}  // namespace yaze
