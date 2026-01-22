// sdl3_window_backend.cc - SDL3 Window Backend Implementation

// Only compile SDL3 backend when YAZE_USE_SDL3 is defined
#ifdef YAZE_USE_SDL3

#include "app/platform/sdl3_window_backend.h"

#include <SDL3/SDL.h>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/style.h"
#include "app/platform/font_loader.h"
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlrenderer3.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {

// Forward reference to the global resize flag defined in window.cc
namespace core {
extern bool g_window_is_resizing;
}

namespace platform {

// Alias to core's resize flag for compatibility
#define g_window_is_resizing yaze::core::g_window_is_resizing

SDL3WindowBackend::~SDL3WindowBackend() {
  if (initialized_) {
    Shutdown();
  }
}

absl::Status SDL3WindowBackend::Initialize(const WindowConfig& config) {
  if (initialized_) {
    LOG_WARN("SDL3WindowBackend", "Already initialized, shutting down first");
    RETURN_IF_ERROR(Shutdown());
  }

  // Initialize SDL3 subsystems
  // Note: SDL3 removed SDL_INIT_TIMER (timer is always available)
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init failed: %s", SDL_GetError()));
  }

  // Determine window size
  int screen_width = config.width;
  int screen_height = config.height;

  if (screen_width == 0 || screen_height == 0) {
    // Auto-detect from display
    // SDL3 uses SDL_GetPrimaryDisplay() and SDL_GetCurrentDisplayMode()
    SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_id);

    if (mode) {
      screen_width = static_cast<int>(mode->w * config.display_scale);
      screen_height = static_cast<int>(mode->h * config.display_scale);
    } else {
      // Fallback to reasonable defaults
      screen_width = 1280;
      screen_height = 720;
      LOG_WARN("SDL3WindowBackend",
               "Failed to get display mode, using defaults: %dx%d",
               screen_width, screen_height);
    }
  }

  // Build window flags
  // Note: SDL3 changed some flag names
  SDL_WindowFlags flags = 0;
  if (config.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }
  if (config.maximized) {
    flags |= SDL_WINDOW_MAXIMIZED;
  }
  if (config.fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }
  if (config.high_dpi) {
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  }
  if (config.hidden) {
    flags |= SDL_WINDOW_HIDDEN;
  }

  // Create window
  // Note: SDL3 uses SDL_CreateWindow with different signature
  SDL_Window* raw_window = SDL_CreateWindow(config.title.c_str(), screen_width,
                                            screen_height, flags);

  if (!raw_window) {
    SDL_Quit();
    return absl::InternalError(
        absl::StrFormat("SDL_CreateWindow failed: %s", SDL_GetError()));
  }

  window_ = std::unique_ptr<SDL_Window, SDL3WindowDeleter>(raw_window);

  // Allocate legacy audio buffer for backwards compatibility
  const int audio_frequency = 48000;
  const size_t buffer_size = (audio_frequency / 50) * 2;  // Stereo PAL
  audio_buffer_ = std::shared_ptr<int16_t>(new int16_t[buffer_size],
                                           std::default_delete<int16_t[]>());

  LOG_INFO("SDL3WindowBackend", "Initialized: %dx%d, audio buffer: %zu samples",
           screen_width, screen_height, buffer_size);

  initialized_ = true;
  active_ = true;
  return absl::OkStatus();
}

absl::Status SDL3WindowBackend::Shutdown() {
  if (!initialized_) {
    return absl::OkStatus();
  }

  // Shutdown ImGui if initialized
  if (imgui_initialized_) {
    ShutdownImGui();
  }

  // Shutdown graphics arena while renderer is still valid
  LOG_INFO("SDL3WindowBackend", "Shutting down graphics arena...");
  gfx::Arena::Get().Shutdown();

  // Destroy window
  if (window_) {
    LOG_INFO("SDL3WindowBackend", "Destroying window...");
    window_.reset();
  }

  // Quit SDL
  LOG_INFO("SDL3WindowBackend", "Shutting down SDL...");
  SDL_Quit();

  initialized_ = false;
  LOG_INFO("SDL3WindowBackend", "Shutdown complete");
  return absl::OkStatus();
}

bool SDL3WindowBackend::PollEvent(WindowEvent& out_event) {
  SDL_Event sdl_event;
  if (SDL_PollEvent(&sdl_event)) {
    // Let ImGui process the event first
    if (imgui_initialized_) {
      ImGui_ImplSDL3_ProcessEvent(&sdl_event);
    }

    // Convert to platform-agnostic event
    out_event = ConvertSDL3Event(sdl_event);
    out_event.has_native_event = true;
    out_event.native_event = sdl_event;
    return true;
  }
  return false;
}

