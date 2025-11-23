#ifndef YAZE_APP_EMU_INPUT_SDL3_INPUT_BACKEND_H_
#define YAZE_APP_EMU_INPUT_SDL3_INPUT_BACKEND_H_

#include "app/emu/input/input_backend.h"
#include "app/platform/sdl_compat.h"

namespace yaze {
namespace emu {
namespace input {

/**
 * @brief SDL3 input backend implementation
 *
 * Implements the IInputBackend interface using SDL3 APIs.
 * Key differences from SDL2:
 * - SDL_GetKeyboardState() returns bool* instead of Uint8*
 * - SDL_GameController is replaced with SDL_Gamepad
 * - Event types use SDL_EVENT_* prefix instead of SDL_*
 * - event.key.keysym.sym is replaced with event.key.key
 */
class SDL3InputBackend : public IInputBackend {
 public:
  SDL3InputBackend();
  ~SDL3InputBackend() override;

  // IInputBackend interface
  bool Initialize(const InputConfig& config) override;
  void Shutdown() override;
  ControllerState Poll(int player = 1) override;
  void ProcessEvent(void* event) override;
  InputConfig GetConfig() const override;
  void SetConfig(const InputConfig& config) override;
  std::string GetBackendName() const override;
  bool IsInitialized() const override;

 private:
  /**
   * @brief Update event state from keyboard event
   * @param keycode The keycode from the event
   * @param pressed Whether the key is pressed
   */
  void UpdateEventState(int keycode, bool pressed);

  /**
   * @brief Poll gamepad state and update controller state
   * @param state The controller state to update
   * @param player The player number (1-4)
   */
  void PollGamepad(ControllerState& state, int player);

  /**
   * @brief Handle gamepad connection/disconnection
   * @param event The SDL event
   */
  void HandleGamepadEvent(const SDL_Event& event);

  InputConfig config_;
  bool initialized_ = false;
  ControllerState event_state_;  // Cached state for event-based mode

  // Gamepad handles for up to 4 players
  platform::GamepadHandle gamepads_[4] = {nullptr, nullptr, nullptr, nullptr};

  // Axis deadzone for analog sticks
  static constexpr int16_t kAxisDeadzone = 8000;
};

}  // namespace input
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_INPUT_SDL3_INPUT_BACKEND_H_
