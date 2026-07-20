#include "cli/handlers/game/dungeon_stream_plan_commands.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/hack_manifest.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::cli::handlers {
namespace {

struct StreamSelection {
  core::DungeonStreamType manifest_type;
  const char* name;
};

absl::StatusOr<StreamSelection> ParseStreamSelection(const std::string& kind) {
  if (kind == "objects") {
    return StreamSelection{core::DungeonStreamType::kObjects, "objects"};
  }
  if (kind == "sprites") {
    return StreamSelection{core::DungeonStreamType::kSprites, "sprites"};
  }
  if (kind == "pot_items") {
    return StreamSelection{core::DungeonStreamType::kPotItems, "pot_items"};
  }
  return absl::InvalidArgumentError(
      "--kind must be one of: objects, sprites, pot_items");
}

const char* PointerEncodingName(core::DungeonPointerEncoding encoding) {
  switch (encoding) {
    case core::DungeonPointerEncoding::kLong24:
      return "long24";
    case core::DungeonPointerEncoding::kBank16:
      return "bank16";
  }
  return "unknown";
}

const char* WriteStrategyName(core::DungeonWriteStrategy strategy) {
  switch (strategy) {
    case core::DungeonWriteStrategy::kCopyOnWrite:
      return "copy_on_write";
    case core::DungeonWriteStrategy::kRepackAll:
      return "repack_all";
  }
  return "unknown";
}

const char* IssueCodeName(zelda3::DungeonStreamIssueCode code) {
  switch (code) {
    case zelda3::DungeonStreamIssueCode::kInvalidPointer:
      return "invalid_pointer";
    case zelda3::DungeonStreamIssueCode::kPointerOutsideDataRanges:
      return "pointer_outside_data_ranges";
    case zelda3::DungeonStreamIssueCode::kMalformedStream:
      return "malformed_stream";
  }
  return "unknown";
}

const char* OverlapKindName(zelda3::DungeonStreamOverlapKind kind) {
  switch (kind) {
    case zelda3::DungeonStreamOverlapKind::kSuffix:
      return "suffix";
    case zelda3::DungeonStreamOverlapKind::kInterior:
      return "interior";
  }
  return "unknown";
}

uint64_t SumIntervalBytes(
    const std::vector<zelda3::DungeonStreamPcRange>& intervals) {
  uint64_t total = 0;
  for (const auto& interval : intervals) {
    total += static_cast<uint64_t>(interval.end) - interval.begin;
  }
  return total;
}

void AddRoomIds(resources::OutputFormatter& formatter,
                const std::vector<uint32_t>& room_ids) {
  formatter.BeginArray("room_ids");
  for (uint32_t room_id : room_ids) {
    formatter.AddArrayItem(absl::StrFormat("0x%03X", room_id));
  }
  formatter.EndArray();
}

void AddIntervals(resources::OutputFormatter& formatter,
                  const std::string& name,
                  const std::vector<zelda3::DungeonStreamPcRange>& intervals) {
  formatter.BeginArray(name);
  for (const auto& interval : intervals) {
    formatter.BeginObject();
    formatter.AddHexField("begin_pc", interval.begin, 6);
    formatter.AddHexField("end_pc", interval.end, 6);
    formatter.AddField("bytes",
                       static_cast<uint64_t>(interval.end) - interval.begin);
    formatter.EndObject();
  }
  formatter.EndArray();
}

struct UniqueStream {
  uint32_t end_pc = 0;
  std::vector<uint32_t> room_ids;
};

}  // namespace

absl::Status DungeonStreamPlanCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"kind", "manifest"}));
  if (parser.HasFlag("write") || parser.GetString("write").has_value()) {
    return absl::InvalidArgumentError(
        "dungeon-stream-plan is read-only and does not accept --write");
  }
  return ParseStreamSelection(*parser.GetString("kind")).status();
}

