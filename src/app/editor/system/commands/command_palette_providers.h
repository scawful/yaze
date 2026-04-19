#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_PROVIDERS_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_PROVIDERS_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/system/commands/command_palette.h"

namespace yaze {
namespace editor {

class WorkspaceWindowManager;
class RecentProjectsModel;
class UserSettings;

/**
 * Thin, pull-based CommandProvider adapters over the existing Register*Commands
 * bulk methods. Each provider owns only the dependencies needed to repopulate
 * its entries and can be re-run via CommandPalette::RefreshProvider(id).
 *
 * Provider IDs are stable strings — treat them as part of the public contract
 * for anything that wants to trigger a targeted refresh.
 */

/// ID: "panels". Rebuilds when the set of open windows/panels changes.
class PanelCommandsProvider : public CommandProvider {
 public:
  PanelCommandsProvider(WorkspaceWindowManager* window_manager,
                        size_t session_id);
  std::string ProviderId() const override { return "panels"; }
  void Provide(CommandPalette* palette) override;

 private:
  WorkspaceWindowManager* window_manager_;
  size_t session_id_;
};

/// ID: "editors". Rebuilds on editor-registry changes (rare).
class EditorCommandsProvider : public CommandProvider {
 public:
  explicit EditorCommandsProvider(
      std::function<void(const std::string&)> switch_callback);
  std::string ProviderId() const override { return "editors"; }
  void Provide(CommandPalette* palette) override;

 private:
  std::function<void(const std::string&)> switch_callback_;
};

/// ID: "recent-files". Refresh after RecentFilesManager mutates.
class RecentFilesCommandsProvider : public CommandProvider {
 public:
  explicit RecentFilesCommandsProvider(
      std::function<void(const std::string&)> open_callback);
  std::string ProviderId() const override { return "recent-files"; }
  void Provide(CommandPalette* palette) override;

 private:
  std::function<void(const std::string&)> open_callback_;
};

/// ID: "dungeon-rooms". One-shot; only depends on the session id.
class DungeonRoomCommandsProvider : public CommandProvider {
 public:
  explicit DungeonRoomCommandsProvider(size_t session_id);
  std::string ProviderId() const override { return "dungeon-rooms"; }
  void Provide(CommandPalette* palette) override;

 private:
  size_t session_id_;
};

/// ID: "workflow". Refresh when workflow actions change.
class WorkflowCommandsProvider : public CommandProvider {
 public:
  WorkflowCommandsProvider(WorkspaceWindowManager* window_manager,
                           size_t session_id);
  std::string ProviderId() const override { return "workflow"; }
  void Provide(CommandPalette* palette) override;

 private:
  WorkspaceWindowManager* window_manager_;
  size_t session_id_;
};

/// ID: "sidebar". Contributes activity-bar customization commands: pin,
/// hide, reset-order, and show-all toggles. Reads and mutates UserSettings
/// directly; refresh after mutation to keep palette entries in sync with the
/// sidebar state.
class SidebarCommandsProvider : public CommandProvider {
 public:
  SidebarCommandsProvider(WorkspaceWindowManager* window_manager,
                          UserSettings* user_settings, size_t session_id);
  std::string ProviderId() const override { return "sidebar"; }
  void Provide(CommandPalette* palette) override;

 private:
  WorkspaceWindowManager* window_manager_;
  UserSettings* user_settings_;
  size_t session_id_;
};

/// ID: "welcome". Refresh after the recent-projects model mutates (pin,
/// remove, undo, clear). Callbacks are bundled so the provider stays a single
/// registration site.
class WelcomeCommandsProvider : public CommandProvider {
 public:
  struct Callbacks {
    const RecentProjectsModel* model = nullptr;
    std::vector<std::string> template_names;
    std::function<void(const std::string&)> remove;
    std::function<void(const std::string&)> toggle_pin;
    std::function<void()> undo_remove;
    std::function<void()> clear_recents;
    std::function<void(const std::string&)> create_from_template;
    std::function<void()> dismiss_welcome;
    std::function<void()> show_welcome;
  };
  explicit WelcomeCommandsProvider(Callbacks callbacks);
  std::string ProviderId() const override { return "welcome"; }
  void Provide(CommandPalette* palette) override;

 private:
  Callbacks callbacks_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_PROVIDERS_H_
