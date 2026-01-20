#include "app/editor/core/content_registry.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/editor/core/event_bus.h"
#include "app/editor/system/editor_panel.h"
#include "app/editor/editor.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

namespace {

// Singleton storage for ContentRegistry state
// Uses lazy initialization to avoid static initialization order issues
struct RegistryState {
  std::mutex mutex;
  Rom* current_rom = nullptr;
  ::yaze::EventBus* event_bus = nullptr;
  Editor* current_editor = nullptr;
  ::yaze::zelda3::GameData* game_data = nullptr;
  std::vector<std::unique_ptr<EditorPanel>> panels;
  std::vector<ContentRegistry::Panels::PanelFactory> factories;
  std::vector<ContentRegistry::Editors::EditorFactory> editor_factories;
  std::vector<ContentRegistry::Shortcuts::ShortcutDef> shortcuts;
  std::vector<ContentRegistry::Settings::SettingDef> settings;

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

Editor* current_editor() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().current_editor;
}

void SetCurrentEditor(Editor* editor) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().current_editor = editor;
}

::yaze::zelda3::GameData* game_data() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().game_data;
}

void SetGameData(::yaze::zelda3::GameData* data) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().game_data = data;
}

void Clear() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().current_rom = nullptr;
  RegistryState::Get().event_bus = nullptr;
  RegistryState::Get().current_editor = nullptr;
  RegistryState::Get().game_data = nullptr;
}

}  // namespace Context

// =============================================================================
// Panels Implementation
// =============================================================================

namespace Panels {

void RegisterFactory(PanelFactory factory) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().factories.push_back(std::move(factory));
}

std::vector<std::unique_ptr<EditorPanel>> CreateAll() {
  std::vector<PanelFactory> factories;
  {
    std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
    factories = RegistryState::Get().factories;
  }

  std::vector<std::unique_ptr<EditorPanel>> result;
  result.reserve(factories.size());

  for (const auto& factory : factories) {
    if (auto panel = factory()) {
      result.push_back(std::move(panel));
    }
  }
  return result;
}

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

// =============================================================================
// Editors Implementation
// =============================================================================

namespace Editors {

void RegisterFactory(EditorFactory factory) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().editor_factories.push_back(std::move(factory));
}

std::vector<std::unique_ptr<Editor>> CreateAll(const EditorDependencies& deps) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  std::vector<std::unique_ptr<Editor>> result;
  result.reserve(RegistryState::Get().editor_factories.size());

  for (const auto& factory : RegistryState::Get().editor_factories) {
    if (auto editor = factory(deps)) {
      result.push_back(std::move(editor));
    }
  }
  return result;
}

}  // namespace Editors

// =============================================================================
// Shortcuts Implementation
// =============================================================================

namespace Shortcuts {

void Register(const ShortcutDef& shortcut) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().shortcuts.push_back(shortcut);
}

void add(const std::string& id, const std::string& key, const std::string& desc) {
  Register({id, key, desc});
}

std::vector<ShortcutDef> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().shortcuts;
}

}  // namespace Shortcuts

// =============================================================================
// Settings Implementation
// =============================================================================

namespace Settings {

void Register(const SettingDef& setting) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().settings.push_back(setting);
}

void add(const std::string& section, const std::string& key, const std::string& default_val, const std::string& desc) {
  Register({key, section, default_val, desc});
}

std::vector<SettingDef> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().settings;
}

}  // namespace Settings

}  // namespace ContentRegistry

}  // namespace yaze::editor
