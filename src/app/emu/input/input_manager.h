#ifndef YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_
#define YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_

#include <memory>
#include "app/emu/input/input_backend.h"

namespace yaze {
namespace emu {

// Forward declaration
class Snes;

namespace input {

class InputManager {
 public:
  InputManager() = default;
  ~InputManager() { Shutdown(); }
  
  bool Initialize(InputBackendFactory::BackendType type = InputBackendFactory::BackendType::SDL2);
  void Initialize(std::unique_ptr<IInputBackend> backend);
  void Shutdown();
  void Poll(Snes* snes, int player = 1);
  void ProcessEvent(void* event);
  
  IInputBackend* backend() { return backend_.get(); }
  const IInputBackend* backend() const { return backend_.get(); }
  
  bool IsInitialized() const { return backend_ && backend_->IsInitialized(); }
  
  InputConfig GetConfig() const;
  void SetConfig(const InputConfig& config);
  
  // --- Agent Control API ---
  void PressButton(SnesButton button);
  void ReleaseButton(SnesButton button);
  
 private:
  std::unique_ptr<IInputBackend> backend_;
  ControllerState agent_controller_state_; // State controlled by agent
};

}  // namespace input
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_INPUT_INPUT_MANAGER_H_