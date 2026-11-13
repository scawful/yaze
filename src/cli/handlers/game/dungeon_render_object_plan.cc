#include "cli/handlers/game/dungeon_render_object_plan.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/hack_manifest.h"
#include "core/source_artifact_publisher.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::cli::handlers {
namespace {

using Json = nlohmann::json;
using ObjectKey = std::tuple<int, int, int, int, int>;

std::string HashBytes(const std::vector<uint8_t>& bytes) {
  return core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

absl::Status CheckKeys(const Json& value,
                       std::initializer_list<const char*> required,
                       std::initializer_list<const char*> optional = {}) {
  if (!value.is_object()) {
    return absl::InvalidArgumentError(
        "Object plan entries must be JSON objects");
  }
  for (const char* key : required) {
    if (!value.contains(key)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Object plan is missing '%s'", key));
    }
  }
  for (const auto& [key, unused] : value.items()) {
    if (std::none_of(required.begin(), required.end(),
                     [&key](const char* name) { return key == name; }) &&
        std::none_of(optional.begin(), optional.end(),
                     [&key](const char* name) { return key == name; })) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown object-plan field '%s'", key));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<int> Integer(const Json& object, const char* key, int maximum) {
  const auto& value = object.at(key);
  if (!value.is_number_integer()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Object-plan '%s' must be a JSON integer", key));
  }
  if (value.is_number_unsigned() &&
      value.get<uint64_t>() > static_cast<uint64_t>(maximum)) {
    return absl::OutOfRangeError(
        absl::StrFormat("Object-plan '%s' out of range", key));
  }
  const int64_t number = value.get<int64_t>();
  if (number < 0 || number > maximum) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Object-plan '%s' must be between 0 and %d", key, maximum));
  }
  return static_cast<int>(number);
}

ObjectKey Key(const zelda3::RoomObject& object) {
  return {object.id_, object.x(), object.y(), object.size(),
          object.GetLayerValue()};
}

absl::StatusOr<zelda3::RoomObject> ParseObject(const Json& value) {
  RETURN_IF_ERROR(CheckKeys(value, {"id", "x", "y", "size", "list"}));
  ASSIGN_OR_RETURN(const int id, Integer(value, "id", 0xFFF));
  ASSIGN_OR_RETURN(const int x, Integer(value, "x", 63));
  ASSIGN_OR_RETURN(const int y, Integer(value, "y", 63));
  ASSIGN_OR_RETURN(const int size, Integer(value, "size", 255));
  ASSIGN_OR_RETURN(const int list, Integer(value, "list", 2));
  zelda3::RoomObject object(id, x, y, size, list);
  RETURN_IF_ERROR(zelda3::ValidateRoomObjectStreamEntryForSave(object));
  return object;
}

auto Lists(const zelda3::Room& room) {
  std::array<std::vector<ObjectKey>, 3> lists;
  for (const auto& object : room.GetTileObjects()) {
    lists.at(object.GetLayerValue()).push_back(Key(object));
  }
  return lists;
}

auto Doors(const zelda3::Room& room) {
  std::vector<std::pair<uint8_t, uint8_t>> doors;
  for (const auto& door : room.GetDoors()) {
    doors.emplace_back(door.byte1, door.byte2);
  }
  return doors;
}

absl::StatusOr<std::unique_ptr<Rom>> DetachedRom(
    const std::vector<uint8_t>& image) {
  auto rom = std::make_unique<Rom>();
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(rom->LoadFromData(image, options));
  return rom;
}

}  // namespace

absl::StatusOr<DungeonRenderObjectPreview> StageDungeonRenderObjectPlan(
    const Rom& source, int room_id, const Json& plan,
    const core::HackManifest* manifest) {
  if (!source.is_loaded() || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        "Object plan requires a loaded ROM and valid room");
  }
  RETURN_IF_ERROR(CheckKeys(plan, {"version", "room", "rom_sha256", "edits"},
                            {"object_stream_sha256"}));
  ASSIGN_OR_RETURN(const int version, Integer(plan, "version", 1));
  ASSIGN_OR_RETURN(const int plan_room,
                   Integer(plan, "room", zelda3::kNumberOfRooms - 1));
  if (version != 1 || plan_room != room_id) {
    return absl::FailedPreconditionError(
        "Object-plan version or room does not match invocation");
  }
  if (!plan["rom_sha256"].is_string() ||
      plan["rom_sha256"].get<std::string>() != HashBytes(source.vector())) {
    return absl::FailedPreconditionError(
        "Object-plan ROM SHA-256 preimage differs");
  }
  if (!plan["edits"].is_array() || plan["edits"].size() > 4096) {
    return absl::InvalidArgumentError(
        "Object-plan edits must be an array of at most 4096 entries");
  }
  ASSIGN_OR_RETURN(const auto stream,
                   zelda3::ReadDungeonObjectStream(source, room_id));
  const auto stream_hash = HashBytes(stream.encoded_stream);
  if (plan.contains("object_stream_sha256") &&
      (!plan["object_stream_sha256"].is_string() ||
       plan["object_stream_sha256"].get<std::string>() != stream_hash)) {
    return absl::FailedPreconditionError(
        "Object-plan stream SHA-256 preimage differs");
  }
  ASSIGN_OR_RETURN(auto scratch, DetachedRom(source.vector()));
  auto room = std::make_unique<zelda3::Room>(
      zelda3::LoadRoomFromRom(scratch.get(), room_id));
  // Stream indices exclude pot-table/special objects, exactly as describe-room.
  room->LoadObjects();
  const auto original_objects = room->GetTileObjects();
  const auto original_doors = Doors(*room);
  const bool original_marker = room->door_marker();
  std::map<int, std::optional<zelda3::RoomObject>, std::greater<int>> indexed;
  std::vector<zelda3::RoomObject> appended;
  for (const auto& edit : plan["edits"]) {
    if (!edit.is_object() || !edit.contains("op") || !edit["op"].is_string()) {
      return absl::InvalidArgumentError("Object edit requires a string 'op'");
    }
    const std::string op = edit["op"].get<std::string>();
    if (op == "append") {
      RETURN_IF_ERROR(CheckKeys(edit, {"op", "object"}));
      ASSIGN_OR_RETURN(auto object, ParseObject(edit["object"]));
      appended.push_back(std::move(object));
      continue;
    }
    if (op != "replace" && op != "remove") {
      return absl::InvalidArgumentError(
          "Object-plan op must be append, replace or remove");
    }
    if (op == "replace") {
      RETURN_IF_ERROR(CheckKeys(edit, {"op", "index", "before", "object"}));
    } else {
      RETURN_IF_ERROR(CheckKeys(edit, {"op", "index", "before"}));
    }
    ASSIGN_OR_RETURN(const int index,
                     Integer(edit, "index", std::numeric_limits<int>::max()));
    ASSIGN_OR_RETURN(const auto expected, ParseObject(edit["before"]));
    if (index >= static_cast<int>(original_objects.size()) ||
        Key(original_objects[index]) != Key(expected)) {
      return absl::FailedPreconditionError(
          "Object-plan index/before preimage differs");
    }
    if (indexed.contains(index)) {
      return absl::InvalidArgumentError(
          "Object-plan edits repeat an original stream index");
    }
    if (op == "replace") {
      ASSIGN_OR_RETURN(auto object, ParseObject(edit["object"]));
      indexed.emplace(index, std::move(object));
    } else {
      indexed.emplace(index, std::nullopt);
    }
  }
  for (const auto& [index, object] : indexed) {
    if (object.has_value()) {
      RETURN_IF_ERROR(room->UpdateObject(index, *object));
    } else {
      RETURN_IF_ERROR(room->RemoveObject(index));
    }
  }
  for (const auto& object : appended) {
    RETURN_IF_ERROR(room->AddObject(object));
  }
  const auto expected_lists = Lists(*room);

