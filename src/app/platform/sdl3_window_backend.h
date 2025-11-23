// sdl3_window_backend.h - SDL3 Window Backend Implementation

#ifndef YAZE_APP_PLATFORM_SDL3_WINDOW_BACKEND_H_
#define YAZE_APP_PLATFORM_SDL3_WINDOW_BACKEND_H_

// Only compile SDL3 backend when YAZE_USE_SDL3 is defined
#ifdef YAZE_USE_SDL3

#include <SDL3/SDL.h>

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/platform/iwindow.h"

namespace yaze {
namespace platform {

// Forward declaration for unique_ptr custom deleter
struct SDL3WindowDeleter {
  void operator()(SDL_Window* p) const {
    if (p) SDL_DestroyWindow(p);
  }
};

/**
 * @brief SDL3 implementation of the window backend interface
 *
 * Handles the significant event handling changes in SDL3:
 * - Individual window events instead of SDL_WINDOWEVENT
 * - SDL_EVENT_* naming convention
 * - event.key.key instead of event.key.keysym.sym
 * - bool* keyboard state instead of Uint8*
 */
class SDL3WindowBackend : public IWindowBackend {
 public:
  SDL3WindowBackend() = default;
  ~SDL3WindowBackend() override;

  // =========================================================================
  // IWindowBackend Implementation
  // =========================================================================

  absl::Status Initialize(const WindowConfig& config) override;
  absl::Status Shutdown() override;
  bool IsInitialized() const override { return initialized_; }

  bool PollEvent(WindowEvent& out_event) override;
  void ProcessNativeEvent(void* native_event) override;

  WindowStatus GetStatus() const override;
  bool IsActive() const override { return active_; }
  void SetActive(bool active) override { active_ = active; }

  void GetSize(int* width, int* height) const override;
  void SetSize(int width, int height) override;
  std::string GetTitle() const override;
  void SetTitle(const std::string& title) override;

  bool InitializeRenderer(gfx::IRenderer* renderer) override;
  SDL_Window* GetNativeWindow() override { return window_.get(); }

  absl::Status InitializeImGui(gfx::IRenderer* renderer) override;
  void ShutdownImGui() override;
  void NewImGuiFrame() override;

  uint32_t GetAudioDevice() const override { return 0; }  // SDL3 uses streams
  std::shared_ptr<int16_t> GetAudioBuffer() const override {
    return audio_buffer_;
  }

  std::string GetBackendName() const override { return "SDL3"; }
  int GetSDLVersion() const override { return 3; }

 private:
  // Convert SDL3 event to platform-agnostic WindowEvent
  WindowEvent ConvertSDL3Event(const SDL_Event& sdl_event);

  // Update modifier key state from SDL3
  void UpdateModifierState();

  std::unique_ptr<SDL_Window, SDL3WindowDeleter> window_;
  bool initialized_ = false;
  bool active_ = true;
  bool is_resizing_ = false;
  bool imgui_initialized_ = false;

  // Modifier key state
  bool key_shift_ = false;
  bool key_ctrl_ = false;
  bool key_alt_ = false;
  bool key_super_ = false;

  // Legacy audio buffer for compatibility
  std::shared_ptr<int16_t> audio_buffer_;
};

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_USE_SDL3
#endif  // YAZE_APP_PLATFORM_SDL3_WINDOW_BACKEND_H_
