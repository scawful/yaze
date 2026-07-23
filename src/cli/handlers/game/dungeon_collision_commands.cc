#include "cli/handlers/game/dungeon_collision_commands.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cli/util/hex_util.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

ABSL_DECLARE_FLAG(bool, sandbox);

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

absl::StatusOr<std::filesystem::path> ResolveStableArtifactPath(
    const std::filesystem::path& path) {
  std::error_code absolute_ec;
  std::filesystem::path absolute = std::filesystem::absolute(path, absolute_ec);
  if (absolute_ec) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot normalize path %s: %s", path.string(), absolute_ec.message()));
  }
  absolute = absolute.lexically_normal();

  // Resolve every existing component once. Publication then uses this stable
  // path instead of following a caller-supplied parent symlink a second time.
  std::error_code canonical_ec;
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(absolute, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot safely resolve path %s: %s", absolute.string(),
                        canonical_ec.message()));
  }
  const std::filesystem::path resolved = canonical.lexically_normal();
  const std::filesystem::path parent = resolved.parent_path();
  std::error_code parent_ec;
  const auto parent_status = std::filesystem::status(parent, parent_ec);
  if (parent_ec || !std::filesystem::exists(parent_status) ||
      !std::filesystem::is_directory(parent_status)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Artifact parent directory must already exist: %s%s", parent.string(),
        !parent_ec ? "" : absl::StrFormat(" (%s)", parent_ec.message())));
  }
  return resolved;
}

std::filesystem::path NextArtifactTempPath(
    const std::filesystem::path& target_path) {
  static std::atomic<uint64_t> sequence{0};
  const uint64_t tick = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const uint64_t id = sequence.fetch_add(1, std::memory_order_relaxed);

  std::filesystem::path temp_name = target_path.filename();
  temp_name += absl::StrFormat(".yaze-tmp-%016x-%016x", tick, id);
  const auto parent = target_path.parent_path().empty()
                          ? std::filesystem::path(".")
                          : target_path.parent_path();
  return parent / temp_name;
}

absl::StatusOr<std::filesystem::path> WriteExclusiveArtifactTemp(
    const std::filesystem::path& target_path, absl::string_view content) {
  constexpr int kMaxCreateAttempts = 100;

  for (int attempt = 0; attempt < kMaxCreateAttempts; ++attempt) {
    const std::filesystem::path temp_path = NextArtifactTempPath(target_path);

#if defined(_WIN32)
    HANDLE file = CreateFileW(
        temp_path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
        continue;
      }
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot create temporary artifact for %s: %s", target_path.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }

    size_t written_total = 0;
    while (written_total < content.size()) {
      const size_t remaining = content.size() - written_total;
      const DWORD chunk = static_cast<DWORD>(
          std::min<size_t>(remaining, std::numeric_limits<DWORD>::max()));
      DWORD written = 0;
      if (!WriteFile(file, content.data() + written_total, chunk, &written,
                     nullptr) ||
          written == 0) {
        const DWORD error = GetLastError();
        CloseHandle(file);
        std::error_code cleanup_ec;
        std::filesystem::remove(temp_path, cleanup_ec);
        return absl::InternalError(absl::StrFormat(
            "Failed while writing temporary artifact for %s: %s",
            target_path.string(),
            std::error_code(static_cast<int>(error), std::system_category())
                .message()));
      }
      written_total += written;
    }

    if (!FlushFileBuffers(file)) {
      const DWORD error = GetLastError();
      CloseHandle(file);
      std::error_code cleanup_ec;
      std::filesystem::remove(temp_path, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary artifact for %s: %s", target_path.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
    if (!CloseHandle(file)) {
      const DWORD error = GetLastError();
      std::error_code cleanup_ec;
      std::filesystem::remove(temp_path, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to close temporary artifact for %s: %s", target_path.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
#else
    const int fd =
        open(temp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      const int error = errno;
      if (error == EEXIST) {
        continue;
      }
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot create temporary artifact for %s: %s", target_path.string(),
          std::error_code(error, std::generic_category()).message()));
    }

    size_t written_total = 0;
    while (written_total < content.size()) {
      const size_t remaining = content.size() - written_total;
      const size_t chunk = std::min<size_t>(remaining, 1024 * 1024);
      const ssize_t written = write(fd, content.data() + written_total, chunk);
      if (written < 0 && errno == EINTR) {
        continue;
      }
      if (written <= 0) {
        const int error = written < 0 ? errno : EIO;
        close(fd);
        std::error_code cleanup_ec;
        std::filesystem::remove(temp_path, cleanup_ec);
        return absl::InternalError(absl::StrFormat(
            "Failed while writing temporary artifact for %s: %s",
            target_path.string(),
            std::error_code(error, std::generic_category()).message()));
      }
      written_total += static_cast<size_t>(written);
    }

    if (fsync(fd) != 0) {
      const int error = errno;
      close(fd);
      std::error_code cleanup_ec;
      std::filesystem::remove(temp_path, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary artifact for %s: %s", target_path.string(),
          std::error_code(error, std::generic_category()).message()));
    }
    if (close(fd) != 0) {
      const int error = errno;
      std::error_code cleanup_ec;
      std::filesystem::remove(temp_path, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to close temporary artifact for %s: %s", target_path.string(),
          std::error_code(error, std::generic_category()).message()));
    }
#endif

    return temp_path;
  }

  return absl::ResourceExhaustedError(
      absl::StrFormat("Could not allocate a unique temporary artifact for %s",
                      target_path.string()));
}