  std::optional<zelda3::DungeonStreamLayout> layout;
  if (manifest != nullptr) {
    if (const auto* declared = manifest->GetDungeonStreamLayout(
            core::DungeonStreamType::kObjects)) {
      if (declared->strategy != core::DungeonWriteStrategy::kCopyOnWrite) {
        return absl::FailedPreconditionError(
            "Object preview requires copy_on_write stream planning");
      }
      ASSIGN_OR_RETURN(layout,
                       core::ToDungeonStreamAllocatorLayout(
                           core::DungeonStreamType::kObjects, *declared));
    }
  }
  auto candidate = source.vector();
  ASSIGN_OR_RETURN(
      const auto ranges,
      room->StageObjectSave(layout ? &*layout : nullptr, candidate));
  if (manifest != nullptr && !manifest->AnalyzePcWriteRanges(ranges).empty()) {
    return absl::PermissionDeniedError(
        "Object preview conflicts with Hack Manifest");
  }
  room.reset();
  if (rom::IsNativeSpecialOverworldLayout(scratch->InspectLyraLayout())) {
    RETURN_IF_ERROR(scratch->CommitNativeNonOverworldImage(
        source.vector(), std::move(candidate), ranges));
  } else {
    ASSIGN_OR_RETURN(scratch, DetachedRom(candidate));
  }
  auto reopened = std::make_unique<zelda3::Room>(
      zelda3::LoadRoomFromRom(scratch.get(), room_id));
  reopened->LoadObjects();
  ASSIGN_OR_RETURN(const auto saved,
                   zelda3::ReadDungeonObjectStream(*scratch, room_id));
  if (Lists(*reopened) != expected_lists ||
      Doors(*reopened) != original_doors ||
      reopened->door_marker() != original_marker ||
      saved.encoded_stream[0] != stream.encoded_stream[0] ||
      saved.encoded_stream[1] != stream.encoded_stream[1]) {
    return absl::FailedPreconditionError(
        "Object preview does not preserve object/header/door readback");
  }
  DungeonRenderObjectPreview result;
  result.rom = std::move(scratch);
  result.source_stream_sha256 = stream_hash;
  result.objects_before = static_cast<int>(original_objects.size());
  result.objects_after = static_cast<int>(reopened->GetTileObjects().size());
  result.edit_count = static_cast<int>(plan["edits"].size());
  return result;
}

}  // namespace yaze::cli::handlers
