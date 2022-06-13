#include "controller.h"

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Core/renderer.h"
#include "Core/window.h"
#include "Editor/editor.h"

namespace yaze {
namespace Application {
namespace Core {

bool Controller::isActive() const { return active_; }

void Controller::onEntry() {
  window_.Create();
  renderer_.Create(window_.Get());
  ImGuiIO &io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_Backspace] = SDL_GetScancodeFromKey(SDLK_BACKSPACE);
  io.KeyMap[ImGuiKey_Enter] = SDL_GetScancodeFromKey(SDLK_RETURN);
  io.KeyMap[ImGuiKey_UpArrow] = SDL_GetScancodeFromKey(SDLK_UP);
  io.KeyMap[ImGuiKey_DownArrow] = SDL_GetScancodeFromKey(SDLK_DOWN);
  io.KeyMap[ImGuiKey_Tab] = SDL_GetScancodeFromKey(SDLK_TAB);
  io.KeyMap[ImGuiKey_LeftCtrl] = SDL_GetScancodeFromKey(SDLK_LCTRL);
  active_ = true;
}

void Controller::onInput() {
  int wheel = 0;
  int mouseX;
  int mouseY;
  SDL_Event event;
  ImGuiIO &io = ImGui::GetIO();
  const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_UP:
          case SDLK_DOWN:
          case SDLK_RETURN:
          case SDLK_BACKSPACE:
          case SDLK_TAB:
            io.KeysDown[event.key.keysym.scancode] =
                (event.type == SDL_KEYDOWN);
            break;
          default:
            break;
        }
        break;

      case SDL_KEYUP: {
        int key = event.key.keysym.scancode;
        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[key] = (event.type == SDL_KEYDOWN);
        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
        break;
      }
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_CLOSE:
            quit();
            break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
            io.DisplaySize.x = static_cast<float>(event.window.data1);
            io.DisplaySize.y = static_cast<float>(event.window.data2);
            break;
          default:
            break;
        }
        break;
      case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event.text.text);
        break;
      case SDL_MOUSEWHEEL:
        wheel = event.wheel.y;
        break;
      default:
        break;
    }
  }
  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
  io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
  io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
  io.MouseWheel = static_cast<float>(wheel);
}

void Controller::onLoad() { editor_.UpdateScreen(); }

void Controller::doRender() { renderer_.Render(); }

void Controller::onExit() {
  ImGui_ImplSDLRenderer_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  window_.Destroy();
  renderer_.Destroy();
  SDL_Quit();
}

}  // namespace Core
}  // namespace Application
}  // namespace yaze