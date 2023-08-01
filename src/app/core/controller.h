#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "absl/status/status.h"
#include "app/editor/master_editor.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"

int main(int argc, char **argv);

namespace yaze {
namespace app {
namespace core {

class Controller {
 public:
  bool IsActive() const;
  absl::Status OnEntry();
  void OnInput();
  void OnLoad();
  void DoRender() const;
  void OnExit() const;

 private:
  struct sdl_deleter {
    void operator()(SDL_Window *p) const { SDL_DestroyWindow(p); }
    void operator()(SDL_Renderer *p) const { SDL_DestroyRenderer(p); }
    void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
  };

  absl::Status CreateWindow();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext() const;
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