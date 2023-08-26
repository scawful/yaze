#include "controller.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/master_editor.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"

namespace yaze {
namespace app {
namespace core {

namespace {

void InitializeKeymap() {
  ImGuiIO &io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_Backspace] = SDL_GetScancodeFromKey(SDLK_BACKSPACE);
  io.KeyMap[ImGuiKey_LeftShift] = SDL_GetScancodeFromKey(SDLK_LSHIFT);
  io.KeyMap[ImGuiKey_Enter] = SDL_GetScancodeFromKey(SDLK_RETURN);
  io.KeyMap[ImGuiKey_UpArrow] = SDL_GetScancodeFromKey(SDLK_UP);
  io.KeyMap[ImGuiKey_DownArrow] = SDL_GetScancodeFromKey(SDLK_DOWN);
  io.KeyMap[ImGuiKey_Tab] = SDL_GetScancodeFromKey(SDLK_TAB);
  io.KeyMap[ImGuiKey_LeftCtrl] = SDL_GetScancodeFromKey(SDLK_LCTRL);
}

void ImGui_ImplSDL2_SetClipboardText(void *user_data, const char *text) {
  SDL_SetClipboardText(text);
}

const char *ImGui_ImplSDL2_GetClipboardText(void *user_data) {
  return SDL_GetClipboardText();
}

void InitializeClipboard() {
  ImGuiIO &io = ImGui::GetIO();
  io.SetClipboardTextFn = ImGui_ImplSDL2_SetClipboardText;
  io.GetClipboardTextFn = ImGui_ImplSDL2_GetClipboardText;
  io.ClipboardUserData = nullptr;
}

void HandleKeyDown(SDL_Event &event) {
  ImGuiIO &io = ImGui::GetIO();
  switch (event.key.keysym.sym) {
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_RETURN:
    case SDLK_BACKSPACE:
    case SDLK_LSHIFT:
    case SDLK_LCTRL:
    case SDLK_TAB:
      io.KeysDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN);
      break;
    default:
      break;
  }
}

void HandleKeyUp(SDL_Event &event) {
  ImGuiIO &io = ImGui::GetIO();
  int key = event.key.keysym.scancode;
  IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
  io.KeysDown[key] = (event.type == SDL_KEYDOWN);
  io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
  io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
  io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
  io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
}

void ChangeWindowSizeEvent(SDL_Event &event) {
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = static_cast<float>(event.window.data1);
  io.DisplaySize.y = static_cast<float>(event.window.data2);
}

void HandleMouseMovement(int &wheel) {
  ImGuiIO &io = ImGui::GetIO();
  int mouseX;
  int mouseY;
  const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
  io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
  io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
  io.MouseWheel = static_cast<float>(wheel);
}

}  // namespace

bool Controller::IsActive() const { return active_; }

absl::Status Controller::OnEntry() {
  RETURN_IF_ERROR(CreateWindow())
  RETURN_IF_ERROR(CreateRenderer())
  RETURN_IF_ERROR(CreateGuiContext())
  InitializeKeymap();
  master_editor_.SetupScreen(renderer_);
  active_ = true;
  return absl::OkStatus();
}

void Controller::OnInput() {
  int wheel = 0;
  SDL_Event event;
  ImGuiIO &io = ImGui::GetIO();

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_KEYDOWN:
        HandleKeyDown(event);
        break;
      case SDL_KEYUP:
        HandleKeyUp(event);
        break;
      case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event.text.text);
        break;
      case SDL_MOUSEWHEEL:
        wheel = event.wheel.y;
        break;
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_CLOSE:
            CloseWindow();
            break;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
            ChangeWindowSizeEvent(event);
            break;
          default:
            break;
        }
        break;

      default:
        break;
    }
  }

  HandleMouseMovement(wheel);
}

void Controller::OnLoad() { master_editor_.UpdateScreen(); }

void Controller::DoRender() const {
  SDL_RenderClear(renderer_.get());
  ImGui::Render();
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
  SDL_RenderPresent(renderer_.get());
}

