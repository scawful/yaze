#include "app/emu/input/sdl3_input_backend.h"

#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace input {

SDL3InputBackend::SDL3InputBackend() = default;

SDL3InputBackend::~SDL3InputBackend() { Shutdown(); }

bool SDL3InputBackend::Initialize(const InputConfig& config) {
  if (initialized_) {
    LOG_WARN("InputBackend", "SDL3 backend already initialized");
    return true;
  }

  config_ = config;

  ApplyDefaultKeyBindings(config_);

  // Initialize gamepad if enabled
  if (config_.enable_gamepad) {
    gamepads_[0] = platform::OpenGamepad(config_.gamepad_index);
    if (gamepads_[0]) {
      LOG_INFO("InputBackend", "SDL3 Gamepad connected for player 1");
    }
  }

  initialized_ = true;
  LOG_INFO("InputBackend", "SDL3 Input Backend initialized");
  return true;
}

void SDL3InputBackend::Shutdown() {
  if (initialized_) {
    // Close all gamepads
    for (int i = 0; i < 4; ++i) {
      if (gamepads_[i]) {
        platform::CloseGamepad(gamepads_[i]);
        gamepads_[i] = nullptr;
      }
    }
    initialized_ = false;
    LOG_INFO("InputBackend", "SDL3 Input Backend shut down");
  }
}

ControllerState SDL3InputBackend::Poll(int player) {
  if (!initialized_) return ControllerState{};

  ControllerState state;

  if (config_.continuous_polling) {
    // Pump events to update keyboard state - critical for edge detection
    // when multiple emulated frames run per host frame.
    // Without this, SDL_GetKeyboardState returns stale data.
    SDL_PumpEvents();

    // Continuous polling mode (for games)
    // SDL3: SDL_GetKeyboardState returns const bool* instead of const Uint8*
    platform::KeyboardState keyboard_state = SDL_GetKeyboardState(nullptr);

    // Respect ImGui text capture unless explicitly overridden
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput && !config_.ignore_imgui_text_input) {
      return ControllerState{};
    }

    // Map keyboard to SNES buttons using SDL3 API
    // Use platform::IsKeyPressed helper to handle bool* vs Uint8* difference
    state.SetButton(
        SnesButton::B,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_b, nullptr)));
    state.SetButton(
        SnesButton::Y,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_y, nullptr)));
    state.SetButton(
        SnesButton::SELECT,
        platform::IsKeyPressed(
            keyboard_state,
            SDL_GetScancodeFromKey(config_.key_select, nullptr)));
    state.SetButton(
        SnesButton::START,
        platform::IsKeyPressed(
            keyboard_state,
            SDL_GetScancodeFromKey(config_.key_start, nullptr)));
    state.SetButton(
        SnesButton::UP,
        platform::IsKeyPressed(
            keyboard_state, SDL_GetScancodeFromKey(config_.key_up, nullptr)));
    state.SetButton(
        SnesButton::DOWN,
        platform::IsKeyPressed(
            keyboard_state, SDL_GetScancodeFromKey(config_.key_down, nullptr)));
    state.SetButton(
        SnesButton::LEFT,
        platform::IsKeyPressed(
            keyboard_state, SDL_GetScancodeFromKey(config_.key_left, nullptr)));
    state.SetButton(
        SnesButton::RIGHT,
        platform::IsKeyPressed(
            keyboard_state,
            SDL_GetScancodeFromKey(config_.key_right, nullptr)));
    state.SetButton(
        SnesButton::A,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_a, nullptr)));
    state.SetButton(
        SnesButton::X,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_x, nullptr)));
    state.SetButton(
        SnesButton::L,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_l, nullptr)));
    state.SetButton(
        SnesButton::R,
        platform::IsKeyPressed(keyboard_state,
                               SDL_GetScancodeFromKey(config_.key_r, nullptr)));

    // Poll gamepad if enabled
    if (config_.enable_gamepad) {
      PollGamepad(state, player);
    }
  } else {
    // Event-based mode (use cached event state)
    state = event_state_;
  }

  return state;
}

void SDL3InputBackend::PollGamepad(ControllerState& state, int player) {
  int gamepad_index = (player > 0 && player <= 4) ? player - 1 : 0;
  platform::GamepadHandle gamepad = gamepads_[gamepad_index];

  if (!gamepad) return;

  // Map gamepad buttons to SNES buttons using SDL3 Gamepad API
  // SDL3 uses SDL_GAMEPAD_BUTTON_* with directional naming (SOUTH, EAST, etc.)
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonA)) {
    state.SetButton(SnesButton::A, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonB)) {
    state.SetButton(SnesButton::B, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonX)) {
    state.SetButton(SnesButton::X, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonY)) {
    state.SetButton(SnesButton::Y, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonLeftShoulder)) {
    state.SetButton(SnesButton::L, true);
  }
  if (platform::GetGamepadButton(gamepad,
                                 platform::kGamepadButtonRightShoulder)) {
    state.SetButton(SnesButton::R, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonStart)) {
    state.SetButton(SnesButton::START, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonBack)) {
    state.SetButton(SnesButton::SELECT, true);
  }

  // D-pad buttons
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonDpadUp)) {
    state.SetButton(SnesButton::UP, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonDpadDown)) {
    state.SetButton(SnesButton::DOWN, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonDpadLeft)) {
    state.SetButton(SnesButton::LEFT, true);
  }
  if (platform::GetGamepadButton(gamepad, platform::kGamepadButtonDpadRight)) {
    state.SetButton(SnesButton::RIGHT, true);
  }

  // Left analog stick for D-pad (with deadzone)
  int16_t axis_x = platform::GetGamepadAxis(gamepad, platform::kGamepadAxisLeftX);
  int16_t axis_y = platform::GetGamepadAxis(gamepad, platform::kGamepadAxisLeftY);

  if (axis_x < -kAxisDeadzone) {
    state.SetButton(SnesButton::LEFT, true);
  } else if (axis_x > kAxisDeadzone) {
    state.SetButton(SnesButton::RIGHT, true);
  }

  if (axis_y < -kAxisDeadzone) {
    state.SetButton(SnesButton::UP, true);
  } else if (axis_y > kAxisDeadzone) {
    state.SetButton(SnesButton::DOWN, true);
  }
}

