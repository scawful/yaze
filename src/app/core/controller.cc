#include "controller.h"

#include <SDL.h>

#include <filesystem>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/font_loader.h"
#include "app/editor/editor_manager.h"
#include "app/gui/style.h"
#include "core/utils/file_util.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace core {

namespace {

constexpr ImGuiWindowFlags kMainEditorFlags =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;

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

void HandleKeyDown(SDL_Event &event, editor::EditorManager &editor) {
  ImGuiIO &io = ImGui::GetIO();
  io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
  io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
  io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
  io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);

  switch (event.key.keysym.sym) {
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

void HandleKeyUp(SDL_Event &event, editor::EditorManager &editor) {
  ImGuiIO &io = ImGui::GetIO();
  int key = event.key.keysym.scancode;
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
#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
  platform_ = Platform::kiOS;
#elif TARGET_OS_MAC == 1
  platform_ = Platform::kMacOS;
#endif
#elif defined(_WIN32)
  platform_ = Platform::kWindows;
#elif defined(__linux__)
  platform_ = Platform::kLinux;
#else
  platform_ = Platform::kUnknown;
#endif
  RETURN_IF_ERROR(CreateWindow())
  RETURN_IF_ERROR(CreateRenderer())
  RETURN_IF_ERROR(CreateGuiContext())
  RETURN_IF_ERROR(LoadAudioDevice())
  editor_manager_.SetupScreen(filename);
  active_ = true;
  return absl::OkStatus();
}

void Controller::OnInput() {
  int wheel = 0;
  SDL_Event event;
  ImGuiIO &io = ImGui::GetIO();

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type) {
      case SDL_KEYDOWN:
        HandleKeyDown(event, editor_manager_);
        break;
      case SDL_KEYUP:
        HandleKeyUp(event, editor_manager_);
        break;
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
  }

  HandleMouseMovement(wheel);
}

absl::Status Controller::OnLoad() {
  if (editor_manager_.quit()) {
    active_ = false;
  }
#if TARGET_OS_IPHONE != 1
  if (platform_ != Platform::kiOS) {
    NewMasterFrame();
  }
#endif
  RETURN_IF_ERROR(editor_manager_.Update());
#if TARGET_OS_IPHONE != 1
  if (platform_ != Platform::kiOS) {
    End();
  }
#endif
  return absl::OkStatus();
}

absl::Status Controller::OnTestLoad() {
  RETURN_IF_ERROR(test_editor_->Update());
  return absl::OkStatus();
}

void Controller::DoRender() const {
  ImGui::Render();
  SDL_RenderClear(Renderer::GetInstance().renderer());
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),
                                        Renderer::GetInstance().renderer());
  SDL_RenderPresent(Renderer::GetInstance().renderer());
}

void Controller::OnExit() {
  SDL_PauseAudioDevice(audio_device_, 1);
  SDL_CloseAudioDevice(audio_device_);
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_Quit();
}

absl::Status Controller::CreateWindow() {
  auto sdl_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
  if (flags()->kUseNewImGuiInput) {
    sdl_flags |= SDL_INIT_GAMECONTROLLER;
  }

  if (SDL_Init(sdl_flags) != 0) {
    return absl::InternalError(
        absl::StrFormat("SDL_Init: %s\n", SDL_GetError()));
  }

  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  int screen_width = display_mode.w * 0.8;
  int screen_height = display_mode.h * 0.8;

  window_ = std::unique_ptr<SDL_Window, core::SDL_Deleter>(
      SDL_CreateWindow("Yet Another Zelda3 Editor",  // window title
                       SDL_WINDOWPOS_UNDEFINED,      // initial x position
                       SDL_WINDOWPOS_UNDEFINED,      // initial y position
                       screen_width,                 // width, in pixels
                       screen_height,                // height, in pixels
                       SDL_WINDOW_RESIZABLE),
      core::SDL_Deleter());
  if (window_ == nullptr) {
    return absl::InternalError(
        absl::StrFormat("SDL_CreateWindow: %s\n", SDL_GetError()));
  }

  return absl::OkStatus();
}

absl::Status Controller::CreateRenderer() {
  return Renderer::GetInstance().CreateRenderer(window_.get());
}

absl::Status Controller::CreateGuiContext() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  if (flags()->kUseNewImGuiInput) {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  }

  // Initialize ImGui based on the backend
  ImGui_ImplSDL2_InitForSDLRenderer(window_.get(),
                                    Renderer::GetInstance().renderer());
  ImGui_ImplSDLRenderer2_Init(Renderer::GetInstance().renderer());

  RETURN_IF_ERROR(LoadFontFamilies());

  // Set the default style
  gui::ColorsYaze();

  // Build a new ImGui frame
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();

  return absl::OkStatus();
}

absl::Status Controller::LoadFontFamilies() const {
  // LoadSystemFonts();
  return LoadPackageFonts();
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
  // audio_buffer_ = new int16_t[audio_frequency_ / 50 * 4];
  audio_buffer_ = std::make_shared<int16_t>(audio_frequency_ / 50 * 4);
  SDL_PauseAudioDevice(audio_device_, 0);
  editor_manager_.emulator().set_audio_buffer(audio_buffer_.get());
  editor_manager_.emulator().set_audio_device_id(audio_device_);
  return absl::OkStatus();
}

absl::Status Controller::LoadConfigFiles() {
  // Create and load a dotfile for the application
  // This will store the user's preferences and settings
  std::string config_directory = GetConfigDirectory(platform_);

  // Create the directory if it doesn't exist
  if (!std::filesystem::exists(config_directory)) {
    if (!std::filesystem::create_directory(config_directory)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to create config directory %s", config_directory));
    }
  }

  // Check if the config file exists
  std::string config_file = config_directory + "yaze.cfg";
  if (!std::filesystem::exists(config_file)) {
    // Create the file if it doesn't exist
    std::ofstream file(config_file);
    if (!file.is_open()) {
      return absl::InternalError(
          absl::StrFormat("Failed to create config file %s", config_file));
    }
    file.close();
  }

  return absl::OkStatus();
}

}  // namespace core
}  // namespace app
}  // namespace yaze
