#include "controller.h"

#include <SDL.h>
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/font_loader.h"
#include "app/editor/master_editor.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"

namespace yaze {
namespace app {
namespace core {

namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

using ::ImVec2;
using ImGui::Begin;
using ImGui::End;
using ImGui::GetIO;
using ImGui::NewFrame;
using ImGui::SetNextWindowPos;
using ImGui::SetNextWindowSize;

void NewMasterFrame() {
  const ImGuiIO &io = GetIO();
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  NewFrame();
  SetNextWindowPos(gui::kZeroPos);
  ImVec2 dimensions(io.DisplaySize.x, io.DisplaySize.y);
  SetNextWindowSize(dimensions, ImGuiCond_Always);

  if (!Begin("##YazeMain", nullptr, kMainEditorFlags)) {
    End();
    return;
  }
}

void InitializeKeymap() {
  ImGuiIO &io = ImGui::GetIO();
  io.KeyMap[ImGuiKey_LeftSuper] = SDL_GetScancodeFromKey(SDLK_LGUI);
  io.KeyMap[ImGuiKey_Backspace] = SDL_GetScancodeFromKey(SDLK_BACKSPACE);
  io.KeyMap[ImGuiKey_LeftShift] = SDL_GetScancodeFromKey(SDLK_LSHIFT);
  io.KeyMap[ImGuiKey_Enter] = SDL_GetScancodeFromKey(SDLK_RETURN);
  io.KeyMap[ImGuiKey_UpArrow] = SDL_GetScancodeFromKey(SDLK_UP);
  io.KeyMap[ImGuiKey_DownArrow] = SDL_GetScancodeFromKey(SDLK_DOWN);
  io.KeyMap[ImGuiKey_LeftArrow] = SDL_GetScancodeFromKey(SDLK_LEFT);
  io.KeyMap[ImGuiKey_RightArrow] = SDL_GetScancodeFromKey(SDLK_RIGHT);
  io.KeyMap[ImGuiKey_Delete] = SDL_GetScancodeFromKey(SDLK_DELETE);
  io.KeyMap[ImGuiKey_Escape] = SDL_GetScancodeFromKey(SDLK_ESCAPE);
  io.KeyMap[ImGuiKey_Tab] = SDL_GetScancodeFromKey(SDLK_TAB);
  io.KeyMap[ImGuiKey_LeftCtrl] = SDL_GetScancodeFromKey(SDLK_LCTRL);
  io.KeyMap[ImGuiKey_PageUp] = SDL_GetScancodeFromKey(SDLK_PAGEUP);
  io.KeyMap[ImGuiKey_PageDown] = SDL_GetScancodeFromKey(SDLK_PAGEDOWN);
  io.KeyMap[ImGuiKey_Home] = SDL_GetScancodeFromKey(SDLK_HOME);
  io.KeyMap[ImGuiKey_Space] = SDL_GetScancodeFromKey(SDLK_SPACE);
  io.KeyMap[ImGuiKey_1] = SDL_GetScancodeFromKey(SDLK_1);
  io.KeyMap[ImGuiKey_2] = SDL_GetScancodeFromKey(SDLK_2);
  io.KeyMap[ImGuiKey_3] = SDL_GetScancodeFromKey(SDLK_3);
  io.KeyMap[ImGuiKey_4] = SDL_GetScancodeFromKey(SDLK_4);
  io.KeyMap[ImGuiKey_5] = SDL_GetScancodeFromKey(SDLK_5);
  io.KeyMap[ImGuiKey_6] = SDL_GetScancodeFromKey(SDLK_6);
  io.KeyMap[ImGuiKey_7] = SDL_GetScancodeFromKey(SDLK_7);
  io.KeyMap[ImGuiKey_8] = SDL_GetScancodeFromKey(SDLK_8);
  io.KeyMap[ImGuiKey_9] = SDL_GetScancodeFromKey(SDLK_9);
  io.KeyMap[ImGuiKey_0] = SDL_GetScancodeFromKey(SDLK_0);
  io.KeyMap[ImGuiKey_A] = SDL_GetScancodeFromKey(SDLK_a);
  io.KeyMap[ImGuiKey_B] = SDL_GetScancodeFromKey(SDLK_b);
  io.KeyMap[ImGuiKey_C] = SDL_GetScancodeFromKey(SDLK_c);
  io.KeyMap[ImGuiKey_D] = SDL_GetScancodeFromKey(SDLK_d);
  io.KeyMap[ImGuiKey_E] = SDL_GetScancodeFromKey(SDLK_e);
  io.KeyMap[ImGuiKey_F] = SDL_GetScancodeFromKey(SDLK_f);
  io.KeyMap[ImGuiKey_G] = SDL_GetScancodeFromKey(SDLK_g);
  io.KeyMap[ImGuiKey_H] = SDL_GetScancodeFromKey(SDLK_h);
  io.KeyMap[ImGuiKey_I] = SDL_GetScancodeFromKey(SDLK_i);
  io.KeyMap[ImGuiKey_J] = SDL_GetScancodeFromKey(SDLK_j);
  io.KeyMap[ImGuiKey_K] = SDL_GetScancodeFromKey(SDLK_k);
  io.KeyMap[ImGuiKey_L] = SDL_GetScancodeFromKey(SDLK_l);
  io.KeyMap[ImGuiKey_M] = SDL_GetScancodeFromKey(SDLK_m);
  io.KeyMap[ImGuiKey_N] = SDL_GetScancodeFromKey(SDLK_n);
  io.KeyMap[ImGuiKey_O] = SDL_GetScancodeFromKey(SDLK_o);
  io.KeyMap[ImGuiKey_P] = SDL_GetScancodeFromKey(SDLK_p);
  io.KeyMap[ImGuiKey_Q] = SDL_GetScancodeFromKey(SDLK_q);
  io.KeyMap[ImGuiKey_R] = SDL_GetScancodeFromKey(SDLK_r);
  io.KeyMap[ImGuiKey_S] = SDL_GetScancodeFromKey(SDLK_s);
  io.KeyMap[ImGuiKey_T] = SDL_GetScancodeFromKey(SDLK_t);
  io.KeyMap[ImGuiKey_U] = SDL_GetScancodeFromKey(SDLK_u);
  io.KeyMap[ImGuiKey_V] = SDL_GetScancodeFromKey(SDLK_v);
  io.KeyMap[ImGuiKey_W] = SDL_GetScancodeFromKey(SDLK_w);
  io.KeyMap[ImGuiKey_X] = SDL_GetScancodeFromKey(SDLK_x);
  io.KeyMap[ImGuiKey_Y] = SDL_GetScancodeFromKey(SDLK_y);
  io.KeyMap[ImGuiKey_Z] = SDL_GetScancodeFromKey(SDLK_z);
  io.KeyMap[ImGuiKey_F1] = SDL_GetScancodeFromKey(SDLK_F1);
  io.KeyMap[ImGuiKey_F2] = SDL_GetScancodeFromKey(SDLK_F2);
  io.KeyMap[ImGuiKey_F3] = SDL_GetScancodeFromKey(SDLK_F3);
  io.KeyMap[ImGuiKey_F4] = SDL_GetScancodeFromKey(SDLK_F4);
  io.KeyMap[ImGuiKey_F5] = SDL_GetScancodeFromKey(SDLK_F5);
  io.KeyMap[ImGuiKey_F6] = SDL_GetScancodeFromKey(SDLK_F6);
  io.KeyMap[ImGuiKey_F7] = SDL_GetScancodeFromKey(SDLK_F7);
  io.KeyMap[ImGuiKey_F8] = SDL_GetScancodeFromKey(SDLK_F8);
  io.KeyMap[ImGuiKey_F9] = SDL_GetScancodeFromKey(SDLK_F9);
  io.KeyMap[ImGuiKey_F10] = SDL_GetScancodeFromKey(SDLK_F10);
  io.KeyMap[ImGuiKey_F11] = SDL_GetScancodeFromKey(SDLK_F11);
  io.KeyMap[ImGuiKey_F12] = SDL_GetScancodeFromKey(SDLK_F12);
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

void HandleKeyDown(SDL_Event &event, editor::MasterEditor &editor) {
  ImGuiIO &io = ImGui::GetIO();
  io.KeysDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN);
  io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
  io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
  io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
  io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);

