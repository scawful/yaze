#include "zelda3/dungeon/room_spatial.h"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "util/macro.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_layout.h"

namespace yaze::zelda3 {
namespace {

bool OutsideRoom(const RoomSpatialBounds& bounds) {
  return bounds.width > 0 && bounds.height > 0 &&
         (bounds.x < 0 || bounds.y < 0 || bounds.right() > 64 ||
          bounds.bottom() > 64);
}

uint16_t TraceWord(const ObjectDrawer::TileTrace& cell) {
  return static_cast<uint16_t>(
      (cell.tile_id & 0x03FF) | (((cell.flags >> 3) & 7) << 10) |
      ((cell.flags & 4) ? 0x2000 : 0) | ((cell.flags & 1) ? 0x4000 : 0) |
      ((cell.flags & 2) ? 0x8000 : 0));
}

RoomSpatialBounds TraceBounds(const std::vector<RoomSpatialCell>& cells) {
  if (cells.empty())
    return {};
  int left = cells.front().x, top = cells.front().y;
  int right = left + 1, bottom = top + 1;
  for (const auto& cell : cells) {
    left = std::min(left, cell.x);
    top = std::min(top, cell.y);
    right = std::max(right, cell.x + 1);
    bottom = std::max(bottom, cell.y + 1);
  }
  return {left, top, right - left, bottom - top};
}

void IncludeBounds(RoomSpatialBounds* bounds, const RoomSpatialBounds& other) {
  if (other.width <= 0 || other.height <= 0)
    return;
  if (bounds->width <= 0 || bounds->height <= 0) {
    *bounds = other;
    return;
  }
  const int right = std::max(bounds->right(), other.right());
  const int bottom = std::max(bounds->bottom(), other.bottom());
  bounds->x = std::min(bounds->x, other.x);
  bounds->y = std::min(bounds->y, other.y);
  bounds->width = right - bounds->x;
  bounds->height = bottom - bounds->y;
}

RoomObjectFootprint Unmeasured(const RoomObject& object, size_t index,
                               bool layout, const absl::Status& status) {
  RoomObjectFootprint result;
  result.object_index = index;
  result.object_id = object.id_;
  result.size = object.size_;
  result.list_index = object.GetLayerValue();
  result.anchor_x = object.x_;
  result.anchor_y = object.y_;
  result.is_layout = layout;
  result.measurement_source = "unmeasured";
  result.diagnostics.emplace_back(status.message());
  return result;
}

void FindOverlaps(RoomSpatialAnalysis* analysis) {
  using Owner = std::pair<bool, size_t>;  // layout, original vector index
  using Pair = std::pair<Owner, Owner>;
  std::map<std::tuple<uint8_t, int, int>, std::vector<Owner>> owners;
  std::map<Pair, std::pair<size_t, size_t>> counts;
  const auto collect = [&](const auto& objects) {
    for (const auto& object : objects) {
      const Owner owner{object.is_layout, object.object_index};
      for (const auto& cell : object.cells) {
        auto& prior = owners[{cell.layer, cell.y, cell.x}];
        for (const auto& previous : prior) {
          auto& count = counts[{previous, owner}];
          ++(cell.layer == 0 ? count.first : count.second);
        }
        prior.push_back(owner);
      }
    }
  };
  collect(analysis->layout);
  collect(analysis->objects);
  for (const auto& [pair, count] : counts) {
    analysis->overlaps.push_back(
        {pair.first.second, pair.second.second, pair.first.first,
         pair.second.first, count.first, count.second,
         pair.first.first != pair.second.first ? "layout_underlay"
                                               : "ordered_shared_write"});
  }
}

}  // namespace

absl::StatusOr<RoomObjectFootprint> MeasureRoomObjectFootprint(
    Rom* rom, int room_id, const RoomObject& source,
    const DungeonState* state) {
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("Spatial measurement requires a ROM");
  if (room_id < 0 || room_id >= kNumberOfRooms)
    return absl::OutOfRangeError("Spatial room ID must be 0x000-0x127");
  if (!UsesRoomObjectStream(source)) {
    return absl::UnimplementedError(
        "Special-table footprints require their dedicated room draw pass");
  }
  RETURN_IF_ERROR(ValidateRoomObjectStreamEntryForSave(source));

  RoomObjectFootprint result;
  result.object_id = source.id_;
  result.size = source.size_;
  result.list_index = source.GetLayerValue();
  result.anchor_x = source.x_;
  result.anchor_y = source.y_;
  RoomObject object = source;
  object.SetRom(rom);
  object.layer_ = MapRoomObjectListIndexToDrawLayer(result.list_index);
  result.layers = GetEffectiveObjectLayerSemantics(object);
  if (result.layers.routine_id < 0 && !result.layers.custom_override_active)
    return absl::UnimplementedError("No spatial draw routine for this object");

  std::vector<ObjectDrawer::TileTrace> trace;
  ObjectDrawer drawer(rom, room_id, nullptr);
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);
  gfx::BackgroundBuffer upper(512, 512), lower(512, 512);
  gfx::PaletteGroup palette;
  RETURN_IF_ERROR(drawer.DrawObject(object, upper, lower, palette, state));
  // Later writes by this object to the same cell supersede earlier writes.
  std::map<std::tuple<uint8_t, int, int>, uint16_t> cells;
  for (const auto& cell : trace) {
    const uint8_t layer = cell.layer == RoomObject::BG2 ? 1 : 0;
    cells[{layer, cell.y_tile, cell.x_tile}] = TraceWord(cell);
  }
  for (const auto& [key, word] : cells) {
    const auto [layer, y, x] = key;
    result.cells.push_back({x, y, layer, word});
  }
  result.bounds = TraceBounds(result.cells);
  result.measured = true;
  result.measurement_source = "object_drawer_trace";
  const auto geometry =
      ObjectGeometry::Get().MeasureByObjectIdForState(object, state);
  result.full_bounds_measured =
      geometry.ok() &&
      ((geometry->width_tiles > 0 && geometry->height_tiles > 0) ||
       result.cells.empty());
  if (geometry.ok() && geometry->width_tiles > 0 &&
      geometry->height_tiles > 0) {
    IncludeBounds(
        &result.bounds,
        {source.x_ + geometry->min_x_tiles, source.y_ + geometry->min_y_tiles,
         geometry->width_tiles, geometry->height_tiles});
    result.measurement_source = "object_drawer_trace_and_state_geometry";
  } else if (!geometry.ok()) {
    result.diagnostics.emplace_back("Unclipped geometry unavailable: " +
                                    std::string(geometry.status().message()));
  }
  result.outside_room = OutsideRoom(result.bounds);
  if (result.outside_room)
    result.diagnostics.emplace_back(
        "Full bounds extend outside the room; trace cells may be clipped");
  if (result.cells.empty()) {
    result.diagnostics.emplace_back(
        "No raster writes in this state; empty does not imply passability");
    if (!geometry.ok()) {
      result.measured = false;
      result.measurement_source = "unmeasured";
    }
  }
  if (result.layers.custom_override_active)
    result.diagnostics.emplace_back(
        "Custom artwork footprint does not declare custom runtime behavior");
  if (result.layers.custom_override_active &&
      DrawRoutineRegistry::Get().GetRoutineIdForObject(object.id_) !=
          DrawRoutineIds::kCustomObject) {
    result.full_bounds_measured = false;
    result.diagnostics.emplace_back(
        "Custom override bypasses the measured built-in geometry");
  }
  if (result.layers.render_routing == ObjectRenderRouting::kMixedBg1Bg2) {
    result.full_bounds_measured = false;
    result.diagnostics.emplace_back(
        "Standalone geometry does not establish unclipped extents on both "
        "planes for mixed routing");
  }
  result.diagnostics.emplace_back(
      "Isolated drawing has no prior room tilemap; context-sensitive edge "
      "masks do not establish complete composed ownership");
  if (state != nullptr) {
    // ObjectGeometry uses a standalone room-0 draw context; ObjectDrawer here
    // uses the requested room but starts event counters at zero. Keep those
    // partial observations useful without admitting strict placement bounds.
    result.full_bounds_measured = false;
    result.diagnostics.emplace_back(
        "Explicit-state isolated replay does not preserve room event ordinals; "
        "full-state bounds are not qualified");
  }
  return result;
}

