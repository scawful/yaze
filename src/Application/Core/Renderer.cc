#include "Renderer.h"

namespace yaze {
namespace Application {
namespace Core {

void Renderer::Create(SDL_Window* window) {
  if (window == nullptr) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
  } else {
    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
      SDL_Log("SDL_CreateRenderer: %s\n", SDL_GetError());
      SDL_Quit();
    } else {
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    }
  }

  // Create the ImGui and ImPlot contexts
  ImGui::CreateContext();
  
  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer_Init(renderer);

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer_NewFrame();
  ImGui_ImplSDL2_NewFrame(window);
}

void Renderer::Render() {
  SDL_RenderClear(renderer);
  ImGui::Render();
  ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderPresent(renderer);
}

void Renderer::Destroy() {
  SDL_DestroyRenderer(renderer);
  renderer = nullptr;
}


}  // namespace Core
}  // namespace Application
}  // namespace yaze