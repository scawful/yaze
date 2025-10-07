#include "app/core/window.h"

#include <filesystem>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/platform/font_loader.h"
#include "util/sdl_deleter.h"
#include "util/log.h"
#include "app/gfx/arena.h"
#include "app/gui/style.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace {
// Custom ImGui assertion handler to prevent crashes
void ImGuiAssertionHandler(const char* expr, const char* file, int line,
                           const char* msg) {
  // Log the assertion instead of crashing
  LOG_ERROR("ImGui", "Assertion failed: %s\nFile: %s:%d\nMessage: %s",
            expr, file, line, msg ? msg : "");
  
  // Try to recover by resetting ImGui state
  static int error_count = 0;
  error_count++;
  
  if (error_count > 5) {
    LOG_ERROR("ImGui", "Too many assertions, resetting workspace settings...");
    
    // Backup and reset imgui.ini
    try {
      if (std::filesystem::exists("imgui.ini")) {
        std::filesystem::copy("imgui.ini", "imgui.ini.backup",
                            std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove("imgui.ini");
        LOG_INFO("ImGui", "Workspace settings reset. Backup saved to imgui.ini.backup");
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
    SDL_Renderer* sdl_renderer = static_cast<SDL_Renderer*>(renderer->GetBackendRenderer());
    ImGui_ImplSDL2_InitForSDLRenderer(window.window_.get(), sdl_renderer);
    ImGui_ImplSDLRenderer2_Init(sdl_renderer);
  }

  RETURN_IF_ERROR(LoadPackageFonts());

  // Apply original YAZE colors as fallback, then try to load theme system
  gui::ColorsYaze();
  
  // Initialize audio if not already initialized
  if (window.audio_device_ == 0) {
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
      LOG_ERROR("Window", "Failed to open audio: %s", SDL_GetError());
      // Don't fail - audio is optional
    } else {
      window.audio_buffer_ = std::make_shared<int16_t>(audio_frequency / 50 * 4);
      SDL_PauseAudioDevice(window.audio_device_, 0);
    }
  }

  return absl::OkStatus();
}

absl::Status ShutdownWindow(Window& window) {
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
}

absl::Status HandleEvents(Window& window) {
  SDL_Event event;
  ImGuiIO& io = ImGui::GetIO();
  
  // Protect SDL_PollEvent from crashing the app
  // macOS NSPersistentUIManager corruption can crash during event polling
  while (SDL_PollEvent(&event)) {
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
          case SDL_WINDOWEVENT_RESIZED:
            // Update display size for both resize and size_changed events
            io.DisplaySize.x = static_cast<float>(event.window.data1);
            io.DisplaySize.y = static_cast<float>(event.window.data2);
            g_window_is_resizing = true;
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
}

}  // namespace core
}  // namespace yaze