  switch (event.key.keysym.sym) {
    case SDLK_BACKSPACE:
    case SDLK_LSHIFT:
    case SDLK_LCTRL:
    case SDLK_TAB:
      io.KeysDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN);
      break;
    case SDLK_z:
      editor.emulator().snes().SetButtonState(1, 0, true);
      break;
    case SDLK_a:
      editor.emulator().snes().SetButtonState(1, 1, true);
      break;
    case SDLK_RSHIFT:
      editor.emulator().snes().SetButtonState(1, 2, true);
      break;
    case SDLK_RETURN:
      editor.emulator().snes().SetButtonState(1, 3, true);
      break;
    case SDLK_UP:
      editor.emulator().snes().SetButtonState(1, 4, true);
      break;
    case SDLK_DOWN:
      editor.emulator().snes().SetButtonState(1, 5, true);
      break;
    case SDLK_LEFT:
      editor.emulator().snes().SetButtonState(1, 6, true);
      break;
    case SDLK_RIGHT:
      editor.emulator().snes().SetButtonState(1, 7, true);
      break;
    case SDLK_x:
      editor.emulator().snes().SetButtonState(1, 8, true);
      break;
    case SDLK_s:
      editor.emulator().snes().SetButtonState(1, 9, true);
      break;
    case SDLK_d:
      editor.emulator().snes().SetButtonState(1, 10, true);
      break;
    case SDLK_c:
      editor.emulator().snes().SetButtonState(1, 11, true);
      break;
    default:
      break;
  }
}

void HandleKeyUp(SDL_Event &event, editor::MasterEditor &editor) {
  ImGuiIO &io = ImGui::GetIO();
  int key = event.key.keysym.scancode;
  IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
  io.KeysDown[key] = (event.type == SDL_KEYDOWN);
  io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
  io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
  io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
  io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);

