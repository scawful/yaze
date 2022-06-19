#include "controller.h"

#include <SDL2/SDL.h>
#include <imgui/backends/imgui_impl_sdl.h>
#include <imgui/backends/imgui_impl_sdlrenderer.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "Editor/editor.h"

namespace yaze {
namespace application {
namespace Core {

bool Controller::isActive() const { return active_; }

void Controller::onEntry() {
  CreateWindow();
  CreateRenderer();
  CreateGuiContext();
  editor_.SetupScreen(sdl_renderer_);
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

void Controller::doRender() {
  SDL_RenderClear(sdl_renderer_.get());
  ImGui::Render();
  ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderPresent(sdl_renderer_.get());
}

void Controller::onExit() {
  ImGui_ImplSDLRenderer_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}

void Controller::CreateWindow() {
  if (SDL_Init(SDL_INIT_EVERYTHING)) {
    SDL_Log("SDL_Init: %s\n", SDL_GetError());
  } else {
    sdl_window_ = std::unique_ptr<SDL_Window, sdl_deleter>(
        SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                         SDL_WINDOWPOS_UNDEFINED,      // initial x position
                         SDL_WINDOWPOS_UNDEFINED,      // initial y position
                         1200,                         // width, in pixels
                         800,                          // height, in pixels
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL),
        sdl_deleter());
  }
}

void Controller::CreateRenderer() {
  if (sdl_window_ == nullptr) {
    SDL_Log("SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
  } else {
    sdl_renderer_ = std::unique_ptr<SDL_Renderer, sdl_deleter>(
        SDL_CreateRenderer(
            sdl_window_.get(), -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
        sdl_deleter());
    if (sdl_renderer_ == nullptr) {
      SDL_Log("SDL_CreateRenderer: %s\n", SDL_GetError());
      SDL_Quit();
    } else {
      SDL_SetRenderDrawBlendMode(sdl_renderer_.get(), SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(sdl_renderer_.get(), 0x00, 0x00, 0x00, 0x00);
    }
  }
}

void Controller::CreateGuiContext() {
  // Create the ImGui and ImPlot contexts
  ImGui::CreateContext();

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(sdl_window_.get(), sdl_renderer_.get());
  ImGui_ImplSDLRenderer_Init(sdl_renderer_.get());

  // Load available fonts
  const ImGuiIO &io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Karla-Regular.ttf", 14.0f);

  // merge in icons from Google Material Design
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;
  io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, 18.0f, &icons_config,
                               icons_ranges);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Roboto-Medium.ttf", 14.0f);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/Cousine-Regular.ttf", 14.0f);
  io.Fonts->AddFontFromFileTTF("assets/Fonts/DroidSans.ttf", 16.0f);

  // Set the default style
  Style::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer_NewFrame();
  ImGui_ImplSDL2_NewFrame(sdl_window_.get());
}

}  // namespace Core
}  // namespace application
}  // namespace yaze