void SDL3InputBackend::ProcessEvent(void* event) {
  if (!initialized_ || !event) return;

  SDL_Event* sdl_event = static_cast<SDL_Event*>(event);

  // Handle keyboard events
  // SDL3: Uses SDL_EVENT_KEY_DOWN/UP instead of SDL_KEYDOWN/UP
  // SDL3: Uses event.key.key instead of event.key.keysym.sym
  if (sdl_event->type == platform::kEventKeyDown) {
    UpdateEventState(platform::GetKeyFromEvent(*sdl_event), true);
  } else if (sdl_event->type == platform::kEventKeyUp) {
    UpdateEventState(platform::GetKeyFromEvent(*sdl_event), false);
  }

  // Handle gamepad connection/disconnection events
  HandleGamepadEvent(*sdl_event);
}

void SDL3InputBackend::HandleGamepadEvent(const SDL_Event& event) {
#ifdef YAZE_USE_SDL3
  // SDL3 uses SDL_EVENT_GAMEPAD_ADDED/REMOVED
  if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
    // Try to open the gamepad if we have a free slot
    for (int i = 0; i < 4; ++i) {
      if (!gamepads_[i]) {
        gamepads_[i] = SDL_OpenGamepad(event.gdevice.which);
        if (gamepads_[i]) {
          LOG_INFO("InputBackend", "SDL3 Gamepad connected for player %d", i + 1);
        }
        break;
      }
    }
  } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
    // Find and close the disconnected gamepad
    for (int i = 0; i < 4; ++i) {
      if (gamepads_[i] &&
          SDL_GetGamepadID(gamepads_[i]) == event.gdevice.which) {
        SDL_CloseGamepad(gamepads_[i]);
        gamepads_[i] = nullptr;
        LOG_INFO("InputBackend", "SDL3 Gamepad disconnected for player %d", i + 1);
        break;
      }
    }
  }
#else
  // SDL2 uses SDL_CONTROLLERDEVICEADDED/REMOVED
  if (event.type == SDL_CONTROLLERDEVICEADDED) {
    for (int i = 0; i < 4; ++i) {
      if (!gamepads_[i]) {
        gamepads_[i] = platform::OpenGamepad(event.cdevice.which);
        if (gamepads_[i]) {
          LOG_INFO("InputBackend", "Gamepad connected for player " +
                                       std::to_string(i + 1));
        }
        break;
      }
    }
  } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
    for (int i = 0; i < 4; ++i) {
      if (gamepads_[i] && SDL_JoystickInstanceID(
                              SDL_GameControllerGetJoystick(gamepads_[i])) ==
                              event.cdevice.which) {
        platform::CloseGamepad(gamepads_[i]);
        gamepads_[i] = nullptr;
        LOG_INFO("InputBackend", "Gamepad disconnected for player " +
                                     std::to_string(i + 1));
        break;
      }
    }
  }
#endif
}

void SDL3InputBackend::UpdateEventState(int keycode, bool pressed) {
  // Map keycode to button and update event state
  if (keycode == config_.key_a)
    event_state_.SetButton(SnesButton::A, pressed);
  else if (keycode == config_.key_b)
    event_state_.SetButton(SnesButton::B, pressed);
  else if (keycode == config_.key_x)
    event_state_.SetButton(SnesButton::X, pressed);
  else if (keycode == config_.key_y)
    event_state_.SetButton(SnesButton::Y, pressed);
  else if (keycode == config_.key_l)
    event_state_.SetButton(SnesButton::L, pressed);
  else if (keycode == config_.key_r)
    event_state_.SetButton(SnesButton::R, pressed);
  else if (keycode == config_.key_start)
    event_state_.SetButton(SnesButton::START, pressed);
  else if (keycode == config_.key_select)
    event_state_.SetButton(SnesButton::SELECT, pressed);
  else if (keycode == config_.key_up)
    event_state_.SetButton(SnesButton::UP, pressed);
  else if (keycode == config_.key_down)
    event_state_.SetButton(SnesButton::DOWN, pressed);
  else if (keycode == config_.key_left)
    event_state_.SetButton(SnesButton::LEFT, pressed);
  else if (keycode == config_.key_right)
    event_state_.SetButton(SnesButton::RIGHT, pressed);
}

InputConfig SDL3InputBackend::GetConfig() const { return config_; }

void SDL3InputBackend::SetConfig(const InputConfig& config) {
  config_ = config;

  // Re-initialize gamepad if gamepad settings changed
  if (config_.enable_gamepad && !gamepads_[0]) {
    gamepads_[0] = platform::OpenGamepad(config_.gamepad_index);
    if (gamepads_[0]) {
      LOG_INFO("InputBackend", "SDL3 Gamepad connected for player 1");
    }
  } else if (!config_.enable_gamepad && gamepads_[0]) {
    platform::CloseGamepad(gamepads_[0]);
    gamepads_[0] = nullptr;
  }
}

std::string SDL3InputBackend::GetBackendName() const { return "SDL3"; }

bool SDL3InputBackend::IsInitialized() const { return initialized_; }

}  // namespace input
}  // namespace emu
}  // namespace yaze
