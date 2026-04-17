#include "zelda3/dungeon/geometry/object_geometry.h"

#include <algorithm>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

namespace yaze {
namespace zelda3 {

namespace {

constexpr int kDummyTileCount = 512;

struct AnchorPos {
  int x = 0;
  int y = 0;
};

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

// Choose an anchor (x, y) that avoids buffer clipping for routines that draw
// leftward or upward from the object origin. Without enough headroom, the
// off-screen replay in MeasureRoutine loses rows/columns that the real draw
// routine would have emitted, producing undersized bounds.
AnchorPos ChooseAnchor(const DrawRoutineInfo& routine,
                       const RoomObject& object) {
  AnchorPos anchor;
  const int size_nibble = object.size_ & 0x0F;

  // Acute diagonals (ids 5, 17) move upward by (y - s) per step.
  if (routine.category == DrawRoutineInfo::Category::Diagonal &&
      (routine.id == 5 || routine.id == 17)) {
    const int count = (routine.id == 5) ? (size_nibble + 7) : (size_nibble + 6);
    const int max_anchor =
        DrawContext::kMaxTilesY - 5;  // 4 rows headroom below
    anchor.y = std::clamp(count - 1, 0, std::max(0, max_anchor));
    return anchor;
  }

  // Diagonal ceilings (Corner category, ids 75-78) anchor at the visual corner.
  // The draw routine offsets base by -(side-1) for the mirrored axis, so
  // bottom-anchored (76, 78) writes into negative Y and right-anchored (77, 78)
  // writes into negative X without headroom. See corner_routines.cc.
  if (routine.category == DrawRoutineInfo::Category::Corner &&
      routine.id >= 75 && routine.id <= 78) {
    const int side = size_nibble + 4;
    const bool mirror_x = (routine.id == 77 || routine.id == 78);
    const bool mirror_y = (routine.id == 76 || routine.id == 78);
    if (mirror_x) {
      anchor.x = std::clamp(side - 1, 0, DrawContext::kMaxTilesX - 1);
    }
    if (mirror_y) {
      anchor.y = std::clamp(side - 1, 0, DrawContext::kMaxTilesY - 1);
    }
    return anchor;
  }

  // Default: top-left of canvas.
  return anchor;
}

}  // namespace

ObjectGeometry& ObjectGeometry::Get() {
  static ObjectGeometry instance;
  return instance;
}

ObjectGeometry::ObjectGeometry() {
  BuildRegistry();
}

void ObjectGeometry::BuildRegistry() {
  routines_.clear();
  routine_map_.clear();

  // Use the unified DrawRoutineRegistry to ensure consistent routine IDs
  // between ObjectGeometry and ObjectDrawer
  const auto& registry = DrawRoutineRegistry::Get();
  routines_ = registry.GetAllRoutines();

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
  auto bounds = MeasureRoutine(*info, object);
  if (!bounds.ok()) {
    return bounds;
  }
  return ApplySelectionBounds(*bounds, routine_id);
}

absl::StatusOr<GeometryBounds> ObjectGeometry::MeasureRoutine(
    const DrawRoutineInfo& routine, const RoomObject& object) const {
  // Anchor object so routines that move upward or leftward stay within bounds.
  RoomObject adjusted = object;
  const AnchorPos anchor = ChooseAnchor(routine, object);
  adjusted.x_ = anchor.x;
  adjusted.y_ = anchor.y;

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
      if (bg.GetTileAt(x, y) == 0)
        continue;
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
  bounds.min_x_tiles = min_x - anchor.x;
  bounds.min_y_tiles = min_y - anchor.y;
  bounds.width_tiles = (max_x - min_x) + 1;
  bounds.height_tiles = (max_y - min_y) + 1;
  bounds.is_bg2_overlay = false;  // Default, set by MeasureForLayerCompositing
  return bounds;
}

absl::StatusOr<GeometryBounds> ObjectGeometry::MeasureByObjectId(
    const RoomObject& object) const {
  int routine_id = DrawRoutineRegistry::Get().GetRoutineIdForObject(object.id_);
  if (routine_id < 0) {
    return absl::NotFoundError(
        absl::StrFormat("No routine mapping for object 0x%03X", object.id_));
  }

  // Check cache
  CacheKey key{routine_id, object.id_, object.size_};
  auto cache_it = cache_.find(key);
  if (cache_it != cache_.end()) {
    return cache_it->second;
  }

  // Measure and cache
  auto result = MeasureByRoutineId(routine_id, object);
  if (result.ok()) {
    cache_[key] = *result;
  }
  return result;
}

void ObjectGeometry::ClearCache() {
  cache_.clear();
}

absl::StatusOr<GeometryBounds> ObjectGeometry::MeasureForLayerCompositing(
    int routine_id, const RoomObject& object) const {
  auto result = MeasureByRoutineId(routine_id, object);
  if (!result.ok()) {
    return result;
  }

  GeometryBounds bounds = *result;

  // Mark as BG2 overlay if the object's layer indicates Layer 1 (BG2)
  // Layer 1 objects write to the lower tilemap (BG2) and need BG1 transparency
  bounds.is_bg2_overlay = (object.layer_ == RoomObject::LayerType::BG2);

  return bounds;
}

bool ObjectGeometry::IsLayerOneRoutine(int routine_id) {
  // Layer 1 routines are those that explicitly draw to BG2 only.
  // Most objects draw to the current layer pointer; this list is for
  // routines that have special BG2-only behavior.
  //
  // From ASM analysis:
  // - Objects decoded with $BF == $4000 (lower_layer) are Layer 1/BG2
  // - The routine itself doesn't determine the layer; the object's position
  //   in the room data determines which layer pointer it uses
  //
  // This method is primarily for documentation; actual layer determination
  // comes from the object's layer_ field set during room loading.
  (void)
      routine_id;  // Currently unused - layer determined by object, not routine
  return false;
}

bool ObjectGeometry::IsDiagonalCeilingRoutine(int routine_id) {
  // Diagonal ceiling routines from draw_routine_registry.h
  // kDiagonalCeilingTopLeft = 75
  // kDiagonalCeilingBottomLeft = 76
  // kDiagonalCeilingTopRight = 77
  // kDiagonalCeilingBottomRight = 78
  return routine_id >= 75 && routine_id <= 78;
}

GeometryBounds ObjectGeometry::ApplySelectionBounds(
    GeometryBounds render_bounds, int routine_id) {
  if (!IsDiagonalCeilingRoutine(routine_id)) {
    // Not a diagonal ceiling - return render bounds unchanged
    return render_bounds;
  }

  // For diagonal ceilings, compute a tighter selection box.
  // The visual triangle fills roughly 50% of the bounding box area.
  // We use a selection rectangle that's 70% of the size, centered,
  // to provide a reasonable hit target without excessive false positives.

  int reduced_width = std::max(1, (render_bounds.width_tiles * 7) / 10);
  int reduced_height = std::max(1, (render_bounds.height_tiles * 7) / 10);

  // Center the reduced selection box within the render bounds
  int offset_x = (render_bounds.width_tiles - reduced_width) / 2;
  int offset_y = (render_bounds.height_tiles - reduced_height) / 2;

  SelectionRect selection;
  selection.x_tiles = render_bounds.min_x_tiles + offset_x;
  selection.y_tiles = render_bounds.min_y_tiles + offset_y;
  selection.width_tiles = reduced_width;
  selection.height_tiles = reduced_height;

  render_bounds.selection_bounds = selection;
  return render_bounds;
}

}  // namespace zelda3
}  // namespace yaze
