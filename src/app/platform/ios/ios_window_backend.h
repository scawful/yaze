#pragma once

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/platform/iwindow.h"

namespace yaze {
namespace platform {

class IOSWindowBackend final : public IWindowBackend {
 public:
  IOSWindowBackend() = default;
  ~IOSWindowBackend() override = default;

  absl::Status Initialize(const WindowConfig& config) override;
  absl::Status Shutdown() override;
  bool IsInitialized() const override;

  bool PollEvent(WindowEvent& out_event) override;
  void ProcessNativeEvent(void* native_event) override;

  WindowStatus GetStatus() const override;
  bool IsActive() const override;
  void SetActive(bool active) override;
  void GetSize(int* width, int* height) const override;
  void SetSize(int width, int height) override;
  std::string GetTitle() const override;
  void SetTitle(const std::string& title) override;

  bool InitializeRenderer(gfx::IRenderer* renderer) override;
  SDL_Window* GetNativeWindow() override;

  absl::Status InitializeImGui(gfx::IRenderer* renderer) override;
  void ShutdownImGui() override;
  void NewImGuiFrame() override;
  void RenderImGui(gfx::IRenderer* renderer) override;

  uint32_t GetAudioDevice() const override;
  std::shared_ptr<int16_t> GetAudioBuffer() const override;

  std::string GetBackendName() const override;
  int GetSDLVersion() const override;

 private:
  bool initialized_ = false;
  bool imgui_initialized_ = false;
  WindowStatus status_{};
  std::string title_;
  void* metal_view_ = nullptr;
  void* command_queue_ = nullptr;
};

}  // namespace platform
}  // namespace yaze