absl::StatusOr<RoomSpatialAnalysis> AnalyzeRoomSpatial(
    Rom* rom, const Room& room, const RoomSpatialOptions& options) {
  if (!rom || !rom->is_loaded())
    return absl::FailedPreconditionError("Spatial analysis requires a ROM");
  if (room.id() < 0 || room.id() >= kNumberOfRooms)
    return absl::OutOfRangeError("Spatial room ID must be 0x000-0x127");
  if (options.tilemaps && (options.tilemaps->bg1.size() != kRoomTilemapWords ||
                           options.tilemaps->bg2.size() != kRoomTilemapWords))
    return absl::InvalidArgumentError(
        "Spatial tilemaps must be 64x64 per plane");

  RoomSpatialAnalysis result;
  result.room_id = room.id();
  result.layout_id = room.layout_id();
  result.floor1 = room.floor1();
  result.floor2 = room.floor2();
  result.layer2_mode = room.layer2_mode();
  result.collision_mode = static_cast<int>(room.collision());
  result.effect = static_cast<int>(room.effect());
  result.tag1 = static_cast<int>(room.tag1());
  result.tag2 = static_cast<int>(room.tag2());
  result.registers =
      room.GameLayerRegisters(options.load_state.room_save_flags);
  result.explicit_object_state = options.object_state != nullptr;
  result.load_state = options.load_state;

  const auto append = [&](const RoomObject& object, size_t index, bool layout) {
    auto footprint = MeasureRoomObjectFootprint(rom, room.id(), object,
                                                options.object_state);
    auto measured = footprint.ok()
                        ? std::move(*footprint)
                        : Unmeasured(object, index, layout, footprint.status());
    measured.object_index = index;
    measured.is_layout = layout;
    (layout ? result.layout : result.objects).push_back(std::move(measured));
  };
  RoomLayout decoded_layout(rom);
  const std::vector<RoomObject>* layout = &room.GetLayout().GetObjects();
  if (layout->empty()) {
    const auto loaded = decoded_layout.LoadLayout(room.layout_id());
    if (loaded.ok()) {
      layout = &decoded_layout.GetObjects();
    } else {
      result.diagnostics.emplace_back("Layout could not be decoded: " +
                                      std::string(loaded.message()));
    }
  }
  for (size_t index = 0; index < layout->size(); ++index)
    append((*layout)[index], index, true);
  const auto& objects = room.GetTileObjects();
  for (uint8_t list = 0; list < 3; ++list)
    for (size_t index = 0; index < objects.size(); ++index)
      if (UsesRoomObjectStream(objects[index]) &&
          objects[index].GetLayerValue() == list)
        append(objects[index], index, false);
  for (size_t index = 0; index < objects.size(); ++index)
    if (!UsesRoomObjectStream(objects[index]) ||
        objects[index].GetLayerValue() > 2)
      append(objects[index], index, false);

  const auto& doors = room.GetDoors();
  for (size_t index = 0; index < doors.size(); ++index) {
    const auto& door = doors[index];
    const auto [x, y] = DoorPositionManager::PositionToTileCoords(
        door.position, door.direction);
    const auto [px, py, width, height] =
        DoorPositionManager::GetDoorEditorBounds(door.position, door.direction,
                                                 door.type);
    result.doors.push_back(
        {index,
         door.position,
         static_cast<int>(door.direction),
         static_cast<int>(door.type),
         x,
         y,
         {px / 8, py / 8, width / 8, height / 8},
         DoorPositionManager::IsValidPosition(door.position, door.direction)});
  }
  FindOverlaps(&result);
  for (size_t index = 0; index < room.custom_collision().tiles.size(); ++index)
    result.custom_collision_overrides +=
        room.custom_collision().HasOverride(index);
  if (options.tilemaps) {
    auto collision_input = MakeRoomCollisionInput(room, *options.tilemaps);
    collision_input.objects.insert(collision_input.objects.begin(),
                                   layout->begin(), layout->end());
    result.collision =
        ComputeRoomCollisionMaps(*rom, collision_input, options.load_state);
  }
  result.diagnostics.emplace_back(
      "Raster overlaps describe ordered writes, not forbidden gameplay "
      "collisions");
  result.diagnostics.emplace_back(
      "Trace-only replay excludes priority-only mutations and pixel reveal "
      "masks");
  result.diagnostics.emplace_back(
      "Collision maps are vanilla room-load replay; custom overrides are "
      "counted separately");
  if (!options.object_state)
    result.diagnostics.emplace_back(
        "Object geometry uses default routine state, not live gameplay state");
  if (!options.tilemaps)
    result.diagnostics.emplace_back(
        "No caller-rendered tilemaps supplied; collision maps were not "
        "computed");
  return result;
}

}  // namespace yaze::zelda3