void SDL3WindowBackend::ProcessNativeEvent(void* native_event) {
  if (native_event && imgui_initialized_) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event*>(native_event));
  }
}

WindowEvent SDL3WindowBackend::ConvertSDL3Event(const SDL_Event& sdl_event) {
  WindowEvent event;
  event.type = WindowEventType::None;

  switch (sdl_event.type) {
    // =========================================================================
    // Application Events
    // =========================================================================
    case SDL_EVENT_QUIT:
      event.type = WindowEventType::Quit;
      active_ = false;
      break;

    // =========================================================================
    // Keyboard Events
    // Note: SDL3 uses event.key.key instead of event.key.keysym.sym
    // =========================================================================
    case SDL_EVENT_KEY_DOWN:
      event.type = WindowEventType::KeyDown;
      event.key_code = sdl_event.key.key;
      event.scan_code = sdl_event.key.scancode;
      UpdateModifierState();
      event.key_shift = key_shift_;
      event.key_ctrl = key_ctrl_;
      event.key_alt = key_alt_;
      event.key_super = key_super_;
      break;

    case SDL_EVENT_KEY_UP:
      event.type = WindowEventType::KeyUp;
      event.key_code = sdl_event.key.key;
      event.scan_code = sdl_event.key.scancode;
      UpdateModifierState();
      event.key_shift = key_shift_;
      event.key_ctrl = key_ctrl_;
      event.key_alt = key_alt_;
      event.key_super = key_super_;
      break;

    // =========================================================================
    // Mouse Events
    // Note: SDL3 uses float coordinates
    // =========================================================================
    case SDL_EVENT_MOUSE_MOTION:
      event.type = WindowEventType::MouseMotion;
      event.mouse_x = sdl_event.motion.x;
      event.mouse_y = sdl_event.motion.y;
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      event.type = WindowEventType::MouseButtonDown;
      event.mouse_x = sdl_event.button.x;
      event.mouse_y = sdl_event.button.y;
      event.mouse_button = sdl_event.button.button;
      break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
      event.type = WindowEventType::MouseButtonUp;
      event.mouse_x = sdl_event.button.x;
      event.mouse_y = sdl_event.button.y;
      event.mouse_button = sdl_event.button.button;
      break;

    case SDL_EVENT_MOUSE_WHEEL:
      event.type = WindowEventType::MouseWheel;
      event.wheel_x = sdl_event.wheel.x;
      event.wheel_y = sdl_event.wheel.y;
      break;

    // =========================================================================
    // Drop Events
    // =========================================================================
    case SDL_EVENT_DROP_FILE:
      event.type = WindowEventType::DropFile;
      if (sdl_event.drop.data) {
        event.dropped_file = sdl_event.drop.data;
        // Note: SDL3 drop.data is managed by SDL, don't free it
      }
      break;

    // =========================================================================
    // Window Events - SDL3 Major Change
    // SDL3 no longer uses SDL_WINDOWEVENT with sub-types.
    // Each window event type is now a top-level event.
    // =========================================================================
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      event.type = WindowEventType::Close;
      active_ = false;
      break;

    case SDL_EVENT_WINDOW_RESIZED:
      event.type = WindowEventType::Resize;
      event.window_width = sdl_event.window.data1;
      event.window_height = sdl_event.window.data2;
      is_resizing_ = true;
      g_window_is_resizing = true;
      break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
      // This is the SDL3 equivalent of SDL_WINDOWEVENT_SIZE_CHANGED
      event.type = WindowEventType::Resize;
      event.window_width = sdl_event.window.data1;
      event.window_height = sdl_event.window.data2;
      is_resizing_ = true;
      g_window_is_resizing = true;
      break;

    case SDL_EVENT_WINDOW_MINIMIZED:
      event.type = WindowEventType::Minimized;
      is_resizing_ = false;
      g_window_is_resizing = false;
      break;

    case SDL_EVENT_WINDOW_MAXIMIZED:
      event.type = WindowEventType::Maximized;
      break;

    case SDL_EVENT_WINDOW_RESTORED:
      event.type = WindowEventType::Restored;
      is_resizing_ = false;
      g_window_is_resizing = false;
      break;

    case SDL_EVENT_WINDOW_SHOWN:
      event.type = WindowEventType::Shown;
      is_resizing_ = false;
      g_window_is_resizing = false;
      break;

    case SDL_EVENT_WINDOW_HIDDEN:
      event.type = WindowEventType::Hidden;
      is_resizing_ = false;
      g_window_is_resizing = false;
      break;

    case SDL_EVENT_WINDOW_EXPOSED:
      event.type = WindowEventType::Exposed;
      is_resizing_ = false;
      g_window_is_resizing = false;
      break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
      event.type = WindowEventType::FocusGained;
      break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
      event.type = WindowEventType::FocusLost;
      break;
  }

  return event;
}

