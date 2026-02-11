#ifndef YAZE_APP_EDITOR_MESSAGE_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_MESSAGE_UNDO_ACTIONS_H_

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/message/message_data.h"

namespace yaze {
namespace editor {

/// Snapshot of the message editor state at a point in time.
/// Defined here (rather than inside MessageEditor) so that
/// MessageEditAction can reference it without a circular dependency.
struct MessageSnapshot {
  MessageData message;
  std::string parsed_text;
  int message_index;
  bool is_expanded;
};

/**
 * @class MessageEditAction
 * @brief Undoable action for message text edits.
 *
 * Stores before/after MessageSnapshot values and a callback that knows
 * how to apply a snapshot back to the owning MessageEditor.  The editor
 * binds the callback at action-creation time via a lambda that calls
 * MessageEditor::ApplySnapshot().
 */
class MessageEditAction : public UndoAction {
 public:
  using ApplyFn = std::function<void(const MessageSnapshot&)>;

  MessageEditAction(MessageSnapshot before, MessageSnapshot after,
                    ApplyFn apply_fn)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_fn_(std::move(apply_fn)) {}

  absl::Status Undo() override {
    apply_fn_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    apply_fn_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit message %d", before_.message_index);
  }

  size_t MemoryUsage() const override {
    return before_.message.Data.size() + after_.message.Data.size() +
           before_.parsed_text.size() + after_.parsed_text.size();
  }

 private:
  MessageSnapshot before_;
  MessageSnapshot after_;
  ApplyFn apply_fn_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_UNDO_ACTIONS_H_
