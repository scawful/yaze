#include "Controller.h"

namespace yaze {
namespace Application {
namespace Core {

bool Controller::isActive() const { return active; }

void Controller::onEntry() noexcept(false) {
  window.Create();
  renderer.Create(window.Get());
  ImGuiIO &io = ImGui::GetIO();

  io.KeyMap[ImGuiKey_Backspace] = SDL_GetScancodeFromKey(SDLK_BACKSPACE);
  io.KeyMap[ImGuiKey_Enter] = SDL_GetScancodeFromKey(SDLK_RETURN);
  io.KeyMap[ImGuiKey_UpArrow] = SDL_GetScancodeFromKey(SDLK_UP);
  io.KeyMap[ImGuiKey_DownArrow] = SDL_GetScancodeFromKey(SDLK_DOWN);
  io.KeyMap[ImGuiKey_Tab] = SDL_GetScancodeFromKey(SDLK_TAB);
  active = true;
}

void Controller::onInput() {
  int wheel = 0;
  SDL_Event event;
  ImGuiIO &io = ImGui::GetIO();

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_UP:
      case SDLK_DOWN:
      case SDLK_RETURN:
      case SDLK_BACKSPACE:
      case SDLK_TAB:
        io.KeysDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN);
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
        active = false;
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

  int mouseX;
  int mouseY;
  const int buttons = SDL_GetMouseState(&mouseX, &mouseY);
  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
  io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
  io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
  io.MouseWheel = static_cast<float>(wheel);
}

void Controller::onLoad() { editor.UpdateScreen(); }

void Controller::doRender() { renderer.Render(); }

void Controller::onExit() {
  ImGui_ImplSDLRenderer_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  window.Destroy();
  renderer.Destroy();
  SDL_Quit();
}

} // namespace Core
} // namespace Application
} // namespace yaze