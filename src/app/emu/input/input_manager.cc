#include "app/emu/input/input_manager.h"

#include "app/emu/snes.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace input {

bool InputManager::Initialize(InputBackendFactory::BackendType type) {
  backend_ = InputBackendFactory::Create(type);
  if (!backend_) {
    LOG_ERROR("InputManager", "Failed to create input backend");
    return false;
  }
  
  InputConfig config;
  config.continuous_polling = true;  // Always use continuous polling for games
  config.enable_gamepad = false;      // TODO: Enable when gamepad support added
  
  if (!backend_->Initialize(config)) {
    LOG_ERROR("InputManager", "Failed to initialize input backend");
    return false;
  }
  
  LOG_INFO("InputManager", "Initialized with backend: %s", 
           backend_->GetBackendName().c_str());
  return true;
}

void InputManager::Initialize(std::unique_ptr<IInputBackend> backend) {
  backend_ = std::move(backend);
  
  if (backend_) {
    LOG_INFO("InputManager", "Initialized with custom backend: %s",
             backend_->GetBackendName().c_str());
  }
}

void InputManager::Shutdown() {
  if (backend_) {
    backend_->Shutdown();
    backend_.reset();
  }
}

void InputManager::Poll(Snes* snes, int player) {
  if (!snes || !backend_) return;
  
  // Poll backend for current controller state
  ControllerState state = backend_->Poll(player);
  
  // Update SNES controller state using the hardware button layout
  // SNES controller bits: 0=B, 1=Y, 2=Select, 3=Start, 4-7=DPad, 8=A, 9=X, 10=L, 11=R
  for (int i = 0; i < 12; i++) {
    bool pressed = (state.buttons & (1 << i)) != 0;
    snes->SetButtonState(player, i, pressed);
  }
}

void InputManager::ProcessEvent(void* event) {
  if (backend_) {
    backend_->ProcessEvent(event);
  }
}

InputConfig InputManager::GetConfig() const {
  if (backend_) {
    return backend_->GetConfig();
  }
  return InputConfig{};
}

void InputManager::SetConfig(const InputConfig& config) {
  if (backend_) {
    backend_->SetConfig(config);
  }
}

}  // namespace input
}  // namespace emu
}  // namespace yaze

