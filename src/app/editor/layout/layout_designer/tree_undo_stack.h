#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_TREE_UNDO_STACK_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_TREE_UNDO_STACK_H_

#include <cstddef>
#include <vector>

#include "app/editor/layout/layout_designer/dock_tree.h"

namespace yaze {
namespace editor {
namespace layout_designer {

// Snapshot-based undo/redo for the Layout Designer. Stores full DockTree
// clones so any mutation is reversible without per-operation delta math.
// The stack is bounded (kDefaultMaxSteps = 64 entries) to keep memory
// predictable even for deeply-edited trees. Designer-local — no
// integration with the editor-wide undo system.
class TreeUndoStack {
 public:
  static constexpr std::size_t kDefaultMaxSteps = 64;

  explicit TreeUndoStack(std::size_t max_steps = kDefaultMaxSteps);

  // Snapshot the current state before a mutation. Clears redo history
  // (a new timeline invalidates the branch the user was previously
  // stepping back through).
  void Push(const DockTree& current);

  bool CanUndo() const { return !undo_.empty(); }
  bool CanRedo() const { return !redo_.empty(); }

  // Pop the most recent snapshot into `*current`, moving the replaced
  // `*current` onto the redo stack. No-op when `current` is null or the
  // undo stack is empty; returns true on successful mutation.
  bool Undo(DockTree* current);
  bool Redo(DockTree* current);

  // Discard the most recent Push() without touching `*current` or the
  // redo stack. Used by callers that speculatively snapshot before a
  // mutation that then refuses (validation failure, etc.) so the stack
  // stays clean without polluting redo history.
  void PopLastPush();

  void Clear();

  std::size_t UndoDepth() const { return undo_.size(); }
  std::size_t RedoDepth() const { return redo_.size(); }

 private:
  std::size_t max_steps_;
  std::vector<DockTree> undo_;
  std::vector<DockTree> redo_;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_TREE_UNDO_STACK_H_
