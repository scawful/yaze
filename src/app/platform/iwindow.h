// iwindow.h - Window Backend Abstraction Layer
// Provides interface for swapping window implementations (SDL2, SDL3)

#ifndef YAZE_APP_PLATFORM_IWINDOW_H_
#define YAZE_APP_PLATFORM_IWINDOW_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/gfx/backend/irenderer.h"
#include "app/platform/sdl_compat.h"

// Forward declarations to avoid SDL header dependency in interface
struct SDL_Window;

namespace yaze {
namespace platform {

/**
 * @brief Window configuration parameters
 */
struct WindowConfig {
  std::string title = "Yet Another Zelda3 Editor";
  int width = 0;   // 0 means auto-detect from display
  int height = 0;  // 0 means auto-detect from display
  float display_scale = 0.8f;  // Percentage of display to use when auto-detect
  bool resizable = true;
  bool maximized = false;
  bool fullscreen = false;
  bool high_dpi = false;  // Disabled by default - causes issues on macOS Retina with SDL_Renderer
  bool hidden = false;    // Start window hidden (for service mode)
};

/**
 * @brief Window event types (platform-agnostic)
 */
enum class WindowEventType {
  None,
  Close,
  Resize,
  Minimized,
  Maximized,
  Restored,
  Shown,
  Hidden,
  Exposed,
  FocusGained,
  FocusLost,
  KeyDown,
  KeyUp,
  MouseMotion,
  MouseButtonDown,
  MouseButtonUp,
  MouseWheel,
  Quit,
  DropFile
};

/**
 * @brief Platform-agnostic window event data
 */
struct WindowEvent {
  WindowEventType type = WindowEventType::None;

  // Window resize data
  int window_width = 0;
  int window_height = 0;

  // Keyboard data
  int key_code = 0;
  int scan_code = 0;
  bool key_shift = false;
  bool key_ctrl = false;
  bool key_alt = false;
  bool key_super = false;

  // Mouse data
  float mouse_x = 0.0f;
  float mouse_y = 0.0f;
  int mouse_button = 0;
  float wheel_x = 0.0f;
  float wheel_y = 0.0f;

  // Drop file data
  std::string dropped_file;

  // Native event copy (SDL2/SDL3). Only valid when has_native_event is true.
  bool has_native_event = false;
  SDL_Event native_event{};
};

/**
 * @brief Window backend status information
 */
struct WindowStatus {
  bool is_active = true;
  bool is_minimized = false;
  bool is_maximized = false;
  bool is_fullscreen = false;
  bool is_focused = true;
  bool is_resizing = false;
  int width = 0;
  int height = 0;
};

/**
 * @brief Abstract window backend interface
 *
 * Provides platform-agnostic window management, allowing different
 * SDL versions or other windowing libraries to be swapped without
 * changing application code.
 */
class IWindowBackend {
 public:
  virtual ~IWindowBackend() = default;

  // =========================================================================
  // Lifecycle Management
  // =========================================================================

  /**
   * @brief Initialize the window backend with configuration
   * @param config Window configuration parameters
   * @return Status indicating success or failure
   */
  virtual absl::Status Initialize(const WindowConfig& config) = 0;

  /**
   * @brief Shutdown the window backend and release resources
   * @return Status indicating success or failure
   */
  virtual absl::Status Shutdown() = 0;

  /**
   * @brief Check if the backend is initialized
   */
  virtual bool IsInitialized() const = 0;

  // =========================================================================
  // Event Processing
  // =========================================================================

  /**
   * @brief Poll and process pending events
   * @param out_event Output parameter for the next event
   * @return True if an event was available, false otherwise
   */
  virtual bool PollEvent(WindowEvent& out_event) = 0;

  /**
   * @brief Process a native SDL event (for ImGui integration)
   * @param native_event Pointer to native SDL_Event
   */
  virtual void ProcessNativeEvent(void* native_event) = 0;

  // =========================================================================
  // Window State
  // =========================================================================

  /**
   * @brief Get current window status
   */
  virtual WindowStatus GetStatus() const = 0;

  /**
   * @brief Check if window is still active (not closed)
   */
  virtual bool IsActive() const = 0;

  /**
   * @brief Set window active state
   */
  virtual void SetActive(bool active) = 0;

  /**
   * @brief Get window dimensions
   */
  virtual void GetSize(int* width, int* height) const = 0;

  /**
   * @brief Set window dimensions
   */
  virtual void SetSize(int width, int height) = 0;

  /**
   * @brief Get window title
   */
  virtual std::string GetTitle() const = 0;

  /**
   * @brief Set window title
   */
  virtual void SetTitle(const std::string& title) = 0;

  /**
   * @brief Show the window
   */
  virtual void ShowWindow() = 0;

  /**
   * @brief Hide the window
   */
  virtual void HideWindow() = 0;

  // =========================================================================
  // Renderer Integration
  // =========================================================================

  /**
   * @brief Initialize renderer for this window
   * @param renderer The renderer to initialize
   * @return True if successful
   */
  virtual bool InitializeRenderer(gfx::IRenderer* renderer) = 0;

  /**
   * @brief Get the underlying SDL_Window pointer for ImGui integration
   * @return Native window handle (SDL_Window*)
   */
  virtual SDL_Window* GetNativeWindow() = 0;

  // =========================================================================
  // ImGui Integration
  // =========================================================================

  /**
   * @brief Initialize ImGui backends for this window/renderer combo
   * @param renderer The renderer (for backend-specific init)
   * @return Status indicating success or failure
   */
  virtual absl::Status InitializeImGui(gfx::IRenderer* renderer) = 0;

  /**
   * @brief Shutdown ImGui backends
   */
  virtual void ShutdownImGui() = 0;

  /**
   * @brief Start a new ImGui frame
   */
  virtual void NewImGuiFrame() = 0;

  /**
   * @brief Render ImGui draw data (and viewports if enabled)
   * @param renderer The renderer to use for drawing (needed to get backend renderer)
   */
  virtual void RenderImGui(gfx::IRenderer* renderer) = 0;

  // =========================================================================
  // Audio Support (Legacy compatibility)
  // =========================================================================

  /**
   * @brief Get audio device ID (for legacy audio buffer management)
   */
  virtual uint32_t GetAudioDevice() const = 0;

  /**
   * @brief Get audio buffer (for legacy audio management)
   */
  virtual std::shared_ptr<int16_t> GetAudioBuffer() const = 0;

  // =========================================================================
  // Backend Information
  // =========================================================================

  /**
   * @brief Get backend name for debugging/logging
   */
  virtual std::string GetBackendName() const = 0;

  /**
   * @brief Get SDL version being used
   */
  virtual int GetSDLVersion() const = 0;
};

/**
 * @brief Backend type enumeration for factory
 */
enum class WindowBackendType {
  SDL2,
  SDL3,
  IOS,
  Null,  // Headless/Server mode
  Auto  // Automatically select based on availability
};

/**
 * @brief Factory for creating window backends
 */
class WindowBackendFactory {
 public:
  /**
   * @brief Create a window backend of the specified type
   * @param type The type of backend to create
   * @return Unique pointer to the created backend
   */
  static std::unique_ptr<IWindowBackend> Create(WindowBackendType type);

  /**
   * @brief Get the default backend type for this build
   */
  static WindowBackendType GetDefaultType();

  /**
   * @brief Check if a backend type is available
   */
  static bool IsAvailable(WindowBackendType type);
};

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_IWINDOW_H_