void SDL3WindowBackend::UpdateModifierState() {
  // SDL3 uses SDL_GetModState which returns SDL_Keymod
  SDL_Keymod mod = SDL_GetModState();
  key_shift_ = (mod & SDL_KMOD_SHIFT) != 0;
  key_ctrl_ = (mod & SDL_KMOD_CTRL) != 0;
  key_alt_ = (mod & SDL_KMOD_ALT) != 0;
  key_super_ = (mod & SDL_KMOD_GUI) != 0;
}

WindowStatus SDL3WindowBackend::GetStatus() const {
  WindowStatus status;
  status.is_active = active_;
  status.is_resizing = is_resizing_;

  if (window_) {
    SDL_WindowFlags flags = SDL_GetWindowFlags(window_.get());
    status.is_minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    status.is_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    status.is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    status.is_focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

    SDL_GetWindowSize(window_.get(), &status.width, &status.height);
  }

  return status;
}

void SDL3WindowBackend::GetSize(int* width, int* height) const {
  if (window_) {
    SDL_GetWindowSize(window_.get(), width, height);
  } else {
    if (width)
      *width = 0;
    if (height)
      *height = 0;
  }
}

void SDL3WindowBackend::SetSize(int width, int height) {
  if (window_) {
    SDL_SetWindowSize(window_.get(), width, height);
  }
}

std::string SDL3WindowBackend::GetTitle() const {
  if (window_) {
    const char* title = SDL_GetWindowTitle(window_.get());
    return title ? title : "";
  }
  return "";
}

void SDL3WindowBackend::SetTitle(const std::string& title) {
  if (window_) {
    SDL_SetWindowTitle(window_.get(), title.c_str());
  }
}

void SDL3WindowBackend::ShowWindow() {
  if (window_) {
    SDL_ShowWindow(window_.get());
  }
}

void SDL3WindowBackend::HideWindow() {
  if (window_) {
    SDL_HideWindow(window_.get());
  }
}

bool SDL3WindowBackend::InitializeRenderer(gfx::IRenderer* renderer) {
  if (!window_ || !renderer) {
    return false;
  }

  if (renderer->GetBackendRenderer()) {
    // Already initialized
    return true;
  }

  return renderer->Initialize(window_.get());
}

absl::Status SDL3WindowBackend::InitializeImGui(gfx::IRenderer* renderer) {
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
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  // Ensure macOS-style behavior (Cmd acts as Ctrl for shortcuts)
#ifdef __APPLE__
  io.ConfigMacOSXBehaviors = true;
#endif

  // Initialize ImGui backends for SDL3
  SDL_Renderer* sdl_renderer =
      static_cast<SDL_Renderer*>(renderer->GetBackendRenderer());

  if (!sdl_renderer) {
    return absl::InternalError("Failed to get SDL renderer from IRenderer");
  }

  // Note: SDL3 uses different ImGui backend functions
  if (!ImGui_ImplSDL3_InitForSDLRenderer(window_.get(), sdl_renderer)) {
    return absl::InternalError("ImGui_ImplSDL3_InitForSDLRenderer failed");
  }

  if (!ImGui_ImplSDLRenderer3_Init(sdl_renderer)) {
    ImGui_ImplSDL3_Shutdown();
    return absl::InternalError("ImGui_ImplSDLRenderer3_Init failed");
  }

  // Load fonts
  RETURN_IF_ERROR(LoadPackageFonts());

  // Apply default style
  gui::ColorsYaze();

  imgui_initialized_ = true;
  LOG_INFO("SDL3WindowBackend", "ImGui initialized successfully");
  return absl::OkStatus();
}

void SDL3WindowBackend::ShutdownImGui() {
  if (!imgui_initialized_) {
    return;
  }

  LOG_INFO("SDL3WindowBackend", "Shutting down ImGui implementations...");
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();

  LOG_INFO("SDL3WindowBackend", "Destroying ImGui context...");
  ImGui::DestroyContext();

  imgui_initialized_ = false;
}

void SDL3WindowBackend::NewImGuiFrame() {
  if (!imgui_initialized_) {
    return;
  }

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
}

void SDL3WindowBackend::RenderImGui(gfx::IRenderer* renderer) {
  if (!imgui_initialized_) {
    return;
  }

  // Finalize ImGui frame and render draw data
  ImGui::Render();

  if (renderer) {
    SDL_Renderer* sdl_renderer =
        static_cast<SDL_Renderer*>(renderer->GetBackendRenderer());
    if (sdl_renderer) {
      ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer);
    }
  }

  // Multi-viewport support
  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_USE_SDL3
