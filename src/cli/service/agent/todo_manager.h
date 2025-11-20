#ifndef YAZE_CLI_SERVICE_AGENT_TODO_MANAGER_H_
#define YAZE_CLI_SERVICE_AGENT_TODO_MANAGER_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace cli {
namespace agent {

/**
 * @struct TodoItem
 * @brief Represents a single TODO item for task management
 */
struct TodoItem {
  std::string id;
  std::string description;
  enum class Status {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    BLOCKED,
    CANCELLED
  } status = Status::PENDING;

  std::string category;  // e.g., "rom_edit", "ai_task", "build", "test"
  int priority = 0;      // Higher = more important
  std::vector<std::string>
      dependencies;  // IDs of tasks that must complete first
  std::vector<std::string> tools_needed;  // Tools/functions required
  std::string created_at;
  std::string updated_at;
  std::string notes;

  // Convert status enum to string
  std::string StatusToString() const;
  static Status StringToStatus(const std::string& str);
};

/**
 * @class TodoManager
 * @brief Manages TODO lists for z3ed agent task execution
 * 
 * Enables the AI agent to:
 * - Create and track TODO lists for complex tasks
 * - Break down goals into executable steps
 * - Track dependencies between tasks
 * - Persist state between sessions
 * - Generate execution plans
 */
class TodoManager {
 public:
  TodoManager();
  explicit TodoManager(const std::string& data_dir);

  /**
   * @brief Initialize the TODO manager and load persisted data
   */
  absl::Status Initialize();

  /**
   * @brief Create a new TODO item
   */
  absl::StatusOr<TodoItem> CreateTodo(const std::string& description,
                                      const std::string& category = "",
                                      int priority = 0);

  /**
   * @brief Update an existing TODO item
   */
  absl::Status UpdateTodo(const std::string& id, const TodoItem& item);

  /**
   * @brief Update TODO status
   */
  absl::Status UpdateStatus(const std::string& id, TodoItem::Status status);

  /**
   * @brief Get a TODO item by ID
   */
  absl::StatusOr<TodoItem> GetTodo(const std::string& id) const;

  /**
   * @brief Get all TODO items
   */
  std::vector<TodoItem> GetAllTodos() const;

  /**
   * @brief Get TODO items by status
   */
  std::vector<TodoItem> GetTodosByStatus(TodoItem::Status status) const;

  /**
   * @brief Get TODO items by category
   */
  std::vector<TodoItem> GetTodosByCategory(const std::string& category) const;

  /**
   * @brief Get the next actionable TODO (respecting dependencies and priority)
   */
  absl::StatusOr<TodoItem> GetNextActionableTodo() const;

  /**
   * @brief Delete a TODO item
   */
  absl::Status DeleteTodo(const std::string& id);

  /**
   * @brief Clear all completed TODOs
   */
  absl::Status ClearCompleted();

  /**
   * @brief Save TODOs to persistent storage
   */
  absl::Status Save();

  /**
   * @brief Load TODOs from persistent storage
   */
  absl::Status Load();

  /**
   * @brief Generate an execution plan based on dependencies
   */
  absl::StatusOr<std::vector<TodoItem>> GenerateExecutionPlan() const;

  /**
   * @brief Export TODOs as JSON string
   */
  std::string ExportAsJson() const;

  /**
   * @brief Import TODOs from JSON string
   */
  absl::Status ImportFromJson(const std::string& json);

 private:
  std::string data_dir_;
  std::string todos_file_;
  std::vector<TodoItem> todos_;
  int next_id_ = 1;

  std::string GenerateId();
  std::string GetTimestamp() const;
  bool CanExecute(const TodoItem& item) const;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_TODO_MANAGER_H_