  switch (event.key.keysym.sym) {
    case SDLK_z:
      editor.emulator().snes().SetButtonState(1, 0, false);
      break;
    case SDLK_a:
      editor.emulator().snes().SetButtonState(1, 1, false);
      break;
    case SDLK_RSHIFT:
      editor.emulator().snes().SetButtonState(1, 2, false);
      break;
    case SDLK_RETURN:
      editor.emulator().snes().SetButtonState(1, 3, false);
      break;
    case SDLK_UP:
      editor.emulator().snes().SetButtonState(1, 4, false);
      break;
    case SDLK_DOWN:
      editor.emulator().snes().SetButtonState(1, 5, false);
      break;
    case SDLK_LEFT:
      editor.emulator().snes().SetButtonState(1, 6, false);
      break;
    case SDLK_RIGHT:
      editor.emulator().snes().SetButtonState(1, 7, false);
      break;
    case SDLK_x:
      editor.emulator().snes().SetButtonState(1, 8, false);
      break;
    case SDLK_s:
      editor.emulator().snes().SetButtonState(1, 9, false);
      break;
    case SDLK_d:
      editor.emulator().snes().SetButtonState(1, 10, false);
      break;
    case SDLK_c:
      editor.emulator().snes().SetButtonState(1, 11, false);
      break;
    default:
      break;
  }
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
  io.MouseDown[2] = buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE);
  io.MouseWheel = static_cast<float>(wheel);
}

}  // namespace

absl::Status Controller::OnEntry(std::string filename) {
#if defined(__APPLE__) && defined(__MACH__)
#if TARGET_IPHONE_SIMULATOR == 1
  /* iOS in Xcode simulator */
  platform_ = Platform::kiOS;
#elif TARGET_OS_IPHONE == 1
  /* iOS */
  platform_ = Platform::kiOS;
#elif TARGET_OS_MAC == 1
  /* macOS */
  platform_ = Platform::kMacOS;
#endif
#elif defined(_WIN32)
  platform_ = Platform::kWindows;
#elif defined(__linux__)
  platform_ = Platform::kLinux;
#else
  platform_ = Platform::kUnknown;
#endif 

  RETURN_IF_ERROR(CreateSDL_Window())
  RETURN_IF_ERROR(CreateRenderer())
  RETURN_IF_ERROR(CreateGuiContext())
  if (flags()->kLoadAudioDevice) {
    RETURN_IF_ERROR(LoadAudioDevice())
    master_editor_.emulator().set_audio_buffer(audio_buffer_);
    master_editor_.emulator().set_audio_device_id(audio_device_);
  }
  InitializeKeymap();
  master_editor_.SetupScreen(renderer_, filename);
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
        HandleKeyDown(event, master_editor_);
        break;
      case SDL_KEYUP:
        HandleKeyUp(event, master_editor_);
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

absl::Status Controller::OnLoad() {
  if (master_editor_.quit()) {
    active_ = false;
  }
  NewMasterFrame();
  RETURN_IF_ERROR(master_editor_.Update());
  return absl::OkStatus();
}

void Controller::DoRender() const {
  ImGui::Render();
  SDL_RenderClear(renderer_.get());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_.get());
  SDL_RenderPresent(renderer_.get());
}

