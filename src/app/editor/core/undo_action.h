#ifndef YAZE_APP_EDITOR_CORE_UNDO_ACTION_H_
#define YAZE_APP_EDITOR_CORE_UNDO_ACTION_H_

#include <cstddef>
#include <string>

#include "absl/status/status.h"

namespace yaze {
namespace editor {

/**
 * @class UndoAction
 * @brief Abstract base for all undoable actions (Command pattern)
 *
 * Each concrete action captures enough state to undo and redo itself.
 * Actions may optionally merge with previous actions of the same type
 * (e.g., sequential tile paints become a single undo step).
 */
class UndoAction {
 public:
  virtual ~UndoAction() = default;

  virtual absl::Status Undo() = 0;
  virtual absl::Status Redo() = 0;

  /// Human-readable description (e.g., "Paint 12 tiles on map 5")
  virtual std::string Description() const = 0;

  /// Approximate memory footprint for budget enforcement
  virtual size_t MemoryUsage() const { return 0; }

  /// Whether this action can merge with the previous action on the stack.
  /// Called only when both actions have the same concrete type.
  virtual bool CanMergeWith(const UndoAction& /*prev*/) const { return false; }

  /// Absorb the previous action into this one.
  /// Precondition: CanMergeWith(prev) returned true.
  virtual void MergeWith(UndoAction& /*prev*/) {}
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CORE_UNDO_ACTION_H_
