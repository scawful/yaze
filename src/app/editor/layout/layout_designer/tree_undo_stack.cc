#include "app/editor/layout/layout_designer/tree_undo_stack.h"

#include <utility>

namespace yaze {
namespace editor {
namespace layout_designer {

TreeUndoStack::TreeUndoStack(std::size_t max_steps) : max_steps_(max_steps) {}

void TreeUndoStack::Push(const DockTree& current) {
  undo_.push_back(current.Clone());
  redo_.clear();
  if (max_steps_ > 0 && undo_.size() > max_steps_) {
    const std::size_t excess = undo_.size() - max_steps_;
    undo_.erase(undo_.begin(), undo_.begin() + excess);
  }
}

bool TreeUndoStack::Undo(DockTree* current) {
  if (current == nullptr || undo_.empty())
    return false;
  redo_.push_back(std::move(*current));
  *current = std::move(undo_.back());
  undo_.pop_back();
  return true;
}

bool TreeUndoStack::Redo(DockTree* current) {
  if (current == nullptr || redo_.empty())
    return false;
  undo_.push_back(std::move(*current));
  *current = std::move(redo_.back());
  redo_.pop_back();
  return true;
}

void TreeUndoStack::Clear() {
  undo_.clear();
  redo_.clear();
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