absl::Status ReplaceArtifactFromTemp(const std::filesystem::path& temp_path,
                                     const std::filesystem::path& target_path) {
  std::error_code rename_ec;
  std::filesystem::rename(temp_path, target_path, rename_ec);
#if defined(_WIN32)
  if (rename_ec) {
    if (MoveFileExW(temp_path.wstring().c_str(), target_path.wstring().c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      rename_ec.clear();
    } else {
      rename_ec = std::error_code(static_cast<int>(GetLastError()),
                                  std::system_category());
    }
  }
#endif
  if (rename_ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to publish artifact %s: %s",
                        target_path.string(), rename_ec.message()));
  }
  return absl::OkStatus();
}

absl::Status ValidatePublishTarget(const std::filesystem::path& target_path) {
  std::error_code status_ec;
  const auto status = std::filesystem::symlink_status(target_path, status_ec);
  if (status_ec && status_ec != std::errc::no_such_file_or_directory) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect artifact target %s: %s",
                        target_path.string(), status_ec.message()));
  }
  if (!status_ec && std::filesystem::is_directory(status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Artifact target is a directory, not a file: %s",
                        target_path.string()));
  }
  if (!status_ec && std::filesystem::exists(status) &&
      !std::filesystem::is_regular_file(status) &&
      !std::filesystem::is_symlink(status)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Artifact target is not a replaceable file: %s", target_path.string()));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::optional<std::filesystem::path>> CreateArtifactRollbackCopy(
    const std::filesystem::path& target_path) {
  std::error_code status_ec;
  const auto status = std::filesystem::symlink_status(target_path, status_ec);
  if (status_ec == std::errc::no_such_file_or_directory ||
      (!status_ec && !std::filesystem::exists(status))) {
    return std::nullopt;
  }
  if (status_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect existing artifact %s: %s",
                        target_path.string(), status_ec.message()));
  }

  constexpr int kMaxCreateAttempts = 100;
  for (int attempt = 0; attempt < kMaxCreateAttempts; ++attempt) {
    const std::filesystem::path rollback_path =
        NextArtifactTempPath(target_path);
    std::error_code copy_ec;
    if (std::filesystem::is_symlink(status)) {
      std::filesystem::copy_symlink(target_path, rollback_path, copy_ec);
    } else {
      std::filesystem::copy_file(target_path, rollback_path,
                                 std::filesystem::copy_options::none, copy_ec);
    }
    if (!copy_ec) {
      return rollback_path;
    }
    if (copy_ec == std::errc::file_exists) {
      continue;
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot preserve existing artifact %s before publication: %s",
        target_path.string(), copy_ec.message()));
  }

  return absl::ResourceExhaustedError(
      absl::StrFormat("Could not allocate a rollback copy for artifact %s",
                      target_path.string()));
}

struct PendingArtifactPublication {
  std::filesystem::path target_path;
  std::string content;
  std::filesystem::path temp_path;
  std::optional<std::filesystem::path> rollback_path;
  bool published = false;
  bool preserve_rollback_copy = false;
};

void CleanupPendingArtifactFiles(
    const std::vector<PendingArtifactPublication>& artifacts) {
  for (const auto& artifact : artifacts) {
    std::error_code cleanup_ec;
    if (!artifact.temp_path.empty()) {
      std::filesystem::remove(artifact.temp_path, cleanup_ec);
    }
    if (artifact.rollback_path.has_value() &&
        !artifact.preserve_rollback_copy) {
      cleanup_ec.clear();
      std::filesystem::remove(*artifact.rollback_path, cleanup_ec);
    }
  }
}

