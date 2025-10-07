#pragma once

#include "app/gfx/backend/irenderer.h"
#include "util/sdl_deleter.h"

namespace yaze {
namespace gfx {

/**
 * @class SDL2Renderer
 * @brief A concrete implementation of the IRenderer interface using SDL2.
 *
 * This class encapsulates all rendering logic that is specific to the SDL2_render API.
 * It translates the abstract calls from the IRenderer interface into concrete SDL2 commands.
 * This is the first step in abstracting the renderer, allowing the rest of the application
 * to be independent of SDL2.
 */
class SDL2Renderer : public IRenderer {
public:
    SDL2Renderer();
    ~SDL2Renderer() override;

    // --- Lifecycle and Initialization ---
    bool Initialize(SDL_Window* window) override;
    void Shutdown() override;

    // --- Texture Management ---
    TextureHandle CreateTexture(int width, int height) override;
    void UpdateTexture(TextureHandle texture, const Bitmap& bitmap) override;
    void DestroyTexture(TextureHandle texture) override;

    // --- Direct Pixel Access ---
    bool LockTexture(TextureHandle texture, SDL_Rect* rect, void** pixels, int* pitch) override;
    void UnlockTexture(TextureHandle texture) override;

    // --- Rendering Primitives ---
    void Clear() override;
    void Present() override;
    void RenderCopy(TextureHandle texture, const SDL_Rect* srcrect, const SDL_Rect* dstrect) override;
    void SetRenderTarget(TextureHandle texture) override;
    void SetDrawColor(SDL_Color color) override;

    /**
     * @brief Provides access to the underlying SDL_Renderer*.
     * @return A void pointer that can be safely cast to an SDL_Renderer*.
     */
    void* GetBackendRenderer() override { return renderer_.get(); }

private:
    // The core SDL2 renderer object, managed by a unique_ptr with a custom deleter.
    std::unique_ptr<SDL_Renderer, util::SDL_Deleter> renderer_;
};

} // namespace gfx
} // namespace yaze
