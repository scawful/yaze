#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_MANAGER_H

#include <string>
#include <unordered_map>

namespace yaze {
namespace app {
namespace editor {

class Command {
 public:
  virtual ~Command() = default;
  virtual void Execute() = 0;
};

class CommandManager {
 public:
  void RegisterCommand(const std::string& shortcut, Command* command) {
    commands_[shortcut] = command;
  }

  void ExecuteCommand(const std::string& shortcut) {
    if (commands_.find(shortcut) != commands_.end()) {
      commands_[shortcut]->Execute();
    }
  }

 private:
  std::unordered_map<std::string, Command*> commands_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze