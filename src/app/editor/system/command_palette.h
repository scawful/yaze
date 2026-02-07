#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace editor {

class PanelManager;
class EditorRegistry;

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
   * @brief Clear all commands
   */
  void Clear() { commands_.clear(); }

  // ============================================================================
  // Bulk Registration Methods
  // ============================================================================

  /**
   * @brief Register all panel toggle commands from PanelManager
   * @param panel_manager The panel manager to query for panels
   * @param session_id Current session ID for panel prefixing
   */
  void RegisterPanelCommands(PanelManager* panel_manager, size_t session_id);

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
   * @brief Save command usage history to disk
   * @param filepath Path to save JSON history file
   */
  void SaveHistory(const std::string& filepath);

  /**
   * @brief Load command usage history from disk
   * @param filepath Path to load JSON history file from
   */
  void LoadHistory(const std::string& filepath);

 private:
  std::unordered_map<std::string, CommandEntry> commands_;

  int FuzzyScore(const std::string& text, const std::string& query);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
