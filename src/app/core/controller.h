#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/platform/backend.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/editor/editor_manager.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imconfig.h"
#include "imgui/imgui.h"

int main(int argc, char **argv);

namespace yaze {
namespace core {

/**
 * @brief Main controller for the application.
 *
 * This class is responsible for managing the main window and the
 * main editor. It is the main entry point for the application.
 */
class Controller {
 public:
  bool IsActive() const { return active_; }
  absl::Status OnEntry(std::string filename = "");
  void Initialize(std::string filename = "");
  void OnInput();
  absl::Status OnLoad();
  void DoRender() const;
  void OnExit();

  absl::Status CreateWindow();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  absl::Status LoadConfigFiles();

  auto window() -> SDL_Window * { return window_.get(); }
  void set_active(bool active) { active_ = active; }
  auto active() const { return active_; }

 private:
  friend int ::main(int argc, char **argv);

  bool active_ = false;
  editor::EditorManager editor_manager_;
  std::shared_ptr<SDL_Window> window_;
  core::PlatformBackend<Sdl2Backend> backend_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
