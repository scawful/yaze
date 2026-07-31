#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_RENDER_ROUTING_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_RENDER_ROUTING_H_

namespace yaze {
namespace zelda3 {

// Describes how a built-in object's draw routine chooses its destination
// tilemap(s). This is intentionally separate from RoomObject::layer_, which
// remains the stored room-object stream index used for serialization and draw
// order. Custom-object overrides are resolved before built-in routine routing.
enum class ObjectRenderRouting {
  kStoredPlacement,
  kFixedBg1,
  kFixedBg2,
  kFullBothBg1Bg2,
  kMixedBg1Bg2,
};

namespace object_render_routing {

inline constexpr bool IsFullBothAutoStairsObject(int object_id) {
  return object_id == 0x130 || object_id == 0x131 || object_id == 0xF9B ||
         object_id == 0xF9C;
}

inline constexpr bool IsFixedBg1StraightInterroomObject(int object_id) {
  return object_id >= 0xF9E && object_id <= 0xFA1;
}

inline constexpr bool IsMixedStraightInterroomObject(int object_id) {
  return object_id >= 0xFA6 && object_id <= 0xFA9;
}

inline constexpr bool IsSouthMixedStraightInterroomObject(int object_id) {
  return object_id == 0xFA8 || object_id == 0xFA9;
}

}  // namespace object_render_routing
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_RENDER_ROUTING_H_
