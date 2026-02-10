#ifndef YAZE_APP_EDITOR_CORE_UNDO_MANAGER_H_
#define YAZE_APP_EDITOR_CORE_UNDO_MANAGER_H_

#include <cstddef>
#include <deque>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "app/editor/core/undo_action.h"

namespace yaze {
namespace editor {

/**
 * @class UndoManager
 * @brief Manages undo/redo stacks for a single editor
 *
 * Each editor gets its own UndoManager (via EditorDependencies).
 * Supports bounded stacks, action merging, and description queries
 * for status bar / toast feedback.
 */
class UndoManager {
 public:
  UndoManager() = default;
  ~UndoManager() = default;

  // Non-copyable, movable
  UndoManager(const UndoManager&) = delete;
  UndoManager& operator=(const UndoManager&) = delete;
  UndoManager(UndoManager&&) = default;
  UndoManager& operator=(UndoManager&&) = default;

  /// Push a new action. Clears the redo stack.
  /// If the action can merge with the top of the undo stack, merges instead.
  void Push(std::unique_ptr<UndoAction> action);

  /// Undo the top action. Returns error if stack is empty.
  absl::Status Undo();

  /// Redo the top action. Returns error if stack is empty.
  absl::Status Redo();

  bool CanUndo() const { return !undo_stack_.empty(); }
  bool CanRedo() const { return !redo_stack_.empty(); }

  /// Description of the action that would be undone (for UI)
  std::string GetUndoDescription() const;

  /// Description of the action that would be redone (for UI)
  std::string GetRedoDescription() const;

  void SetMaxStackSize(size_t max) { max_stack_size_ = max; }
  size_t GetMaxStackSize() const { return max_stack_size_; }

  size_t UndoStackSize() const { return undo_stack_.size(); }
  size_t RedoStackSize() const { return redo_stack_.size(); }

  void Clear();

 private:
  void EnforceStackLimit();

  std::deque<std::unique_ptr<UndoAction>> undo_stack_;
  std::deque<std::unique_ptr<UndoAction>> redo_stack_;
  size_t max_stack_size_ = 50;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CORE_UNDO_MANAGER_H_
