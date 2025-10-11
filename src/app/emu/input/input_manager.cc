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
  config.continuous_polling = true;
  config.enable_gamepad = false;
  
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
  
  ControllerState physical_state = backend_->Poll(player);
  
  // Combine physical input with agent-controlled input (OR operation)
  ControllerState final_state;
  final_state.buttons = physical_state.buttons | agent_controller_state_.buttons;

  // Update ALL button states every frame to ensure proper press/release
  // This is critical for games that check button state every frame
  for (int i = 0; i < 12; i++) {
    bool pressed = (final_state.buttons & (1 << i)) != 0;
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

void InputManager::PressButton(SnesButton button) {
    agent_controller_state_.SetButton(button, true);
}

void InputManager::ReleaseButton(SnesButton button) {
    agent_controller_state_.SetButton(button, false);
}

}  // namespace input
}  // namespace emu
}  // namespace yaze