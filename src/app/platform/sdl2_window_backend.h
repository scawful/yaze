// sdl2_window_backend.h - SDL2 Window Backend Implementation

#ifndef YAZE_APP_PLATFORM_SDL2_WINDOW_BACKEND_H_
#define YAZE_APP_PLATFORM_SDL2_WINDOW_BACKEND_H_

#include "app/platform/sdl_compat.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/platform/iwindow.h"
#include "util/sdl_deleter.h"

namespace yaze {
namespace platform {

/**
 * @brief SDL2 implementation of the window backend interface
 *
 * Wraps SDL2 window management, event handling, and ImGui integration
 * for the main YAZE application window.
 */
class SDL2WindowBackend : public IWindowBackend {
 public:
  SDL2WindowBackend() = default;
  ~SDL2WindowBackend() override;

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

  void ShowWindow() override;
  void HideWindow() override;

  bool InitializeRenderer(gfx::IRenderer* renderer) override;
  SDL_Window* GetNativeWindow() override { return window_.get(); }

  absl::Status InitializeImGui(gfx::IRenderer* renderer) override;
  void ShutdownImGui() override;
  void NewImGuiFrame() override;
  void RenderImGui(gfx::IRenderer* renderer) override;

  uint32_t GetAudioDevice() const override { return audio_device_; }
  std::shared_ptr<int16_t> GetAudioBuffer() const override {
    return audio_buffer_;
  }

  std::string GetBackendName() const override { return "SDL2"; }
  int GetSDLVersion() const override { return 2; }

 private:
  // Convert SDL2 event to platform-agnostic WindowEvent
  WindowEvent ConvertSDL2Event(const SDL_Event& sdl_event);

  // Update modifier key state from SDL
  void UpdateModifierState();

  std::unique_ptr<SDL_Window, util::SDL_Deleter> window_;
  bool initialized_ = false;
  bool active_ = true;
  bool is_resizing_ = false;
  bool imgui_initialized_ = false;

  // Modifier key state
  bool key_shift_ = false;
  bool key_ctrl_ = false;
  bool key_alt_ = false;
  bool key_super_ = false;

  // Legacy audio support
  SDL_AudioDeviceID audio_device_ = 0;
  std::shared_ptr<int16_t> audio_buffer_;
};

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_SDL2_WINDOW_BACKEND_H_