void Controller::OnExit() {
  ImGui::DestroyContext();
  if (flags()->kLoadAudioDevice) {
    SDL_PauseAudioDevice(audio_device_, 1);
    SDL_CloseAudioDevice(audio_device_);
    delete audio_buffer_;
  }
  switch (platform_) {
    case Platform::kMacOS:
    case Platform::kWindows:
    case Platform::kLinux:
      ImGui_ImplSDLRenderer2_Shutdown();
      ImGui_ImplSDL2_Shutdown();
      break;
    case Platform::kiOS:
          // Deferred
      break;
    default:
      break;
  }
  ImGui::DestroyContext();
  SDL_Quit();
}

absl::Status Controller::CreateSDL_Window() {
  auto sdl_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
  if (flags()->kUseNewImGuiInput) {
    sdl_flags |= SDL_INIT_GAMECONTROLLER;
  }

  if (flags()->kLoadAudioDevice) {
    sdl_flags |= SDL_INIT_AUDIO;
  }

  if (SDL_Init(sdl_flags) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init: %s\n", SDL_GetError()));
  } else {
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    int screenWidth = displayMode.w * 0.8;
    int screenHeight = displayMode.h * 0.8;

    window_ = std::unique_ptr<SDL_Window, sdl_deleter>(
        SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                         SDL_WINDOWPOS_UNDEFINED,      // initial x position
                         SDL_WINDOWPOS_UNDEFINED,      // initial y position
                         screenWidth,                  // width, in pixels
                         screenHeight,                 // height, in pixels
                         SDL_WINDOW_RESIZABLE),
        sdl_deleter());
    if (window_ == nullptr) {
      return absl::InternalError(
          absl::StrFormat("SDL_CreateWindow: %s\n", SDL_GetError()));
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

absl::Status Controller::CreateGuiContext() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  if (flags()->kUseNewImGuiInput) {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  }

  // Initialize ImGui for SDL
  ImGui_ImplSDL2_InitForSDLRenderer(window_.get(), renderer_.get());
  ImGui_ImplSDLRenderer2_Init(renderer_.get());

  if (flags()->kLoadSystemFonts) {
    LoadSystemFonts();
  } else {
    RETURN_IF_ERROR(LoadFontFamilies());
  }

  // Set the default style
  gui::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  return absl::OkStatus();
}

absl::Status Controller::LoadFontFamilies() const {
  ImGuiIO &io = ImGui::GetIO();

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
    std::string actual_font_path =
        std::filesystem::absolute(font_path).string();

    if (!io.Fonts->AddFontFromFileTTF(actual_font_path.data(), font_size)) {
      return absl::InternalError(
          absl::StrFormat("Failed to load font from %s", actual_font_path));
    }

    // Merge icon set
    const char *icon_font_path = FONT_ICON_FILE_NAME_MD;
    std::string actual_icon_font_path =
        std::filesystem::absolute(icon_font_path).string();
    io.Fonts->AddFontFromFileTTF(actual_icon_font_path.data(), ICON_FONT_SIZE,
                                 &icons_config, icons_ranges);

    // Merge Japanese font
    const char *japanese_font_path = NOTO_SANS_JP;
    std::string actual_japanese_font_path =
        std::filesystem::absolute(japanese_font_path).string();
    io.Fonts->AddFontFromFileTTF(actual_japanese_font_path.data(), 18.0f,
                                 &japanese_font_config,
                                 io.Fonts->GetGlyphRangesJapanese());
  }

  return absl::OkStatus();
}

absl::Status Controller::LoadAudioDevice() {
  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));
  want.freq = audio_frequency_;
  want.format = AUDIO_S16;
  want.channels = 2;
  want.samples = 2048;
  want.callback = NULL;  // Uses the queue
  audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device_ == 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to open audio: %s\n", SDL_GetError()));
  }
  audio_buffer_ = new int16_t[audio_frequency_ / 50 * 4];
  master_editor_.emulator().set_audio_buffer(audio_buffer_);
  SDL_PauseAudioDevice(audio_device_, 0);
  return absl::OkStatus();
}

}  // namespace core
}  // namespace app
}  // namespace yaze