absl::Status RollBackPublishedArtifacts(
    std::vector<PendingArtifactPublication>* artifacts,
    const absl::Status& publication_failure) {
  absl::Status rollback_failure = absl::OkStatus();
  for (auto it = artifacts->rbegin(); it != artifacts->rend(); ++it) {
    if (!it->published) {
      continue;
    }

    if (it->rollback_path.has_value()) {
      const absl::Status restore_status =
          ReplaceArtifactFromTemp(*it->rollback_path, it->target_path);
      if (!restore_status.ok() && rollback_failure.ok()) {
        it->preserve_rollback_copy = true;
        rollback_failure = absl::InternalError(absl::StrFormat(
            "%s; preserved the original artifact at %s",
            restore_status.message(), it->rollback_path->string()));
      } else if (!restore_status.ok()) {
        it->preserve_rollback_copy = true;
      } else if (restore_status.ok()) {
        it->rollback_path.reset();
      }
    } else {
      std::error_code remove_ec;
      if (!std::filesystem::remove(it->target_path, remove_ec) || remove_ec) {
        if (rollback_failure.ok()) {
          rollback_failure = absl::InternalError(absl::StrFormat(
              "Could not remove newly published artifact %s during rollback: "
              "%s",
              it->target_path.string(),
              remove_ec ? remove_ec.message() : "artifact was missing"));
        }
      }
    }
    it->published = false;
  }

  if (!rollback_failure.ok()) {
    return absl::DataLossError(absl::StrFormat(
        "Artifact publication failed (%s) and rollback failed (%s)",
        publication_failure.message(), rollback_failure.message()));
  }
  return publication_failure;
}

absl::Status RejectArtifactSetAliasesAfterPublication(
    const std::vector<PendingArtifactPublication>& artifacts) {
  for (size_t lhs_index = 0; lhs_index < artifacts.size(); ++lhs_index) {
    for (size_t rhs_index = lhs_index + 1; rhs_index < artifacts.size();
         ++rhs_index) {
      std::error_code equivalent_ec;
      if (std::filesystem::equivalent(artifacts[lhs_index].target_path,
                                      artifacts[rhs_index].target_path,
                                      equivalent_ec)) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Export artifact paths alias each other on this filesystem: %s "
            "and %s",
            artifacts[lhs_index].target_path.string(),
            artifacts[rhs_index].target_path.string()));
      }

      if (!equivalent_ec) {
        continue;
      }

      // A just-published target makes native case/Unicode aliases exist under
      // both spellings. If both spellings now exist but the filesystem cannot
      // compare them, fail closed and roll the publication back.
      std::error_code lhs_exists_ec;
      std::error_code rhs_exists_ec;
      const bool lhs_exists = std::filesystem::exists(
          artifacts[lhs_index].target_path, lhs_exists_ec);
      const bool rhs_exists = std::filesystem::exists(
          artifacts[rhs_index].target_path, rhs_exists_ec);
      if (lhs_exists_ec || rhs_exists_ec || (lhs_exists && rhs_exists)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Could not safely distinguish export artifact paths %s and %s: "
            "%s",
            artifacts[lhs_index].target_path.string(),
            artifacts[rhs_index].target_path.string(),
            equivalent_ec.message()));
      }
    }
  }
  return absl::OkStatus();
}

absl::Status PublishArtifactSetAtomically(
    std::vector<PendingArtifactPublication> artifacts) {
  for (const auto& artifact : artifacts) {
    const absl::Status target_status =
        ValidatePublishTarget(artifact.target_path);
    if (!target_status.ok()) {
      CleanupPendingArtifactFiles(artifacts);
      return target_status;
    }
  }

  for (auto& artifact : artifacts) {
    auto temp_or =
        WriteExclusiveArtifactTemp(artifact.target_path, artifact.content);
    if (!temp_or.ok()) {
      CleanupPendingArtifactFiles(artifacts);
      return temp_or.status();
    }
    artifact.temp_path = *temp_or;
  }

  for (auto& artifact : artifacts) {
    auto rollback_or = CreateArtifactRollbackCopy(artifact.target_path);
    if (!rollback_or.ok()) {
      CleanupPendingArtifactFiles(artifacts);
      return rollback_or.status();
    }
    artifact.rollback_path = std::move(*rollback_or);
  }

  for (auto& artifact : artifacts) {
    const absl::Status publish_status =
        ReplaceArtifactFromTemp(artifact.temp_path, artifact.target_path);
    if (!publish_status.ok()) {
      const absl::Status final_status =
          RollBackPublishedArtifacts(&artifacts, publish_status);
      CleanupPendingArtifactFiles(artifacts);
      return final_status;
    }
    artifact.temp_path.clear();
    artifact.published = true;

    const absl::Status alias_status =
        RejectArtifactSetAliasesAfterPublication(artifacts);
    if (!alias_status.ok()) {
      const absl::Status final_status =
          RollBackPublishedArtifacts(&artifacts, alias_status);
      CleanupPendingArtifactFiles(artifacts);
      return final_status;
    }
  }

  CleanupPendingArtifactFiles(artifacts);
  return absl::OkStatus();
}

