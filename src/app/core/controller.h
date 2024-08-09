#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>

#include <memory>

#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imconfig.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/platform/renderer.h"
#include "app/editor/master_editor.h"
#include "app/editor/utils/editor.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"

int main(int argc, char **argv);

namespace yaze {
namespace app {
namespace core {

enum class Platform { kUnknown, kMacOS, kiOS, kWindows, kLinux };

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
  void DoRender() const;
  void OnExit();

  absl::Status CreateSDL_Window();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  absl::Status LoadFontFamilies() const;
  absl::Status LoadAudioDevice();

  void SetupScreen() { master_editor_.SetupScreen(); }
  auto master_editor() -> editor::MasterEditor & { return master_editor_; }
  auto renderer() -> SDL_Renderer * {
    return Renderer::GetInstance().renderer();
  }
  auto window() -> SDL_Window * { return window_.get(); }

 private:
  friend int ::main(int argc, char **argv);

  bool active_;
  Platform platform_;
  editor::MasterEditor master_editor_;

  int audio_frequency_ = 48000;
  int16_t *audio_buffer_;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<SDL_Window> window_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
