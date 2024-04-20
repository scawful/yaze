#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include <SDL.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/editor/master_editor.h"
#include "app/editor/utils/editor.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"

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
  absl::Status OnEntry();
  void OnInput();
  void OnLoad();
  void PlayAudio();
  void DoRender() const;
  void OnExit();

 private:
  struct sdl_deleter {
    void operator()(SDL_Window *p) const {
      if (p) {
        SDL_DestroyWindow(p);
      }
    }
    void operator()(SDL_Renderer *p) const {
      if (p) {
        SDL_DestroyRenderer(p);
      }
    }
    void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
  };

  absl::Status CreateSDL_Window();
  absl::Status CreateRenderer();
  absl::Status CreateGuiContext();
  absl::Status LoadFontFamilies() const;
  absl::Status LoadAudioDevice();
  void CloseWindow() { active_ = false; }

  friend int ::main(int argc, char **argv);

  bool active_;
  int wanted_samples_;
  int audio_frequency_ = 48000;
  int16_t *audio_buffer_;
  editor::MasterEditor master_editor_;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<SDL_Window> window_;
  std::shared_ptr<SDL_Renderer> renderer_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H