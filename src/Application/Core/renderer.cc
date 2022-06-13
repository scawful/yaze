#include "Renderer.h"

#include <SDL2/SDL.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>
#include <imgui/imgui.h>

#include "Graphics/icons.h"
#include "Graphics/style.h"

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

  // Load available fonts
  const ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Karla-Regular.ttf", 14.0f);

  // merge in icons from Google Material Design
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;
  io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, 18.0f, &icons_config,
                               icons_ranges);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Roboto-Medium.ttf", 14.0f);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Cousine-Regular.ttf", 14.0f);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/DroidSans.ttf", 16.0f);

  // Set the default style
  Style::ColorsYaze();

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