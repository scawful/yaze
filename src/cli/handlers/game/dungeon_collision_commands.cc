#include "cli/handlers/game/dungeon_collision_commands.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "cli/util/hex_util.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

constexpr int kCollisionGridSize = 64;
using json = nlohmann::json;

absl::StatusOr<std::unordered_set<int>> ParseTileFilter(
    const resources::ArgumentParser& parser) {
  std::unordered_set<int> tiles;
  auto tiles_opt = parser.GetString("tiles");
  if (!tiles_opt.has_value()) {
    return tiles;
  }

  for (absl::string_view token :
       absl::StrSplit(tiles_opt.value(), ',', absl::SkipEmpty())) {
    std::string t = std::string(absl::StripAsciiWhitespace(token));
    int v = 0;
    if (!ParseHexString(t, &v)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid tile value in --tiles: %s", t));
    }
    if (v < 0 || v > 0xFF) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Tile value out of range (0x00-0xFF): %s", t));
    }
    tiles.insert(v);
  }

  return tiles;
}

absl::StatusOr<int> ParseRoomIdToken(absl::string_view token) {
  std::string trimmed = std::string(absl::StripAsciiWhitespace(token));
  int room_id = 0;
  if (!ParseHexString(trimmed, &room_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid room ID: %s", trimmed));
  }
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::OutOfRangeError(
        absl::StrFormat("Room ID out of range: 0x%02X", room_id));
  }
  return room_id;
}

absl::StatusOr<std::vector<int>> ParseRoomSelection(
    const resources::ArgumentParser& parser) {
  std::vector<int> room_ids;
  bool any_explicit = false;

  if (auto room_opt = parser.GetString("room"); room_opt.has_value()) {
    any_explicit = true;
    ASSIGN_OR_RETURN(int room_id, ParseRoomIdToken(room_opt.value()));
    room_ids.push_back(room_id);
  }

  if (auto rooms_opt = parser.GetString("rooms"); rooms_opt.has_value()) {
    any_explicit = true;
    for (absl::string_view token :
         absl::StrSplit(rooms_opt.value(), ',', absl::SkipEmpty())) {
      ASSIGN_OR_RETURN(int room_id, ParseRoomIdToken(token));
      room_ids.push_back(room_id);
    }
  }

  if (parser.HasFlag("all")) {
    any_explicit = true;
    room_ids.clear();
    room_ids.reserve(zelda3::kNumberOfRooms);
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      room_ids.push_back(room_id);
    }
  }

  if (!any_explicit) {
    room_ids.reserve(zelda3::kNumberOfRooms);
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      room_ids.push_back(room_id);
    }
  }

  std::sort(room_ids.begin(), room_ids.end());
  room_ids.erase(std::unique(room_ids.begin(), room_ids.end()), room_ids.end());

  if (room_ids.empty()) {
    return absl::InvalidArgumentError(
        "No rooms selected (use --room, --rooms, or --all)");
  }
  return room_ids;
}

absl::StatusOr<std::string> ReadTextFile(const std::string& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open file for reading: %s", path));
  }
  std::stringstream ss;
  ss << in.rdbuf();
  if (!in.good() && !in.eof()) {
    return absl::InternalError(
        absl::StrFormat("Failed while reading file: %s", path));
  }
  return ss.str();
}

absl::Status WriteTextFile(const std::string& path,
                           const std::string& content) {
  std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return absl::PermissionDeniedError(
        absl::StrFormat("Cannot open file for writing: %s", path));
  }
  out << content;
  if (!out.good()) {
    return absl::InternalError(
        absl::StrFormat("Failed while writing file: %s", path));
  }
  return absl::OkStatus();
}

std::string StatusCodeName(absl::StatusCode code) {
  switch (code) {
    case absl::StatusCode::kOk:
      return "OK";
    case absl::StatusCode::kCancelled:
      return "CANCELLED";
    case absl::StatusCode::kUnknown:
      return "UNKNOWN";
    case absl::StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case absl::StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case absl::StatusCode::kNotFound:
      return "NOT_FOUND";
    case absl::StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case absl::StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case absl::StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case absl::StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case absl::StatusCode::kAborted:
      return "ABORTED";
    case absl::StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case absl::StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case absl::StatusCode::kInternal:
      return "INTERNAL";
    case absl::StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case absl::StatusCode::kDataLoss:
      return "DATA_LOSS";
    case absl::StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
  }
  return "UNKNOWN";
}

