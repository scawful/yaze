#include "zelda3/dungeon/geometry/object_geometry.h"

#include <algorithm>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/draw_routines/corner_routines.h"
#include "zelda3/dungeon/draw_routines/diagonal_routines.h"
#include "zelda3/dungeon/draw_routines/downwards_routines.h"
#include "zelda3/dungeon/draw_routines/rightwards_routines.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"

namespace yaze {
namespace zelda3 {

namespace {

constexpr int kDummyTileCount = 512;
constexpr int kAnchorX = 0;

std::vector<gfx::TileInfo> MakeDummyTiles() {
  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(kDummyTileCount);
  for (int i = 0; i < kDummyTileCount; ++i) {
    // Non-zero tile IDs so writes are detectable in the buffer
    tiles.push_back(gfx::TileInfo(static_cast<uint16_t>(i + 1), 0,
                                  /*v=*/false, /*h=*/false, /*o=*/false));
  }
  return tiles;
}

// Choose an anchor Y that avoids clipping for routines that move upward.
int ChooseAnchorY(const DrawRoutineInfo& routine, const RoomObject& object) {
  // Acute diagonals move upward (y - s); give them headroom.
  if (routine.category == DrawRoutineInfo::Category::Diagonal &&
      (routine.id == 5 || routine.id == 17)) {
    int size = object.size_ & 0x0F;
    int count = (routine.id == 5) ? (size + 7) : (size + 6);
    // Need count-1 tiles of upward travel plus 4 extra rows for the column.
    int min_anchor = count - 1;
    int max_anchor = DrawContext::kMaxTilesY - 5;  // leave 4 rows below ceiling
    if (min_anchor > max_anchor) min_anchor = max_anchor;
    return std::clamp(min_anchor, 0, max_anchor);
  }

  // Default: start at top of canvas.
  return 0;
}

}  // namespace

ObjectGeometry& ObjectGeometry::Get() {
  static ObjectGeometry instance;
  return instance;
}

ObjectGeometry::ObjectGeometry() { BuildRegistry(); }

void ObjectGeometry::BuildRegistry() {
  routines_.clear();
  routine_map_.clear();

  // Populate routine metadata from the existing draw routine registry.
  draw_routines::RegisterRightwardsRoutines(routines_);
  draw_routines::RegisterDownwardsRoutines(routines_);
  draw_routines::RegisterDiagonalRoutines(routines_);
  draw_routines::RegisterCornerRoutines(routines_);
  draw_routines::RegisterSpecialRoutines(routines_);

  for (const auto& info : routines_) {
    routine_map_[info.id] = info;
  }
}

const DrawRoutineInfo* ObjectGeometry::LookupRoutine(int routine_id) const {
  auto it = routine_map_.find(routine_id);
  if (it == routine_map_.end()) {
    return nullptr;
  }
  return &it->second;
}

absl::StatusOr<GeometryBounds> ObjectGeometry::MeasureByRoutineId(
    int routine_id, const RoomObject& object) const {
  const DrawRoutineInfo* info = LookupRoutine(routine_id);
  if (info == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unknown routine id %d", routine_id));
  }
  return MeasureRoutine(*info, object);
}

absl::StatusOr<GeometryBounds> ObjectGeometry::MeasureRoutine(
    const DrawRoutineInfo& routine, const RoomObject& object) const {
  // Anchor object so routines that move upward stay within bounds.
  RoomObject adjusted = object;
  adjusted.x_ = kAnchorX;
  const int anchor_y = ChooseAnchorY(routine, object);
  adjusted.y_ = anchor_y;

  // Allocate a dummy tile list large enough for every routine.
  static const std::vector<gfx::TileInfo> kTiles = MakeDummyTiles();

  gfx::BackgroundBuffer bg(DrawContext::kMaxTilesX * 8,
                           DrawContext::kMaxTilesY * 8);

  DrawContext ctx{
      .target_bg = bg,
      .object = adjusted,
      .tiles = std::span<const gfx::TileInfo>(kTiles.data(), kTiles.size()),
      .state = nullptr,
      .rom = nullptr,
      .room_id = 0,
      .room_gfx_buffer = nullptr,
      .secondary_bg = nullptr,
  };

  // Execute the routine to mark tiles in the buffer.
  routine.function(ctx);

  // Scan buffer for written tiles.
  const int tiles_w = DrawContext::kMaxTilesX;
  const int tiles_h = DrawContext::kMaxTilesY;

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();

  for (int y = 0; y < tiles_h; ++y) {
    for (int x = 0; x < tiles_w; ++x) {
      if (bg.GetTileAt(x, y) == 0) continue;
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
    }
  }

  // Handle routines that intentionally draw nothing.
  if (max_x == std::numeric_limits<int>::min()) {
    return GeometryBounds{};
  }

  GeometryBounds bounds;
  bounds.min_x_tiles = min_x - kAnchorX;
  bounds.min_y_tiles = min_y - anchor_y;
  bounds.width_tiles = (max_x - min_x) + 1;
  bounds.height_tiles = (max_y - min_y) + 1;
  return bounds;
}

}  // namespace zelda3
}  // namespace yaze
