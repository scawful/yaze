#include "app/editor/dungeon/ui/minecart/minecart_assembly.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include "util/macro.h"

namespace yaze::editor {
namespace {
bool Grid(int x, int y) {
  return x >= 0 && y >= 0 && x <= 62 && y <= 62 && !(x % 2) && !(y % 2);
}
bool Node(int id) {
  return id == 0xA3 || id == 0xAF || id == 0xB0;
}
bool SpecialCollision(uint8_t value) {
  return (value >= 0xB7 && value <= 0xBA) || (value >= 0xD0 && value <= 0xD3);
}
absl::Status Rail(const zelda3::RoomObject& object,
                  zelda3::MinecartRailProfile profile) {
  const auto* descriptor =
      zelda3::FindMinecartRailDescriptor(object.id_, object.size_, profile);
  if (!descriptor || !descriptor->collision_verified ||
      object.GetLayerValue() != 0 || !Grid(object.x_, object.y_))
    return absl::InvalidArgumentError(
        "Select admitted primary-stream rails on the 16px grid");
  return absl::OkStatus();
}
}  // namespace

absl::StatusOr<DungeonSelectionClipboard> BuildMinecartRailPath(
    std::span<const std::pair<int, int>> waypoints,
    zelda3::MinecartRailStyle style, zelda3::MinecartRailProfile profile) {
  if (waypoints.size() < 2)
    return absl::InvalidArgumentError(
        "Click at least two orthogonal waypoints");
  std::vector<std::pair<int, int>> cells;
  const bool closed =
      waypoints.size() > 3 && waypoints.front() == waypoints.back();
  std::set<std::pair<int, int>> visited;
  for (size_t i = 0; i < waypoints.size(); ++i) {
    const auto [x, y] = waypoints[i];
    if (!Grid(x, y))
      return absl::InvalidArgumentError(
          "Keep every waypoint inside the 16px room grid");
    if (!i) {
      cells.push_back({x, y});
      visited.insert({x, y});
      continue;
    }
    auto [px, py] = waypoints[i - 1];
    if ((px == x) == (py == y))
      return absl::InvalidArgumentError(
          "Waypoints must form nonzero orthogonal segments");
    const int dx = (x > px ? 2 : x < px ? -2 : 0);
    const int dy = (y > py ? 2 : y < py ? -2 : 0);
    while (px != x || py != y) {
      px += dx;
      py += dy;
      if (closed && i + 1 == waypoints.size() &&
          std::pair{px, py} == cells.front())
        continue;
      if (!visited.insert({px, py}).second)
        return absl::FailedPreconditionError(
            "Crossings and retraced rail cells are ambiguous");
      cells.push_back({px, py});
    }
  }
  DungeonSelectionClipboard result;
  for (size_t i = 0; i < cells.size(); ++i)
    for (size_t j = i + 2; j < cells.size(); ++j)
      if (!(closed && i == 0 && j + 1 == cells.size()) &&
          std::abs(cells[i].first - cells[j].first) +
                  std::abs(cells[i].second - cells[j].second) ==
              2)
        return absl::FailedPreconditionError(
            "Nonconsecutive path segments touch; choose an unambiguous route");
  result.minecart.emplace();
  result.minecart->profile = profile;
  for (size_t i = 0; i < cells.size(); ++i) {
    const auto [x, y] = cells[i];
    unsigned exits = 0;
    for (size_t neighbor :
         {i == 0 ? (closed ? cells.size() - 1 : 1) : i - 1,
          i + 1 == cells.size() ? (closed ? 0 : i - 1) : i + 1}) {
      const auto [nx, ny] = cells[neighbor];
      exits |= nx < x ? 1 : nx > x ? 2 : ny < y ? 4 : 8;
    }
    using Shape = zelda3::MinecartRailShape;
    const auto shape = (exits & 3) == exits    ? Shape::kHorizontal
                       : (exits & 12) == exits ? Shape::kVertical
                       : exits == 10           ? Shape::kCornerTL
                       : exits == 9            ? Shape::kCornerTR
                       : exits == 6            ? Shape::kCornerBL
                                               : Shape::kCornerBR;
    const auto catalog = zelda3::MinecartRailDescriptors(profile);
    const auto descriptor =
        std::find_if(catalog.begin(), catalog.end(), [&](const auto& d) {
          return d.style == style && d.shape == shape && d.collision_verified;
        });
    if (descriptor == catalog.end())
      return absl::FailedPreconditionError(
          "The admitted catalog has no rail for this turn");
    result.objects.emplace_back(descriptor->object_id, x, y,
                                descriptor->subtype, 0);
    result.object_chests.push_back(std::nullopt);
  }
  result.origin_pixel_x = cells.front().first * 8;
  result.origin_pixel_y = cells.front().second * 8;
  return result;
}

absl::StatusOr<DungeonSelectionClipboard> CaptureMinecartAssembly(
    const zelda3::Room& room, const std::vector<size_t>& objects,
    const std::vector<SelectedEntity>& entities,
    std::span<const MinecartTrack> tracks,
    zelda3::MinecartRailProfile profile) {
  ASSIGN_OR_RETURN(auto result, CopyDungeonSelection(room, objects, entities));
  if (result.empty() || !result.doors.empty() || !result.items.empty())
    return absl::InvalidArgumentError(
        "Select rails and their cart/switch sprites only");
  DungeonMinecartClipboardData data;
  data.source_room = room.id();
  data.profile = profile;
  std::set<size_t> offsets;
  for (const auto& object : result.objects) {
    RETURN_IF_ERROR(Rail(object, profile));
    const auto* d =
        zelda3::FindMinecartRailDescriptor(object.id_, object.size_, profile);
    for (int cell = 0; cell < 4; ++cell)
      if (d->collision_mask & (1 << cell))
        offsets.insert((object.y_ + cell / 2) * 64 + object.x_ + cell % 2);
  }
  std::set<int> carts, channels;
  for (const auto& sprite : result.sprites) {
    if (!Node(sprite.id) || sprite.deleted || sprite.layer != 0 ||
        !Grid(sprite.x * 2, sprite.y * 2) || sprite.subtype < 0 ||
        sprite.subtype > 31)
      return absl::InvalidArgumentError(
          "Assembly nodes must be live upper-layer minecart sprites");
    const size_t offset = sprite.y * 2 * 64 + sprite.x * 2;
    if (sprite.id == 0xA3) {
      if (!carts.insert(sprite.subtype).second ||
          tracks.size() != kMinecartTrackSlotCount)
        return absl::FailedPreconditionError(
            "Include one cart per loaded source-track identity");
      const auto& start = tracks[sprite.subtype];
      if (start.id != sprite.subtype || start.room_id != room.id() ||
          start.start_x != (room.id() & 15) * 512 + sprite.x * 16 ||
          start.start_y != (room.id() >> 4) * 512 + sprite.y * 16)
        return absl::FailedPreconditionError(
            "The selected cart does not match its source start");
      data.starts.push_back(start);
    } else {
      channels.insert(sprite.subtype);
    }
    if (sprite.id != 0xAF) {
      if (!room.custom_collision().HasOverride(offset) ||
          (sprite.id == 0xA3
               ? room.custom_collision().tiles[offset] < 0xB7 ||
                     room.custom_collision().tiles[offset] > 0xBA
               : room.custom_collision().tiles[offset] < 0xD0 ||
                     room.custom_collision().tiles[offset] > 0xD3))
        return absl::FailedPreconditionError(
            "Include the node's exact stop or rotating collision");
      offsets.insert(offset);
    }
  }
  for (const auto& actor : room.GetSprites()) {
    if (actor.deleted() || !Node(actor.id()))
      continue;
    const bool selected = std::any_of(
        result.sprites.begin(), result.sprites.end(), [&](const auto& s) {
          return s.id == actor.id() && s.x == actor.x() && s.y == actor.y() &&
                 s.layer == actor.layer() && s.subtype == actor.subtype();
        });
    if ((!selected && offsets.contains(actor.y() * 2 * 64 + actor.x() * 2)) ||
        (!selected && actor.id() != 0xA3 && channels.contains(actor.subtype())))
      return absl::FailedPreconditionError(
          "Selection omits a cart or switch-channel dependency; include its "
          "sprites");
  }
  for (int channel : channels) {
    const auto count = [&](int id) {
      return std::count_if(
          result.sprites.begin(), result.sprites.end(),
          [&](const auto& s) { return s.id == id && s.subtype == channel; });
    };
    if (count(0xAF) != 1 || count(0xB0) == 0)
      return absl::FailedPreconditionError(
          "Include one lever and all rotating tracks for each selected "
          "channel");
  }
  for (size_t offset : offsets) {
    if (!room.custom_collision().HasOverride(offset))
      return absl::FailedPreconditionError(
          "Selected rail assembly lacks required authored collision");
    data.collision[offset] = room.custom_collision().tiles[offset];
  }
  result.minecart = std::move(data);
  return result;
}

absl::StatusOr<MinecartAssemblyPlan> PlanMinecartAssembly(
    const zelda3::Room& room, const DungeonSelectionClipboard& clipboard,
    int anchor_x, int anchor_y, const MinecartAssemblyMapping& mapping,
    std::span<const MinecartTrack> tracks,
    const zelda3::MinecartTrackCollisionOwnership& ownership,
    zelda3::MinecartRailProfile profile) {
  if (!clipboard.minecart || clipboard.minecart->profile != profile ||
      !Grid(anchor_x, anchor_y) || clipboard.empty())
    return absl::InvalidArgumentError(
        "Assembly needs its original catalog and a 16px target anchor");
  auto plain = clipboard;
  plain.minecart.reset();
  if (!plain.doors.empty() || !plain.items.empty())
    return absl::InvalidArgumentError(
        "Only rail and node assemblies are supported");
  std::set<int> mapped_carts, mapped_channels;
  for (const auto& [old_id, new_id] : mapping.carts)
    if (old_id < 0 || old_id > 31 || new_id < 0 || new_id > 31 ||
        !mapped_carts.insert(new_id).second)
      return absl::InvalidArgumentError(
          "Give every cart a distinct explicit ID from 0..31");
  for (const auto& [old_id, new_id] : mapping.channels)
    if (old_id < 0 || old_id > 31 || new_id < 0 || new_id > 31 ||
        !mapped_channels.insert(new_id).second)
      return absl::InvalidArgumentError(
          "Give every channel a distinct explicit ID from 0..31");
  for (auto& sprite : plain.sprites) {
    if (!Node(sprite.id) || sprite.deleted || sprite.layer != 0)
      return absl::InvalidArgumentError("Unsupported assembly actor");
    if (sprite.id != 0xAF) {
      const auto value =
          clipboard.minecart->collision.find(sprite.y * 2 * 64 + sprite.x * 2);
      if (value == clipboard.minecart->collision.end() ||
          (sprite.id == 0xA3 ? value->second < 0xB7 || value->second > 0xBA
                             : value->second < 0xD0 || value->second > 0xD3))
        return absl::FailedPreconditionError(
            "Copied node is missing its collision dependency");
    }
    const auto& ids = sprite.id == 0xA3 ? mapping.carts : mapping.channels;
    if (sprite.id == 0xA3 &&
        std::count_if(
            clipboard.minecart->starts.begin(),
            clipboard.minecart->starts.end(), [&](const auto& s) {
              return s.id == sprite.subtype &&
                     s.room_id == clipboard.minecart->source_room &&
                     s.start_x == (s.room_id & 15) * 512 + sprite.x * 16 &&
                     s.start_y == (s.room_id >> 4) * 512 + sprite.y * 16;
            }) != 1)
      return absl::FailedPreconditionError(
          "Copied cart is missing its exact source-track dependency");
    const auto found = ids.find(sprite.subtype);
    if (found == ids.end())
      return absl::FailedPreconditionError(
          "Map every copied cart ID and switch channel before preview");
    sprite.subtype = found->second;
  }
  const int dx = anchor_x * 8 - clipboard.origin_pixel_x;
  const int dy = anchor_y * 8 - clipboard.origin_pixel_y;
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kPaste;
  request.clipboard = &plain;
  request.delta_x_pixels = dx;
  request.delta_y_pixels = dy;
  ASSIGN_OR_RETURN(auto selection, PlanDungeonSelectionEdit(room, request));
  for (size_t i = room.GetTileObjects().size();
       i < selection.after.objects.size(); ++i) {
    const auto& object = selection.after.objects[i];
    RETURN_IF_ERROR(Rail(object, profile));
    for (const auto& existing : room.GetTileObjects()) {
      if (zelda3::FindMinecartRailDescriptor(existing.id_, existing.size_,
                                             profile) &&
          std::abs(existing.x_ - object.x_) < 2 &&
          std::abs(existing.y_ - object.y_) < 2)
        return absl::FailedPreconditionError(
            "A rail already occupies this path; crossings need an explicit "
            "junction");
    }
    for (size_t j = room.GetTileObjects().size(); j < i; ++j)
      if (std::abs(selection.after.objects[j].x_ - object.x_) < 2 &&
          std::abs(selection.after.objects[j].y_ - object.y_) < 2)
        return absl::FailedPreconditionError("Copied rails overlap each other");
    for (const auto& sprite : room.GetSprites())
      if (!sprite.deleted() && sprite.layer() == 0 &&
          sprite.x() * 2 >= object.x_ && sprite.x() * 2 < object.x_ + 2 &&
          sprite.y() * 2 >= object.y_ && sprite.y() * 2 < object.y_ + 2)
        return absl::FailedPreconditionError(
            "A placed actor occupies this rail cell");
  }
  ASSIGN_OR_RETURN(auto collision,
                   zelda3::SynchronizeMinecartTrackCollision(
                       room.GetTileObjects(), selection.after.objects,
                       room.custom_collision(), ownership, profile));
  for (const auto& [offset, value] : clipboard.minecart->collision) {
    const int x = offset % 64 + dx / 8, y = offset / 64 + dy / 8;
    if (x < 0 || x > 63 || y < 0 || y > 63)
      return absl::OutOfRangeError("Translated collision leaves this room");
    const size_t target = y * 64 + x;
    if (room.custom_collision().HasOverride(target) &&
        room.custom_collision().tiles[target] != value)
      return absl::FailedPreconditionError(
          "Authored collision conflicts with the copied assembly");
    collision.collision.SetOverride(target, value);
    if (SpecialCollision(value))
      collision.ownership.erase(target);
  }
  // A node cannot share its anchor with any pre-existing sprite.
  for (size_t i = room.GetSprites().size(); i < selection.after.sprites.size();
       ++i) {
    const auto& node = selection.after.sprites[i];
    const size_t anchor = node.y * 2 * 64 + node.x * 2;
    if (node.id == 0xAF && room.custom_collision().HasOverride(anchor) &&
        room.custom_collision().tiles[anchor] != 0)
      return absl::FailedPreconditionError(
          "Authored collision occupies the copied lever anchor");
    for (size_t j = 0; j < i; ++j) {
      const auto& other = selection.after.sprites[j];
      if (!other.deleted && other.layer == node.layer && other.x == node.x &&
          other.y == node.y)
        return absl::FailedPreconditionError(
            "A sprite already occupies the copied node anchor");
    }
  }
  MinecartAssemblyPlan plan;
  plan.selection = std::move(selection);
  plan.collision_before = room.custom_collision();
  plan.collision_after = std::move(collision.collision);
  plan.ownership_before = ownership;
  plan.ownership_after = std::move(collision.ownership);
  plan.profile = profile;
  plan.tracks_before.assign(tracks.begin(), tracks.end());
  plan.tracks_after = plan.tracks_before;
  for (const auto& start : clipboard.minecart->starts) {
    const auto mapped = mapping.carts.find(start.id);
    if (mapped == mapping.carts.end() ||
        tracks.size() != kMinecartTrackSlotCount || room.id() == 0)
      return absl::FailedPreconditionError(
          "Map copied starts into a complete source table outside room zero");
    if (start.id < 0 || start.id > 31 || tracks[start.id] != start)
      return absl::FailedPreconditionError(
          "Source cart start changed after capture");
    const auto& slot = tracks[mapped->second];
    if (slot.id != mapped->second || slot.room_id != 0 || slot.start_x ||
        slot.start_y)
      return absl::FailedPreconditionError(
          "A copied cart needs an unused source slot");
    plan.tracks_after[mapped->second] = {
        mapped->second, room.id(),
        (room.id() & 15) * 512 + start.start_x - (start.room_id & 15) * 512 +
            dx,
        (room.id() >> 4) * 512 + start.start_y - (start.room_id >> 4) * 512 +
            dy};
  }
  plan.description = clipboard.minecart->source_room >= 0 ? "Copy rail assembly"
                                                          : "Draw rail path";
  return plan;
}
}  // namespace yaze::editor
