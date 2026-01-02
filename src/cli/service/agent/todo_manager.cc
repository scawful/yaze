#include "cli/service/agent/todo_manager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/platform_paths.h"

#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_storage.h"
#endif

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
using json = nlohmann::json;
#endif

namespace yaze {
namespace cli {
namespace agent {

namespace {

std::string CurrentTimestamp() {
  auto now = absl::Now();
  return absl::FormatTime("%Y-%m-%d %H:%M:%S", now, absl::LocalTimeZone());
}

}  // namespace

std::string TodoItem::StatusToString() const {
  switch (status) {
    case Status::PENDING:
      return "pending";
    case Status::IN_PROGRESS:
      return "in_progress";
    case Status::COMPLETED:
      return "completed";
    case Status::BLOCKED:
      return "blocked";
    case Status::CANCELLED:
      return "cancelled";
    default:
      return "unknown";
  }
}

TodoItem::Status TodoItem::StringToStatus(const std::string& str) {
  if (str == "pending")
    return Status::PENDING;
  if (str == "in_progress")
    return Status::IN_PROGRESS;
  if (str == "completed")
    return Status::COMPLETED;
  if (str == "blocked")
    return Status::BLOCKED;
  if (str == "cancelled")
    return Status::CANCELLED;
  return Status::PENDING;
}

TodoManager::TodoManager() {
  auto result = util::PlatformPaths::GetAppDataSubdirectory("agent");
  if (result.ok()) {
    data_dir_ = result->string();
  } else {
    data_dir_ = (std::filesystem::current_path() / ".yaze" / "agent").string();
  }
  todos_file_ = (std::filesystem::path(data_dir_) / "todos.json").string();
}

TodoManager::TodoManager(const std::string& data_dir)
    : data_dir_(data_dir),
      todos_file_((std::filesystem::path(data_dir) / "todos.json").string()) {}

absl::Status TodoManager::Initialize() {
#ifdef __EMSCRIPTEN__
  // For WASM, we try to load from IndexedDB immediately
  return Load();
#else
  auto status = util::PlatformPaths::EnsureDirectoryExists(data_dir_);
  if (!status.ok()) {
    return status;
  }

  // Try to load existing TODOs
  if (util::PlatformPaths::Exists(todos_file_)) {
    return Load();
  }

  return absl::OkStatus();
#endif
}

std::string TodoManager::GenerateId() {
  return absl::StrFormat("todo_%d", next_id_++);
}

std::string TodoManager::GetTimestamp() const {
  return CurrentTimestamp();
}

absl::StatusOr<TodoItem> TodoManager::CreateTodo(const std::string& description,
                                                 const std::string& category,
                                                 int priority) {
  TodoItem item;
  item.id = GenerateId();
  item.description = description;
  item.category = category;
  item.priority = priority;
  item.status = TodoItem::Status::PENDING;
  item.created_at = GetTimestamp();
  item.updated_at = item.created_at;

  todos_.push_back(item);

  auto status = Save();
  if (!status.ok()) {
    todos_.pop_back();  // Rollback
    return status;
  }

  return item;
}

absl::Status TodoManager::UpdateTodo(const std::string& id,
                                     const TodoItem& item) {
  auto it = std::find_if(todos_.begin(), todos_.end(),
                         [&id](const TodoItem& t) { return t.id == id; });

  if (it == todos_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("TODO with ID %s not found", id));
  }

  TodoItem updated = item;
  updated.id = id;  // Preserve ID
  updated.updated_at = GetTimestamp();

  *it = updated;
  return Save();
}

absl::Status TodoManager::UpdateStatus(const std::string& id,
                                       TodoItem::Status status) {
  auto it = std::find_if(todos_.begin(), todos_.end(),
                         [&id](const TodoItem& t) { return t.id == id; });

  if (it == todos_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("TODO with ID %s not found", id));
  }

  it->status = status;
  it->updated_at = GetTimestamp();

  return Save();
}

absl::StatusOr<TodoItem> TodoManager::GetTodo(const std::string& id) const {
  auto it = std::find_if(todos_.begin(), todos_.end(),
                         [&id](const TodoItem& t) { return t.id == id; });

  if (it == todos_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("TODO with ID %s not found", id));
  }

  return *it;
}

