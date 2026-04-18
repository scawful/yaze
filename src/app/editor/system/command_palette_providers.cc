#include "app/editor/system/command_palette_providers.h"

#include <utility>

namespace yaze {
namespace editor {

PanelCommandsProvider::PanelCommandsProvider(
    WorkspaceWindowManager* window_manager, size_t session_id)
    : window_manager_(window_manager), session_id_(session_id) {}

void PanelCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterPanelCommands(window_manager_, session_id_);
}

EditorCommandsProvider::EditorCommandsProvider(
    std::function<void(const std::string&)> switch_callback)
    : switch_callback_(std::move(switch_callback)) {}

void EditorCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterEditorCommands(switch_callback_);
}

RecentFilesCommandsProvider::RecentFilesCommandsProvider(
    std::function<void(const std::string&)> open_callback)
    : open_callback_(std::move(open_callback)) {}

void RecentFilesCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterRecentFilesCommands(open_callback_);
}

DungeonRoomCommandsProvider::DungeonRoomCommandsProvider(size_t session_id)
    : session_id_(session_id) {}

void DungeonRoomCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterDungeonRoomCommands(session_id_);
}

WorkflowCommandsProvider::WorkflowCommandsProvider(
    WorkspaceWindowManager* window_manager, size_t session_id)
    : window_manager_(window_manager), session_id_(session_id) {}

void WorkflowCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterWorkflowCommands(window_manager_, session_id_);
}

WelcomeCommandsProvider::WelcomeCommandsProvider(Callbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

void WelcomeCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterWelcomeCommands(
      callbacks_.model, callbacks_.template_names, callbacks_.remove,
      callbacks_.toggle_pin, callbacks_.undo_remove, callbacks_.clear_recents,
      callbacks_.create_from_template, callbacks_.dismiss_welcome,
      callbacks_.show_welcome);
}

}  // namespace editor
}  // namespace yaze
