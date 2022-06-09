#ifndef YAZE_APPLICATION_CORE_RENDERER_H
#define YAZE_APPLICATION_CORE_RENDERER_H

#include <SDL2/SDL.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace Application {
namespace Core {

  class Renderer {
    public:
      void Create(SDL_Window* window);
      void Render();
      void Destroy();

    private:
      SDL_Renderer* renderer = nullptr;
  };

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_RENDERER_H