json BuildBaseReport(absl::string_view command_name, bool dry_run) {
  return json{
      {"command", std::string(command_name)},
      {"status", "success"},
      {"dry_run", dry_run},
      {"mode", dry_run ? "dry-run" : "write"},
  };
}

absl::Status WriteReportIfRequested(const resources::ArgumentParser& parser,
                                    const json& report) {
  auto report_path = parser.GetString("report");
  if (!report_path.has_value()) {
    return absl::OkStatus();
  }
  return WriteTextFile(*report_path, report.dump(2) + "\n");
}

absl::Status FinalizeWithReport(const resources::ArgumentParser& parser,
                                json report, const absl::Status& status) {
  if (!status.ok()) {
    report["status"] = "error";
    report["error"] = json{
        {"code", StatusCodeName(status.code())},
        {"message", std::string(status.message())},
    };
  }

  const auto report_status = WriteReportIfRequested(parser, report);
  if (!report_status.ok()) {
    if (status.ok()) {
      return report_status;
    }
    return absl::InternalError(absl::StrFormat(
        "Command failed (%s) and report write failed (%s)", status.message(),
        report_status.message()));
  }

  return status;
}

}  // namespace

absl::Status DungeonListCustomCollisionCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();

  int room_id = 0;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID format. Must be hex.");
  }

  ASSIGN_OR_RETURN(auto filter_tiles, ParseTileFilter(parser));

  const bool list_all = parser.HasFlag("all");
  const bool list_nonzero =
      parser.HasFlag("nonzero") || (!list_all && filter_tiles.empty());

  formatter.BeginObject("Dungeon Custom Collision");
  formatter.AddField("room_id", room_id);
  formatter.AddHexField("room_id_hex", room_id, 2);
  formatter.AddField("filter_mode",
                     !filter_tiles.empty()
                         ? "tiles"
                         : (list_all ? "all" : (list_nonzero ? "nonzero" : "all")));

  auto map_or = zelda3::LoadCustomCollisionMap(rom, room_id);
  if (!map_or.ok()) {
    formatter.AddField("status", "error");
    formatter.AddField("error", map_or.status().ToString());
    formatter.EndObject();
    return map_or.status();
  }

  const auto& map = map_or.value();
  formatter.AddField("has_data", map.has_data);

  int nonzero_count = 0;
  for (uint8_t tile : map.tiles) {
    if (tile != 0) {
      ++nonzero_count;
    }
  }
  formatter.AddField("nonzero_tiles", nonzero_count);

  formatter.BeginArray("tiles");
  int match_count = 0;
  if (map.has_data) {
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        uint8_t tile = map.tiles[static_cast<size_t>(y * 64 + x)];

        if (!filter_tiles.empty()) {
          if (filter_tiles.find(static_cast<int>(tile)) == filter_tiles.end()) {
            continue;
          }
        } else if (list_nonzero) {
          if (tile == 0) {
            continue;
          }
        } else if (!list_all) {
          // Default behavior if neither filter nor flags are set is nonzero.
          if (tile == 0) {
            continue;
          }
        }

        formatter.BeginObject();
        formatter.AddField("x", x);
        formatter.AddField("y", y);
        formatter.AddHexField("tile", tile, 2);
        formatter.EndObject();
        ++match_count;
      }
    }
  }
  formatter.EndArray();

  formatter.AddField("match_count", match_count);
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status DungeonExportCustomCollisionJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  json report = BuildBaseReport(GetName(), /*dry_run=*/false);
  const absl::Status status = [&]() -> absl::Status {
    ASSIGN_OR_RETURN(const auto room_ids, ParseRoomSelection(parser));
    const std::string out_path = parser.GetString("out").value();
    report["out_path"] = out_path;
    report["requested_rooms"] = static_cast<int>(room_ids.size());

    std::vector<zelda3::CustomCollisionRoomEntry> export_rooms;
    export_rooms.reserve(room_ids.size());

    for (int room_id : room_ids) {
      ASSIGN_OR_RETURN(auto map, zelda3::LoadCustomCollisionMap(rom, room_id));
      if (!map.has_data) {
        continue;
      }

      zelda3::CustomCollisionRoomEntry entry;
      entry.room_id = room_id;
      for (int offset = 0; offset < kCollisionGridSize * kCollisionGridSize;
           ++offset) {
        const uint8_t tile = map.tiles[static_cast<size_t>(offset)];
        if (tile == 0) {
          continue;
        }
        entry.tiles.push_back(zelda3::CustomCollisionTileEntry{
            static_cast<uint16_t>(offset), tile});
      }
      if (!entry.tiles.empty()) {
        export_rooms.push_back(std::move(entry));
      }
    }

    ASSIGN_OR_RETURN(
        const std::string exported_json,
        zelda3::DumpCustomCollisionRoomsToJsonString(export_rooms));
    RETURN_IF_ERROR(WriteTextFile(out_path, exported_json));
    report["exported_rooms"] = static_cast<int>(export_rooms.size());

    formatter.BeginObject("Custom Collision Export");
    formatter.AddField("out_path", out_path);
    formatter.AddField("requested_rooms", static_cast<int>(room_ids.size()));
    formatter.AddField("exported_rooms", static_cast<int>(export_rooms.size()));
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(parser, std::move(report), status);
}

absl::Status DungeonImportCustomCollisionJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool dry_run = parser.HasFlag("dry-run");
  json report = BuildBaseReport(GetName(), dry_run);
  const absl::Status status = [&]() -> absl::Status {
    const std::string in_path = parser.GetString("in").value();
    const bool replace_all = parser.HasFlag("replace-all");
    const bool force = parser.HasFlag("force");
    report["in_path"] = in_path;
    report["replace_all"] = replace_all;
    report["force"] = force;

    if (!zelda3::HasCustomCollisionWriteSupport(rom->vector().size())) {
      return absl::FailedPreconditionError(
          "Custom collision write support not present in this ROM");
    }

    if (replace_all && !dry_run && !force) {
      return absl::FailedPreconditionError(
          "--replace-all requires --force (run with --dry-run first)");
    }

    ASSIGN_OR_RETURN(const std::string json_content, ReadTextFile(in_path));
    ASSIGN_OR_RETURN(
        auto imported_rooms,
        zelda3::LoadCustomCollisionRoomsFromJsonString(json_content));
    report["imported_room_entries"] = static_cast<int>(imported_rooms.size());

    std::array<zelda3::Room, zelda3::kNumberOfRooms> rooms;
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      rooms[room_id] = zelda3::Room(room_id, rom, nullptr);
    }

    int populated_rooms = 0;
    int cleared_rooms = 0;
    std::unordered_set<int> touched_rooms;
    for (const auto& imported : imported_rooms) {
      touched_rooms.insert(imported.room_id);
      auto& room = rooms[imported.room_id];
      room.custom_collision().tiles.fill(0);
      room.custom_collision().has_data = false;
      room.MarkCustomCollisionDirty();

      bool has_nonzero = false;
      for (const auto& tile : imported.tiles) {
        const int offset = static_cast<int>(tile.offset);
        if (offset < 0 || offset >= kCollisionGridSize * kCollisionGridSize) {
          continue;
        }
        if (tile.value == 0) {
          continue;
        }
        room.SetCollisionTile(offset % kCollisionGridSize,
                              offset / kCollisionGridSize, tile.value);
        has_nonzero = true;
      }

      if (has_nonzero) {
        ++populated_rooms;
      } else {
        ++cleared_rooms;
      }
    }

    int replace_all_clears = 0;
    if (replace_all) {
      for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
        if (touched_rooms.contains(room_id)) {
          continue;
        }
        auto& room = rooms[room_id];
        room.custom_collision().tiles.fill(0);
        room.custom_collision().has_data = false;
        room.MarkCustomCollisionDirty();
        ++cleared_rooms;
        ++replace_all_clears;
      }
    }
    report["replace_all_clears"] = replace_all_clears;

    if (!dry_run) {
      RETURN_IF_ERROR(zelda3::SaveAllCollision(rom, absl::MakeSpan(rooms)));
    }

    report["populated_rooms"] = populated_rooms;
    report["cleared_rooms"] = cleared_rooms;

    formatter.BeginObject("Custom Collision Import");
    formatter.AddField("in_path", in_path);
    formatter.AddField("replace_all", replace_all);
    formatter.AddField("force", force);
    formatter.AddField("mode", dry_run ? "dry-run" : "write");
    formatter.AddField("imported_room_entries",
                       static_cast<int>(imported_rooms.size()));
    formatter.AddField("populated_rooms", populated_rooms);
    formatter.AddField("cleared_rooms", cleared_rooms);
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(parser, std::move(report), status);
}

