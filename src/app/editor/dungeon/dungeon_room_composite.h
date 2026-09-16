#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_COMPOSITE_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_COMPOSITE_H_

#include <cstdint>
#include <memory>

namespace yaze::gfx {
class Bitmap;
}  // namespace yaze::gfx

namespace yaze::zelda3 {
class Room;
class RoomLayerManager;
}  // namespace yaze::zelda3

namespace yaze::editor {

// Stable, consumer-owned output for a room presentation. Bitmap addresses must
// not move because Arena texture commands retain raw Bitmap pointers.
class RoomCompositeOutput {
 public:
  RoomCompositeOutput();
  ~RoomCompositeOutput();

  RoomCompositeOutput(const RoomCompositeOutput&) = delete;
  RoomCompositeOutput& operator=(const RoomCompositeOutput&) = delete;
  RoomCompositeOutput(RoomCompositeOutput&&) = delete;
  RoomCompositeOutput& operator=(RoomCompositeOutput&&) = delete;

  gfx::Bitmap& Prepare(zelda3::Room& room,
                       const zelda3::RoomLayerManager& layer_manager);
  void Retire();

  gfx::Bitmap& bitmap() { return *bitmap_; }
  const gfx::Bitmap& bitmap() const { return *bitmap_; }

 private:
  std::unique_ptr<gfx::Bitmap> bitmap_;
  const zelda3::Room* rendered_room_ = nullptr;
  uint64_t rendered_room_revision_ = 0;
  uint64_t rendered_layer_signature_ = 0;
  bool valid_ = false;
};

// Build the header-derived presentation used by non-canvas previews into
// storage owned by that preview. Callers that display it must queue/process
// its texture before drawing.
gfx::Bitmap& PrepareCanonicalRoomComposite(zelda3::Room& room,
                                           RoomCompositeOutput& output);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_COMPOSITE_H_
