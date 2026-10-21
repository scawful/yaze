#include "app/editor/dungeon/dungeon_agent_tools.h"

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "zelda3/dungeon/room_object_batch.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze::editor {
namespace {

#ifdef YAZE_WITH_JSON
using Json = nlohmann::json;

bool HasKeys(const Json& object, std::initializer_list<const char*> keys) {
  if (!object.is_object() || object.size() != keys.size())
    return false;
  for (const char* key : keys) {
    if (!object.contains(key))
      return false;
  }
  return true;
}

// JSON booleans/floats and overflowing unsigned values are never coordinates.
bool ReadInt(const Json& object, const char* key, int& result) {
  if (!object.contains(key))
    return false;
  const auto& value = object.at(key);
  if (!value.is_number_integer())
    return false;
  if (value.is_number_unsigned()) {
    const auto number = value.get<uint64_t>();
    if (number > static_cast<uint64_t>(std::numeric_limits<int>::max()))
      return false;
    result = static_cast<int>(number);
  } else {
    const auto number = value.get<int64_t>();
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max())
      return false;
    result = static_cast<int>(number);
  }
  return true;
}

Json ObjectsJson(const zelda3::Room& room) {
  Json objects = Json::array();
  for (const auto& guard :
       zelda3::CaptureRoomObjectGuards(room.GetTileObjects())) {
    objects.push_back({{"object_index", guard.object_index},
                       {"object_id", guard.object_id},
                       {"x_tile", guard.x_tile},
                       {"y_tile", guard.y_tile},
                       {"size", guard.size},
                       {"stored_list", guard.stored_list}});
  }
  return objects;
}

absl::StatusOr<int> ParseRoom(std::string_view text) {
  int base = 10;
  if (text.substr(0, 2) == "0x" || text.substr(0, 2) == "0X") {
    text.remove_prefix(2);
    base = 16;
  } else if (!text.empty() && text.front() == '$') {
    text.remove_prefix(1);
    base = 16;
  }
  int room = -1;
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), room, base);
  if (text.empty() || parsed.ec != std::errc() ||
      parsed.ptr != text.data() + text.size() || room < 0)
    return absl::InvalidArgumentError(
        "room must be a decimal or hexadecimal id");
  return room;
}

absl::StatusOr<zelda3::RoomObjectBatch> ParseBatch(
    const std::string& text, const std::string& session_id, int room_id) {
  if (text.size() > 1024 * 1024)
    return absl::InvalidArgumentError("batch exceeds 1 MiB");
  try {
    std::vector<std::set<std::string>> keys;
    Json document =
        Json::parse(text, [&keys](int, Json::parse_event_t event, Json& value) {
          if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
          if (event == Json::parse_event_t::key &&
              !keys.back().insert(value.get<std::string>()).second)
            throw std::invalid_argument("duplicate JSON key");
          if (event == Json::parse_event_t::object_end)
            keys.pop_back();
          return true;
        });
    if (!HasKeys(document, {"session_id", "expected_objects", "moves"}) ||
        !document["session_id"].is_string() ||
        !document["expected_objects"].is_array() ||
        !document["moves"].is_array())
      return absl::InvalidArgumentError(
          "batch requires exactly session_id, expected_objects and moves");
    if (document["session_id"].get<std::string>() != session_id)
      return absl::FailedPreconditionError(
          "Editor session changed; inspect the loaded room again");
    zelda3::RoomObjectBatch batch;
    batch.room_id = room_id;
    for (const auto& entry : document["expected_objects"]) {
      int index;
      zelda3::RoomObjectGuard guard{};
      if (!HasKeys(entry, {"object_index", "object_id", "x_tile", "y_tile",
                           "size", "stored_list"}) ||
          !ReadInt(entry, "object_index", index) || index < 0 ||
          !ReadInt(entry, "object_id", guard.object_id) ||
          !ReadInt(entry, "x_tile", guard.x_tile) ||
          !ReadInt(entry, "y_tile", guard.y_tile) ||
          !ReadInt(entry, "size", guard.size) ||
          !ReadInt(entry, "stored_list", guard.stored_list))
        return absl::InvalidArgumentError("Invalid expected_objects entry");
      guard.object_index = static_cast<size_t>(index);
      batch.expected_objects.push_back(guard);
    }
    for (const auto& entry : document["moves"]) {
      int index;
      zelda3::RoomObjectMove move{};
      if (!HasKeys(entry, {"object_index", "x_tile", "y_tile"}) ||
          !ReadInt(entry, "object_index", index) || index < 0 ||
          !ReadInt(entry, "x_tile", move.x_tile) ||
          !ReadInt(entry, "y_tile", move.y_tile))
        return absl::InvalidArgumentError("Invalid moves entry");
      move.object_index = static_cast<size_t>(index);
      batch.moves.push_back(move);
    }
    return batch;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid batch JSON: ") +
                                      error.what());
  }
}