absl::Status PublishResolvedTextFileAtomically(
    const std::filesystem::path& target_path, absl::string_view content) {
  std::vector<PendingArtifactPublication> artifacts;
  artifacts.push_back(PendingArtifactPublication{
      .target_path = target_path,
      .content = std::string(content),
  });
  return PublishArtifactSetAtomically(std::move(artifacts));
}

absl::Status PublishExportArtifactsAtomically(
    const std::filesystem::path& out_path, absl::string_view out_content,
    const std::optional<std::filesystem::path>& report_path,
    absl::string_view report_content) {
  // Callers pass paths resolved and ROM-checked at command start. Do not follow
  // the caller's raw path again after a parent symlink could have changed.
  std::vector<PendingArtifactPublication> artifacts;
  artifacts.push_back(PendingArtifactPublication{
      .target_path = out_path,
      .content = std::string(out_content),
  });
  if (report_path.has_value()) {
    artifacts.push_back(PendingArtifactPublication{
        .target_path = *report_path,
        .content = std::string(report_content),
    });
  }
  return PublishArtifactSetAtomically(std::move(artifacts));
}

absl::Status ValidateImportArguments(const resources::ArgumentParser& parser,
                                     absl::string_view command_name) {
  RETURN_IF_ERROR(parser.RequireArgs({"in"}));
  const auto report_path = parser.GetString("report");
  const bool has_report = report_path.has_value();
  if (has_report && report_path->empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s: --report cannot be empty", command_name));
  }
  const bool sandbox_requested =
      parser.HasFlag("sandbox") || absl::GetFlag(FLAGS_sandbox);
  if (has_report && !parser.HasFlag("dry-run")) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s: --report is supported only with --dry-run; write-mode imports "
        "must rely on command output and ROM backup verification",
        command_name));
  }
  if (has_report && sandbox_requested) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s: --report cannot be combined with --sandbox; run the reported "
        "dry-run against the source ROM",
        command_name));
  }
  if (parser.HasFlag("mock-rom") && sandbox_requested) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s: --mock-rom and --sandbox are mutually exclusive", command_name));
  }
  return absl::OkStatus();
}

absl::Status ValidateExportArguments(const resources::ArgumentParser& parser,
                                     absl::string_view command_name) {
  RETURN_IF_ERROR(parser.RequireArgs({"out"}));
  if (parser.GetString("out")->empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s: --out cannot be empty", command_name));
  }
  if (const auto report = parser.GetString("report");
      report.has_value() && report->empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s: --report cannot be empty", command_name));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::filesystem::path> NormalizedAbsolutePath(
    const std::filesystem::path& path) {
  return ResolveStableArtifactPath(path);
}

absl::StatusOr<bool> PathsAlias(const std::filesystem::path& lhs,
                                const std::filesystem::path& rhs) {
  ASSIGN_OR_RETURN(const auto normalized_lhs, NormalizedAbsolutePath(lhs));
  ASSIGN_OR_RETURN(const auto normalized_rhs, NormalizedAbsolutePath(rhs));
  if (normalized_lhs == normalized_rhs) {
    return true;
  }

  std::error_code equivalent_ec;
  const bool equivalent = std::filesystem::equivalent(
      normalized_lhs, normalized_rhs, equivalent_ec);
  if (!equivalent_ec) {
    return equivalent;
  }

  // equivalent() reports an error when either path does not exist. Lexical
  // normalization above is sufficient in that case. If both paths do exist,
  // fail closed rather than risk truncating a ROM we could not compare.
  std::error_code lhs_exists_ec;
  std::error_code rhs_exists_ec;
  const bool lhs_exists =
      std::filesystem::exists(normalized_lhs, lhs_exists_ec);
  const bool rhs_exists =
      std::filesystem::exists(normalized_rhs, rhs_exists_ec);
  if (lhs_exists_ec || rhs_exists_ec || (lhs_exists && rhs_exists)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Could not safely compare paths %s and %s: %s", normalized_lhs.string(),
        normalized_rhs.string(), equivalent_ec.message()));
  }
  return false;
}

