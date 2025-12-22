#include "app/platform/window.h"

#include <filesystem>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/style.h"
#include "app/platform/font_loader.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/sdl_deleter.h"

#ifndef YAZE_USE_SDL3
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#endif

namespace {
// Custom ImGui assertion handler to prevent crashes
void ImGuiAssertionHandler(const char* expr, const char* file, int line,
                           const char* msg) {
  // Log the assertion instead of crashing
  LOG_ERROR("ImGui", "Assertion failed: %s\nFile: %s:%d\nMessage: %s", expr,
            file, line, msg ? msg : "");

  // Try to recover by resetting ImGui state
  static int error_count = 0;
  error_count++;

  if (error_count > 5) {
    LOG_ERROR("ImGui", "Too many assertions, resetting workspace settings...");

    // Backup and reset imgui.ini
    try {
      if (std::filesystem::exists("imgui.ini")) {
        std::filesystem::copy(
            "imgui.ini", "imgui.ini.backup",
            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove("imgui.ini");
        LOG_INFO("ImGui",
                 "Workspace settings reset. Backup saved to imgui.ini.backup");
      }
    } catch (const std::exception& e) {
      LOG_ERROR("ImGui", "Failed to reset workspace: %s", e.what());
    }

    error_count = 0;  // Reset counter
  }

  // Don't abort - let the program continue
  // The assertion is logged and workspace can be reset if needed
}
}  // namespace

namespace yaze {
namespace core {

// Global flag for window resize state (used by emulator to pause)
bool g_window_is_resizing = false;

absl::Status CreateWindow(Window& window, gfx::IRenderer* renderer, int flags) {
#ifdef YAZE_USE_SDL3
  return absl::FailedPreconditionError(
      "Legacy SDL2 window path is unavailable when building with SDL3");
#else
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init: %s\n", SDL_GetError()));
  }

  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  int screen_width = display_mode.w * 0.8;
  int screen_height = display_mode.h * 0.8;

  window.window_ = std::unique_ptr<SDL_Window, util::SDL_Deleter>(
      SDL_CreateWindow("Yet Another Zelda3 Editor", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height,
                       flags),
      util::SDL_Deleter());
  if (window.window_ == nullptr) {
    return absl::InternalError(
        absl::StrFormat("SDL_CreateWindow: %s\n", SDL_GetError()));
  }

