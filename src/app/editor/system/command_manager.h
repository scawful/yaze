#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class CommandManager {
 public:
  CommandManager() = default;
  ~CommandManager() = default;

  using Command = std::function<void()>;

  struct CommandInfo {
    Command command;
    char mnemonic;
    std::string name;
    std::string desc;
    CommandInfo(Command command, char mnemonic, const std::string &name,
                const std::string &desc)
        : command(std::move(command)),
          mnemonic(mnemonic),
          name(name),
          desc(desc) {}
    CommandInfo() = default;
  };

  // Simplified command structure without recursive types
  struct CommandGroup {
    CommandInfo main_command;
    std::unordered_map<std::string, CommandInfo> subcommands;
    
    CommandGroup() = default;
    CommandGroup(CommandInfo main) : main_command(std::move(main)) {}
  };

  void RegisterPrefix(const std::string &group_name, const char prefix,
                      const std::string &name, const std::string &desc) {
    commands_[group_name].main_command = {nullptr, prefix, name, desc};
  }

  void RegisterSubcommand(const std::string &group_name,
                          const std::string &shortcut, const char mnemonic,
                          const std::string &name, const std::string &desc,
                          Command command) {
    commands_[group_name].subcommands[shortcut] = {command, mnemonic, name, desc};
  }

  void RegisterCommand(const std::string &shortcut, Command command,
                       char mnemonic, const std::string &name,
                       const std::string &desc) {
    commands_[shortcut].main_command = {std::move(command), mnemonic, name, desc};
  }

  void ExecuteCommand(const std::string &shortcut) {
    if (commands_.find(shortcut) != commands_.end()) {
      commands_[shortcut].main_command.command();
    }
  }

  void ShowWhichKey();

  void SaveKeybindings(const std::string &filepath);
  void LoadKeybindings(const std::string &filepath);

 private:
  std::unordered_map<std::string, CommandGroup> commands_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