absl::Status RejectArtifactRomAliases(
    absl::string_view option_name, const std::filesystem::path& artifact_path,
    const resources::CommandInvocationContext& invocation_context) {
  if (invocation_context.active_rom_path.has_value()) {
    ASSIGN_OR_RETURN(
        const bool aliases_active_rom,
        PathsAlias(artifact_path, *invocation_context.active_rom_path));
    if (aliases_active_rom) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s path aliases the active ROM; choose a separate artifact file: "
          "%s",
          option_name, artifact_path.string()));
    }
  }

  if (invocation_context.source_rom_path.has_value()) {
    ASSIGN_OR_RETURN(
        const bool aliases_source_rom,
        PathsAlias(artifact_path, *invocation_context.source_rom_path));
    if (aliases_source_rom) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s path aliases the %s; choose a separate artifact file: %s",
          option_name,
          invocation_context.sandbox_enabled ? "sandbox source ROM"
                                             : "source ROM",
          artifact_path.string()));
    }
  }

  return absl::OkStatus();
}

struct ResolvedExportArtifactPaths {
  std::filesystem::path out_path;
  std::optional<std::filesystem::path> report_path;

  bool operator==(const ResolvedExportArtifactPaths&) const = default;
};

absl::StatusOr<ResolvedExportArtifactPaths> ResolveExportArtifactPaths(
    const resources::ArgumentParser& parser,
    const resources::CommandInvocationContext& invocation_context) {
  const auto out_path_value = parser.GetString("out");
  if (!out_path_value.has_value() || out_path_value->empty()) {
    return absl::InvalidArgumentError("--out must name an artifact file");
  }

  ASSIGN_OR_RETURN(const auto out_path,
                   ResolveStableArtifactPath(*out_path_value));
  RETURN_IF_ERROR(
      RejectArtifactRomAliases("--out", out_path, invocation_context));

  ResolvedExportArtifactPaths resolved_paths{.out_path = out_path};

  const auto report_path_value = parser.GetString("report");
  if (!report_path_value.has_value()) {
    return resolved_paths;
  }
  if (report_path_value->empty()) {
    return absl::InvalidArgumentError("--report must name an artifact file");
  }

  ASSIGN_OR_RETURN(const auto report_path,
                   ResolveStableArtifactPath(*report_path_value));
  RETURN_IF_ERROR(
      RejectArtifactRomAliases("--report", report_path, invocation_context));

  ASSIGN_OR_RETURN(const bool artifacts_alias,
                   PathsAlias(out_path, report_path));
  if (artifacts_alias) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--out and --report paths alias each other; choose separate artifact "
        "files: %s",
        out_path.string()));
  }

  resolved_paths.report_path = report_path;
  return resolved_paths;
}

absl::StatusOr<std::optional<std::filesystem::path>> ResolveReportArtifactPath(
    const resources::ArgumentParser& parser, const Rom* rom) {
  const auto report_path_value = parser.GetString("report");
  if (!report_path_value.has_value() || report_path_value->empty()) {
    return std::nullopt;
  }

  ASSIGN_OR_RETURN(const auto report_path,
                   ResolveStableArtifactPath(*report_path_value));
  if (rom != nullptr && !rom->filename().empty()) {
    ASSIGN_OR_RETURN(
        const bool aliases_active_rom,
        PathsAlias(report_path, std::filesystem::path(rom->filename())));
    if (aliases_active_rom) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "--report path aliases the active ROM; choose a separate report "
          "file: %s",
          report_path.string()));
    }
  }

  return report_path;
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

void AddStructuredError(resources::OutputFormatter& formatter,
                        const absl::Status& status) {
  formatter.AddField("status", "error");
  formatter.BeginObject("error");
  formatter.AddField("code", StatusCodeName(status.code()));
  formatter.AddField("message", std::string(status.message()));
  formatter.EndObject();
}

