// null_window_backend.h - Null Window Backend Implementation (Headless)

#ifndef YAZE_APP_PLATFORM_NULL_WINDOW_BACKEND_H_
#define YAZE_APP_PLATFORM_NULL_WINDOW_BACKEND_H_

#include "app/platform/iwindow.h"

#include <memory>
#include <string>

#include "absl/status/status.h"

namespace yaze {
namespace platform {

/**
 * @brief Null implementation of the window backend for headless mode
 *
 * Implements IWindowBackend but does nothing or minimal work.
 * Useful for CI, servers, and tests where no window/display is available.
 */
class NullWindowBackend : public IWindowBackend {
 public:
  NullWindowBackend() = default;
  ~NullWindowBackend() override = default;

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
  std::string GetTitle() const override { return "Headless"; }
  void SetTitle(const std::string& title) override {}

  bool InitializeRenderer(gfx::IRenderer* renderer) override;
  SDL_Window* GetNativeWindow() override { return nullptr; }

  absl::Status InitializeImGui(gfx::IRenderer* renderer) override;
  void ShutdownImGui() override;
  void NewImGuiFrame() override;
  void RenderImGui(gfx::IRenderer* renderer) override;

  uint32_t GetAudioDevice() const override { return 0; }
  std::shared_ptr<int16_t> GetAudioBuffer() const override { return nullptr; }

  std::string GetBackendName() const override { return "Null"; }
  int GetSDLVersion() const override { return 0; }

 private:
  bool initialized_ = false;
  bool active_ = true;
  bool imgui_initialized_ = false;
  int width_ = 1280;
  int height_ = 720;
};

}  // namespace platform
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_NULL_WINDOW_BACKEND_H_
