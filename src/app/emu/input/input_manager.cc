#include "app/emu/input/input_manager.h"

#include "app/emu/snes.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace input {

InputManager::InputManager() {
  config_.continuous_polling = true;
  config_.enable_gamepad = false;
  config_.gamepad_index = 0;
  ApplyDefaultKeyBindings(config_);
}

bool InputManager::Initialize(InputBackendFactory::BackendType type) {
  backend_ = InputBackendFactory::Create(type);
  if (!backend_) {
    LOG_ERROR("InputManager", "Failed to create input backend");
    return false;
  }

  ApplyDefaultKeyBindings(config_);

  if (!backend_->Initialize(config_)) {
    LOG_ERROR("InputManager", "Failed to initialize input backend");
    return false;
  }

  config_ = backend_->GetConfig();

  LOG_INFO("InputManager", "Initialized with backend: %s",
           backend_->GetBackendName().c_str());
  return true;
}

void InputManager::Initialize(std::unique_ptr<IInputBackend> backend) {
  backend_ = std::move(backend);

  if (backend_) {
    if (!backend_->IsInitialized()) {
      ApplyDefaultKeyBindings(config_);
      backend_->Initialize(config_);
    }
    config_ = backend_->GetConfig();
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
  if (!snes || !backend_)
    return;

  ControllerState physical_state = backend_->Poll(player);

  // Combine physical input with agent-controlled input (OR operation)
  ControllerState final_state;
  final_state.buttons =
      physical_state.buttons | agent_controller_state_.buttons;

  // Apply button state directly to SNES
  // Just send the raw button state on every Poll() call
  // The button state will be latched by HandleInput() at VBlank
  for (int i = 0; i < 12; i++) {
    bool button_held = (final_state.buttons & (1 << i)) != 0;
    snes->SetButtonState(player, i, button_held);
  }

  // Debug: Log complete button state when any button is pressed
  static int poll_log_count = 0;
  if (final_state.buttons != 0 && poll_log_count++ < 50) {
    LOG_INFO("InputManager", "Poll: buttons=0x%04X (passed to SetButtonState)",
             final_state.buttons);
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
  return config_;
}

void InputManager::SetConfig(const InputConfig& config) {
  config_ = config;
  if (!config_.continuous_polling) {
    LOG_WARN("InputManager",
             "continuous_polling disabled in config; forcing it ON to keep edge "
             "detection working for menus (event-based path is not wired)");
    config_.continuous_polling = true;
  }
  // Always ignore ImGui text input capture for game controls to avoid blocking
  if (!config_.ignore_imgui_text_input) {
    LOG_WARN("InputManager",
             "ignore_imgui_text_input was false; forcing true so game input is not blocked");
    config_.ignore_imgui_text_input = true;
  }
  ApplyDefaultKeyBindings(config_);
  if (backend_) {
    backend_->SetConfig(config_);
    config_ = backend_->GetConfig();
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
