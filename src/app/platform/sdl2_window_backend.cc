// sdl2_window_backend.cc - SDL2 Window Backend Implementation

#include "app/platform/sdl2_window_backend.h"

#include "app/platform/sdl_compat.h"

#include <filesystem>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/style.h"
#include "app/platform/font_loader.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace platform {

// Global flag for window resize state (used by emulator to pause)
// This maintains compatibility with the legacy window.cc
extern bool g_window_is_resizing;

SDL2WindowBackend::~SDL2WindowBackend() {
  if (initialized_) {
    Shutdown();
  }
}

absl::Status SDL2WindowBackend::Initialize(const WindowConfig& config) {
  if (initialized_) {
    LOG_WARN("SDL2WindowBackend", "Already initialized, shutting down first");
    RETURN_IF_ERROR(Shutdown());
  }

  // Initialize SDL2 subsystems
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init failed: %s", SDL_GetError()));
  }

  // Determine window size
  int screen_width = config.width;
  int screen_height = config.height;

  if (screen_width == 0 || screen_height == 0) {
    // Auto-detect from display
    SDL_DisplayMode display_mode;
    if (SDL_GetCurrentDisplayMode(0, &display_mode) == 0) {
      screen_width = static_cast<int>(display_mode.w * config.display_scale);
      screen_height = static_cast<int>(display_mode.h * config.display_scale);
    } else {
      // Fallback to reasonable defaults
      screen_width = 1280;
      screen_height = 720;
      LOG_WARN("SDL2WindowBackend",
               "Failed to get display mode, using defaults: %dx%d",
               screen_width, screen_height);
    }
  }

  // Build window flags
  Uint32 flags = 0;
  if (config.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }
  if (config.maximized) {
    flags |= SDL_WINDOW_MAXIMIZED;
  }
  if (config.fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  }
  if (config.high_dpi) {
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
  }

  // Create window
  window_ = std::unique_ptr<SDL_Window, util::SDL_Deleter>(
      SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height,
                       flags),
      util::SDL_Deleter());

  if (!window_) {
    SDL_Quit();
    return absl::InternalError(
        absl::StrFormat("SDL_CreateWindow failed: %s", SDL_GetError()));
  }

  // Allocate legacy audio buffer for backwards compatibility
  const int audio_frequency = 48000;
  const size_t buffer_size = (audio_frequency / 50) * 2;  // Stereo PAL
  audio_buffer_ = std::shared_ptr<int16_t>(new int16_t[buffer_size],
                                           std::default_delete<int16_t[]>());

  LOG_INFO("SDL2WindowBackend",
           "Initialized: %dx%d, audio buffer: %zu samples", screen_width,
           screen_height, buffer_size);

  initialized_ = true;
  active_ = true;
  return absl::OkStatus();
}

absl::Status SDL2WindowBackend::Shutdown() {
  if (!initialized_) {
    return absl::OkStatus();
  }

  // Pause and close audio device if open
  if (audio_device_ != 0) {
    SDL_PauseAudioDevice(audio_device_, 1);
    SDL_CloseAudioDevice(audio_device_);
    audio_device_ = 0;
  }

  // Shutdown ImGui if initialized
  if (imgui_initialized_) {
    ShutdownImGui();
  }

  // Shutdown graphics arena while renderer is still valid
  LOG_INFO("SDL2WindowBackend", "Shutting down graphics arena...");
  gfx::Arena::Get().Shutdown();

  // Destroy window
  if (window_) {
    LOG_INFO("SDL2WindowBackend", "Destroying window...");
    window_.reset();
  }

  // Quit SDL
  LOG_INFO("SDL2WindowBackend", "Shutting down SDL...");
  SDL_Quit();

  initialized_ = false;
  LOG_INFO("SDL2WindowBackend", "Shutdown complete");
  return absl::OkStatus();
}

bool SDL2WindowBackend::PollEvent(WindowEvent& out_event) {
  SDL_Event sdl_event;
  if (SDL_PollEvent(&sdl_event)) {
    // Let ImGui process the event first
    if (imgui_initialized_) {
      ImGui_ImplSDL2_ProcessEvent(&sdl_event);
    }

    // Convert to platform-agnostic event
    out_event = ConvertSDL2Event(sdl_event);
    return true;
  }
  return false;
}

