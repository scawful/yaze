#ifndef YAZE_APP_CORE_CONTROLLER_H
#define YAZE_APP_CORE_CONTROLLER_H

#include "app/platform/sdl_compat.h"

#include <memory>
#include <mutex>
#include <queue>

#include "absl/status/status.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/irenderer.h"
#include "app/platform/iwindow.h"
#include "rom/rom.h"

int main(int argc, char** argv);

namespace yaze {

class CanvasAutomationServiceImpl;

namespace test {
struct ScreenshotArtifact;
}

/**
 * @brief Main controller for the application.
 *
 * This class is responsible for managing the main window and the
 * main editor. It is the main entry point for the application.
 */
class Controller {
 public:
  struct ScreenshotRequest {
    std::string preferred_path;
    std::function<void(absl::StatusOr<test::ScreenshotArtifact>)> callback;
  };

  bool IsActive() const { return active_; }
  absl::Status OnEntry(std::string filename = "");
  void OnInput();
  absl::Status OnLoad();
  void DoRender() const;
  void OnExit();

  // Defer a screenshot capture to the next render frame (thread-safe)
  void RequestScreenshot(const ScreenshotRequest& request);

  // Set startup editor and cards from command-line flags
  void SetStartupEditor(const std::string& editor_name,
                        const std::string& cards);

  auto window() -> SDL_Window* {
    return window_backend_ ? window_backend_->GetNativeWindow() : nullptr;
  }
  void set_active(bool active) { active_ = active; }
  auto active() const { return active_; }
  auto overworld() -> yaze::zelda3::Overworld* {
    return editor_manager_.overworld();
  }
  auto GetCurrentRom() -> Rom* { return editor_manager_.GetCurrentRom(); }
  auto renderer() -> gfx::IRenderer* { return renderer_.get(); }

  // Test-friendly accessors for GUI testing with ImGuiTestEngine
  editor::EditorManager* editor_manager() { return &editor_manager_; }

  // Window backend accessor
  platform::IWindowBackend* window_backend() { return window_backend_.get(); }

  // Load a ROM file and initialize all editors for testing
  // This performs the full initialization flow including LoadAssets()
  absl::Status LoadRomForTesting(const std::string& rom_path);

#ifdef YAZE_WITH_GRPC
  void SetCanvasAutomationService(CanvasAutomationServiceImpl* service) {
    editor_manager_.SetCanvasAutomationService(service);
  }
#endif

 private:
  friend int ::main(int argc, char** argv);

  bool active_ = false;
  std::unique_ptr<platform::IWindowBackend> window_backend_;
  editor::EditorManager editor_manager_;
  std::unique_ptr<gfx::IRenderer> renderer_;

  // Thread-safe screenshot queue
  mutable std::mutex screenshot_mutex_;
  mutable std::queue<ScreenshotRequest> screenshot_requests_;

  void ProcessScreenshotRequests() const;
};

}  // namespace yaze

#endif  // YAZE_APP_CORE_CONTROLLER_H
