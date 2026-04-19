#include "app/editor/registry/content_registry.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/hack/workflow/hack_workflow_backend.h"
#include "app/editor/registry/editor_context.h"
#include "app/editor/registry/event_bus.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

namespace {

// Singleton storage for ContentRegistry state
// Uses lazy initialization to avoid static initialization order issues
struct RegistryState {
  std::mutex mutex;

  // When global_context is set, Context delegates to it.
  // The raw pointers below are fallback storage used only before
  // GlobalEditorContext is wired in (early startup / tests).
  GlobalEditorContext* global_context = nullptr;

  Rom* current_rom = nullptr;
  ::yaze::EventBus* event_bus = nullptr;
  Editor* current_editor = nullptr;
  std::unordered_map<std::string, Editor*> current_window_editors;
  ::yaze::zelda3::GameData* game_data = nullptr;
  ::yaze::project::YazeProject* current_project = nullptr;
  workflow::HackWorkflowBackend* hack_workflow_backend = nullptr;
  ProjectWorkflowStatus build_workflow_status;
  ProjectWorkflowStatus run_workflow_status;
  std::string build_workflow_log;
  std::vector<ProjectWorkflowHistoryEntry> workflow_history;
  std::function<void()> start_build_workflow_callback;
  std::function<void()> run_project_workflow_callback;
  std::function<void()> show_workflow_output_callback;
  std::function<void()> cancel_build_workflow_callback;
  std::vector<std::unique_ptr<WindowContent>> panels;
  std::vector<ContentRegistry::Panels::PanelFactory> factories;
  std::vector<ContentRegistry::Editors::EditorFactory> editor_factories;
  std::vector<ContentRegistry::Shortcuts::ShortcutDef> shortcuts;
  std::vector<ContentRegistry::WorkflowActions::ActionDef> workflow_actions;
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

void SetGlobalContext(GlobalEditorContext* ctx) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().global_context = ctx;
}

Rom* rom() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context)
    return state.global_context->GetCurrentRom();
  return state.current_rom;
}

void SetRom(Rom* rom) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context) {
    state.global_context->SetCurrentRom(rom);
  }
  state.current_rom = rom;  // keep fallback in sync
}

::yaze::EventBus* event_bus() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context)
    return &state.global_context->GetEventBus();
  return state.event_bus;
}

void SetEventBus(::yaze::EventBus* bus) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  // EventBus in GlobalEditorContext is a reference set at construction,
  // so we don't update it here. Just keep fallback in sync.
  state.event_bus = bus;
}

Editor* current_editor() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context)
    return state.global_context->GetCurrentEditor();
  return state.current_editor;
}

void SetCurrentEditor(Editor* editor) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context) {
    state.global_context->SetCurrentEditor(editor);
  }
  state.current_editor = editor;
}

Editor* editor_window_context(const std::string& category) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  auto it = state.current_window_editors.find(category);
  return it != state.current_window_editors.end() ? it->second : nullptr;
}

void SetEditorWindowContext(const std::string& category, Editor* editor) {
  if (category.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().current_window_editors[category] = editor;
}

::yaze::zelda3::GameData* game_data() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context)
    return state.global_context->GetGameData();
  return state.game_data;
}

void SetGameData(::yaze::zelda3::GameData* data) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context) {
    state.global_context->SetGameData(data);
  }
  state.game_data = data;
}

::yaze::project::YazeProject* current_project() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context)
    return state.global_context->GetCurrentProject();
  return state.current_project;
}

void SetCurrentProject(::yaze::project::YazeProject* project) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context) {
    state.global_context->SetCurrentProject(project);
  }
  state.current_project = project;
}

workflow::HackWorkflowBackend* hack_workflow_backend() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().hack_workflow_backend;
}

workflow::ValidationCapability* hack_validation_backend() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return dynamic_cast<workflow::ValidationCapability*>(
      RegistryState::Get().hack_workflow_backend);
}

workflow::ProgressionCapability* hack_progression_backend() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return dynamic_cast<workflow::ProgressionCapability*>(
      RegistryState::Get().hack_workflow_backend);
}

workflow::PlanningCapability* hack_planning_backend() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return dynamic_cast<workflow::PlanningCapability*>(
      RegistryState::Get().hack_workflow_backend);
}

void SetHackWorkflowBackend(workflow::HackWorkflowBackend* backend) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().hack_workflow_backend = backend;
}

ProjectWorkflowStatus build_workflow_status() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().build_workflow_status;
}

void SetBuildWorkflowStatus(const ProjectWorkflowStatus& status) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().build_workflow_status = status;
}