absl::Status DungeonStreamPlanCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  StreamSelection selection;
  ASSIGN_OR_RETURN(selection, ParseStreamSelection(*parser.GetString("kind")));
  const std::string manifest_path = *parser.GetString("manifest");

  core::HackManifest manifest;
  RETURN_IF_ERROR(manifest.LoadFromFile(manifest_path));
  const core::DungeonStreamLayout* manifest_layout =
      manifest.GetDungeonStreamLayout(selection.manifest_type);
  if (manifest_layout == nullptr) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Manifest does not define dungeon_stream_regions.%s", selection.name));
  }

  zelda3::DungeonStreamLayout allocator_layout;
  ASSIGN_OR_RETURN(allocator_layout,
                   core::ToDungeonStreamAllocatorLayout(selection.manifest_type,
                                                        *manifest_layout));
  zelda3::DungeonStreamInventory inventory;
  ASSIGN_OR_RETURN(inventory,
                   zelda3::InventoryDungeonStreams(*rom, allocator_layout));

  std::map<uint32_t, UniqueStream> unique_streams;
  uint64_t valid_stream_slots = 0;
  for (const auto& stream : inventory.streams) {
    if (!stream.valid) {
      continue;
    }
    ++valid_stream_slots;
    auto& unique = unique_streams[stream.data_pc];
    unique.end_pc = stream.logical_end_pc;
    unique.room_ids.push_back(stream.room_id);
  }

  uint64_t suffix_overlaps = 0;
  uint64_t interior_overlaps = 0;
  for (const auto& overlap : inventory.overlaps) {
    if (overlap.kind == zelda3::DungeonStreamOverlapKind::kSuffix) {
      ++suffix_overlaps;
    } else {
      ++interior_overlaps;
    }
  }
  uint64_t exact_aliased_rooms = 0;
  for (const auto& alias : inventory.aliases) {
    exact_aliased_rooms += alias.room_ids.size();
  }

  formatter.AddField("status", inventory.ok() ? "ok" : "issues");
  formatter.AddField("kind", selection.name);
  formatter.AddField("manifest", manifest_path);
  formatter.AddField("strategy", WriteStrategyName(manifest_layout->strategy));
  formatter.AddField("pointer_encoding",
                     PointerEncodingName(manifest_layout->pointer_encoding));
  formatter.AddHexField("pointer_table_snes", manifest_layout->pointer_table,
                        6);
  formatter.AddHexField("pointer_table_pc", allocator_layout.pointer_table_pc,
                        6);
  formatter.AddField("pointer_count",
                     static_cast<uint64_t>(allocator_layout.pointer_count));
  if (manifest_layout->pointer_bank.has_value()) {
    formatter.AddHexField("pointer_bank", *manifest_layout->pointer_bank, 2);
  }
  formatter.AddField("source_size",
                     static_cast<uint64_t>(inventory.source_size));
  formatter.AddHexField("source_crc32", inventory.source_crc32, 8);

  formatter.AddField("stream_slot_count",
                     static_cast<uint64_t>(inventory.streams.size()));
  formatter.AddField("valid_stream_slot_count", valid_stream_slots);
  formatter.AddField("unique_stream_count",
                     static_cast<uint64_t>(unique_streams.size()));
  formatter.AddField("exact_alias_group_count",
                     static_cast<uint64_t>(inventory.aliases.size()));
  formatter.AddField("exact_aliased_room_count", exact_aliased_rooms);
  formatter.AddField("suffix_overlap_count", suffix_overlaps);
  formatter.AddField("interior_overlap_count", interior_overlaps);
  formatter.AddField("issue_count",
                     static_cast<uint64_t>(inventory.issues.size()));

  formatter.AddField("occupied_bytes",
                     SumIntervalBytes(inventory.occupied_intervals));
  formatter.AddField("free_bytes", SumIntervalBytes(inventory.free_intervals));
  formatter.AddField("allocatable_bytes",
                     SumIntervalBytes(inventory.allocatable_free_intervals));

  formatter.BeginArray("unique_streams");
  for (const auto& [data_pc, stream] : unique_streams) {
    formatter.BeginObject();
    formatter.AddHexField("data_pc", data_pc, 6);
    formatter.AddHexField("end_pc", stream.end_pc, 6);
    formatter.AddField("bytes", static_cast<uint64_t>(stream.end_pc) - data_pc);
    AddRoomIds(formatter, stream.room_ids);
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("exact_aliases");
  for (const auto& alias : inventory.aliases) {
    formatter.BeginObject();
    formatter.AddHexField("data_pc", alias.data_pc, 6);
    const auto unique = unique_streams.find(alias.data_pc);
    if (unique != unique_streams.end()) {
      formatter.AddHexField("end_pc", unique->second.end_pc, 6);
      formatter.AddField("bytes", static_cast<uint64_t>(unique->second.end_pc) -
                                      alias.data_pc);
    }
    AddRoomIds(formatter, alias.room_ids);
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("overlaps");
  for (const auto& overlap : inventory.overlaps) {
    formatter.BeginObject();
    formatter.AddField("kind", OverlapKindName(overlap.kind));
    formatter.BeginArray("first_room_ids");
    for (uint32_t room_id : overlap.first_room_ids) {
      formatter.AddArrayItem(absl::StrFormat("0x%03X", room_id));
    }
    formatter.EndArray();
    formatter.BeginArray("second_room_ids");
    for (uint32_t room_id : overlap.second_room_ids) {
      formatter.AddArrayItem(absl::StrFormat("0x%03X", room_id));
    }
    formatter.EndArray();
    formatter.AddHexField("begin_pc", overlap.intersection.begin, 6);
    formatter.AddHexField("end_pc", overlap.intersection.end, 6);
    formatter.AddField("bytes",
                       static_cast<uint64_t>(overlap.intersection.end) -
                           overlap.intersection.begin);
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("issues");
  for (const auto& issue : inventory.issues) {
    formatter.BeginObject();
    formatter.AddField("code", IssueCodeName(issue.code));
    formatter.AddField("room_id", absl::StrFormat("0x%03X", issue.room_id));
    formatter.AddHexField("address_pc", issue.address, 6);
    formatter.AddField("message", issue.message);
    formatter.EndObject();
  }
  formatter.EndArray();

  AddIntervals(formatter, "occupied_intervals", inventory.occupied_intervals);
  AddIntervals(formatter, "free_intervals", inventory.free_intervals);
  AddIntervals(formatter, "allocatable_intervals",
               inventory.allocatable_free_intervals);

  if (!inventory.ok()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Dungeon stream inventory contains %zu issue(s)",
                        inventory.issues.size()));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