std::vector<TodoItem> TodoManager::GetAllTodos() const {
  return todos_;
}

std::vector<TodoItem> TodoManager::GetTodosByStatus(
    TodoItem::Status status) const {
  std::vector<TodoItem> result;
  std::copy_if(todos_.begin(), todos_.end(), std::back_inserter(result),
               [status](const TodoItem& t) { return t.status == status; });
  return result;
}

std::vector<TodoItem> TodoManager::GetTodosByCategory(
    const std::string& category) const {
  std::vector<TodoItem> result;
  std::copy_if(
      todos_.begin(), todos_.end(), std::back_inserter(result),
      [&category](const TodoItem& t) { return t.category == category; });
  return result;
}

bool TodoManager::CanExecute(const TodoItem& item) const {
  // Check if all dependencies are completed
  for (const auto& dep_id : item.dependencies) {
    auto dep_result = GetTodo(dep_id);
    if (!dep_result.ok()) {
      return false;  // Dependency not found
    }
    if (dep_result->status != TodoItem::Status::COMPLETED) {
      return false;  // Dependency not completed
    }
  }
  return true;
}

absl::StatusOr<TodoItem> TodoManager::GetNextActionableTodo() const {
  // Find pending/blocked TODOs
  std::vector<TodoItem> actionable;
  for (const auto& item : todos_) {
    if ((item.status == TodoItem::Status::PENDING ||
         item.status == TodoItem::Status::BLOCKED) &&
        CanExecute(item)) {
      actionable.push_back(item);
    }
  }

  if (actionable.empty()) {
    return absl::NotFoundError("No actionable TODOs found");
  }

  // Sort by priority (descending)
  std::sort(actionable.begin(), actionable.end(),
            [](const TodoItem& a, const TodoItem& b) {
              return a.priority > b.priority;
            });

  return actionable[0];
}

absl::Status TodoManager::DeleteTodo(const std::string& id) {
  auto it = std::find_if(todos_.begin(), todos_.end(),
                         [&id](const TodoItem& t) { return t.id == id; });

  if (it == todos_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("TODO with ID %s not found", id));
  }

  todos_.erase(it);
  return Save();
}

absl::Status TodoManager::ClearCompleted() {
  auto it = std::remove_if(todos_.begin(), todos_.end(), [](const TodoItem& t) {
    return t.status == TodoItem::Status::COMPLETED;
  });
  todos_.erase(it, todos_.end());
  return Save();
}

absl::StatusOr<std::vector<TodoItem>> TodoManager::GenerateExecutionPlan()
    const {
  std::vector<TodoItem> plan;
  std::vector<TodoItem> pending;

  // Get all pending TODOs
  std::copy_if(todos_.begin(), todos_.end(), std::back_inserter(pending),
               [](const TodoItem& t) {
                 return t.status == TodoItem::Status::PENDING ||
                        t.status == TodoItem::Status::BLOCKED;
               });

  // Topological sort based on dependencies
  std::vector<TodoItem> sorted;
  std::set<std::string> completed_ids;

  while (!pending.empty()) {
    bool made_progress = false;

    for (auto it = pending.begin(); it != pending.end();) {
      bool can_add = true;
      for (const auto& dep_id : it->dependencies) {
        if (completed_ids.find(dep_id) == completed_ids.end()) {
          can_add = false;
          break;
        }
      }

      if (can_add) {
        sorted.push_back(*it);
        completed_ids.insert(it->id);
        it = pending.erase(it);
        made_progress = true;
      } else {
        ++it;
      }
    }

    if (!made_progress && !pending.empty()) {
      return absl::FailedPreconditionError(
          "Circular dependency detected in TODOs");
    }
  }

  // Sort by priority within dependency levels
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const TodoItem& a, const TodoItem& b) {
                     return a.priority > b.priority;
                   });

  return sorted;
}

