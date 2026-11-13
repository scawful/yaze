#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_SWAP_DIALOG_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_SWAP_DIALOG_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"

namespace yaze::editor {

// Exchanges the selected contents of two rooms. Nothing changes until a
// reviewed preview is applied; the editor owns preflight and history.
class DungeonRoomSwapDialog {
 public:
  using PreviewCallback = std::function<absl::StatusOr<DungeonRoomSwapPlan>(
      int, int, const DungeonRoomTransferOptions&)>;
  using ApplyCallback = std::function<absl::Status(const DungeonRoomSwapPlan&)>;

  void SetCallbacks(PreviewCallback preview, ApplyCallback apply) {
    preview_callback_ = std::move(preview);
    apply_callback_ = std::move(apply);
  }
  void SetReadOnly(bool read_only) { read_only_ = read_only; }

  // The modal opens on the next Draw(), in the caller's ImGui ID scope.
  void Open(int first_room_id, int second_room_id);
  void Draw();
  // Drops any reviewed plan, e.g. when the ROM session changes.
  void Reset();

  int first_room_id() const { return first_room_id_; }
  int second_room_id() const { return second_room_id_; }
  uint16_t domains() const { return domains_; }
  const DungeonRoomSwapPlan* preview() const { return preview_.get(); }
  const std::string& error() const { return error_; }
  const std::string& status() const { return status_; }
  bool is_open() const { return open_; }

 private:
  void InvalidatePreview();
  void RunPreview();
  bool HasValidRooms() const;

  PreviewCallback preview_callback_;
  ApplyCallback apply_callback_;
  bool read_only_ = false;
  int first_room_id_ = -1;
  int second_room_id_ = -1;
  uint16_t domains_ = kTransferCore;
  bool copy_destinations_ = false;
  std::shared_ptr<const DungeonRoomSwapPlan> preview_;
  std::string error_;
  std::string status_;
  bool can_retry_without_properties_ = false;
  bool request_open_ = false;
  bool open_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_DUNGEON_ROOM_SWAP_DIALOG_H_
