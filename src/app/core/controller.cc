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
  Initialize(filename);
  return absl::OkStatus();
}

void Controller::Initialize(std::string filename) {
  editor_manager_.Initialize(filename);
  active_ = true;
}

void Controller::OnInput() {
  ImGuiIO &io = ImGui::GetIO();
  SDL_Event event;

  SDL_WaitEvent(&event);
  ImGui_ImplSDL2_ProcessEvent(&event);
  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
      io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
      io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
      io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
      break;
    }
    case SDL_WINDOWEVENT:
      switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
          active_ = false;
          break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          io.DisplaySize.x = static_cast<float>(event.window.data1);
          io.DisplaySize.y = static_cast<float>(event.window.data2);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  int mouseX;
  int mouseY;
  const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
  io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
  io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
  io.MouseDown[2] = buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE);

  int wheel = 0;
  io.MouseWheel = static_cast<float>(wheel);
}

absl::Status Controller::OnLoad() {
  if (editor_manager_.quit()) {
    active_ = false;
  }

#if TARGET_OS_IPHONE != 1
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
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

  // Create DockSpace
  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                   ImGuiDockNodeFlags_PassthruCentralNode);

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

void Controller::OnExit() {
  PRINT_IF_ERROR(ShutdownWindow(window_));
}

}  // namespace core
}  // namespace yaze
