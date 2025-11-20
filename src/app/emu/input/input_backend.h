#ifndef YAZE_APP_EMU_INPUT_INPUT_BACKEND_H_
#define YAZE_APP_EMU_INPUT_INPUT_BACKEND_H_

#include <cstdint>
#include <memory>
#include <string>

namespace yaze {
namespace emu {
namespace input {

/**
 * @brief SNES controller button mapping (platform-agnostic)
 */
enum class SnesButton : uint8_t {
  B = 0,       // Bit 0
  Y = 1,       // Bit 1
  SELECT = 2,  // Bit 2
  START = 3,   // Bit 3
  UP = 4,      // Bit 4
  DOWN = 5,    // Bit 5
  LEFT = 6,    // Bit 6
  RIGHT = 7,   // Bit 7
  A = 8,       // Bit 8
  X = 9,       // Bit 9
  L = 10,      // Bit 10
  R = 11       // Bit 11
};

/**
 * @brief Controller state (16-bit SNES controller format)
 */
struct ControllerState {
  uint16_t buttons = 0;  // Bit field matching SNES hardware layout

  bool IsPressed(SnesButton button) const {
    return (buttons & (1 << static_cast<uint8_t>(button))) != 0;
  }

  void SetButton(SnesButton button, bool pressed) {
    if (pressed) {
      buttons |= (1 << static_cast<uint8_t>(button));
    } else {
      buttons &= ~(1 << static_cast<uint8_t>(button));
    }
  }

  void Clear() { buttons = 0; }
};

/**
 * @brief Input configuration (platform-agnostic key codes)
 */
struct InputConfig {
  // Platform-agnostic key codes (mapped to platform-specific in backend)
  // Using generic names that can be mapped to SDL2/SDL3/other
  int key_a = 0;       // Default: X key
  int key_b = 0;       // Default: Z key
  int key_x = 0;       // Default: S key
  int key_y = 0;       // Default: A key
  int key_l = 0;       // Default: D key
  int key_r = 0;       // Default: C key
  int key_start = 0;   // Default: Enter
  int key_select = 0;  // Default: RShift
  int key_up = 0;      // Default: Up arrow
  int key_down = 0;    // Default: Down arrow
  int key_left = 0;    // Default: Left arrow
  int key_right = 0;   // Default: Right arrow

  // Enable/disable continuous polling (vs event-based)
  bool continuous_polling = true;

  // Enable gamepad support
  bool enable_gamepad = true;
  int gamepad_index = 0;  // Which gamepad to use (0-3)
};

/**
 * @brief Abstract input backend interface
 *
 * Allows swapping between SDL2, SDL3, or custom input implementations
 * without changing emulator code.
 */
class IInputBackend {
 public:
  virtual ~IInputBackend() = default;

  /**
   * @brief Initialize the input backend
   */
  virtual bool Initialize(const InputConfig& config) = 0;

  /**
   * @brief Shutdown the input backend
   */
  virtual void Shutdown() = 0;

  /**
   * @brief Poll current input state (call every frame)
   * @param player Player number (1-4)
   * @return Current controller state
   */
  virtual ControllerState Poll(int player = 1) = 0;

  /**
   * @brief Process platform-specific events (optional)
   * @param event Platform-specific event data (e.g., SDL_Event*)
   */
  virtual void ProcessEvent(void* event) = 0;

  /**
   * @brief Get current configuration
   */
  virtual InputConfig GetConfig() const = 0;

  /**
   * @brief Update configuration (hot-reload)
   */
  virtual void SetConfig(const InputConfig& config) = 0;

  /**
   * @brief Get backend name for debugging
   */
  virtual std::string GetBackendName() const = 0;

  /**
   * @brief Check if backend is initialized
   */
  virtual bool IsInitialized() const = 0;
};

/**
 * @brief Factory for creating input backends
 */
class InputBackendFactory {
 public:
  enum class BackendType {
    SDL2,
    SDL3,         // Future
    NULL_BACKEND  // For testing/replay
  };

  static std::unique_ptr<IInputBackend> Create(BackendType type);
};

}  // namespace input
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_INPUT_INPUT_BACKEND_H_
