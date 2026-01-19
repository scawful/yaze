#include "app/editor/core/content_registry.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/editor/core/event_bus.h"
#include "app/editor/system/editor_panel.h"
#include "rom/rom.h"

namespace yaze::editor {

namespace {

// Singleton storage for ContentRegistry state
// Uses lazy initialization to avoid static initialization order issues
struct RegistryState {
  std::mutex mutex;
  Rom* current_rom = nullptr;
  ::yaze::EventBus* event_bus = nullptr;
  std::vector<std::unique_ptr<EditorPanel>> panels;

  static RegistryState& Get() {
    static RegistryState instance;
    return instance;
  }
};

}  // namespace

// =============================================================================
// Context Implementation
// =============================================================================

namespace ContentRegistry {

namespace Context {

Rom* rom() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().current_rom;
}

void SetRom(Rom* rom) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().current_rom = rom;
}

::yaze::EventBus* event_bus() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().event_bus;
}

void SetEventBus(::yaze::EventBus* bus) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().event_bus = bus;
}

void Clear() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().current_rom = nullptr;
  RegistryState::Get().event_bus = nullptr;
}

}  // namespace Context

// =============================================================================
// Panels Implementation
// =============================================================================

namespace Panels {

void Register(std::unique_ptr<EditorPanel> panel) {
  if (!panel) return;

  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().panels.push_back(std::move(panel));
}

std::vector<EditorPanel*> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);

  std::vector<EditorPanel*> result;
  result.reserve(RegistryState::Get().panels.size());

  for (const auto& panel : RegistryState::Get().panels) {
    result.push_back(panel.get());
  }

  return result;
}

EditorPanel* Get(const std::string& id) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);

  for (const auto& panel : RegistryState::Get().panels) {
    if (panel->GetId() == id) {
      return panel.get();
    }
  }

  return nullptr;
}

void Clear() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().panels.clear();
}

}  // namespace Panels

}  // namespace ContentRegistry

}  // namespace yaze::editor
