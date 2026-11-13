#ifndef YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_NODE_TOOL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_NODE_TOOL_H_

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::zelda3 {
class Room;
}

namespace yaze::editor {

enum class MinecartNodeKind { kCartStart, kSwitch, kSwitchTrack };

struct MinecartNodeBrush {
  MinecartNodeKind kind = MinecartNodeKind::kCartStart;
  int id = 0;                // Cart identity or shared switch channel, 0..31.
  uint8_t collision = 0xB7;  // B7..BA for carts; D0..D3 for switch tracks.
  int source_x = -1;         // Optional existing AF/B0 anchor, in 8px cells.
  int source_y = -1;
  uint8_t source_key_drop = 0;
  std::optional<zelda3::CustomCollisionMap> expected_collision;
};

inline constexpr const char* kMinecartNodeDragPayload = "YAZE_MINECART_NODE";

struct MinecartToolPayload {
  MinecartNodeBrush node;
  bool collision_only = false;
  uint8_t collision = 0xB7;
  int size = 1;
  const void* owner = nullptr;
  const void* source_viewer = nullptr;  // Existing actor drags only.
  uint64_t revision = 0;
  int room_id = -1;
};

struct MinecartNodePlacement {
  std::vector<zelda3::Sprite> sprites;
  zelda3::CustomCollisionMap collision;
  std::optional<MinecartTrack> track_before;
  std::optional<MinecartTrack> track_after;
};

// Pure local candidate: tile coordinates use the 8px grid and must be even.
// Runtime samples the sprite's exact upper-layer anchor. Only that one cell
// receives a stop/switch override; this never guesses a 2x2 rail stencil.
// Global cart-identity checks and encoded-stream preflight belong to the
// editor.
absl::StatusOr<MinecartNodePlacement> PreviewMinecartNodePlacement(
    const zelda3::Room& room, int tile_x, int tile_y,
    const MinecartNodeBrush& brush, std::span<const MinecartTrack> tracks = {},
    int layer = 0);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_MINECART_MINECART_NODE_TOOL_H_
