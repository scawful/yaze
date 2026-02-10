#include "app/editor/core/undo_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"

namespace yaze {
namespace editor {

void UndoManager::Push(std::unique_ptr<UndoAction> action) {
  // Clear redo stack on new action
  redo_stack_.clear();

  // Try to merge with the top of the undo stack
  if (!undo_stack_.empty() && action->CanMergeWith(*undo_stack_.back())) {
    action->MergeWith(*undo_stack_.back());
    undo_stack_.pop_back();
  }

  undo_stack_.push_back(std::move(action));
  EnforceStackLimit();
}

absl::Status UndoManager::Undo() {
  if (undo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  auto& action = undo_stack_.back();
  auto status = action->Undo();
  if (!status.ok()) {
    return status;
  }

  redo_stack_.push_back(std::move(action));
  undo_stack_.pop_back();
  return absl::OkStatus();
}

absl::Status UndoManager::Redo() {
  if (redo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  auto& action = redo_stack_.back();
  auto status = action->Redo();
  if (!status.ok()) {
    return status;
  }

  undo_stack_.push_back(std::move(action));
  redo_stack_.pop_back();
  return absl::OkStatus();
}

std::string UndoManager::GetUndoDescription() const {
  if (undo_stack_.empty()) return "";
  return undo_stack_.back()->Description();
}

std::string UndoManager::GetRedoDescription() const {
  if (redo_stack_.empty()) return "";
  return redo_stack_.back()->Description();
}

void UndoManager::Clear() {
  undo_stack_.clear();
  redo_stack_.clear();
}

void UndoManager::EnforceStackLimit() {
  while (undo_stack_.size() > max_stack_size_) {
    undo_stack_.pop_front();
  }
}

}  // namespace editor
}  // namespace yaze
