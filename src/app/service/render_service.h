#ifndef YAZE_APP_SERVICE_RENDER_SERVICE_H_
#define YAZE_APP_SERVICE_RENDER_SERVICE_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace app {
namespace service {

// Overlay bitmask — callers OR these together in RenderRequest::overlay_flags.
// Adding new overlays costs nothing for existing callers.
namespace RenderOverlay {
constexpr uint32_t kNone = 0;
constexpr uint32_t kCollision = 1 << 0;    // custom collision tile IDs
constexpr uint32_t kSprites = 1 << 1;      // sprite positions
constexpr uint32_t kObjects = 1 << 2;      // tile object extents
constexpr uint32_t kTrack = 1 << 3;        // minecart track tiles
constexpr uint32_t kCameraQuads = 1 << 4;  // camera quadrant boundaries
constexpr uint32_t kGrid = 1 << 5;         // 8×8 tile grid
constexpr uint32_t kAll = ~0u;
}  // namespace RenderOverlay

struct RenderRequest {
  int room_id = 0;
  uint32_t overlay_flags = RenderOverlay::kNone;
  float scale = 1.0f;  // Output pixel scale (1.0 = 512×512 native)
};

struct RenderResult {
  std::vector<uint8_t> png_data;
  int width = 0;
  int height = 0;
};

// Metadata about a dungeon room — no rendering required.
struct RoomMetadata {
  int room_id = 0;
  uint8_t blockset = 0;
  uint8_t spriteset = 0;
  uint8_t palette = 0;
  int layout_id = 0;
  int effect = 0;
  int collision = 0;
  int tag1 = 0;
  int tag2 = 0;
  uint16_t message_id = 0;
  bool has_custom_collision = false;
  int object_count = 0;
  int sprite_count = 0;
};

// Renders dungeon rooms to PNG images headlessly (no ImGui or GPU required).
// rom and game_data are non-owning; caller must keep them alive.
// Thread-safe: each RenderDungeonRoom call is protected by an internal mutex.
class RenderService {
 public:
  RenderService(Rom* rom, zelda3::GameData* game_data);

  // Render room_id to PNG with the requested overlays at the given scale.
  absl::StatusOr<RenderResult> RenderDungeonRoom(const RenderRequest& req);

  // Return metadata for a room without rendering.
  absl::StatusOr<RoomMetadata> GetDungeonRoomMetadata(int room_id);

 private:
  // Convert the composite Bitmap's indexed+palette surface to an RGBA buffer.
  absl::StatusOr<std::vector<uint8_t>> BitmapToRgba(const gfx::Bitmap& bitmap,
                                                    int width, int height);

  // Paint requested overlays into an RGBA buffer (CPU rasterizer).
  void ApplyOverlays(std::vector<uint8_t>& rgba, int width, int height,
                     const zelda3::Room& room, uint32_t flags, float scale);

  // Encode an RGBA buffer to PNG bytes.
  absl::StatusOr<std::vector<uint8_t>> EncodePng(
      const std::vector<uint8_t>& rgba, int width, int height);

  Rom* rom_;                     // non-owning
  zelda3::GameData* game_data_;  // non-owning
  mutable std::mutex mu_;
};

}  // namespace service
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_SERVICE_RENDER_SERVICE_H_