absl::StatusOr<std::string> Dispatch(const cli::ToolCall& call,
                                     Rom* expected_rom,
                                     const DungeonEditorToolContext& context) {
  if (!context.rom || !context.rom->is_loaded() || !context.dungeon ||
      expected_rom != context.rom || context.dungeon->rom() != context.rom ||
      context.session_id.empty())
    return absl::FailedPreconditionError(
        "Native dungeon editor session is unavailable or changed");
  const bool apply = call.tool_name == "dungeon-edit-apply";
  if (!apply && call.tool_name != "dungeon-edit-inspect")
    return absl::InvalidArgumentError("Unsupported native dungeon editor tool");
  if (!call.args.count("room") || (apply && !call.args.count("batch")) ||
      call.args.size() != (apply ? 2u : 1u))
    return absl::InvalidArgumentError(
        "Invalid native dungeon editor arguments");
  auto room_id = ParseRoom(call.args.at("room"));
  if (!room_id.ok())
    return room_id.status();
  if (static_cast<size_t>(*room_id) >= context.dungeon->rooms().size())
    return absl::InvalidArgumentError(
        "Room id is outside the dungeon room table");
  auto* room = context.dungeon->rooms().GetIfLoaded(*room_id);
  if (!room)
    return absl::FailedPreconditionError(
        "Room is not loaded; open it in the dungeon editor before editing");
  Json result = {{"session_id", context.session_id},
                 {"room_id", *room_id},
                 {"rom_saved", false}};
  if (apply) {
    auto batch =
        ParseBatch(call.args.at("batch"), context.session_id, *room_id);
    if (!batch.ok())
      return batch.status();
    auto applied = context.dungeon->ApplyRoomObjectBatch(*batch);
    if (!applied.ok())
      return applied.status();
    result["changed_object_count"] = applied->changed_object_count;
    result["rendered"] = applied->changed_object_count > 0;
    result["undo_recorded"] = applied->changed_object_count > 0;
  }
  result["objects"] = ObjectsJson(*room);
  return result.dump();
}
#endif  // YAZE_WITH_JSON

}  // namespace

cli::agent::ToolDispatcher::DungeonEditorToolCallback
MakeDungeonEditorToolCallback(
    std::function<DungeonEditorToolContext()> current_context) {
  const auto editor_thread = std::this_thread::get_id();
  return [editor_thread, current_context = std::move(current_context)](
             const cli::ToolCall& call,
             Rom* expected_rom) -> absl::StatusOr<std::string> {
    if (std::this_thread::get_id() != editor_thread)
      return absl::FailedPreconditionError(
          "Native dungeon tools must run on the editor thread");
    if (!current_context)
      return absl::FailedPreconditionError(
          "Native editor context is unavailable");
#ifdef YAZE_WITH_JSON
    return Dispatch(call, expected_rom, current_context());
#else
    return absl::UnimplementedError(
        "Native dungeon tools require JSON support");
#endif
  };
}

}  // namespace yaze::editor