absl::Status DungeonExportWaterFillJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  json report = BuildBaseReport(GetName(), /*dry_run=*/false);
  const absl::Status status = [&]() -> absl::Status {
    const std::string out_path = parser.GetString("out").value();
    ASSIGN_OR_RETURN(const auto room_ids, ParseRoomSelection(parser));
    report["out_path"] = out_path;
    report["requested_rooms"] = static_cast<int>(room_ids.size());

    if (!zelda3::HasWaterFillReservedRegion(rom->vector().size())) {
      return absl::FailedPreconditionError(
          "WaterFill reserved region missing in this ROM");
    }

    ASSIGN_OR_RETURN(auto zones, zelda3::LoadWaterFillTable(rom));
    std::unordered_set<int> room_filter(room_ids.begin(), room_ids.end());
    std::vector<zelda3::WaterFillZoneEntry> filtered;
    filtered.reserve(zones.size());
    for (const auto& zone : zones) {
      if (!room_filter.contains(zone.room_id)) {
        continue;
      }
      filtered.push_back(zone);
    }

    ASSIGN_OR_RETURN(const std::string exported_json,
                     zelda3::DumpWaterFillZonesToJsonString(filtered));
    RETURN_IF_ERROR(WriteTextFile(out_path, exported_json));
    report["exported_zones"] = static_cast<int>(filtered.size());

    formatter.BeginObject("Water Fill Export");
    formatter.AddField("out_path", out_path);
    formatter.AddField("requested_rooms", static_cast<int>(room_ids.size()));
    formatter.AddField("exported_zones", static_cast<int>(filtered.size()));
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(parser, std::move(report), status);
}

absl::Status DungeonImportWaterFillJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool dry_run = parser.HasFlag("dry-run");
  const bool strict_masks = parser.HasFlag("strict-masks");
  json report = BuildBaseReport(GetName(), dry_run);
  const absl::Status status = [&]() -> absl::Status {
    const std::string in_path = parser.GetString("in").value();
    report["in_path"] = in_path;
    report["strict_masks"] = strict_masks;

    if (!zelda3::HasWaterFillReservedRegion(rom->vector().size())) {
      return absl::FailedPreconditionError(
          "WaterFill reserved region missing in this ROM");
    }

    ASSIGN_OR_RETURN(const std::string json_content, ReadTextFile(in_path));
    ASSIGN_OR_RETURN(auto zones,
                     zelda3::LoadWaterFillZonesFromJsonString(json_content));

    auto original_zones = zones;
    RETURN_IF_ERROR(zelda3::NormalizeWaterFillZoneMasks(&zones));

    int normalized_masks = 0;
    std::unordered_map<int, uint8_t> before_masks;
    before_masks.reserve(original_zones.size());
    for (const auto& z : original_zones) {
      before_masks[z.room_id] = z.sram_bit_mask;
    }
    for (const auto& z : zones) {
      auto it = before_masks.find(z.room_id);
      if (it == before_masks.end() || it->second != z.sram_bit_mask) {
        ++normalized_masks;
      }
    }

    report["zone_count"] = static_cast<int>(zones.size());
    report["normalized_masks"] = normalized_masks;

    if (strict_masks && normalized_masks > 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "WaterFill masks require normalization (%d changed); rerun without "
          "--strict-masks to apply normalized masks",
          normalized_masks));
    }

    if (!dry_run) {
      RETURN_IF_ERROR(zelda3::WriteWaterFillTable(rom, zones));
    }

    formatter.BeginObject("Water Fill Import");
    formatter.AddField("in_path", in_path);
    formatter.AddField("mode", dry_run ? "dry-run" : "write");
    formatter.AddField("strict_masks", strict_masks);
    formatter.AddField("zone_count", static_cast<int>(zones.size()));
    formatter.AddField("normalized_masks", normalized_masks);
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(parser, std::move(report), status);
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
