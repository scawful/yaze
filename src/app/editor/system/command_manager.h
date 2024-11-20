#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>

#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

ImGuiKey MapKeyToImGuiKey(char key);

class CommandManager {
public:
  using Command = std::function<void()>;

  struct CommandInfo {
    Command command;
    char mnemonic;
    std::string name;
    std::string desc;
    CommandInfo(Command command, char mnemonic, const std::string &name,
                const std::string &desc)
        : command(std::move(command)), mnemonic(mnemonic), name(name),
          desc(desc) {}
    CommandInfo() = default;
  };

  // New command info which supports subsections of commands
  struct CommandInfoOrPrefix {
    CommandInfo command_info;
    std::unordered_map<std::string, CommandInfoOrPrefix> subcommands;
    CommandInfoOrPrefix(CommandInfo command_info)
        : command_info(std::move(command_info)) {}
    CommandInfoOrPrefix() = default;
  };

  void RegisterPrefix(const std::string &group_name, const char prefix,
                      const std::string &name, const std::string &desc) {
    commands_[group_name].command_info = {nullptr, prefix, name, desc};
  }

  void RegisterSubcommand(const std::string &group_name,
                          const std::string &shortcut, const char mnemonic,
                          const std::string &name, const std::string &desc,
                          Command command) {
    commands_[group_name].subcommands[shortcut].command_info = {
        command, mnemonic, name, desc};
  }

  void RegisterCommand(const std::string &shortcut, Command command,
                       char mnemonic, const std::string &name,
                       const std::string &desc) {
    commands_[shortcut].command_info = {std::move(command), mnemonic, name,
                                        desc};
  }

  void ExecuteCommand(const std::string &shortcut) {
    if (commands_.find(shortcut) != commands_.end()) {
      commands_[shortcut].command_info.command();
    }
  }

  void ShowWhichKey();

  void SaveKeybindings(const std::string &filepath);
  void LoadKeybindings(const std::string &filepath);

private:
  std::unordered_map<std::string, CommandInfoOrPrefix> commands_;
};

} // namespace editor
} // namespace app
} // namespace yaze

#endif // YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
