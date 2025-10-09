#ifndef YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_
#define YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_

#include <memory>
#include "app/emu/input/input_backend.h"

namespace yaze {
namespace emu {

// Forward declaration
class Snes;

namespace input {

/**
 * @brief High-level input manager that bridges backend and SNES
 * 
 * This class provides a simple interface for both GUI and headless modes:
 * - Manages input backend lifecycle
 * - Polls input and updates SNES controller state
 * - Handles multiple players
 * - Supports hot-swapping input configurations
 */
class InputManager {
 public:
  InputManager() = default;
  ~InputManager() { Shutdown(); }
  
  /**
   * @brief Initialize with specific backend
   */
  bool Initialize(InputBackendFactory::BackendType type = InputBackendFactory::BackendType::SDL2);
  
  /**
   * @brief Initialize with custom backend
   */
  void Initialize(std::unique_ptr<IInputBackend> backend);
  
  /**
   * @brief Shutdown input system
   */
  void Shutdown();
  
  /**
   * @brief Poll input and update SNES controller state
   * @param snes SNES instance to update
   * @param player Player number (1-4)
   */
  void Poll(Snes* snes, int player = 1);
  
  /**
   * @brief Process platform-specific event (optional)
   * @param event Platform event (e.g., SDL_Event*)
   */
  void ProcessEvent(void* event);
  
  /**
   * @brief Get backend for configuration
   */
  IInputBackend* backend() { return backend_.get(); }
  const IInputBackend* backend() const { return backend_.get(); }
  
  /**
   * @brief Check if initialized
   */
  bool IsInitialized() const { return backend_ && backend_->IsInitialized(); }
  
  /**
   * @brief Get/set configuration
   */
  InputConfig GetConfig() const;
  void SetConfig(const InputConfig& config);
  
 private:
  std::unique_ptr<IInputBackend> backend_;
};

}  // namespace input
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_