void SDL2WindowBackend::ProcessNativeEvent(void* native_event) {
  if (native_event && imgui_initialized_) {
    ImGui_ImplSDL2_ProcessEvent(static_cast<SDL_Event*>(native_event));
  }
}

WindowEvent SDL2WindowBackend::ConvertSDL2Event(const SDL_Event& sdl_event) {
  WindowEvent event;
  event.type = WindowEventType::None;

  switch (sdl_event.type) {
    case SDL_QUIT:
      event.type = WindowEventType::Quit;
      active_ = false;
      break;

    case SDL_KEYDOWN:
      event.type = WindowEventType::KeyDown;
      event.key_code = sdl_event.key.keysym.sym;
      event.scan_code = sdl_event.key.keysym.scancode;
      UpdateModifierState();
      event.key_shift = key_shift_;
      event.key_ctrl = key_ctrl_;
      event.key_alt = key_alt_;
      event.key_super = key_super_;
      break;

    case SDL_KEYUP:
      event.type = WindowEventType::KeyUp;
      event.key_code = sdl_event.key.keysym.sym;
      event.scan_code = sdl_event.key.keysym.scancode;
      UpdateModifierState();
      event.key_shift = key_shift_;
      event.key_ctrl = key_ctrl_;
      event.key_alt = key_alt_;
      event.key_super = key_super_;
      break;

    case SDL_MOUSEMOTION:
      event.type = WindowEventType::MouseMotion;
      event.mouse_x = static_cast<float>(sdl_event.motion.x);
      event.mouse_y = static_cast<float>(sdl_event.motion.y);
      break;

    case SDL_MOUSEBUTTONDOWN:
      event.type = WindowEventType::MouseButtonDown;
      event.mouse_x = static_cast<float>(sdl_event.button.x);
      event.mouse_y = static_cast<float>(sdl_event.button.y);
      event.mouse_button = sdl_event.button.button;
      break;

    case SDL_MOUSEBUTTONUP:
      event.type = WindowEventType::MouseButtonUp;
      event.mouse_x = static_cast<float>(sdl_event.button.x);
      event.mouse_y = static_cast<float>(sdl_event.button.y);
      event.mouse_button = sdl_event.button.button;
      break;

    case SDL_MOUSEWHEEL:
      event.type = WindowEventType::MouseWheel;
      event.wheel_x = static_cast<float>(sdl_event.wheel.x);
      event.wheel_y = static_cast<float>(sdl_event.wheel.y);
      break;

    case SDL_DROPFILE:
      event.type = WindowEventType::DropFile;
      if (sdl_event.drop.file) {
        event.dropped_file = sdl_event.drop.file;
        SDL_free(sdl_event.drop.file);
      }
      break;

    case SDL_WINDOWEVENT:
      switch (sdl_event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          event.type = WindowEventType::Close;
          active_ = false;
          break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_RESIZED:
          event.type = WindowEventType::Resize;
          event.window_width = sdl_event.window.data1;
          event.window_height = sdl_event.window.data2;
          is_resizing_ = true;
          g_window_is_resizing = true;
          break;

        case SDL_WINDOWEVENT_MINIMIZED:
          event.type = WindowEventType::Minimized;
          is_resizing_ = false;
          g_window_is_resizing = false;
          break;

        case SDL_WINDOWEVENT_MAXIMIZED:
          event.type = WindowEventType::Maximized;
          break;

        case SDL_WINDOWEVENT_RESTORED:
          event.type = WindowEventType::Restored;
          is_resizing_ = false;
          g_window_is_resizing = false;
          break;

        case SDL_WINDOWEVENT_SHOWN:
          event.type = WindowEventType::Shown;
          is_resizing_ = false;
          g_window_is_resizing = false;
          break;

        case SDL_WINDOWEVENT_HIDDEN:
          event.type = WindowEventType::Hidden;
          is_resizing_ = false;
          g_window_is_resizing = false;
          break;

        case SDL_WINDOWEVENT_EXPOSED:
          event.type = WindowEventType::Exposed;
          is_resizing_ = false;
          g_window_is_resizing = false;
          break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
          event.type = WindowEventType::FocusGained;
          break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
          event.type = WindowEventType::FocusLost;
          break;
      }
      break;
  }

  return event;
}

