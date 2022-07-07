#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "app/editor/editor.h"
#include "gui/icons.h"
#include "gui/style.h"

int main(int argc, char **argv);

namespace yaze {
namespace app {
namespace core {

class Controller {
 public:
  Controller() = default;

  bool isActive() const;
  void onEntry();
  void onInput();
  void onLoad();
  void doRender() const;
  void onExit() const;

 private:
  void CreateWindow();
  void CreateRenderer();
  void CreateGuiContext() const;
  inline void quit() { active_ = false; }
  friend int ::main(int argc, char **argv);

  struct sdl_deleter {
    void operator()(SDL_Window *p) const { SDL_DestroyWindow(p); }
    void operator()(SDL_Renderer *p) const { SDL_DestroyRenderer(p); }
    void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
  };

  bool active_;
  app::editor::Editor editor_;
  std::shared_ptr<SDL_Window> sdl_window_;
  std::shared_ptr<SDL_Renderer> sdl_renderer_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H