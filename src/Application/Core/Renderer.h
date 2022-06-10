#ifndef YAZE_APPLICATION_CORE_RENDERER_H
#define YAZE_APPLICATION_CORE_RENDERER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui/backends/imgui_impl_opengl2.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
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
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  SDL_GLContext* gl_context_ = nullptr;
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_RENDERER_H