#include "controller.h"

#include <SDL.h>

#include <string>

#include "absl/status/status.h"
#include "app/core/timing.h"
#include "app/core/window.h"
#include "app/editor/editor_manager.h"
#include "app/gui/core/background_renderer.h"
#include "app/gfx/resource/arena.h"                  // Add include for Arena
#include "app/gfx/backend/sdl2_renderer.h"  // Add include for new renderer
#include "app/gui/core/theme_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace yaze {
namespace core {

absl::Status Controller::OnEntry(std::string filename) {
  // Create renderer FIRST
  renderer_ = std::make_unique<gfx::SDL2Renderer>();

  // Call CreateWindow with our renderer
  RETURN_IF_ERROR(CreateWindow(window_, renderer_.get(), SDL_WINDOW_RESIZABLE));

  // Initialize the graphics Arena with the renderer
  gfx::Arena::Get().Initialize(renderer_.get());

  // Set up audio for emulator
  editor_manager_.emulator().set_audio_buffer(window_.audio_buffer_.get());
  editor_manager_.emulator().set_audio_device_id(window_.audio_device_);

  // Initialize editor manager with renderer
  editor_manager_.Initialize(renderer_.get(), filename);

  active_ = true;
  return absl::OkStatus();
}

void Controller::SetStartupEditor(const std::string& editor_name,
                                   const std::string& cards) {
  // Process command-line flags for editor and cards
  // Example: --editor=Dungeon --cards="Rooms List,Room 0,Room 105"
  if (!editor_name.empty()) {
    editor_manager_.OpenEditorAndCardsFromFlags(editor_name, cards);
  }
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

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  // Create DockSpace first
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  gui::DockSpaceRenderer::BeginEnhancedDockSpace(
      dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

  editor_manager_.DrawMenuBar();  // Draw the fixed menu bar at the top

  ImGui::End();
#endif
  gui::WidgetIdRegistry::Instance().BeginFrame();
  absl::Status update_status = editor_manager_.Update();
  gui::WidgetIdRegistry::Instance().EndFrame();
  RETURN_IF_ERROR(update_status);
  return absl::OkStatus();
}

void Controller::DoRender() const {
  // Process all pending texture commands (batched to max 8 per frame).
  gfx::Arena::Get().ProcessTextureQueue(renderer_.get());

  ImGui::Render();
  renderer_->Clear();
  ImGui_ImplSDLRenderer2_RenderDrawData(
      ImGui::GetDrawData(),
      static_cast<SDL_Renderer*>(renderer_->GetBackendRenderer()));
  renderer_->Present();

  // Use TimingManager for accurate frame timing in sync with SDL
  float delta_time = TimingManager::Get().Update();

  // Gentle frame rate cap to prevent excessive CPU usage
  // Only delay if we're rendering faster than 144 FPS (< 7ms per frame)
  if (delta_time < 0.007f) {
    SDL_Delay(1);  // Tiny delay to yield CPU without affecting ImGui timing
  }
}

void Controller::OnExit() {
  renderer_->Shutdown();
  PRINT_IF_ERROR(ShutdownWindow(window_));
}

}  // namespace core
}  // namespace yaze