void SDL2WindowBackend::UpdateModifierState() {
  SDL_Keymod mod = SDL_GetModState();
  key_shift_ = (mod & KMOD_SHIFT) != 0;
  key_ctrl_ = (mod & KMOD_CTRL) != 0;
  key_alt_ = (mod & KMOD_ALT) != 0;
  key_super_ = (mod & KMOD_GUI) != 0;
}

WindowStatus SDL2WindowBackend::GetStatus() const {
  WindowStatus status;
  status.is_active = active_;
  status.is_resizing = is_resizing_;

  if (window_) {
    Uint32 flags = SDL_GetWindowFlags(window_.get());
    status.is_minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    status.is_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    status.is_fullscreen =
        (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
    status.is_focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

    SDL_GetWindowSize(window_.get(), &status.width, &status.height);
  }

  return status;
}

void SDL2WindowBackend::GetSize(int* width, int* height) const {
  if (window_) {
    SDL_GetWindowSize(window_.get(), width, height);
  } else {
    if (width) *width = 0;
    if (height) *height = 0;
  }
}

void SDL2WindowBackend::SetSize(int width, int height) {
  if (window_) {
    SDL_SetWindowSize(window_.get(), width, height);
  }
}

std::string SDL2WindowBackend::GetTitle() const {
  if (window_) {
    const char* title = SDL_GetWindowTitle(window_.get());
    return title ? title : "";
  }
  return "";
}

void SDL2WindowBackend::SetTitle(const std::string& title) {
  if (window_) {
    SDL_SetWindowTitle(window_.get(), title.c_str());
  }
}

bool SDL2WindowBackend::InitializeRenderer(gfx::IRenderer* renderer) {
  if (!window_ || !renderer) {
    return false;
  }

  if (renderer->GetBackendRenderer()) {
    // Already initialized
    return true;
  }

  return renderer->Initialize(window_.get());
}

absl::Status SDL2WindowBackend::InitializeImGui(gfx::IRenderer* renderer) {
  if (imgui_initialized_) {
    return absl::OkStatus();
  }

  if (!renderer) {
    return absl::InvalidArgumentError("Renderer is null");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Initialize ImGui backends
  SDL_Renderer* sdl_renderer =
      static_cast<SDL_Renderer*>(renderer->GetBackendRenderer());

  if (!sdl_renderer) {
    return absl::InternalError("Failed to get SDL renderer from IRenderer");
  }

  if (!ImGui_ImplSDL2_InitForSDLRenderer(window_.get(), sdl_renderer)) {
    return absl::InternalError("ImGui_ImplSDL2_InitForSDLRenderer failed");
  }

  if (!ImGui_ImplSDLRenderer2_Init(sdl_renderer)) {
    ImGui_ImplSDL2_Shutdown();
    return absl::InternalError("ImGui_ImplSDLRenderer2_Init failed");
  }

  // Load fonts
  RETURN_IF_ERROR(LoadPackageFonts());

  // Apply default style
  gui::ColorsYaze();

  imgui_initialized_ = true;
  LOG_INFO("SDL2WindowBackend", "ImGui initialized successfully");
  return absl::OkStatus();
}

void SDL2WindowBackend::ShutdownImGui() {
  if (!imgui_initialized_) {
    return;
  }

  LOG_INFO("SDL2WindowBackend", "Shutting down ImGui implementations...");
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();

  LOG_INFO("SDL2WindowBackend", "Destroying ImGui context...");
  ImGui::DestroyContext();

  imgui_initialized_ = false;
}

void SDL2WindowBackend::NewImGuiFrame() {
  if (!imgui_initialized_) {
    return;
  }

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
}

// Define the global variable for backward compatibility
bool g_window_is_resizing = false;

}  // namespace platform
}  // namespace yaze
