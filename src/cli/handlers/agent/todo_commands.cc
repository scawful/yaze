#include "cli/handlers/agent/todo_commands.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "cli/service/agent/todo_manager.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

using agent::TodoItem;
using agent::TodoManager;

// Global TODO manager instance
TodoManager& GetTodoManager() {
  static TodoManager manager;
  static bool initialized = false;
  if (!initialized) {
    auto status = manager.Initialize();
    if (!status.ok()) {
      std::cerr << "Warning: Failed to initialize TODO manager: "
                << status.message() << std::endl;
    }
    initialized = true;
  }
  return manager;
}

void PrintTodo(const TodoItem& item, bool detailed = false) {
  std::string status_emoji;
  switch (item.status) {
    case TodoItem::Status::PENDING:
      status_emoji = "⏳";
      break;
    case TodoItem::Status::IN_PROGRESS:
      status_emoji = "🔄";
      break;
    case TodoItem::Status::COMPLETED:
      status_emoji = "✅";
      break;
    case TodoItem::Status::BLOCKED:
      status_emoji = "🚫";
      break;
    case TodoItem::Status::CANCELLED:
      status_emoji = "❌";
      break;
  }

  std::cout << absl::StreamFormat("[%s] %s %s", item.id, status_emoji,
                                  item.description);

  if (!item.category.empty()) {
    std::cout << absl::StreamFormat(" [%s]", item.category);
  }

  if (item.priority > 0) {
    std::cout << absl::StreamFormat(" (priority: %d)", item.priority);
  }

  std::cout << std::endl;

  if (detailed) {
    std::cout << "  Status: " << item.StatusToString() << std::endl;
    std::cout << "  Created: " << item.created_at << std::endl;
    std::cout << "  Updated: " << item.updated_at << std::endl;

    if (!item.dependencies.empty()) {
      std::cout << "  Dependencies: " << absl::StrJoin(item.dependencies, ", ")
                << std::endl;
    }

    if (!item.tools_needed.empty()) {
      std::cout << "  Tools needed: " << absl::StrJoin(item.tools_needed, ", ")
                << std::endl;
    }

    if (!item.notes.empty()) {
      std::cout << "  Notes: " << item.notes << std::endl;
    }
  }
}

absl::Status HandleTodoCreate(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent todo create <description> [--category=<cat>] "
        "[--priority=<n>]");
  }

  std::string description = args[0];
  std::string category;
  int priority = 0;

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i].find("--category=") == 0) {
      category = args[i].substr(11);
    } else if (args[i].find("--priority=") == 0) {
      priority = std::stoi(args[i].substr(11));
    }
  }

  auto& manager = GetTodoManager();
  auto result = manager.CreateTodo(description, category, priority);

  if (!result.ok()) {
    return result.status();
  }

  std::cout << "Created TODO:" << std::endl;
  PrintTodo(*result, true);

  return absl::OkStatus();
}

absl::Status HandleTodoList(const std::vector<std::string>& args) {
  std::string status_filter;
  std::string category_filter;

  for (const auto& arg : args) {
    if (arg.find("--status=") == 0) {
      status_filter = arg.substr(9);
    } else if (arg.find("--category=") == 0) {
      category_filter = arg.substr(11);
    }
  }

  auto& manager = GetTodoManager();
  std::vector<TodoItem> todos;

  if (!status_filter.empty()) {
    auto status = TodoItem::StringToStatus(status_filter);
    todos = manager.GetTodosByStatus(status);
  } else if (!category_filter.empty()) {
    todos = manager.GetTodosByCategory(category_filter);
  } else {
    todos = manager.GetAllTodos();
  }

  if (todos.empty()) {
    std::cout << "No TODOs found." << std::endl;
    return absl::OkStatus();
  }

  std::cout << "TODOs (" << todos.size() << "):" << std::endl;
  for (const auto& item : todos) {
    PrintTodo(item);
  }

  return absl::OkStatus();
}

