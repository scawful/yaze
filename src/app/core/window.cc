#include "app/core/window.h"

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/font_loader.h"
#include "app/core/platform/sdl_deleter.h"
#include "app/gfx/arena.h"
#include "app/gui/style.h"
#include "app/gui/theme_manager.h"
#include "app/test/test_manager.h"
#include "util/log.h"
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

  // Initialize ImGuiTestEngine after ImGui context is created
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().InitializeUITesting();
#endif

  ImGui_ImplSDL2_InitForSDLRenderer(window.window_.get(),
                                    Renderer::Get().renderer());
  ImGui_ImplSDLRenderer2_Init(Renderer::Get().renderer());

  RETURN_IF_ERROR(LoadPackageFonts());

  // Apply original YAZE colors as fallback, then try to load theme system
  gui::ColorsYaze();
  
  // Try to initialize theme system (will fallback to ColorsYaze if files fail)
  auto& theme_manager = gui::ThemeManager::Get();
  auto status = theme_manager.LoadTheme("YAZE Classic");
  if (!status.ok()) {
    // Theme system failed, stick with original ColorsYaze()
    util::logf("Theme system failed, using original ColorsYaze(): %s", status.message().data());
  }

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
  
  // Stop test engine WHILE ImGui context is still valid
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().StopUITesting();
#endif
  
  // Shutdown ImGui implementations
  ImGui_ImplSDL2_Shutdown();
  ImGui_ImplSDLRenderer2_Shutdown();
  
  // Destroy ImGui context
  ImGui::DestroyContext();
  
  // NOW destroy test engine context (after ImGui context is destroyed)
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().DestroyUITestingContext();
#endif
  
  // Shutdown graphics arena BEFORE destroying SDL contexts
  gfx::Arena::Get().Shutdown();
  
  SDL_DestroyRenderer(Renderer::Get().renderer());
  SDL_DestroyWindow(window.window_.get());
  SDL_Quit();
  return absl::OkStatus();
}

absl::Status HandleEvents(Window& window) {
  SDL_Event event;
  ImGuiIO& io = ImGui::GetIO();
  SDL_WaitEvent(&event);
  ImGui_ImplSDL2_ProcessEvent(&event);
  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
      io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
      io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
      io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
      break;
    }
    case SDL_WINDOWEVENT:
      switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          window.active_ = false;
          break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          io.DisplaySize.x = static_cast<float>(event.window.data1);
          io.DisplaySize.y = static_cast<float>(event.window.data2);
          break;
      }
      break;
  }
  int mouseX;
  int mouseY;
  const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
  io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
  io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
  io.MouseDown[2] = buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE);

  int wheel = 0;
  io.MouseWheel = static_cast<float>(wheel);
  return absl::OkStatus();
}

}  // namespace core
}  // namespace yaze