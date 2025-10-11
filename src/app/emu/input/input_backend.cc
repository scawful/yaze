#include "app/emu/input/input_backend.h"

#include "SDL.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace input {

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
    
    // Set default SDL2 keycodes if not configured
    if (config_.key_a == 0) {
      config_.key_a = SDLK_x;
      config_.key_b = SDLK_z;
      config_.key_x = SDLK_s;
      config_.key_y = SDLK_a;
      config_.key_l = SDLK_d;
      config_.key_r = SDLK_c;
      config_.key_start = SDLK_RETURN;
      config_.key_select = SDLK_RSHIFT;
      config_.key_up = SDLK_UP;
      config_.key_down = SDLK_DOWN;
      config_.key_left = SDLK_LEFT;
      config_.key_right = SDLK_RIGHT;
    }
    
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
    if (!initialized_) return ControllerState{};

    ControllerState state;

    if (config_.continuous_polling) {
      // Continuous polling mode (for games)
      const uint8_t* keyboard_state = SDL_GetKeyboardState(nullptr);

      // IMPORTANT: Only block input when actively typing in text fields
      // Allow game input even when ImGui windows are open/focused
      ImGuiIO& io = ImGui::GetIO();
      
      // Only block if user is actively typing in a text input field
      // WantTextInput is true only when an InputText widget is active
      if (io.WantTextInput) {
        // User is typing in a text field
        // Return empty state to prevent game from processing input
        static int text_input_log_count = 0;
        if (text_input_log_count++ < 5) {
          LOG_DEBUG("InputBackend", "Blocking game input - WantTextInput=true");
        }
        return ControllerState{};
      }

      // Map keyboard to SNES buttons
      state.SetButton(SnesButton::B,      keyboard_state[SDL_GetScancodeFromKey(config_.key_b)]);
      state.SetButton(SnesButton::Y,      keyboard_state[SDL_GetScancodeFromKey(config_.key_y)]);
      state.SetButton(SnesButton::SELECT, keyboard_state[SDL_GetScancodeFromKey(config_.key_select)]);
      state.SetButton(SnesButton::START,  keyboard_state[SDL_GetScancodeFromKey(config_.key_start)]);
      state.SetButton(SnesButton::UP,     keyboard_state[SDL_GetScancodeFromKey(config_.key_up)]);
      state.SetButton(SnesButton::DOWN,   keyboard_state[SDL_GetScancodeFromKey(config_.key_down)]);
      state.SetButton(SnesButton::LEFT,   keyboard_state[SDL_GetScancodeFromKey(config_.key_left)]);
      state.SetButton(SnesButton::RIGHT,  keyboard_state[SDL_GetScancodeFromKey(config_.key_right)]);
      state.SetButton(SnesButton::A,      keyboard_state[SDL_GetScancodeFromKey(config_.key_a)]);
      state.SetButton(SnesButton::X,      keyboard_state[SDL_GetScancodeFromKey(config_.key_x)]);
      state.SetButton(SnesButton::L,      keyboard_state[SDL_GetScancodeFromKey(config_.key_l)]);
      state.SetButton(SnesButton::R,      keyboard_state[SDL_GetScancodeFromKey(config_.key_r)]);
    } else {
      // Event-based mode (use cached event state)
      state = event_state_;
    }

    // TODO: Add gamepad support
    // if (config_.enable_gamepad) { ... }

    return state;
  }
  
  void ProcessEvent(void* event) override {
    if (!initialized_ || !event) return;
    
    SDL_Event* sdl_event = static_cast<SDL_Event*>(event);
    
    // Cache keyboard events for event-based mode
    if (sdl_event->type == SDL_KEYDOWN) {
      UpdateEventState(sdl_event->key.keysym.sym, true);
    } else if (sdl_event->type == SDL_KEYUP) {
      UpdateEventState(sdl_event->key.keysym.sym, false);
    }
    
    // TODO: Handle gamepad events
  }
  
  InputConfig GetConfig() const override { return config_; }
  
  void SetConfig(const InputConfig& config) override {
    config_ = config;
  }
  
  std::string GetBackendName() const override { return "SDL2"; }
  
  bool IsInitialized() const override { return initialized_; }
  
 private:
  void UpdateEventState(int keycode, bool pressed) {
    // Map keycode to button and update event state
    if (keycode == config_.key_a)      event_state_.SetButton(SnesButton::A, pressed);
    else if (keycode == config_.key_b) event_state_.SetButton(SnesButton::B, pressed);
    else if (keycode == config_.key_x) event_state_.SetButton(SnesButton::X, pressed);
    else if (keycode == config_.key_y) event_state_.SetButton(SnesButton::Y, pressed);
    else if (keycode == config_.key_l) event_state_.SetButton(SnesButton::L, pressed);
    else if (keycode == config_.key_r) event_state_.SetButton(SnesButton::R, pressed);
    else if (keycode == config_.key_start)  event_state_.SetButton(SnesButton::START, pressed);
    else if (keycode == config_.key_select) event_state_.SetButton(SnesButton::SELECT, pressed);
    else if (keycode == config_.key_up)    event_state_.SetButton(SnesButton::UP, pressed);
    else if (keycode == config_.key_down)  event_state_.SetButton(SnesButton::DOWN, pressed);
    else if (keycode == config_.key_left)  event_state_.SetButton(SnesButton::LEFT, pressed);
    else if (keycode == config_.key_right) event_state_.SetButton(SnesButton::RIGHT, pressed);
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
      return std::make_unique<SDL2InputBackend>();
    
    case BackendType::SDL3:
      // TODO: Implement SDL3 backend when SDL3 is stable
      LOG_WARN("InputBackend", "SDL3 backend not yet implemented, using SDL2");
      return std::make_unique<SDL2InputBackend>();
    
    case BackendType::NULL_BACKEND:
      return std::make_unique<NullInputBackend>();
    
    default:
      LOG_ERROR("InputBackend", "Unknown backend type, using SDL2");
      return std::make_unique<SDL2InputBackend>();
  }
}

}  // namespace input
}  // namespace emu
}  // namespace yaze