ProjectWorkflowStatus run_workflow_status() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().run_workflow_status;
}

void SetRunWorkflowStatus(const ProjectWorkflowStatus& status) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().run_workflow_status = status;
}

std::string build_workflow_log() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().build_workflow_log;
}

void SetBuildWorkflowLog(const std::string& output) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().build_workflow_log = output;
}

std::vector<ProjectWorkflowHistoryEntry> workflow_history() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().workflow_history;
}

void AppendWorkflowHistory(const ProjectWorkflowHistoryEntry& entry) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& history = RegistryState::Get().workflow_history;
  history.insert(history.begin(), entry);
  constexpr size_t kMaxEntries = 20;
  if (history.size() > kMaxEntries) {
    history.resize(kMaxEntries);
  }
}

void ClearWorkflowHistory() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().workflow_history.clear();
}

std::function<void()> start_build_workflow_callback() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().start_build_workflow_callback;
}

void SetStartBuildWorkflowCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().start_build_workflow_callback = std::move(callback);
}

std::function<void()> run_project_workflow_callback() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().run_project_workflow_callback;
}

void SetRunProjectWorkflowCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().run_project_workflow_callback = std::move(callback);
}

std::function<void()> show_workflow_output_callback() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().show_workflow_output_callback;
}

void SetShowWorkflowOutputCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().show_workflow_output_callback = std::move(callback);
}

std::function<void()> cancel_build_workflow_callback() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().cancel_build_workflow_callback;
}

void SetCancelBuildWorkflowCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().cancel_build_workflow_callback = std::move(callback);
}

void Clear() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& state = RegistryState::Get();
  if (state.global_context) {
    state.global_context->Clear();
  }
  state.current_rom = nullptr;
  state.event_bus = nullptr;
  state.current_editor = nullptr;
  state.game_data = nullptr;
  state.current_project = nullptr;
  state.hack_workflow_backend = nullptr;
  state.current_window_editors.clear();
  state.build_workflow_status = ProjectWorkflowStatus{};
  state.run_workflow_status = ProjectWorkflowStatus{};
  state.build_workflow_log.clear();
  state.workflow_history.clear();
  state.start_build_workflow_callback = nullptr;
  state.run_project_workflow_callback = nullptr;
  state.show_workflow_output_callback = nullptr;
  state.cancel_build_workflow_callback = nullptr;
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

std::vector<std::unique_ptr<WindowContent>> CreateAll() {
  std::vector<PanelFactory> factories;
  {
    std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
    factories = RegistryState::Get().factories;
  }

  std::vector<std::unique_ptr<WindowContent>> result;
  result.reserve(factories.size());

  for (const auto& factory : factories) {
    if (auto panel = factory()) {
      result.push_back(std::move(panel));
    }
  }
  return result;
}

void Register(std::unique_ptr<WindowContent> panel) {
  if (!panel)
    return;

  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().panels.push_back(std::move(panel));
}

std::vector<WindowContent*> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);

  std::vector<WindowContent*> result;
  result.reserve(RegistryState::Get().panels.size());

  for (const auto& panel : RegistryState::Get().panels) {
    result.push_back(panel.get());
  }

  return result;
}

WindowContent* Get(const std::string& id) {
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

void add(const std::string& id, const std::string& key,
         const std::string& desc) {
  Register({id, key, desc});
}

std::vector<ShortcutDef> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().shortcuts;
}

}  // namespace Shortcuts

// =============================================================================
// WorkflowActions Implementation
// =============================================================================

namespace WorkflowActions {

void Register(const ActionDef& action) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  auto& actions = RegistryState::Get().workflow_actions;
  auto it = std::find_if(
      actions.begin(), actions.end(),
      [&](const ActionDef& existing) { return existing.id == action.id; });
  if (it != actions.end()) {
    *it = action;
    return;
  }
  actions.push_back(action);
}

std::vector<ActionDef> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().workflow_actions;
}

void Clear() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().workflow_actions.clear();
}

}  // namespace WorkflowActions

// =============================================================================
// Settings Implementation
// =============================================================================

namespace Settings {

void Register(const SettingDef& setting) {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  RegistryState::Get().settings.push_back(setting);
}

void add(const std::string& section, const std::string& key,
         const std::string& default_val, const std::string& desc) {
  Register({key, section, default_val, desc});
}

std::vector<SettingDef> GetAll() {
  std::lock_guard<std::mutex> lock(RegistryState::Get().mutex);
  return RegistryState::Get().settings;
}

}  // namespace Settings

}  // namespace ContentRegistry

}  // namespace yaze::editor
