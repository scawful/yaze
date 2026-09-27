#include "cli/handlers/game/dungeon_census_commands.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/util/hex_util.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_census_vanilla_fingerprints.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli::handlers {

namespace {

namespace fs = std::filesystem;

// YazeProject::Open installs its manifest in the global label provider;
// restore the previous binding when done.
class ScopedLabelManifestRestore {
 public:
  explicit ScopedLabelManifestRestore(zelda3::ResourceLabelProvider& provider)
      : provider_(provider), previous_(provider.hack_manifest()) {}
  ~ScopedLabelManifestRestore() { provider_.SetHackManifest(previous_); }
  ScopedLabelManifestRestore(const ScopedLabelManifestRestore&) = delete;
  ScopedLabelManifestRestore& operator=(const ScopedLabelManifestRestore&) =
      delete;

 private:
  zelda3::ResourceLabelProvider& provider_;
  const core::HackManifest* previous_ = nullptr;
};

// --project accepts the project's code folder (with Docs/Dev/Planning/
// dungeons.json and Roms/hack_manifest.json) or a .yaze project file.
absl::StatusOr<fs::path> ResolveCodeFolder(const std::string& project_arg) {
  const fs::path path(project_arg);
  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    return path;
  }
  if (!fs::exists(path, ec)) {
    return absl::NotFoundError(
        absl::StrFormat("--project path does not exist: %s", project_arg));
  }
  project::YazeProject project;
  absl::Status status;
  {
    ScopedLabelManifestRestore restore(zelda3::GetResourceLabels());
    try {
      status = project.Open(project_arg);
    } catch (const std::exception& error) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Cannot load project '%s': %s", project_arg, error.what()));
    }
  }
  if (!status.ok()) {
    return status;
  }
  if (project.code_folder.empty()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Project '%s' has no code folder; pass the folder with "
                        "Docs/Dev/Planning/dungeons.json instead",
                        project_arg));
  }
  return fs::path(project.GetAbsolutePath(project.code_folder));
}

absl::Status LoadProjectData(const fs::path& code_folder,
                             core::HackManifest* manifest) {
  // Manifest first: LoadFromFile resets the registry.
  for (const fs::path candidate : {code_folder / "Roms" / "hack_manifest.json",
                                   code_folder / "hack_manifest.json"}) {
    std::error_code ec;
    if (fs::exists(candidate, ec)) {
      auto status = manifest->LoadFromFile(candidate.string());
      if (!status.ok()) {
        return status;
      }
      break;
    }
  }
  return manifest->LoadProjectRegistry(code_folder.string());
}

std::string Hex(int room_id) {
  return absl::StrFormat("0x%02X", room_id);
}

std::string OwnerId(const zelda3::RoomCensus& census,
                    const zelda3::RoomCensusEntry& entry) {
  return entry.owner_index >= 0 ? census.owners[entry.owner_index].id : "-";
}

std::string FormatTable(const zelda3::RoomCensus& census,
                        const std::vector<int>& rooms) {
  std::string out;
  out += absl::StrFormat(
      "Room census: %d free, %d reclaimable, %d in use (%d orphan), %d "
      "interior\n",
      census.free_count, census.reclaimable_count, census.in_use_count,
      census.orphan_count, census.interior_count);
  out += absl::StrFormat("Owners: %s\n", census.owners_from_project
                                             ? "project dungeons.json"
                                             : "derived from entrances");
  out += absl::StrFormat("Vanilla baseline: %s\n",
                         census.has_vanilla_baseline
                             ? census.vanilla_baseline_source
                             : "none (reclaimable needs one)");
  if (!census.free_clusters.empty()) {
    out += absl::StrFormat("Largest free block: %s\n",
                           census.free_clusters.front().summary);
  }
  out += absl::StrFormat("%-6s %-12s %-6s %s\n", "ROOM", "STATUS", "OWNER",
                         "REASONS");
  for (int room_id : rooms) {
    const auto& entry = census.rooms[room_id];
    std::string status = zelda3::RoomCensusStatusName(entry.status);
    if (entry.orphan) {
      status += "*";
    }
    out += absl::StrFormat("%-6s %-12s %-6s %s\n", Hex(room_id), status,
                           OwnerId(census, entry),
                           absl::StrJoin(entry.reasons, " | "));
  }
  out += "(* = orphan: in use, but nothing reaches it)\n";
  return out;
}

}  // namespace

