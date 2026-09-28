#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
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
  static constexpr const char* kDrawer = "Drawers";
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
  /// Disabled entries are listed (e.g. a go-to kind with no jump path) but
  /// never executed. `note` explains why.
  bool enabled = true;
  std::string note;
};

/// A search hit: the (deduplicated) entry plus the score it matched with.
struct CommandMatch {
  CommandEntry entry;
  int score = 0;
};

class CommandPalette {
 public:
  void AddCommand(const std::string& name, const std::string& category,
                  const std::string& description, const std::string& shortcut,
                  std::function<void()> callback);

  /// Record one execution of @p name. Only names the palette knows (after
  /// dedupe, see CanonicalCommandName) are counted; returns false otherwise.
  /// Usage survives Clear()/provider refreshes.
  bool RecordUsage(const std::string& name);

  /// Ranked, deduplicated search. An empty query lists every visible command
  /// (recently used first, then by name). Recency/frequency only boosts
  /// entries that already match the query.
  std::vector<CommandEntry> SearchCommands(const std::string& query);
  std::vector<CommandMatch> Search(const std::string& query,
                                   int64_t now_ms) const;
  std::vector<CommandMatch> Search(const std::string& query) const;

  std::vector<CommandEntry> GetRecentCommands(int limit = 10);

  std::vector<CommandEntry> GetFrequentCommands(int limit = 10);

  /**
   * @brief Get all registered commands (raw, not deduplicated)
   * @return Vector of all command entries
   */
  std::vector<CommandEntry> GetAllCommands() const;

  /// Commands after hiding duplicates (see NormalizeCommandName). Each group
  /// is represented by one entry that inherits the first non-empty shortcut
  /// and the group's best usage stats. Sorted by name.
  std::vector<CommandEntry> GetVisibleCommands() const;

  /**
   * @brief Get command count
   */
  size_t GetCommandCount() const { return commands_.size(); }

  /**
   * @brief Clear all commands (and forget every registered provider).
   * Usage history is kept so refreshing providers does not reset frecency.
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
   * @brief Register overworld map navigation commands (0x00-0x9F).
   *
   * Commands publish JumpToMapRequestEvent; names carry the resource label
   * when one exists.
   */
  void RegisterOverworldMapCommands(size_t session_id);

  /**
   * @brief Register dungeon room navigation commands.
   *
   * Adds one command per room ID (0x000-0x127) using the ResourceLabelProvider
   * label (when available). Commands publish JumpToRoomRequestEvent.
   */
  void RegisterDungeonRoomCommands(size_t session_id);

  /**
   * @brief Register right-drawer toggle commands from GetDrawerCatalog().
   *
   * Names use a "drawer: " prefix so palette search (`drawer:`) groups them.
   * @param toggle_callback Receives RightDrawerManager::DrawerType as int.
   */
  void RegisterDrawerCommands(
      std::function<void(int drawer_type)> toggle_callback,
      std::function<void()> cycle_next = {},
      std::function<void()> cycle_prev = {});

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

  /// Legacy name for ScoreText (kept for the Window Finder).
  static int FuzzyScore(const std::string& text, const std::string& query);

  /// Unified scorer, case-insensitive. 0 = no match. Tiers (high to low):
  /// exact > prefix > substring at a word start > substring elsewhere >
  /// in-order subsequence. Subsequence matches earn a word-start bonus that
  /// outweighs the consecutive-character bonus, and always stay below every
  /// substring tier. Queries with spaces also match when every token matches.
  static int ScoreText(std::string_view text, std::string_view query);

  /// Score one entry: name first, then category/description at reduced
  /// weight, then a frecency boost that is only applied when the text
  /// matched. Returns 0 when nothing matched.
  static int ScoreEntry(const CommandEntry& entry, std::string_view query,
                        int64_t now_ms);

  /// Lowercase, strip punctuation and "(Alt)" suffixes, collapse spaces, and
  /// treat the word "panel(s)" as "window(s)". "Switch to: Overworld Editor"
  /// and "Switch to Overworld Editor" share a key, as do "Panel Browser" and
  /// "Window Browser", so only one of each is listed (Window names win).
  static std::string NormalizeCommandName(std::string_view name);

  /// True for dotted/underscored ids such as "switch.3" or
  /// "graphics.tool.pencil" that are keybinding ids, not user-facing names.
  static bool IsInternalCommandId(std::string_view name);

  /// Name of the entry that represents @p name after dedupe, or empty when
  /// the palette has no such command.
  std::string CanonicalCommandName(const std::string& name) const;

  static int64_t NowMs();

  /// Bumped on every mutation (commands, providers, usage). Lets callers
  /// cache Search() results per query across frames.
  uint64_t generation() const { return generation_; }

 private:
  uint64_t generation_ = 0;
  struct UsageStats {
    int count = 0;
    int64_t last_used_ms = 0;
  };

  std::unordered_map<std::string, CommandEntry> commands_;
  /// Usage keyed by command name; survives Clear() and provider refreshes.
  std::unordered_map<std::string, UsageStats> usage_;
  std::vector<std::unique_ptr<CommandProvider>> providers_;
  /// Set while a provider's Provide() call is on the stack; stamped into every
  /// CommandEntry added during that call. Empty outside Provide().
  std::string current_provider_id_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
