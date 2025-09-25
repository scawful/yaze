#include "controller.h"

#include <SDL.h>

#include "absl/status/status.h"
#include "app/core/window.h"
#include "app/editor/editor_manager.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace yaze {
namespace core {

absl::Status Controller::OnEntry(std::string filename) {
  RETURN_IF_ERROR(CreateWindow(window_, SDL_WINDOW_RESIZABLE));
  editor_manager_.emulator().set_audio_buffer(window_.audio_buffer_.get());
  editor_manager_.emulator().set_audio_device_id(window_.audio_device_);
  editor_manager_.Initialize(filename);
  active_ = true;
  return absl::OkStatus();
}

void Controller::OnInput() {
  PRINT_IF_ERROR(HandleEvents(window_));
}

absl::Status Controller::OnLoad() {
  if (editor_manager_.quit() || !window_.active_) {
    active_ = false;
    return absl::OkStatus();
  }

#if TARGET_OS_IPHONE != 1
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Simple window setup without docking for now
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("MainWindow", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  editor_manager_.DrawMenuBar();  // Draw the fixed menu bar at the top

  ImGui::End();
#endif
  RETURN_IF_ERROR(editor_manager_.Update());
  return absl::OkStatus();
}

void Controller::DoRender() const {
  ImGui::Render();
  SDL_RenderClear(Renderer::Get().renderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),
                                        Renderer::Get().renderer());
  SDL_RenderPresent(Renderer::Get().renderer());
}

void Controller::OnExit() { PRINT_IF_ERROR(ShutdownWindow(window_)); }

}  // namespace core
}  // namespace yaze
