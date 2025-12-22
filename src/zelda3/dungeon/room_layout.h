#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "rom/rom.h"
#include "zelda3/dungeon/editor_dungeon_state.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

class RoomLayout {
 public:
  RoomLayout() = default;
  explicit RoomLayout(Rom* rom) : rom_(rom) {}

  void SetRom(Rom* rom) { rom_ = rom; }

  absl::Status LoadLayout(int layout_id);

  // Render the layout objects into the provided buffers
  absl::Status Draw(int room_id, const uint8_t* gfx_data,
                    gfx::BackgroundBuffer& bg1, gfx::BackgroundBuffer& bg2,
                    const gfx::PaletteGroup& palette_group,
                    DungeonState* state) const;

  const std::vector<RoomObject>& GetObjects() const { return objects_; }

 private:
  absl::StatusOr<int> GetLayoutAddress(int layout_id) const;

  Rom* rom_ = nullptr;
  std::vector<RoomObject> objects_;
};

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