absl::Status FinalizeWithReport(
    const std::optional<std::filesystem::path>& resolved_report_path,
    json report, const absl::Status& status) {
  if (!status.ok()) {
    report["status"] = "error";
    report["error"] = json{
        {"code", StatusCodeName(status.code())},
        {"message", std::string(status.message())},
    };
  }

  absl::Status report_status = absl::OkStatus();
  if (resolved_report_path.has_value()) {
    report_status = PublishResolvedTextFileAtomically(*resolved_report_path,
                                                      report.dump(2) + "\n");
  }
  if (!report_status.ok()) {
    if (status.ok()) {
      return report_status;
    }
    return absl::InternalError(
        absl::StrFormat("Command failed (%s) and report write failed (%s)",
                        status.message(), report_status.message()));
  }

  return status;
}

absl::Status FinalizeExportWithReport(
    const ResolvedExportArtifactPaths& artifact_paths, json report,
    const absl::Status& status) {
  return FinalizeWithReport(artifact_paths.report_path, std::move(report),
                            status);
}

json BuildPreflightJson(
    const zelda3::OracleRomSafetyPreflightResult& preflight) {
  json out;
  out["ok"] = preflight.ok();
  json errors = json::array();
  for (const auto& err : preflight.errors) {
    json e;
    e["code"] = err.code;
    e["message"] = err.message;
    e["status_code"] = StatusCodeName(err.status_code);
    if (err.room_id >= 0) {
      e["room_id"] = absl::StrFormat("0x%02X", err.room_id);
    }
    errors.push_back(std::move(e));
  }
  out["errors"] = std::move(errors);
  return out;
}

template <typename Serializer>
absl::Status SerializeAndPersistImport(Rom* rom,
                                       const resources::ArgumentParser& parser,
                                       json* report, Serializer&& serializer) {
  ScopedRomTransaction transaction(*rom);

  const absl::Status write_status = std::forward<Serializer>(serializer)();
  if (!write_status.ok()) {
    (*report)["write_error"] = std::string(write_status.message());
    return write_status;
  }
  (*report)["write_status"] = "success";

  // Unit-test and embedding callers can explicitly request an in-memory write.
  // A sandbox ROM is file-backed, so it follows the normal save path and writes
  // only its sandbox copy.
  if (parser.HasFlag("mock-rom")) {
    (*report)["save_status"] = "mock-rom-skipped";
    transaction.Commit();
    return absl::OkStatus();
  }

  Rom::SaveSettings save_settings;
  save_settings.require_backup = true;
  const absl::Status save_status = rom->SaveToFile(save_settings);
  if (!save_status.ok()) {
    (*report)["save_error"] = std::string(save_status.message());
    return save_status;
  }

  (*report)["save_status"] = "saved";
  transaction.Commit();
  return absl::OkStatus();
}