  // Only initialize renderer if one is provided and not already initialized
  if (renderer && !renderer->GetBackendRenderer()) {
    if (!renderer->Initialize(window.window_.get())) {
      return absl::InternalError("Failed to initialize renderer");
    }
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Ensure macOS-style behavior (Cmd acts as Ctrl for shortcuts)
#ifdef __APPLE__
  io.ConfigMacOSXBehaviors = true;
#endif

  // Set custom assertion handler to prevent crashes
#ifdef IMGUI_DISABLE_DEFAULT_ASSERT_HANDLER
  ImGui::SetAssertHandler(ImGuiAssertionHandler);
#else
  // For release builds, assertions are already disabled
  LOG_INFO("Window", "ImGui assertions are disabled in this build");
#endif

  // Initialize ImGuiTestEngine after ImGui context is created
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().InitializeUITesting();
#endif

  // Initialize ImGui backends if renderer is provided
  if (renderer) {
    SDL_Renderer* sdl_renderer =
        static_cast<SDL_Renderer*>(renderer->GetBackendRenderer());
    ImGui_ImplSDL2_InitForSDLRenderer(window.window_.get(), sdl_renderer);
    ImGui_ImplSDLRenderer2_Init(sdl_renderer);
  }

  RETURN_IF_ERROR(LoadPackageFonts());

  // Apply original YAZE colors as fallback, then try to load theme system
  gui::ColorsYaze();

  // Audio is now handled by IAudioBackend in Emulator class
  // Keep legacy buffer allocation for backwards compatibility
  if (window.audio_device_ == 0) {
    const int audio_frequency = 48000;
    const size_t buffer_size =
        (audio_frequency / 50) * 2;  // 1920 int16_t for stereo PAL

    // CRITICAL FIX: Allocate buffer as ARRAY, not single value
    // Use new[] with shared_ptr custom deleter for proper array allocation
    window.audio_buffer_ = std::shared_ptr<int16_t>(
        new int16_t[buffer_size], std::default_delete<int16_t[]>());

  // Note: Actual audio device is created by Emulator's IAudioBackend
  // This maintains compatibility with existing code paths
  LOG_INFO(
      "Window",
      "Audio buffer allocated: %zu int16_t samples (backend in Emulator)",
      buffer_size);
  }

  return absl::OkStatus();
#endif  // YAZE_USE_SDL3
}

absl::Status ShutdownWindow(Window& window) {
#ifdef YAZE_USE_SDL3
  return absl::FailedPreconditionError(
      "Legacy SDL2 window path is unavailable when building with SDL3");
#else
  SDL_PauseAudioDevice(window.audio_device_, 1);
  SDL_CloseAudioDevice(window.audio_device_);

  // Stop test engine WHILE ImGui context is still valid
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().StopUITesting();
#endif

  //  TODO: BAD FIX, SLOW SHUTDOWN TAKES TOO LONG NOW
  // CRITICAL FIX: Shutdown graphics arena FIRST
  // This ensures all textures are destroyed while renderer is still valid
  LOG_INFO("Window", "Shutting down graphics arena...");
  gfx::Arena::Get().Shutdown();

  // Shutdown ImGui implementations (after Arena but before context)
  LOG_INFO("Window", "Shutting down ImGui implementations...");
  ImGui_ImplSDL2_Shutdown();
  ImGui_ImplSDLRenderer2_Shutdown();

  // Destroy ImGui context
  LOG_INFO("Window", "Destroying ImGui context...");
  ImGui::DestroyContext();

  // NOW destroy test engine context (after ImGui context is destroyed)
#ifdef YAZE_ENABLE_IMGUI_TEST_ENGINE
  test::TestManager::Get().DestroyUITestingContext();
#endif

  // Finally destroy window
  LOG_INFO("Window", "Destroying window...");
  SDL_DestroyWindow(window.window_.get());

  LOG_INFO("Window", "Shutting down SDL...");
  SDL_Quit();

  LOG_INFO("Window", "Shutdown complete");
  return absl::OkStatus();
#endif  // YAZE_USE_SDL3
}

absl::Status HandleEvents(Window& window) {
#ifdef YAZE_USE_SDL3
  return absl::FailedPreconditionError(
      "Legacy SDL2 window path is unavailable when building with SDL3");
#else
  SDL_Event event;
  ImGuiIO& io = ImGui::GetIO();

  // Protect SDL_PollEvent from crashing the app
  // macOS NSPersistentUIManager corruption can crash during event polling
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type) {
      // Note: Keyboard modifiers are handled by ImGui_ImplSDL2_ProcessEvent
      // which respects ConfigMacOSXBehaviors for Cmd/Ctrl swapping on macOS.
      // Do NOT manually override io.KeyCtrl/KeySuper here.
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_CLOSE:
            window.active_ = false;
            break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
          case SDL_WINDOWEVENT_RESIZED:
            // Update display size for both resize and size_changed events
            io.DisplaySize.x = static_cast<float>(event.window.data1);
            io.DisplaySize.y = static_cast<float>(event.window.data2);
            core::g_window_is_resizing = true;
            break;
          case SDL_WINDOWEVENT_MINIMIZED:
          case SDL_WINDOWEVENT_HIDDEN:
            // Window is minimized/hidden
            g_window_is_resizing = false;
            break;
          case SDL_WINDOWEVENT_RESTORED:
          case SDL_WINDOWEVENT_SHOWN:
          case SDL_WINDOWEVENT_EXPOSED:
            // Window is restored - clear resize flag
            g_window_is_resizing = false;
            break;
        }
        break;
    }
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
#endif  // YAZE_USE_SDL3
}

}  // namespace core
}  // namespace yaze