absl::Status TodoManager::Save() {
#ifdef YAZE_WITH_JSON
  json j_todos = json::array();

  for (const auto& item : todos_) {
    json j_item;
    j_item["id"] = item.id;
    j_item["description"] = item.description;
    j_item["status"] = item.StatusToString();
    j_item["category"] = item.category;
    j_item["priority"] = item.priority;
    j_item["dependencies"] = item.dependencies;
    j_item["tools_needed"] = item.tools_needed;
    j_item["created_at"] = item.created_at;
    j_item["updated_at"] = item.updated_at;
    j_item["notes"] = item.notes;

    j_todos.push_back(j_item);
  }

#ifdef __EMSCRIPTEN__
  // For WASM, save to IndexedDB via WasmStorage
  // We use "agent_todos" as the project key
  using yaze::platform::WasmStorage;
  return WasmStorage::SaveProject("agent_todos", j_todos.dump());
#else
  std::ofstream file(todos_file_);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file: %s", todos_file_));
  }

  file << j_todos.dump(2);
  return absl::OkStatus();
#endif

#else
  return absl::UnimplementedError("JSON support required for TODO persistence");
#endif
}

absl::Status TodoManager::Load() {
#ifdef YAZE_WITH_JSON
  json j_todos;

#ifdef __EMSCRIPTEN__
  // For WASM, load from IndexedDB via WasmStorage
  using yaze::platform::WasmStorage;
  auto result = WasmStorage::LoadProject("agent_todos");
  if (!result.ok()) {
    // If not found, it might be first run, which is fine
    if (absl::IsNotFound(result.status())) {
      todos_.clear();
      return absl::OkStatus();
    }
    return result.status();
  }

  try {
    j_todos = json::parse(*result);
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse persisted JSON: %s", e.what()));
  }
#else
  std::ifstream file(todos_file_);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file: %s", todos_file_));
  }

  try {
    file >> j_todos;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }
#endif

  todos_.clear();
  for (const auto& j_item : j_todos) {
    TodoItem item;
    item.id = j_item.value("id", "");
    item.description = j_item.value("description", "");
    item.status = TodoItem::StringToStatus(j_item.value("status", "pending"));
    item.category = j_item.value("category", "");
    item.priority = j_item.value("priority", 0);
    item.dependencies =
        j_item.value("dependencies", std::vector<std::string>{});
    item.tools_needed =
        j_item.value("tools_needed", std::vector<std::string>{});
    item.created_at = j_item.value("created_at", "");
    item.updated_at = j_item.value("updated_at", "");
    item.notes = j_item.value("notes", "");

    todos_.push_back(item);

    // Update next_id_
    if (item.id.find("todo_") == 0) {
      int id_num = std::stoi(item.id.substr(5));
      if (id_num >= next_id_) {
        next_id_ = id_num + 1;
      }
    }
  }

  return absl::OkStatus();
#else
  return absl::UnimplementedError("JSON support required for TODO persistence");
#endif
}

std::string TodoManager::ExportAsJson() const {
#ifdef YAZE_WITH_JSON
  json j_todos = json::array();

  for (const auto& item : todos_) {
    json j_item;
    j_item["id"] = item.id;
    j_item["description"] = item.description;
    j_item["status"] = item.StatusToString();
    j_item["category"] = item.category;
    j_item["priority"] = item.priority;
    j_item["dependencies"] = item.dependencies;
    j_item["tools_needed"] = item.tools_needed;
    j_item["created_at"] = item.created_at;
    j_item["updated_at"] = item.updated_at;
    j_item["notes"] = item.notes;

    j_todos.push_back(j_item);
  }

  return j_todos.dump(2);
#else
  return "{}";
#endif
}

absl::Status TodoManager::ImportFromJson(const std::string& json_str) {
#ifdef YAZE_WITH_JSON
  try {
    json j_todos = json::parse(json_str);

    todos_.clear();
    for (const auto& j_item : j_todos) {
      TodoItem item;
      item.id = j_item.value("id", "");
      item.description = j_item.value("description", "");
      item.status = TodoItem::StringToStatus(j_item.value("status", "pending"));
      item.category = j_item.value("category", "");
      item.priority = j_item.value("priority", 0);
      item.dependencies =
          j_item.value("dependencies", std::vector<std::string>{});
      item.tools_needed =
          j_item.value("tools_needed", std::vector<std::string>{});
      item.created_at = j_item.value("created_at", "");
      item.updated_at = j_item.value("updated_at", "");
      item.notes = j_item.value("notes", "");

      todos_.push_back(item);
    }

    return Save();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }
#else
  return absl::UnimplementedError("JSON support required for TODO import");
#endif
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
