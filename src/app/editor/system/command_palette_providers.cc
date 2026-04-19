#include "app/editor/system/command_palette_providers.h"

#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace_window_manager.h"

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

SidebarCommandsProvider::SidebarCommandsProvider(
    WorkspaceWindowManager* window_manager, UserSettings* user_settings,
    size_t session_id)
    : window_manager_(window_manager),
      user_settings_(user_settings),
      session_id_(session_id) {}

void SidebarCommandsProvider::Provide(CommandPalette* palette) {
  if (!palette || !user_settings_ || !window_manager_) return;

  auto categories = window_manager_->GetAllCategories(session_id_);

  // Global helpers first — they're callable any time.
  palette->AddCommand(
      "Sidebar: Reset Order", CommandCategory::kView,
      "Clear the custom sidebar order and revert to defaults",
      /*shortcut=*/"", [this]() {
        if (!user_settings_) return;
        user_settings_->prefs().sidebar_order.clear();
        (void)user_settings_->Save();
      });

  palette->AddCommand(
      "Sidebar: Show All Categories", CommandCategory::kView,
      "Un-hide every sidebar category",
      /*shortcut=*/"", [this]() {
        if (!user_settings_) return;
        user_settings_->prefs().sidebar_hidden.clear();
        (void)user_settings_->Save();
      });

  // Per-category toggles. Using static "Toggle" labels avoids the need to
  // refresh the provider every time state flips.
  for (const auto& cat : categories) {
    if (cat == WorkspaceWindowManager::kDashboardCategory) continue;

    std::string pin_name =
        absl::StrFormat("Sidebar: Toggle Pin: %s", cat);
    std::string pin_desc =
        absl::StrFormat("Pin or unpin %s at the top of the sidebar", cat);
    std::string captured_cat_pin = cat;
    palette->AddCommand(pin_name, CommandCategory::kView, pin_desc,
                        /*shortcut=*/"",
                        [this, captured_cat_pin]() {
                          if (!user_settings_) return;
                          auto& pinned =
                              user_settings_->prefs().sidebar_pinned;
                          if (pinned.count(captured_cat_pin)) {
                            pinned.erase(captured_cat_pin);
                          } else {
                            pinned.insert(captured_cat_pin);
                          }
                          (void)user_settings_->Save();
                        });

    std::string hide_name =
        absl::StrFormat("Sidebar: Toggle Visibility: %s", cat);
    std::string hide_desc =
        absl::StrFormat("Show or hide %s in the sidebar", cat);
    std::string captured_cat_hide = cat;
    palette->AddCommand(hide_name, CommandCategory::kView, hide_desc,
                        /*shortcut=*/"",
                        [this, captured_cat_hide]() {
                          if (!user_settings_) return;
                          auto& hidden =
                              user_settings_->prefs().sidebar_hidden;
                          if (hidden.count(captured_cat_hide)) {
                            hidden.erase(captured_cat_hide);
                          } else {
                            hidden.insert(captured_cat_hide);
                          }
                          (void)user_settings_->Save();
                        });
  }
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