absl::Status HandleTodoUpdate(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    return absl::InvalidArgumentError(
        "Usage: agent todo update <id> --status=<status>");
  }

  std::string id = args[0];
  std::string new_status_str;

  for (size_t i = 1; i < args.size(); ++i) {
    if (args[i].find("--status=") == 0) {
      new_status_str = args[i].substr(9);
    }
  }

  if (new_status_str.empty()) {
    return absl::InvalidArgumentError("--status parameter is required");
  }

  auto new_status = TodoItem::StringToStatus(new_status_str);
  auto& manager = GetTodoManager();
  auto status = manager.UpdateStatus(id, new_status);

  if (!status.ok()) {
    return status;
  }

  std::cout << "Updated TODO " << id << " to status: " << new_status_str
            << std::endl;
  return absl::OkStatus();
}

absl::Status HandleTodoShow(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError("Usage: agent todo show <id>");
  }

  std::string id = args[0];
  auto& manager = GetTodoManager();
  auto result = manager.GetTodo(id);

  if (!result.ok()) {
    return result.status();
  }

  PrintTodo(*result, true);
  return absl::OkStatus();
}

absl::Status HandleTodoDelete(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError("Usage: agent todo delete <id>");
  }

  std::string id = args[0];
  auto& manager = GetTodoManager();
  auto status = manager.DeleteTodo(id);

  if (!status.ok()) {
    return status;
  }

  std::cout << "Deleted TODO " << id << std::endl;
  return absl::OkStatus();
}

absl::Status HandleTodoClearCompleted(const std::vector<std::string>& args) {
  auto& manager = GetTodoManager();
  auto status = manager.ClearCompleted();

  if (!status.ok()) {
    return status;
  }

  std::cout << "Cleared all completed TODOs" << std::endl;
  return absl::OkStatus();
}

absl::Status HandleTodoNext(const std::vector<std::string>& args) {
  auto& manager = GetTodoManager();
  auto result = manager.GetNextActionableTodo();

  if (!result.ok()) {
    return result.status();
  }

  std::cout << "Next actionable TODO:" << std::endl;
  PrintTodo(*result, true);

  return absl::OkStatus();
}

absl::Status HandleTodoPlan(const std::vector<std::string>& args) {
  auto& manager = GetTodoManager();
  auto result = manager.GenerateExecutionPlan();

  if (!result.ok()) {
    return result.status();
  }

  auto& plan = *result;
  if (plan.empty()) {
    std::cout << "No pending TODOs." << std::endl;
    return absl::OkStatus();
  }

  std::cout << "Execution Plan (" << plan.size() << " tasks):" << std::endl;
  for (size_t i = 0; i < plan.size(); ++i) {
    std::cout << absl::StreamFormat("%2d. ", i + 1);
    PrintTodo(plan[i]);
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status HandleTodoCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    std::cerr << "Usage: agent todo <command> [options]" << std::endl;
    std::cerr << "Commands:" << std::endl;
    std::cerr << "  create    - Create a new TODO" << std::endl;
    std::cerr << "  list      - List all TODOs" << std::endl;
    std::cerr << "  update    - Update TODO status" << std::endl;
    std::cerr << "  show      - Show TODO details" << std::endl;
    std::cerr << "  delete    - Delete a TODO" << std::endl;
    std::cerr << "  clear-completed - Clear completed TODOs" << std::endl;
    std::cerr << "  next      - Get next actionable TODO" << std::endl;
    std::cerr << "  plan      - Generate execution plan" << std::endl;
    return absl::InvalidArgumentError("No command specified");
  }

  std::string subcommand = args[0];
  std::vector<std::string> subargs(args.begin() + 1, args.end());

  if (subcommand == "create") {
    return HandleTodoCreate(subargs);
  } else if (subcommand == "list") {
    return HandleTodoList(subargs);
  } else if (subcommand == "update") {
    return HandleTodoUpdate(subargs);
  } else if (subcommand == "show") {
    return HandleTodoShow(subargs);
  } else if (subcommand == "delete") {
    return HandleTodoDelete(subargs);
  } else if (subcommand == "clear-completed") {
    return HandleTodoClearCompleted(subargs);
  } else if (subcommand == "next") {
    return HandleTodoNext(subargs);
  } else if (subcommand == "plan") {
    return HandleTodoPlan(subargs);
  } else {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown todo command: %s", subcommand));
  }
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
