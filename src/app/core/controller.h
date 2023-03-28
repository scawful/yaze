#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "absl/status/status.h"
#include "app/editor/master_editor.h"
#include "gui/icons.h"
#include "gui/style.h"

int main(int argc, char **argv);

namespace yaze {
namespace app {
namespace core {

class Controller {
 public:
  bool isActive() const;
  absl::Status onEntry();
  void onInput();
  void onLoad();
  void doRender() const;
  void onExit() const;

 private:
  struct sdl_deleter {
    void operator()(SDL_Window *p) const { SDL_DestroyWindow(p); }
    void operator()(SDL_Renderer *p) const { SDL_DestroyRenderer(p); }
    void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
  };

  absl::Status CreateWindow();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  void CloseWindow() { active_ = false; }

  friend int ::main(int argc, char **argv);

  bool active_;
  editor::MasterEditor master_editor_;
  std::shared_ptr<SDL_Window> window_;
  std::shared_ptr<SDL_Renderer> renderer_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H