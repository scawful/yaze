#ifndef YAZE_APP_SERVICE_RENDER_SERVICE_H_
#define YAZE_APP_SERVICE_RENDER_SERVICE_H_

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
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

// Overworld overlay bitmask for OverworldRenderRequest::overlay_flags.
namespace OverworldOverlay {
constexpr uint32_t kNone = 0;
constexpr uint32_t kSprites = 1 << 0;    // sprite list entries (id labels)
constexpr uint32_t kEntrances = 1 << 1;  // entrance tiles (E + entrance id)
constexpr uint32_t kExits = 1 << 2;      // exit player spawns (X + room id)
constexpr uint32_t kHoles = 1 << 3;      // hole entrances (H + entrance id)
constexpr uint32_t kItems = 1 << 4;      // hidden items (I + item id)
constexpr uint32_t kGrid = 1 << 5;       // 16px grid + screen boundaries
constexpr uint32_t kAll = ~0u;
}  // namespace OverworldOverlay

struct OverworldRenderRequest {
  int screen_id = 0;  // any screen in the area; children resolve to parent
  uint32_t overlay_flags = OverworldOverlay::kNone;
  int phase = -1;      // sprite game state 0..2; -1 draws every phase
  float scale = 1.0f;  // Finite [0.25, 8.0]; 1.0 = 512px per screen.
  // Composite the area's subscreen overlay (sky, fog, lava, canopy, ...) the
  // way the game layers it: background overlays show through backdrop pixels,
  // foreground overlays blend on top.
  bool area_overlay = true;
};

// One overlay marker drawn on an overworld render, in area-local pixels.
struct OverworldRenderMarker {
  std::string kind;     // sprite, entrance, exit, hole, item
  int id = 0;           // sprite id, entrance id, exit room id, item id
  int phase = -1;       // sprites only
  int list_index = -1;  // sprites: index within the phase list
  int x = 0;
  int y = 0;
};

struct OverworldRenderResult {
  std::vector<uint8_t> png_data;
  int width = 0;
  int height = 0;
  int requested_screen = 0;
  int parent_screen = 0;
  std::string area_size;       // small, large, wide, tall
  int subscreen_overlay = -1;  // overlay screen composited, -1 = none
  std::vector<int> screens;
  std::vector<OverworldRenderMarker> markers;
};

struct RenderRequest {
  int room_id = 0;
  uint32_t overlay_flags = RenderOverlay::kNone;
  float scale = 1.0f;  // Finite [0.25, 8.0]; 1.0 = 512×512 native.
  // Unsaved sheet pixels (8bpp, 4096 bytes per sheet) to render instead of
  // GameData::graphics_buffer. See Room::SetGraphicsSheetOverrides().
  std::map<uint16_t, std::vector<uint8_t>> sheet_overrides;
};

// Shared strict parser for CLI/API scale input. Rejects malformed, non-finite,
// and out-of-range values instead of silently clamping or using a default.
absl::StatusOr<float> ParseRenderScale(absl::string_view value);

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

  // Render the whole overworld area containing screen_id (ZSCustomOverworld
  // large/wide/tall aware) from current ROM data, with optional overlays.
  absl::StatusOr<OverworldRenderResult> RenderOverworldArea(
      const OverworldRenderRequest& req);

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