std::vector<int> RequiredCollisionRoomsForImportedWaterFillZones(
    const std::vector<zelda3::WaterFillZoneEntry>& zones) {
  constexpr std::array<int, 2> kD4RoomIdsRequiringCollision = {0x25, 0x27};

  std::unordered_set<int> imported_rooms;
  imported_rooms.reserve(zones.size());
  for (const auto& zone : zones) {
    imported_rooms.insert(zone.room_id);
  }

  std::vector<int> required_rooms;
  for (int room_id : kD4RoomIdsRequiringCollision) {
    if (imported_rooms.contains(room_id)) {
      required_rooms.push_back(room_id);
    }
  }
  return required_rooms;
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
  formatter.AddField(
      "filter_mode",
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
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status DungeonExportCustomCollisionJsonCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  auto artifact_paths_or =
      ResolveExportArtifactPaths(parser, invocation_context);
  if (!artifact_paths_or.ok()) {
    AddStructuredError(formatter, artifact_paths_or.status());
    return artifact_paths_or.status();
  }
  const ResolvedExportArtifactPaths artifact_paths = *artifact_paths_or;

  json report = BuildBaseReport(GetName(), /*dry_run=*/false);
  std::string out_path;
  std::string exported_json;
  int requested_rooms = 0;
  int exported_room_count = 0;
  const absl::Status status = [&]() -> absl::Status {
    ASSIGN_OR_RETURN(const auto room_ids, ParseRoomSelection(parser));
    out_path = parser.GetString("out").value();
    requested_rooms = static_cast<int>(room_ids.size());
    report["out_path"] = out_path;
    report["requested_rooms"] = requested_rooms;

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
        exported_json,
        zelda3::DumpCustomCollisionRoomsToJsonString(export_rooms));
    exported_room_count = static_cast<int>(export_rooms.size());
    report["exported_rooms"] = exported_room_count;

    return absl::OkStatus();
  }();

  // Resolve again after the export body as defense-in-depth against path
  // identity changes between initial validation and publication.
  auto final_artifact_paths_or =
      ResolveExportArtifactPaths(parser, invocation_context);
  absl::Status final_path_status = final_artifact_paths_or.status();
  if (final_artifact_paths_or.ok() &&
      *final_artifact_paths_or != artifact_paths) {
    final_path_status = absl::FailedPreconditionError(
        "Export artifact path identity changed during the command; no "
        "artifact was published");
  }

  absl::Status final_status;
  if (!status.ok()) {
    // Do not let a coincident path re-resolution failure shadow the original
    // business error. An unsafe final path suppresses report publication, but
    // callers still receive the command failure that stopped the export.
    final_status = final_path_status.ok()
                       ? FinalizeExportWithReport(artifact_paths,
                                                  std::move(report), status)
                       : status;
  } else if (!final_path_status.ok()) {
    final_status = final_path_status;
  } else {
    final_status = PublishExportArtifactsAtomically(
        artifact_paths.out_path, exported_json, artifact_paths.report_path,
        report.dump(2) + "\n");
  }
  if (!final_status.ok()) {
    AddStructuredError(formatter, final_status);
    return final_status;
  }

  formatter.BeginObject("Custom Collision Export");
  formatter.AddField("out_path", out_path);
  formatter.AddField("requested_rooms", requested_rooms);
  formatter.AddField("exported_rooms", exported_room_count);
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status DungeonImportCustomCollisionJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto report_path_or = ResolveReportArtifactPath(parser, rom);
  if (!report_path_or.ok()) {
    AddStructuredError(formatter, report_path_or.status());
    return report_path_or.status();
  }
  const std::optional<std::filesystem::path> report_path = *report_path_or;

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

    zelda3::OracleRomSafetyPreflightOptions preflight_options;
    preflight_options.require_water_fill_reserved_region = true;
    preflight_options.require_custom_collision_write_support = true;
    preflight_options.validate_water_fill_table = true;
    preflight_options.validate_custom_collision_maps = true;
    const auto preflight =
        zelda3::RunOracleRomSafetyPreflight(rom, preflight_options);
    report["preflight"] = BuildPreflightJson(preflight);
    if (!preflight.ok()) {
      return preflight.ToStatus();
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

    // Keep room storage on the heap: zelda3::Room is large enough that a full
    // `kNumberOfRooms` array can overflow stack frames in optimized builds.
    std::vector<zelda3::Room> rooms;
    rooms.reserve(zelda3::kNumberOfRooms);
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      rooms.emplace_back(room_id, rom, nullptr);
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
      RETURN_IF_ERROR(SerializeAndPersistImport(rom, parser, &report, [&]() {
        return zelda3::SaveAllCollision(rom, absl::MakeSpan(rooms));
      }));
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
    if (!dry_run) {
      formatter.AddField("write_status",
                         report.value("write_status", std::string("unknown")));
      formatter.AddField("save_status",
                         report.value("save_status", std::string("unknown")));
    }
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(report_path, std::move(report), status);
}

absl::Status DungeonExportWaterFillJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext invocation_context;
  if (rom != nullptr && !rom->filename().empty()) {
    invocation_context.source_rom_path = std::filesystem::path(rom->filename());
    invocation_context.active_rom_path = std::filesystem::path(rom->filename());
  }
  return ExecuteWithContext(rom, parser, formatter, invocation_context);
}

