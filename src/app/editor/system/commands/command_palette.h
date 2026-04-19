#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace editor {

class CommandPalette;
class WorkspaceWindowManager;
class EditorRegistry;
class RecentProjectsModel;

/**
 * @brief Plug-in command source for the palette.
 *
 * A provider is a cohesive group of commands (e.g., panel toggles, recent
 * files). Entries added during Provide() are attributed to the provider's id
 * so they can be selectively refreshed or removed later.
 */
class CommandProvider {
 public:
  virtual ~CommandProvider() = default;
  /// Stable identifier. Used for selective refresh; must be unique.
  virtual std::string ProviderId() const = 0;
  /// Populate @p palette with this source's commands.
  virtual void Provide(CommandPalette* palette) = 0;
};

/**
 * @brief Categories for command palette entries
 */
struct CommandCategory {
  static constexpr const char* kPanel = "Panels";
  static constexpr const char* kEditor = "Editor";
  static constexpr const char* kLayout = "Layout";
  static constexpr const char* kFile = "File";
  static constexpr const char* kEdit = "Edit";
  static constexpr const char* kView = "View";
  static constexpr const char* kNavigation = "Navigation";
  static constexpr const char* kTools = "Tools";
  static constexpr const char* kWorkflow = "Workflow";
  static constexpr const char* kHelp = "Help";
};

struct CommandEntry {
  std::string name;
  std::string category;
  std::string description;
  std::string shortcut;
  std::function<void()> callback;
  int usage_count = 0;
  int64_t last_used_ms = 0;
  /// Provider attribution. Empty means the entry was added directly (not
  /// through a registered CommandProvider) and cannot be selectively refreshed.
  std::string provider_id;
};

class CommandPalette {
 public:
  void AddCommand(const std::string& name, const std::string& category,
                  const std::string& description, const std::string& shortcut,
                  std::function<void()> callback);

  void RecordUsage(const std::string& name);

  std::vector<CommandEntry> SearchCommands(const std::string& query);

  std::vector<CommandEntry> GetRecentCommands(int limit = 10);

  std::vector<CommandEntry> GetFrequentCommands(int limit = 10);

  /**
   * @brief Get all registered commands
   * @return Vector of all command entries
   */
  std::vector<CommandEntry> GetAllCommands() const;

  /**
   * @brief Get command count
   */
  size_t GetCommandCount() const { return commands_.size(); }

  /**
   * @brief Clear all commands (and forget every registered provider).
   */
  void Clear();

  // ============================================================================
  // Providers
  // ============================================================================

  /// Register @p provider and run it once. Entries added during Provide() are
  /// attributed to the provider's id. The provider stays registered so it can
  /// be re-run via RefreshProvider / RefreshProviders. A duplicate id replaces
  /// the prior registration (the old provider's commands are dropped first).
  void RegisterProvider(std::unique_ptr<CommandProvider> provider);

  /// Remove every command attributed to @p provider_id and drop the provider.
  /// No-op if the id is unknown.
  void UnregisterProvider(const std::string& provider_id);

  /// Remove-and-re-run a single provider. Useful after its underlying data
  /// changed (e.g., recent-files list). No-op if the id is unknown.
  void RefreshProvider(const std::string& provider_id);

  /// Remove-and-re-run every registered provider.
  void RefreshProviders();

  /// Remove every command with the given provider attribution. Leaves the
  /// provider itself registered so RefreshProvider can repopulate later.
  void RemoveProviderCommands(const std::string& provider_id);

  // ============================================================================
  // Bulk Registration Methods (used by built-in providers; kept public so
  // tests and legacy callers can still drive registration directly).
  // ============================================================================

  /**
   * @brief Register all window toggle commands from WorkspaceWindowManager
   * @param window_manager The panel manager to query for panels
   * @param session_id Current session ID for panel prefixing
   */
  void RegisterPanelCommands(WorkspaceWindowManager* window_manager,
                             size_t session_id);

  /**
   * @brief Register all editor switch commands
   * @param switch_callback Callback to switch to an editor category
   */
  void RegisterEditorCommands(
      std::function<void(const std::string&)> switch_callback);

  /**
   * @brief Register layout preset commands
   * @param apply_callback Callback to apply a layout preset by name
   */
  void RegisterLayoutCommands(
      std::function<void(const std::string&)> apply_callback);

  /**
   * @brief Register commands to open recent files
   * @param open_callback Callback to open a file by path
   *
   * Creates "Open Recent: <filename>" commands for each file in
   * RecentFilesManager. Files are checked for existence before registration.
   */
  void RegisterRecentFilesCommands(
      std::function<void(const std::string&)> open_callback);

  /**
   * @brief Register dungeon room navigation commands.
   *
   * Adds one command per room ID (0x000-0x127) using the ResourceLabelProvider
   * label (when available). Commands publish JumpToRoomRequestEvent.
   */
  void RegisterDungeonRoomCommands(size_t session_id);

  /**
   * @brief Register hack workflow commands from workflow-aware panels/actions.
   */
  void RegisterWorkflowCommands(WorkspaceWindowManager* window_manager,
                                size_t session_id);

  /**
   * @brief Expose welcome-screen actions through the command palette.
   *
   * Registers commands that surface recent-project mutations (remove, pin,
   * undo, clear), template-based project creation, and welcome-screen
   * visibility toggles. This lets power users drive the welcome screen
   * entirely from the palette without opening it.
   *
   * The RecentProjectsModel pointer may be null; in that case the per-entry
   * remove/pin commands are skipped but the global commands still register.
   * @param model Source of recent-project entries (not owned, may be null).
   * @param template_names Display names of project templates to register
   *        "Create from Template: <name>" commands for. If empty, the
   *        template commands are skipped.
   * @param remove_callback Invoked with the filepath to remove from recents.
   * @param toggle_pin_callback Invoked with the filepath to flip pin state.
   * @param undo_remove_callback Invoked to restore the last-removed entry.
   * @param clear_recents_callback Invoked to clear all recents.
   * @param create_from_template_callback Invoked with the template display
   *        name to kick off the "new project" flow for that template.
   * @param dismiss_welcome_callback Invoked to hide the welcome screen.
   * @param show_welcome_callback Invoked to bring the welcome screen back.
   */
  void RegisterWelcomeCommands(
      const RecentProjectsModel* model,
      const std::vector<std::string>& template_names,
      std::function<void(const std::string&)> remove_callback,
      std::function<void(const std::string&)> toggle_pin_callback,
      std::function<void()> undo_remove_callback,
      std::function<void()> clear_recents_callback,
      std::function<void(const std::string&)> create_from_template_callback,
      std::function<void()> dismiss_welcome_callback,
      std::function<void()> show_welcome_callback);

  /**
   * @brief Save command usage history to disk
   * @param filepath Path to save JSON history file
   */
  void SaveHistory(const std::string& filepath);

  /**
   * @brief Load command usage history from disk
   * @param filepath Path to load JSON history file from
   */
  void LoadHistory(const std::string& filepath);

  static int FuzzyScore(const std::string& text, const std::string& query);

 private:
  std::unordered_map<std::string, CommandEntry> commands_;
  std::vector<std::unique_ptr<CommandProvider>> providers_;
  /// Set while a provider's Provide() call is on the stack; stamped into every
  /// CommandEntry added during that call. Empty outside Provide().
  std::string current_provider_id_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
