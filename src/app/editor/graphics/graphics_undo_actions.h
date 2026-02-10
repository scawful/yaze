#ifndef YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"

namespace yaze {
namespace editor {

/**
 * @class GraphicsPixelEditAction
 * @brief Undoable action for pixel edits on a graphics sheet.
 *
 * Captures a full snapshot of the sheet pixel data before and after the
 * edit stroke so that Undo restores the before-state and Redo restores
 * the after-state.
 */
class GraphicsPixelEditAction : public UndoAction {
 public:
  GraphicsPixelEditAction(uint16_t sheet_id,
                          std::vector<uint8_t> before_data,
                          std::vector<uint8_t> after_data,
                          std::string description)
      : sheet_id_(sheet_id),
        before_data_(std::move(before_data)),
        after_data_(std::move(after_data)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    auto* sheets = gfx::Arena::Get().mutable_gfx_sheets();
    if (sheet_id_ >= sheets->size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("Sheet %02X out of range", sheet_id_));
    }
    auto& sheet = sheets->at(sheet_id_);
    sheet.set_data(before_data_);
    gfx::Arena::Get().NotifySheetModified(sheet_id_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    auto* sheets = gfx::Arena::Get().mutable_gfx_sheets();
    if (sheet_id_ >= sheets->size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("Sheet %02X out of range", sheet_id_));
    }
    auto& sheet = sheets->at(sheet_id_);
    sheet.set_data(after_data_);
    gfx::Arena::Get().NotifySheetModified(sheet_id_);
    return absl::OkStatus();
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    return before_data_.size() + after_data_.size();
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    // Pixel edit strokes are already batched per mouse-down/mouse-up,
    // so merging is not needed.
    return false;
  }

 private:
  uint16_t sheet_id_;
  std::vector<uint8_t> before_data_;
  std::vector<uint8_t> after_data_;
  std::string description_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_UNDO_ACTIONS_H_