absl::Status DungeonExportWaterFillJsonCommandHandler::ExecuteWithContext(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation_context) {
  auto artifact_paths_or =
      ResolveExportArtifactPaths(parser, invocation_context);
  if (!artifact_paths_or.ok()) {
    AddStructuredError(formatter, artifact_paths_or.status());
    return artifact_paths_or.status();
  }
  const ResolvedExportArtifactPaths artifact_paths = *artifact_paths_or;

  json report = BuildBaseReport(GetName(), /*dry_run=*/false);
  std::string out_path;
  std::string exported_json;
  int requested_rooms = 0;
  int exported_zone_count = 0;
  const absl::Status status = [&]() -> absl::Status {
    out_path = parser.GetString("out").value();
    ASSIGN_OR_RETURN(const auto room_ids, ParseRoomSelection(parser));
    requested_rooms = static_cast<int>(room_ids.size());
    report["out_path"] = out_path;
    report["requested_rooms"] = requested_rooms;

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

    ASSIGN_OR_RETURN(exported_json,
                     zelda3::DumpWaterFillZonesToJsonString(filtered));
    exported_zone_count = static_cast<int>(filtered.size());
    report["exported_zones"] = exported_zone_count;

    return absl::OkStatus();
  }();

  // Resolve again after the export body as defense-in-depth against path
  // identity changes between initial validation and publication.
  auto final_artifact_paths_or =
      ResolveExportArtifactPaths(parser, invocation_context);
  absl::Status final_path_status = final_artifact_paths_or.status();
  if (final_artifact_paths_or.ok() &&
      *final_artifact_paths_or != artifact_paths) {
    final_path_status = absl::FailedPreconditionError(
        "Export artifact path identity changed during the command; no "
        "artifact was published");
  }

  absl::Status final_status;
  if (!status.ok()) {
    // Do not let a coincident path re-resolution failure shadow the original
    // business error. An unsafe final path suppresses report publication, but
    // callers still receive the command failure that stopped the export.
    final_status = final_path_status.ok()
                       ? FinalizeExportWithReport(artifact_paths,
                                                  std::move(report), status)
                       : status;
  } else if (!final_path_status.ok()) {
    final_status = final_path_status;
  } else {
    final_status = PublishExportArtifactsAtomically(
        artifact_paths.out_path, exported_json, artifact_paths.report_path,
        report.dump(2) + "\n");
  }
  if (!final_status.ok()) {
    AddStructuredError(formatter, final_status);
    return final_status;
  }

  formatter.BeginObject("Water Fill Export");
  formatter.AddField("out_path", out_path);
  formatter.AddField("requested_rooms", requested_rooms);
  formatter.AddField("exported_zones", exported_zone_count);
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status DungeonImportWaterFillJsonCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto report_path_or = ResolveReportArtifactPath(parser, rom);
  if (!report_path_or.ok()) {
    AddStructuredError(formatter, report_path_or.status());
    return report_path_or.status();
  }
  const std::optional<std::filesystem::path> report_path = *report_path_or;

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
    const auto required_collision_rooms =
        RequiredCollisionRoomsForImportedWaterFillZones(zones);
    if (!required_collision_rooms.empty()) {
      json required_rooms_json = json::array();
      for (int room_id : required_collision_rooms) {
        required_rooms_json.push_back(absl::StrFormat("0x%02X", room_id));
      }
      report["required_collision_rooms"] = std::move(required_rooms_json);
    }

    zelda3::OracleRomSafetyPreflightOptions preflight_options;
    preflight_options.require_water_fill_reserved_region = true;
    preflight_options.require_custom_collision_write_support = false;
    preflight_options.validate_water_fill_table = true;
    preflight_options.validate_custom_collision_maps = true;
    preflight_options.room_ids_requiring_custom_collision =
        required_collision_rooms;
    const auto preflight =
        zelda3::RunOracleRomSafetyPreflight(rom, preflight_options);
    report["preflight"] = BuildPreflightJson(preflight);
    if (!preflight.ok()) {
      return preflight.ToStatus();
    }

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
      RETURN_IF_ERROR(SerializeAndPersistImport(rom, parser, &report, [&]() {
        return zelda3::WriteWaterFillTable(rom, zones);
      }));
    }

    formatter.BeginObject("Water Fill Import");
    formatter.AddField("in_path", in_path);
    formatter.AddField("mode", dry_run ? "dry-run" : "write");
    formatter.AddField("strict_masks", strict_masks);
    formatter.AddField("zone_count", static_cast<int>(zones.size()));
    formatter.AddField("normalized_masks", normalized_masks);
    if (!dry_run) {
      formatter.AddField("write_status",
                         report.value("write_status", std::string("unknown")));
      formatter.AddField("save_status",
                         report.value("save_status", std::string("unknown")));
    }
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }();

  return FinalizeWithReport(report_path, std::move(report), status);
}

absl::Status DungeonImportCustomCollisionJsonCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateImportArguments(parser, GetName());
}

absl::Status DungeonExportCustomCollisionJsonCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateExportArguments(parser, GetName());
}

absl::Status DungeonExportWaterFillJsonCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateExportArguments(parser, GetName());
}

absl::Status DungeonImportWaterFillJsonCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidateImportArguments(parser, GetName());
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
