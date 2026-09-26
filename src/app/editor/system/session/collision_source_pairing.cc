#include "app/editor/system/session/collision_source_pairing.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"

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

}  // namespace

absl::Status SaveRomWithCollisionSource(Rom* rom, const fs::path& json_path) {
  if (rom == nullptr || !rom->is_loaded() || rom->filename().empty()) {
    return absl::FailedPreconditionError("No ROM file to save");
  }
  const fs::path rom_path(rom->filename());

  ASSIGN_OR_RETURN(const std::string new_json,
                   zelda3::DumpCustomCollisionSourceFromRom(rom));
  ASSIGN_OR_RETURN(const auto disk_json, ReadOptionalFile(json_path));

  // A byte-identical source needs no second file: save the ROM as usual.
  if (disk_json.has_value() && *disk_json == new_json) {
    Rom::SaveSettings settings;
    return rom->SaveToFile(settings);
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
    if (*locked_json != disk_source) {
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
