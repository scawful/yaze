#ifndef YAZE_APP_EDITOR_REGISTRY_EDITOR_CONTEXT_H_
#define YAZE_APP_EDITOR_REGISTRY_EDITOR_CONTEXT_H_

#include "app/editor/registry/event_bus.h"

namespace yaze {
class Rom;

namespace project {
struct YazeProject;
}  // namespace project

namespace zelda3 {
class GameData;
}  // namespace zelda3

namespace editor {

class Editor;

/**
 * @class GlobalEditorContext
 * @brief Instance-based runtime context replacing ContentRegistry::Context.
 *
 * Holds all shared runtime state (ROM, GameData, project, editor, event bus)
 * that was previously accessed through the static ContentRegistry::Context API.
 *
 * New code should receive this via EditorDependencies rather than using
 * ContentRegistry::Context. The static API delegates to the singleton instance
 * set via ContentRegistry::Context::SetGlobalContext().
 */
class GlobalEditorContext {
 public:
  explicit GlobalEditorContext(EventBus& bus) : bus_(bus) {}

  // --- Event bus ---
  EventBus& GetEventBus() { return bus_; }
  const EventBus& GetEventBus() const { return bus_; }

  // --- ROM ---
  void SetCurrentRom(Rom* rom) { rom_ = rom; }
  Rom* GetCurrentRom() const { return rom_; }

  // --- Session ---
  void SetSessionId(size_t id) { session_id_ = id; }
  size_t GetSessionId() const { return session_id_; }

  // --- Game data ---
  void SetGameData(zelda3::GameData* data) { game_data_ = data; }
  zelda3::GameData* GetGameData() const { return game_data_; }

  // --- Project ---
  void SetCurrentProject(project::YazeProject* project) { project_ = project; }
  project::YazeProject* GetCurrentProject() const { return project_; }

  // --- Current editor ---
  void SetCurrentEditor(Editor* editor) { current_editor_ = editor; }
  Editor* GetCurrentEditor() const { return current_editor_; }

  // --- Bulk clear ---
  void Clear() {
    rom_ = nullptr;
    game_data_ = nullptr;
    project_ = nullptr;
    current_editor_ = nullptr;
    session_id_ = 0;
  }

 private:
  EventBus& bus_;
  Rom* rom_ = nullptr;
  size_t session_id_ = 0;
  zelda3::GameData* game_data_ = nullptr;
  project::YazeProject* project_ = nullptr;
  Editor* current_editor_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_REGISTRY_EDITOR_CONTEXT_H_