void Controller::OnExit() const {
  Mix_CloseAudio();
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}

absl::Status Controller::CreateWindow() {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init: %s\n", SDL_GetError()));
  } else {
    window_ = std::unique_ptr<SDL_Window, sdl_deleter>(
        SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                         SDL_WINDOWPOS_UNDEFINED,      // initial x position
                         SDL_WINDOWPOS_UNDEFINED,      // initial y position
                         kScreenWidth,                 // width, in pixels
                         kScreenHeight,                // height, in pixels
                         SDL_WINDOW_RESIZABLE),
        sdl_deleter());
    if (window_ == nullptr) {
      return absl::InternalError(
          absl::StrFormat("SDL_CreateWindow: %s\n", SDL_GetError()));
    }
    // Initialize SDL_mixer
    if (Mix_OpenAudio(32000, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
      printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n",
             Mix_GetError());
    }
  }
  return absl::OkStatus();
}

absl::Status Controller::CreateRenderer() {
  renderer_ = std::unique_ptr<SDL_Renderer, sdl_deleter>(
      SDL_CreateRenderer(window_.get(), -1,
                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      sdl_deleter());
  if (renderer_ == nullptr) {
    return absl::InternalError(
        absl::StrFormat("SDL_CreateRenderer: %s\n", SDL_GetError()));
  } else {
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_.get(), 0x00, 0x00, 0x00, 0x00);
  }
  return absl::OkStatus();
}

absl::Status Controller::CreateGuiContext() const {
  ImGui::CreateContext();

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(window_.get(), renderer_.get());
  ImGui_ImplSDLRenderer2_Init(renderer_.get());

  // Load available fonts
  const ImGuiIO &io = ImGui::GetIO();

  // Define constants
  static const char *KARLA_REGULAR = "assets/font/Karla-Regular.ttf";
  static const char *ROBOTO_MEDIUM = "assets/font/Roboto-Medium.ttf";
  static const char *COUSINE_REGULAR = "assets/font/Cousine-Regular.ttf";
  static const char *DROID_SANS = "assets/font/DroidSans.ttf";
  static const char *NOTO_SANS_JP = "assets/font/NotoSansJP.ttf";
  static const char *IBM_PLEX_JP = "assets/font/IBMPlexSansJP-Bold.ttf";
  static const float FONT_SIZE_DEFAULT = 14.0f;
  static const float FONT_SIZE_DROID_SANS = 16.0f;
  static const float ICON_FONT_SIZE = 18.0f;

  // Icon configuration
  static const ImWchar icons_ranges[] = {ICON_MIN_MD, 0xf900, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;

  // Japanese font configuration
  ImFontConfig japanese_font_config;
  japanese_font_config.MergeMode = true;
  icons_config.GlyphOffset.y = 5.0f;
  icons_config.GlyphMinAdvanceX = 13.0f;
  icons_config.PixelSnapH = true;

  // List of fonts to be loaded
  std::vector<const char *> font_paths = {KARLA_REGULAR, ROBOTO_MEDIUM,
                                          COUSINE_REGULAR, IBM_PLEX_JP};

  // Load fonts with associated icon and Japanese merges
  for (const auto &font_path : font_paths) {
    float font_size =
        (font_path == DROID_SANS) ? FONT_SIZE_DROID_SANS : FONT_SIZE_DEFAULT;

    if (!io.Fonts->AddFontFromFileTTF(font_path, font_size)) {
      return absl::InternalError(
          absl::StrFormat("Failed to load font from %s", font_path));
    }

    // Merge icon set
    io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_MD, ICON_FONT_SIZE,
                                 &icons_config, icons_ranges);

    // Merge Japanese font
    io.Fonts->AddFontFromFileTTF(NOTO_SANS_JP, 18.0f, &japanese_font_config,
                                 io.Fonts->GetGlyphRangesJapanese());
  }

  // Set the default style
  gui::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame(window_.get());

  return absl::OkStatus();
}

}  // namespace core
}  // namespace app
}  // namespace yaze