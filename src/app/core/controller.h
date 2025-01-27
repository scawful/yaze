#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/editor/editor.h"
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
  absl::Status OnTestLoad();
  void DoRender() const;
  void OnExit();

  absl::Status CreateWindow();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  absl::Status LoadFontFamilies() const;
  absl::Status LoadAudioDevice();
  absl::Status LoadConfigFiles();

  auto window() -> SDL_Window * { return window_.get(); }
  void init_test_editor(editor::Editor *editor) { test_editor_ = editor; }
  void set_active(bool active) { active_ = active; }
  auto active() const { return active_; }

 private:
  friend int ::main(int argc, char **argv);

  bool active_ = false;
  editor::Editor *test_editor_ = nullptr;
  editor::EditorManager editor_manager_;

  int audio_frequency_ = 48000;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<int16_t> audio_buffer_;
  std::shared_ptr<SDL_Window> window_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
