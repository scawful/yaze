#ifndef YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_

#include <cstddef>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/graphics/graphics_sheet_sync.h"

namespace yaze {
namespace editor {

class GraphicsEditorState;

/**
 * @class GraphicsPixelEditAction
 * @brief Undoable pixel edit on one graphics sheet.
 *
 * Holds only the pixels the edit changed. Undo and Redo write them through
 * the session's GraphicsSheetStore (graphics_sheet_sync.h), which refreshes
 * the Arena copy and marks the sheet modified.
 */
class GraphicsPixelEditAction : public UndoAction {
 public:
  GraphicsPixelEditAction(GraphicsEditorState* state, SheetPixelDiff diff,
                          std::string description)
      : state_(state),
        diff_(std::move(diff)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    return ApplySheetPixelDiff(*state_, diff_, /*redo=*/false);
  }
  absl::Status Redo() override {
    return ApplySheetPixelDiff(*state_, diff_, /*redo=*/true);
  }

  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override { return diff_.MemoryUsage(); }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    // One action per stroke already.
    return false;
  }

  const SheetPixelDiff& diff() const { return diff_; }

 private:
  GraphicsEditorState* state_;
  SheetPixelDiff diff_;
  std::string description_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_
