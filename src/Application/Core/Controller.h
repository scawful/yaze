#ifndef YAZE_APPLICATION_CORE_CONTROLLER_H
#define YAZE_APPLICATION_CORE_CONTROLLER_H

#include <memory>

#include <SDL2/SDL.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_sdlrenderer.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include "Window.h"
#include "Renderer.h"
#include "Events/Event.h"
#include "View/Editor.h"

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
  void onLoad() const;
  void doRender();
  void onExit();

 private:
  Window window;  
  Renderer renderer;
  std::unique_ptr<View::Editor> editor;
  bool active = false;
  void quit() { active = false; }
  friend int ::main(int argc, char** argv);
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_CONTROLLER_H