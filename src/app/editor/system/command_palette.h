#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace editor {

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

  void SaveHistory(const std::string& filepath);
  void LoadHistory(const std::string& filepath);

 private:
  std::unordered_map<std::string, CommandEntry> commands_;

  int FuzzyScore(const std::string& text, const std::string& query);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_H_
