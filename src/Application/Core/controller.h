#ifndef YAZE_APPLICATION_CORE_CONTROLLER_H
#define YAZE_APPLICATION_CORE_CONTROLLER_H
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Core/renderer.h"
#include "Core/window.h"
#include "Editor/editor.h"

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
  inline void quit() { active_ = false; }
  friend int ::main(int argc, char** argv);

  bool active_;
  Window window_;
  Renderer renderer_;
  Editor::Editor editor_;
};

}  // namespace Core
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_CORE_CONTROLLER_H