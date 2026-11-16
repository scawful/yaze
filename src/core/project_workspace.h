#ifndef YAZE_CORE_PROJECT_WORKSPACE_H_
#define YAZE_CORE_PROJECT_WORKSPACE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "core/project.h"

namespace yaze::project {

// Project descriptor for a path given on the command line: a .yazeproj bundle
// resolves to <bundle>/project.yaze; any other path is returned unchanged.
std::filesystem::path ProjectFileForPath(const std::filesystem::path& path);

// Read only the [workspace] app_data_folder / proposal_folder / sandbox_folder
// keys from a project.yaze file (INI). Values are returned resolved against the
// project file's directory. Missing file or keys yield empty strings.
ProjectWorkspaceFolders ReadProjectWorkspaceFolders(
    const std::filesystem::path& project_file);

// Read only the [rom] editor_control key from a project.yaze file (INI).
// Missing file or key yields false.
bool ReadProjectEditorControl(const std::filesystem::path& project_file);

// Whether yaze starts with editor control: an explicit --editor_control flag
// (on or off) wins; otherwise the project setting decides.
inline bool ResolveEditorControl(bool flag_specified, bool flag_value,
                                 bool project_setting) {
  return flag_specified ? flag_value : project_setting;
}

// Environment variables a project's workspace folders map to.
inline constexpr char kAppDataDirEnv[] = "YAZE_APP_DATA_DIR";
inline constexpr char kProposalRootEnv[] = "YAZE_PROPOSAL_ROOT";
inline constexpr char kSandboxRootEnv[] = "YAZE_SANDBOX_ROOT";

// Before any app-data lookup (logs, recents, settings), point the three yaze
// root variables at the folders the project names, so opening a project
// directly gives the same isolation a launcher script would. A variable that
// is already set wins. Folders are created. Returns the (name, value) pairs
// that were set, for logging.
std::vector<std::pair<std::string, std::string>>
ApplyProjectWorkspaceEnvironment(const std::filesystem::path& rom_or_project);

// Replace the single `"romChecksum": "<hex>"` value in a bundle manifest.json
// with new_sha1. Writes atomically (temp file + rename). Returns
// FailedPrecondition, leaving the file untouched, unless exactly one match is
// found; NotFound when the bundle has no manifest.json.
absl::Status UpdateBundleManifestRomChecksum(
    const std::filesystem::path& bundle_dir, const std::string& new_sha1);

// The hash a project with adopt_hash_on_open should record when it opens, or
// "" when expected_hash already holds it. Matches the Oracle launcher's
// refresh_rom_hash.py: the ROM SHA-1 always wins, so a CRC32 that a yaze save
// wrote is rewritten as the SHA-1 even when the ROM did not change. CRC32 is
// used only when no SHA-1 is known. An empty expected_hash stays empty.
std::string RomHashToRecordOnOpen(const std::string& expected_hash,
                                  const std::string& rom_sha1,
                                  const std::string& rom_crc32);

// Atomically update or insert a single `<key>=<value>` entry inside `[<section>]`
// of an INI-format `project.yaze` descriptor without re-serializing comments,
// timestamps, blank lines, or unrelated sections. Returns the full updated text.
absl::StatusOr<std::string> UpdateProjectDescriptorKey(
    const std::filesystem::path& project_file, absl::string_view section,
    absl::string_view key, absl::string_view value);

// Structured diagnostic details collected when a ROM's identity fails to match
// `[rom] expected_hash`. Shared between the GUI attention modal and CLI errors.
struct RomIdentityMismatchInfo {
  std::string expected_hash;
  std::string loaded_sha1;
  std::string loaded_crc32;
  std::string loaded_hash;
  std::string rom_path;
  std::string rom_mtime;
  std::string newest_backup;
};

RomIdentityMismatchInfo InspectRomIdentityMismatch(
    const uint8_t* rom_data, size_t rom_size, absl::string_view rom_filename,
    const YazeProject& project);

std::string FormatRomIdentityMismatchFields(
    const RomIdentityMismatchInfo& info);

}  // namespace yaze::project

#endif  // YAZE_CORE_PROJECT_WORKSPACE_H_
