#include "app/core/window.h"

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/font_loader.h"
#include "app/core/platform/sdl_deleter.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace yaze {
namespace core {

absl::Status CreateWindow(Window& window, int flags) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init: %s\n", SDL_GetError()));
  }

  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  int screen_width = display_mode.w * 0.8;
  int screen_height = display_mode.h * 0.8;

  window.window_ = std::unique_ptr<SDL_Window, SDL_Deleter>(
      SDL_CreateWindow("Yet Another Zelda3 Editor", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height,
                       flags),
      SDL_Deleter());
  if (window.window_ == nullptr) {
    return absl::InternalError(
        absl::StrFormat("SDL_CreateWindow: %s\n", SDL_GetError()));
  }

  RETURN_IF_ERROR(Renderer::Get().CreateRenderer(window.window_.get()));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui_ImplSDL2_InitForSDLRenderer(window.window_.get(),
                                    Renderer::Get().renderer());
  ImGui_ImplSDLRenderer2_Init(Renderer::Get().renderer());

  RETURN_IF_ERROR(LoadPackageFonts());

  gui::ColorsYaze();

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  const int audio_frequency = 48000;
  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));
  want.freq = audio_frequency;
  want.format = AUDIO_S16;
  want.channels = 2;
  want.samples = 2048;
  want.callback = NULL;  // Uses the queue
  window.audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (window.audio_device_ == 0) {
    throw std::runtime_error(
        absl::StrFormat("Failed to open audio: %s\n", SDL_GetError()));
  }
  window.audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 4);
  SDL_PauseAudioDevice(window.audio_device_, 0);

  return absl::OkStatus();
}

absl::Status ShutdownWindow(Window& window) {
  SDL_PauseAudioDevice(window.audio_device_, 1);
  SDL_CloseAudioDevice(window.audio_device_);
  ImGui_ImplSDL2_Shutdown();
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(Renderer::Get().renderer());
  SDL_DestroyWindow(window.window_.get());
  SDL_Quit();
  return absl::OkStatus();
}

}  // namespace core
}  // namespace yaze