absl::Status DungeonRoomCensusCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto status = parser.GetString("status"); status.has_value()) {
    const std::string lowered = absl::AsciiStrToLower(*status);
    if (lowered != "free" && lowered != "reclaimable" && lowered != "in_use") {
      return absl::InvalidArgumentError(
          "--status must be free, reclaimable or in_use");
    }
  }
  return absl::OkStatus();
}

absl::Status DungeonRoomCensusCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Generator for the built-in vanilla fingerprint table.
  if (auto out_path = parser.GetString("emit-vanilla-fingerprints");
      out_path.has_value()) {
    auto fingerprints = zelda3::ComputeRoomFingerprints(rom);
    if (!fingerprints.ok()) {
      return fingerprints.status();
    }
    std::ofstream file(*out_path, std::ios::binary | std::ios::trunc);
    if (!file) {
      return absl::PermissionDeniedError(
          absl::StrFormat("Cannot write %s", *out_path));
    }
    file << zelda3::FormatVanillaFingerprintTable(
        *fingerprints,
        absl::StrFormat("%s (%d bytes)",
                        fs::path(rom->filename()).filename().string(),
                        static_cast<int>(rom->size())));
    formatter.AddField("status", "success");
    formatter.AddField("written", *out_path);
    return absl::OkStatus();
  }

  zelda3::RoomCensusOptions options;
  core::HackManifest manifest;
  if (auto project_arg = parser.GetString("project"); project_arg.has_value()) {
    auto code_folder = ResolveCodeFolder(*project_arg);
    if (!code_folder.ok()) {
      return code_folder.status();
    }
    if (auto status = LoadProjectData(*code_folder, &manifest); !status.ok()) {
      return status;
    }
    options.project = &manifest.project_registry();
    options.manifest = &manifest;
  }
  Rom vanilla_rom;
  if (auto vanilla_path = parser.GetString("vanilla");
      vanilla_path.has_value()) {
    auto status = vanilla_rom.LoadFromFile(*vanilla_path);
    if (!status.ok()) {
      return status;
    }
    options.vanilla_rom = &vanilla_rom;
  }

  auto census_or = zelda3::ComputeRoomCensus(rom, options);
  if (!census_or.ok()) {
    return census_or.status();
  }
  const zelda3::RoomCensus& census = *census_or;

  std::vector<int> rooms;
  std::optional<zelda3::RoomCensusStatus> status_filter;
  if (auto status = parser.GetString("status"); status.has_value()) {
    const std::string lowered = absl::AsciiStrToLower(*status);
    status_filter = lowered == "free" ? zelda3::RoomCensusStatus::kFree
                    : lowered == "reclaimable"
                        ? zelda3::RoomCensusStatus::kReclaimable
                        : zelda3::RoomCensusStatus::kInUse;
  }
  if (auto room_arg = parser.GetString("room"); room_arg.has_value()) {
    int room_id = -1;
    if (!util::ParseHexString(*room_arg, &room_id) || room_id < 0 ||
        room_id >= zelda3::kRoomCensusRoomCount) {
      return absl::InvalidArgumentError(
          "--room must be a hex room id 0x000-0x127");
    }
    rooms.push_back(room_id);
  } else {
    for (const auto& entry : census.rooms) {
      if (!status_filter.has_value() || entry.status == *status_filter) {
        rooms.push_back(entry.room_id);
      }
    }
  }

  const std::string format =
      absl::AsciiStrToLower(parser.GetString("format").value_or("json"));
  if (format == "table" || format == "text") {
    formatter.AddRawText(FormatTable(census, rooms));
    return absl::OkStatus();
  }

  formatter.AddField("rom", fs::path(rom->filename()).filename().string());
  formatter.AddField("owners_source", census.owners_from_project
                                          ? "project"
                                          : "derived_from_entrances");
  formatter.AddField("vanilla_baseline", census.has_vanilla_baseline
                                             ? census.vanilla_baseline_source
                                             : "none");
  formatter.AddField("expanded_entrance_tables",
                     census.expanded_entrance_tables);
  formatter.BeginObject("counts");
  formatter.AddField("free", census.free_count);
  formatter.AddField("reclaimable", census.reclaimable_count);
  formatter.AddField("in_use", census.in_use_count);
  formatter.AddField("orphan", census.orphan_count);
  formatter.AddField("interior", census.interior_count);
  formatter.EndObject();

  auto add_status_list = [&](const char* key, auto predicate) {
    formatter.BeginArray(key);
    for (const auto& entry : census.rooms) {
      if (predicate(entry)) {
        formatter.AddArrayItem(Hex(entry.room_id));
      }
    }
    formatter.EndArray();
  };
  add_status_list("free_rooms", [](const auto& e) {
    return e.status == zelda3::RoomCensusStatus::kFree;
  });
  add_status_list("reclaimable_rooms", [](const auto& e) {
    return e.status == zelda3::RoomCensusStatus::kReclaimable;
  });
  add_status_list("orphan_rooms", [](const auto& e) { return e.orphan; });

  formatter.AddField("largest_free_block",
                     census.free_clusters.empty()
                         ? std::string()
                         : census.free_clusters.front().summary);
  formatter.BeginArray("free_blocks");
  for (const auto& cluster : census.free_clusters) {
    formatter.BeginObject();
    formatter.AddField("summary", cluster.summary);
    formatter.BeginArray("rooms");
    for (int room : cluster.rooms) {
      formatter.AddArrayItem(Hex(room));
    }
    formatter.EndArray();
    formatter.BeginArray("contiguous_rooms");
    for (int room : cluster.core_rooms) {
      formatter.AddArrayItem(Hex(room));
    }
    formatter.EndArray();
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("owners");
  for (const auto& owner : census.owners) {
    formatter.BeginObject();
    formatter.AddField("id", owner.id);
    formatter.AddField("name", owner.name);
    formatter.AddField("interior", owner.interior);
    formatter.AddField("from_project", owner.from_project);
    formatter.AddField("rooms", owner.room_count);
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("rooms");
  for (int room_id : rooms) {
    const auto& entry = census.rooms[room_id];
    formatter.BeginObject();
    formatter.AddField("room", Hex(room_id));
    formatter.AddField("status", zelda3::RoomCensusStatusName(entry.status));
    formatter.AddField("owner", OwnerId(census, entry));
    formatter.AddField("owner_name", entry.owner_index >= 0
                                         ? census.owners[entry.owner_index].name
                                         : std::string());
    formatter.AddField("interior", entry.interior);
    formatter.AddField("reached", entry.reached);
    formatter.AddField("empty", entry.empty);
    formatter.AddField("orphan", entry.orphan);
    formatter.AddField("objects", entry.object_count);
    formatter.AddField("sprites", entry.sprite_count);
    formatter.AddRawJsonField(
        "vanilla_similarity",
        entry.vanilla_similarity < 0.0f
            ? "null"
            : absl::StrFormat("%.2f", entry.vanilla_similarity));
    formatter.BeginArray("reasons");
    for (const auto& reason : entry.reasons) {
      formatter.AddArrayItem(reason);
    }
    formatter.EndArray();
    formatter.BeginArray("references");
    for (const auto& ref : entry.references) {
      formatter.BeginObject();
      formatter.AddField("kind", zelda3::RoomReferenceKindName(ref.kind));
      formatter.AddField("strong", ref.strong);
      if (ref.from_room >= 0) {
        formatter.AddField("from", Hex(ref.from_room));
      }
      if (ref.entrance_id >= 0) {
        formatter.AddHexField("entrance", ref.entrance_id, 2);
      }
      formatter.AddField("detail", ref.detail);
      formatter.EndObject();
    }
    formatter.EndArray();
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
