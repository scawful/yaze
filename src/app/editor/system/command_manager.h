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

  void Undo() {
    if (!undo_stack_.empty()) {
      undo_stack_.top()->Execute();
      redo_stack_.push(undo_stack_.top());
      undo_stack_.pop();
    }
  }

  void Redo() {
    if (!redo_stack_.empty()) {
      redo_stack_.top()->Execute();
      undo_stack_.push(redo_stack_.top());
      redo_stack_.pop();
    }
  }

 private:
  std::stack<Command*> undo_stack_;
  std::stack<Command*> redo_stack_;
  std::unordered_map<std::string, Command*> commands_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze