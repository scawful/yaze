#ifndef YAZE_APP_CORE_PLATFORM_BACKEND_H
#define YAZE_APP_CORE_PLATFORM_BACKEND_H

#include <SDL.h>

#include <memory>
#include <stdexcept>

#include "app/core/platform/renderer.h"
#include "app/core/platform/sdl_deleter.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"

namespace yaze {
namespace core {

template <typename Derived>
class PlatformBackend {
 public:
  PlatformBackend() = default;
  ~PlatformBackend() = default;

  void init() { static_cast<Derived *>(this)->init(); }
  void init_audio() { static_cast<Derived *>(this)->init_audio(); }
  void shutdown_audio() { static_cast<Derived *>(this)->shutdown_audio(); }
  void shutdown() { static_cast<Derived *>(this)->shutdown(); }
  void new_frame() { static_cast<Derived *>(this)->new_frame(); }
  void render() { static_cast<Derived *>(this)->render(); }
};

class Sdl2Backend : public PlatformBackend<Sdl2Backend> {
 public:
  Sdl2Backend() = default;
  ~Sdl2Backend() = default;

  void init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) !=
        0) {
      throw std::runtime_error("Failed to initialize SDL2");
    }

    SDL_DisplayMode display_mode;
    SDL_GetCurrentDisplayMode(0, &display_mode);
    int screen_width = display_mode.w * 0.8;
    int screen_height = display_mode.h * 0.8;

    window = std::unique_ptr<SDL_Window, core::SDL_Deleter>(
        SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                         SDL_WINDOWPOS_UNDEFINED,      // initial x position
                         SDL_WINDOWPOS_UNDEFINED,      // initial y position
                         screen_width,                 // width, in pixels
                         screen_height,                // height, in pixels
                         SDL_WINDOW_RESIZABLE),
        core::SDL_Deleter());
    if (!window) {
      throw std::runtime_error("Failed to create window");
    }

    if (!Renderer::GetInstance().CreateRenderer(window.get()).ok()) {
      throw std::runtime_error("Failed to create renderer");
    }

    ImGui_ImplSDL2_InitForSDLRenderer(window.get(),
                                      Renderer::GetInstance().renderer());
    ImGui_ImplSDLRenderer2_Init(Renderer::GetInstance().renderer());
  }

  void init_audio() {
    const int audio_frequency = 48000;
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = audio_frequency;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 2048;
    want.callback = NULL;  // Uses the queue
    audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_device_ == 0) {
      throw std::runtime_error(
          absl::StrFormat("Failed to open audio: %s\n", SDL_GetError()));
    }
    audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 4);
    SDL_PauseAudioDevice(audio_device_, 0);
  }

  void shutdown_audio() {
    SDL_PauseAudioDevice(audio_device_, 1);
    SDL_CloseAudioDevice(audio_device_);
  }

  void shutdown() {
    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplSDLRenderer2_Shutdown();
    SDL_DestroyRenderer(Renderer::GetInstance().renderer());
    SDL_DestroyWindow(window.get());
    SDL_Quit();
  }

  void new_frame() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
  }

  void render() {
    ImGui::Render();
    SDL_RenderClear(Renderer::GetInstance().renderer());
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),
                                          Renderer::GetInstance().renderer());
    SDL_RenderPresent(Renderer::GetInstance().renderer());
  }

  auto window_ptr() -> SDL_Window * { return window.get(); }
  auto audio_device() -> SDL_AudioDeviceID { return audio_device_; }
  auto audio_buffer() -> std::shared_ptr<int16_t> { return audio_buffer_; }

 private:
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<int16_t> audio_buffer_;
  std::unique_ptr<SDL_Window, SDL_Deleter> window;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_BACKEND_H
