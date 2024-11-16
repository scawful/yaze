#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/platform/renderer.h"
#include "app/core/utils/file_util.h"
#include "app/editor/editor_manager.h"
#include "app/editor/utils/editor.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imconfig.h"
#include "imgui/imgui.h"

int main(int argc, char **argv);

namespace yaze {
namespace app {
namespace core {

/**
 * @brief Main controller for the application.
 *
 * This class is responsible for managing the main window and the
 * main editor. It is the main entry point for the application.
 */
class Controller : public ExperimentFlags {
 public:
  bool IsActive() const { return active_; }
  absl::Status OnEntry(std::string filename = "");
  void OnInput();
  absl::Status OnLoad();
  absl::Status OnTestLoad();
  void DoRender() const;
  void OnExit();

  absl::Status CreateWindow();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  absl::Status LoadFontFamilies() const;
  absl::Status LoadAudioDevice();
  absl::Status LoadConfigFiles();

  void SetupScreen(std::string filename = "") {
    editor_manager_.SetupScreen(filename);
  }
  auto editor_manager() -> editor::EditorManager & { return editor_manager_; }
  auto renderer() -> SDL_Renderer * {
    return Renderer::GetInstance().renderer();
  }
  auto window() -> SDL_Window * { return window_.get(); }
  void init_test_editor(editor::Editor *editor) { test_editor_ = editor; }
  void set_active(bool active) { active_ = active; }

 private:
  friend int ::main(int argc, char **argv);

  bool active_;
  Platform platform_;
  editor::Editor *test_editor_;
  editor::EditorManager editor_manager_;

  int audio_frequency_ = 48000;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<int16_t> audio_buffer_;
  std::shared_ptr<SDL_Window> window_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
