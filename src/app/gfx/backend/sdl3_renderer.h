#ifndef YAZE_APP_GFX_BACKEND_SDL3_RENDERER_H_
#define YAZE_APP_GFX_BACKEND_SDL3_RENDERER_H_

#ifdef YAZE_USE_SDL3

#include <SDL3/SDL.h>

#include <memory>

#include "app/gfx/backend/irenderer.h"

namespace yaze {
namespace gfx {

/**
 * @class SDL3Renderer
 * @brief A concrete implementation of the IRenderer interface using SDL3.
 *
 * This class encapsulates all rendering logic specific to the SDL3 renderer API.
 * It translates the abstract calls from the IRenderer interface into concrete
 * SDL3 commands.
 *
 * Key SDL3 API differences from SDL2:
 * - SDL_CreateRenderer() takes a driver name (nullptr for auto) instead of index
 * - SDL_RenderCopy() is replaced by SDL_RenderTexture()
 * - Many functions now use SDL_FRect (float) instead of SDL_Rect (int)
 * - SDL_FreeSurface() is replaced by SDL_DestroySurface()
 * - SDL_ConvertSurfaceFormat() is replaced by SDL_ConvertSurface()
 * - Surface pixel format access uses SDL_GetPixelFormatDetails()
 */
class SDL3Renderer : public IRenderer {
 public:
  SDL3Renderer();
  ~SDL3Renderer() override;

  // --- Lifecycle and Initialization ---
  bool Initialize(SDL_Window* window) override;
  void Shutdown() override;

  // --- Texture Management ---
  TextureHandle CreateTexture(int width, int height) override;
  TextureHandle CreateTextureWithFormat(int width, int height, uint32_t format,
                                        int access) override;
  void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) override;
  void DestroyTexture(TextureHandle texture) override;

  // --- Direct Pixel Access ---
  bool LockTexture(TextureHandle texture, SDL_Rect* rect, void** pixels,
                   int* pitch) override;
  void UnlockTexture(TextureHandle texture) override;

  // --- Rendering Primitives ---
  void Clear() override;
  void Present() override;
  void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect,
                  const SDL_Rect* dstrect) override;
  void SetRenderTarget(TextureHandle texture) override;
  void SetDrawColor(SDL_Color color) override;

  /**
   * @brief Provides access to the underlying SDL_Renderer*.
   * @return A void pointer that can be safely cast to an SDL_Renderer*.
   */
  void* GetBackendRenderer() override { return renderer_; }

 private:
  /**
   * @brief Convert SDL_Rect (int) to SDL_FRect (float) for SDL3 API calls.
   * @param rect Pointer to SDL_Rect to convert, may be nullptr.
   * @param frect Output SDL_FRect.
   * @return Pointer to frect if rect was valid, nullptr otherwise.
   */
  static SDL_FRect* ToFRect(const SDL_Rect* rect, SDL_FRect* frect);

  // The core SDL3 renderer object.
  // Unlike SDL2Renderer, we don't use a custom deleter because SDL3 has
  // different cleanup semantics and we want explicit control over shutdown.
  SDL_Renderer* renderer_ = nullptr;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_USE_SDL3

#endif  // YAZE_APP_GFX_BACKEND_SDL3_RENDERER_H_
