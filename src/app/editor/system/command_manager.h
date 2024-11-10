#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H

#include <stack>
#include <string>
#include <unordered_map>

namespace yaze {
namespace app {
namespace editor {

class CommandManager {
 public:
  using Command = std::function<void()>;

  struct CommandInfo {
    Command command;
    char mnemonic;
    std::string name;
    std::string desc;
    CommandInfo(Command command, char mnemonic, const std::string& name,
                const std::string& desc)
        : command(std::move(command)),
          mnemonic(mnemonic),
          name(name),
          desc(desc) {}
    CommandInfo() = default;
  };

  void RegisterCommand(const std::string& shortcut, Command command,
                       char mnemonic, const std::string& name,
                       const std::string& desc) {
    commands_[shortcut] = {std::move(command), mnemonic, name, desc};
  }

  void ExecuteCommand(const std::string& shortcut) {
    if (commands_.find(shortcut) != commands_.end()) {
      commands_[shortcut].command();
    }
  }

  void ShowWhichKey();

  void InitializeDefaults();

  void SaveKeybindings(const std::string& filepath);
  void LoadKeybindings(const std::string& filepath);

 private:
  std::unordered_map<std::string, CommandInfo> commands_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H