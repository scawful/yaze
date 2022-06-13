#ifndef YAZE_APPLICATION_CORE_RENDERER_H
#define YAZE_APPLICATION_CORE_RENDERER_H

#include <SDL2/SDL.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>
#include <imgui/imgui.h>

#include "Graphics/icons.h"
#include "Graphics/style.h"

namespace yaze {
namespace Application {
namespace Core {

static SDL_Renderer* renderer = nullptr;

class Renderer {
 public:
  void Create(SDL_Window* window);
  void Render();
  void Destroy();
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_RENDERER_H