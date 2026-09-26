#include "app/editor/system/commands/command_palette_providers.h"

#include <map>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/editor_registry.h"
#include "app/editor/system/workspace/workspace_window_manager.h"

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

std::string LookupShortcutHint(const ShortcutManager* shortcut_manager,
                               const std::string& name) {
  return shortcut_manager ? shortcut_manager->GetDisplayString(name)
                          : std::string();
}

std::string PaletteNameForShortcut(const std::string& name) {
  if (!CommandPalette::IsInternalCommandId(name))
    return name;
  // "switch.<EditorType>" -> "Switch to <Editor Name>" so it merges with the
  // palette's "Switch to: <Category> Editor" entry and lends it Ctrl+<n>.
  if (absl::StartsWith(name, "switch.")) {
    int type_value = 0;
    if (absl::SimpleAtoi(name.substr(7), &type_value) && type_value > 0 &&
        type_value < static_cast<int>(kEditorTypeCount)) {
      return "Switch to " +
             EditorRegistry::GetEditorName(static_cast<EditorType>(type_value));
    }
  }
  return {};
}

OverworldMapCommandsProvider::OverworldMapCommandsProvider(size_t session_id)
    : session_id_(session_id) {}

void OverworldMapCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterOverworldMapCommands(session_id_);
}

ShortcutCommandsProvider::ShortcutCommandsProvider(
    const ShortcutManager* shortcut_manager)
    : shortcut_manager_(shortcut_manager) {}

void ShortcutCommandsProvider::Provide(CommandPalette* palette) {
  if (!palette || !shortcut_manager_)
    return;
  // Several ids can map to one palette name ("switch.6" and the keyless
  // "Switch to Overworld Editor" command). Keep the bound one so the hint is
  // not lost to unordered_map iteration order.
  struct Pick {
    std::string source;
    std::string hint;
  };
  std::map<std::string, Pick> picks;
  for (const auto& [name, shortcut] : shortcut_manager_->GetShortcuts()) {
    if (!shortcut.callback)
      continue;  // Bindings without an action would be no-op rows.
    const std::string palette_name = PaletteNameForShortcut(name);
    if (palette_name.empty())
      continue;
    Pick pick{name, LookupShortcutHint(shortcut_manager_, name)};
    auto it = picks.find(palette_name);
    if (it == picks.end()) {
      picks.emplace(palette_name, std::move(pick));
    } else if (it->second.hint.empty() && !pick.hint.empty()) {
      it->second = std::move(pick);
    }
  }
  const ShortcutManager* manager = shortcut_manager_;
  for (const auto& [palette_name, pick] : picks) {
    palette->AddCommand(palette_name, "Shortcuts",
                        InferShortcutGroup(pick.source), pick.hint,
                        [manager, source = pick.source]() {
                          const Shortcut* live = manager->FindShortcut(source);
                          if (live && live->callback)
                            live->callback();
                        });
  }
}

DrawerCommandsProvider::DrawerCommandsProvider(
    std::function<void(int drawer_type)> toggle_callback,
    std::function<void()> cycle_next, std::function<void()> cycle_prev)
    : toggle_callback_(std::move(toggle_callback)),
      cycle_next_(std::move(cycle_next)),
      cycle_prev_(std::move(cycle_prev)) {}

void DrawerCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterDrawerCommands(toggle_callback_, cycle_next_, cycle_prev_);
}

LayoutCommandsProvider::LayoutCommandsProvider(
    std::function<void(const std::string&)> apply_callback)
    : apply_callback_(std::move(apply_callback)) {}

void LayoutCommandsProvider::Provide(CommandPalette* palette) {
  palette->RegisterLayoutCommands(apply_callback_);
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
  if (!palette || !user_settings_ || !window_manager_)
    return;

  auto categories = window_manager_->GetAllCategories(session_id_);

  // Global helpers first — they're callable any time.
  palette->AddCommand("Sidebar: Reset Order", CommandCategory::kView,
                      "Clear the custom sidebar order and revert to defaults",
                      /*shortcut=*/"", [this]() {
                        if (!user_settings_)
                          return;
                        user_settings_->prefs().sidebar_order.clear();
                        (void)user_settings_->Save();
                      });

  palette->AddCommand("Sidebar: Show All Categories", CommandCategory::kView,
                      "Un-hide every sidebar category",
                      /*shortcut=*/"", [this]() {
                        if (!user_settings_)
                          return;
                        user_settings_->prefs().sidebar_hidden.clear();
                        (void)user_settings_->Save();
                      });

  // Per-category toggles. Using static "Toggle" labels avoids the need to
  // refresh the provider every time state flips.
  for (const auto& cat : categories) {
    if (cat == WorkspaceWindowManager::kDashboardCategory)
      continue;

    std::string pin_name = absl::StrFormat("Sidebar: Toggle Pin: %s", cat);
    std::string pin_desc =
        absl::StrFormat("Pin or unpin %s at the top of the sidebar", cat);
    std::string captured_cat_pin = cat;
    palette->AddCommand(pin_name, CommandCategory::kView, pin_desc,
                        /*shortcut=*/"", [this, captured_cat_pin]() {
                          if (!user_settings_)
                            return;
                          auto& pinned = user_settings_->prefs().sidebar_pinned;
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
                        /*shortcut=*/"", [this, captured_cat_hide]() {
                          if (!user_settings_)
                            return;
                          auto& hidden = user_settings_->prefs().sidebar_hidden;
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
