#ifndef YAZE_APPLICATION_CORE_CONTROLLER_H
#define YAZE_APPLICATION_CORE_CONTROLLER_H
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>

#include <memory>

#include "Events/Event.h"
#include "Renderer.h"
#include "Editor/Editor.h"
#include "Window.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

int main(int argc, char** argv);

namespace yaze {
namespace Application {
namespace Core {

class Controller {
 public:
  Controller() = default;

  bool isActive() const;

  void onEntry();
  void onInput();
  void onLoad();
  void doRender();
  void onExit();

 private:
  Window window;
  Renderer renderer;
  View::Editor editor;
  bool active = false;
  void quit() { active = false; }
  friend int ::main(int argc, char** argv);
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_CONTROLLER_H