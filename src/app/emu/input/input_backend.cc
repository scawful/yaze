#include "app/emu/input/input_backend.h"

#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "util/log.h"

#ifdef YAZE_USE_SDL3
#include "app/emu/input/sdl3_input_backend.h"
#endif

namespace yaze {
namespace emu {
namespace input {

void ApplyDefaultKeyBindings(InputConfig& config) {
  if (config.key_a == 0) config.key_a = platform::kKeyX;
  if (config.key_b == 0) config.key_b = platform::kKeyZ;
  if (config.key_x == 0) config.key_x = platform::kKeyS;
  if (config.key_y == 0) config.key_y = platform::kKeyA;
  if (config.key_l == 0) config.key_l = platform::kKeyD;
  if (config.key_r == 0) config.key_r = platform::kKeyC;
  if (config.key_start == 0) config.key_start = platform::kKeyReturn;
  if (config.key_select == 0) config.key_select = platform::kKeyRShift;
  if (config.key_up == 0) config.key_up = platform::kKeyUp;
  if (config.key_down == 0) config.key_down = platform::kKeyDown;
  if (config.key_left == 0) config.key_left = platform::kKeyLeft;
  if (config.key_right == 0) config.key_right = platform::kKeyRight;
}

/**
 * @brief SDL2 input backend implementation
 */
class SDL2InputBackend : public IInputBackend {
 public:
  SDL2InputBackend() = default;
  ~SDL2InputBackend() override { Shutdown(); }

  bool Initialize(const InputConfig& config) override {
    if (initialized_) {
      LOG_WARN("InputBackend", "Already initialized");
      return true;
    }

    config_ = config;

    ApplyDefaultKeyBindings(config_);

    initialized_ = true;
    LOG_INFO("InputBackend", "SDL2 Input Backend initialized");
    return true;
  }

  void Shutdown() override {
    if (initialized_) {
      initialized_ = false;
      LOG_INFO("InputBackend", "SDL2 Input Backend shut down");
    }
  }

  ControllerState Poll(int player) override {
    if (!initialized_)
      return ControllerState{};

    ControllerState state;

    if (config_.continuous_polling) {
      // Pump events to update keyboard state - critical for edge detection
      // when multiple emulated frames run per host frame
      SDL_PumpEvents();

      // Continuous polling mode (for games)
      // Continuous polling mode (for games)
      platform::KeyboardState keyboard_state = SDL_GetKeyboardState(nullptr);

      // Do NOT block emulator input when ImGui wants text; games rely on edge detection
      // and we don't want UI focus to interfere with controller state.

      // Map keyboard to SNES buttons
      state.SetButton(SnesButton::B,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_b)]);
      state.SetButton(SnesButton::Y,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_y)]);
      state.SetButton(
          SnesButton::SELECT,
          keyboard_state[platform::GetScancodeFromKey(config_.key_select)]);
      state.SetButton(
          SnesButton::START,
          keyboard_state[platform::GetScancodeFromKey(config_.key_start)]);
      state.SetButton(SnesButton::UP,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_up)]);
      state.SetButton(SnesButton::DOWN,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_down)]);
      state.SetButton(SnesButton::LEFT,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_left)]);
      state.SetButton(
          SnesButton::RIGHT,
          keyboard_state[platform::GetScancodeFromKey(config_.key_right)]);
      state.SetButton(SnesButton::A,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_a)]);
      state.SetButton(SnesButton::X,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_x)]);
      state.SetButton(SnesButton::L,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_l)]);
      state.SetButton(SnesButton::R,
                      keyboard_state[platform::GetScancodeFromKey(config_.key_r)]);

      // Debug: Log when any button is pressed
      static int button_log_count = 0;
      if (state.buttons != 0 && button_log_count++ < 100) {
        LOG_INFO("InputBackend", "SDL2 Poll: buttons=0x%04X (keyboard detected)",
                  state.buttons);
      }
    } else {
      // Event-based mode (use cached event state)
      state = event_state_;
    }

    // TODO: Add gamepad support
    // if (config_.enable_gamepad) { ... }

    return state;
  }

  void ProcessEvent(void* event) override {
    if (!initialized_ || !event)
      return;

    SDL_Event* sdl_event = static_cast<SDL_Event*>(event);

    // Cache keyboard events for event-based mode
    if (sdl_event->type == platform::kEventKeyDown) {
      UpdateEventState(platform::GetKeyFromEvent(*sdl_event), true);
    } else if (sdl_event->type == platform::kEventKeyUp) {
      UpdateEventState(platform::GetKeyFromEvent(*sdl_event), false);
    }

    // TODO: Handle gamepad events
  }

  InputConfig GetConfig() const override { return config_; }

  void SetConfig(const InputConfig& config) override { config_ = config; }

  std::string GetBackendName() const override { return "SDL2"; }

  bool IsInitialized() const override { return initialized_; }

 private:
  void UpdateEventState(int keycode, bool pressed) {
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

  InputConfig config_;
  bool initialized_ = false;
  ControllerState event_state_;  // Cached state for event-based mode
};

/**
 * @brief Null input backend for testing/replay
 */
class NullInputBackend : public IInputBackend {
 public:
  bool Initialize(const InputConfig& config) override {
    config_ = config;
    ApplyDefaultKeyBindings(config_);
    return true;
  }
  void Shutdown() override {}
  ControllerState Poll(int player) override { return replay_state_; }
  void ProcessEvent(void* event) override {}
  InputConfig GetConfig() const override { return config_; }
  void SetConfig(const InputConfig& config) override { config_ = config; }
  std::string GetBackendName() const override { return "NULL"; }
  bool IsInitialized() const override { return true; }

  // For replay/testing - set controller state directly
  void SetReplayState(const ControllerState& state) { replay_state_ = state; }

 private:
  InputConfig config_;
  ControllerState replay_state_;
};

// Factory implementation
std::unique_ptr<IInputBackend> InputBackendFactory::Create(BackendType type) {
  switch (type) {
    case BackendType::SDL2:
#ifdef YAZE_USE_SDL3
      LOG_WARN("InputBackend",
               "SDL2 backend requested but SDL3 build enabled, using SDL3");
      return std::make_unique<SDL3InputBackend>();
#else
      return std::make_unique<SDL2InputBackend>();
#endif

    case BackendType::SDL3:
#ifdef YAZE_USE_SDL3
      return std::make_unique<SDL3InputBackend>();
#else
      LOG_WARN("InputBackend",
               "SDL3 backend requested but not available, using SDL2");
      return std::make_unique<SDL2InputBackend>();
#endif

    case BackendType::NULL_BACKEND:
      return std::make_unique<NullInputBackend>();

    default:
#ifdef YAZE_USE_SDL3
      LOG_ERROR("InputBackend", "Unknown backend type, using SDL3");
      return std::make_unique<SDL3InputBackend>();
#else
      LOG_ERROR("InputBackend", "Unknown backend type, using SDL2");
      return std::make_unique<SDL2InputBackend>();
#endif
  }
}

}  // namespace input
}  // namespace emu
}  // namespace yaze
