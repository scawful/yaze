#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/window.h"
#include "app/editor/editor_manager.h"

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
  void OnInput();
  absl::Status OnLoad();
  void DoRender() const;
  void OnExit();

  auto window() -> SDL_Window * { return window_.window_.get(); }
  void set_active(bool active) { active_ = active; }
  auto active() const { return active_; }
  auto overworld() -> yaze::zelda3::Overworld* { return editor_manager_.overworld(); }

 private:
  friend int ::main(int argc, char **argv);

  bool active_ = false;
  core::Window window_;
  editor::EditorManager editor_manager_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
