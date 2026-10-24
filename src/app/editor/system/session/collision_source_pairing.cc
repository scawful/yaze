#include "app/editor/system/session/collision_source_pairing.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;

absl::StatusOr<std::optional<std::string>> ReadOptionalFile(
    const fs::path& path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    if (ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot inspect %s: %s", path.string(), ec.message()));
    }
    return std::optional<std::string>();
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot read %s", path.string()));
  }
  return std::optional<std::string>(
      std::string(std::istreambuf_iterator<char>(in), {}));
}

// The collision source that the ROM currently on disk would produce.
absl::StatusOr<std::string> CollisionSourceOfRomBytes(
    const std::string& rom_bytes) {
  Rom disk_rom;
  Rom::LoadOptions options;  // Load as yaze does, without resource labels.
  options.load_resource_labels = false;
  RETURN_IF_ERROR(disk_rom.LoadFromData(
      std::vector<uint8_t>(rom_bytes.begin(), rom_bytes.end()), options));
  return zelda3::DumpCustomCollisionSourceFromRom(&disk_rom);
}

// Ignore presentation differences only. The import parser accepts aliases,
// skips some invalid entries, and coalesces duplicate offsets, so it is not a
// suitable source-binding guard. Comparing parsed documents retains every
// array entry (including explicit floor zero), field, and numeric type.
absl::StatusOr<bool> CollisionSourcesMatch(const std::string& source,
                                           const std::string& generated,
                                           const fs::path& source_path) {
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError("JSON support not enabled");
#else
  using json = nlohmann::json;
  if (source.find('\0') != std::string::npos) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Invalid collision source %s: embedded NUL byte",
                        source_path.string()));
  }
  bool duplicate_key = false;
  std::vector<std::unordered_set<std::string>> object_keys;
  try {
    auto parsed =
        json::parse(source, [&](int, json::parse_event_t event, json& value) {
          if (event == json::parse_event_t::object_start) {
            object_keys.emplace_back();
          } else if (event == json::parse_event_t::object_end) {
            object_keys.pop_back();
          } else if (event == json::parse_event_t::key) {
            if (!object_keys.back().insert(value.get<std::string>()).second) {
              duplicate_key = true;
            }
          }
          return true;
        });
    if (duplicate_key) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Invalid collision source %s: duplicate JSON object key",
          source_path.string()));
    }
    // JSON equality coerces 1.0 to integer 1. Canonical serialization preserves
    // that distinction, while normalizing whitespace and object-key ordering.
    return parsed.dump() == json::parse(generated).dump();
  } catch (const json::exception& error) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Invalid collision source %s: %s", source_path.string(), error.what()));
  }
#endif
}

}  // namespace

absl::Status ValidateRomCollisionSourceBinding(Rom* replacement_rom,
                                               const fs::path& json_path) {
  if (replacement_rom == nullptr || !replacement_rom->is_loaded()) {
    return absl::FailedPreconditionError("No replacement ROM loaded");
  }
  ASSIGN_OR_RETURN(const auto source, ReadOptionalFile(json_path));
  if (!source.has_value()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Configured collision source %s is missing; restore it before "
        "swapping the project ROM. The project binding was not changed.",
        json_path.string()));
  }
  ASSIGN_OR_RETURN(const std::string replacement_source,
                   zelda3::DumpCustomCollisionSourceFromRom(replacement_rom));
  ASSIGN_OR_RETURN(
      const bool source_matches,
      CollisionSourcesMatch(*source, replacement_source, json_path));
  if (!source_matches) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Replacement ROM %s does not match configured collision source %s. "
        "Swap ROM does not publish collision changes. Select a ROM matching "
        "this source. For collision edits, use Save ROM on the bound project "
        "and then Build Project. The saved copy and project binding were kept.",
        replacement_rom->filename(), json_path.string()));
  }
  return absl::OkStatus();
}

absl::Status SaveRomWithCollisionSource(Rom* rom, const fs::path& json_path) {
  if (rom == nullptr || !rom->is_loaded() || rom->filename().empty()) {
    return absl::FailedPreconditionError("No ROM file to save");
  }
  RETURN_IF_ERROR(rom->CheckLayoutWritable());
  const fs::path rom_path(rom->filename());

  ASSIGN_OR_RETURN(const std::string new_json,
                   zelda3::DumpCustomCollisionSourceFromRom(rom));
  ASSIGN_OR_RETURN(const auto disk_json, ReadOptionalFile(json_path));

  // Preserve the user's formatting when collision data did not change.
  if (disk_json.has_value()) {
    ASSIGN_OR_RETURN(const bool source_matches,
                     CollisionSourcesMatch(*disk_json, new_json, json_path));
    if (source_matches) {
      Rom::SaveSettings settings;
      return rom->SaveToFile(settings);
    }
  }

  const core::SourceArtifactPublisherLabels labels{
      .subject = "ROM and custom collision source",
      .published_file = "custom collision source"};
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {rom_path, json_path}, labels));

  // Read both files under the lock so no other writer can slip in between.
  ASSIGN_OR_RETURN(const auto disk_rom, ReadOptionalFile(rom_path));
  if (!disk_rom.has_value()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot pair a custom collision source with %s: the ROM file does "
        "not exist yet",
        rom_path.string()));
  }
  ASSIGN_OR_RETURN(const auto locked_json, ReadOptionalFile(json_path));
  if (locked_json != disk_json) {
    return absl::AbortedError(absl::StrFormat(
        "%s changed while saving; nothing was written. Save again.",
        json_path.string()));
  }
  if (locked_json.has_value()) {
    ASSIGN_OR_RETURN(const std::string disk_source,
                     CollisionSourceOfRomBytes(*disk_rom));
    ASSIGN_OR_RETURN(
        const bool source_matches,
        CollisionSourcesMatch(*locked_json, disk_source, json_path));
    if (!source_matches) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s does not match the custom collision in %s. It was changed "
          "outside yaze, or the two were already out of step. Nothing was "
          "written; reconcile them first (Oracle: "
          "Scripts/Generate/validate_custom_collision_source.py).",
          json_path.string(), rom_path.string()));
    }
  }

  const std::vector<uint8_t>& bytes = rom->vector();
  std::vector<core::SourceArtifactUpdate> updates;
  updates.push_back(core::SourceArtifactUpdate{
      .target = rom_path,
      .before = *disk_rom,
      .after = std::string(bytes.begin(), bytes.end())});
  updates.push_back(core::SourceArtifactUpdate{
      .target = json_path, .before = locked_json, .after = new_json});
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *lock, std::move(updates), core::ComputeSourceArtifactSha256(*disk_rom)));
  rom->set_dirty(false);
  return absl::OkStatus();
}

}  // namespace yaze::editor
