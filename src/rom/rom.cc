#include "rom.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "core/source_artifact_publisher.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "app/platform/wasm/wasm_collaboration.h"
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace yaze {

namespace {

// ============================================================================
// Copier Header Detection and Removal
// ============================================================================

/// SNES ROM images are multiples of 32 KB; a copier header adds 0x200.
constexpr size_t kCopierHeaderAlignment = 0x8000;
constexpr size_t kLoRomInternalHeader = 0x7FC0;
constexpr size_t kHiRomInternalHeader = 0xFFC0;

// A plausible SNES internal header: map mode byte of the form 0x2X and a
// checksum/complement pair that XORs to 0xFFFF.
bool HasInternalHeaderAt(const uint8_t* data, size_t size, size_t offset) {
  if (offset + 0x20 > size) {
    return false;
  }
  const uint8_t map_mode = data[offset + 0x15];
  const uint16_t complement = data[offset + 0x1C] | (data[offset + 0x1D] << 8);
  const uint16_t checksum = data[offset + 0x1E] | (data[offset + 0x1F] << 8);
  return (map_mode & 0xE0) == 0x20 && (complement ^ checksum) == 0xFFFF;
}

bool HasInternalHeader(const uint8_t* data, size_t size, size_t base) {
  return HasInternalHeaderAt(data, size, base + kLoRomInternalHeader) ||
         HasInternalHeaderAt(data, size, base + kHiRomInternalHeader);
}

void MaybeStripCopierHeader(std::vector<uint8_t>& rom_data,
                            std::vector<uint8_t>& header,
                            CopierHeaderReason& reason) {
  header.clear();
  reason = DetectCopierHeader(rom_data.data(), rom_data.size());
  if (reason == CopierHeaderReason::kNone) {
    return;
  }
  header.assign(rom_data.begin(), rom_data.begin() + kCopierHeaderSize);
  rom_data.erase(rom_data.begin(), rom_data.begin() + kCopierHeaderSize);
  LOG_INFO("Rom", "Stripped copier header (%s); ROM size 0x%zX",
           CopierHeaderReasonName(reason), rom_data.size());
}

std::string MakeSafeTimestamp(std::time_t now_c) {
  std::string timestamp = std::ctime(&now_c);
  timestamp.erase(std::remove(timestamp.begin(), timestamp.end(), '\n'),
                  timestamp.end());
  std::replace(timestamp.begin(), timestamp.end(), ' ', '_');

  // Keep backup/save-new filenames valid across platforms (especially
  // Windows, where ':' and several other characters are not allowed).
  for (char& ch : timestamp) {
    switch (ch) {
      case '<':
      case '>':
      case ':':
      case '"':
      case '/':
      case '\\':
      case '|':
      case '?':
      case '*':
        ch = '-';
        break;
      default:
        break;
    }
  }
  return timestamp;
}

absl::StatusOr<std::filesystem::path> GetAvailableBackupPath(
    const std::filesystem::path& requested_path) {
  std::filesystem::path candidate = requested_path;
  for (int suffix = 1; suffix <= 1000; ++suffix) {
    std::error_code exists_ec;
    const bool exists = std::filesystem::exists(candidate, exists_ec);
    if (exists_ec) {
      return absl::InternalError(absl::StrCat(
          "Could not inspect required ROM backup path: ", candidate.string(),
          ": ", exists_ec.message()));
    }
    if (!exists) {
      return candidate;
    }
    candidate = requested_path.string() + "_" + std::to_string(suffix);
  }

  return absl::ResourceExhaustedError(
      "Could not allocate a unique required ROM backup path");
}

#if !defined(__EMSCRIPTEN__)
void BestEffortFsyncFile(const std::filesystem::path& path);
void BestEffortFsyncParentDir(const std::filesystem::path& file_path);
#endif

absl::Status CreateRequiredBackup(
    const std::filesystem::path& source_path,
    const std::filesystem::path& requested_backup_path) {
  auto backup_path_or = GetAvailableBackupPath(requested_backup_path);
  if (!backup_path_or.ok()) {
    return backup_path_or.status();
  }

  const std::filesystem::path backup_path = *backup_path_or;
  std::filesystem::path temp_path = backup_path;
  temp_path += ".tmp";

  std::error_code copy_ec;
  const bool copied = std::filesystem::copy_file(
      source_path, temp_path, std::filesystem::copy_options::none, copy_ec);
  if (!copied || copy_ec) {
    std::error_code cleanup_ec;
    std::filesystem::remove(temp_path, cleanup_ec);
    return absl::FailedPreconditionError(absl::StrCat(
        "Could not create required ROM backup: ", source_path.string(), " -> ",
        backup_path.string(), ": ",
        copy_ec ? copy_ec.message() : "copy did not complete"));
  }

#if !defined(__EMSCRIPTEN__)
  BestEffortFsyncFile(temp_path);
#endif

  std::error_code rename_ec;
  std::filesystem::rename(temp_path, backup_path, rename_ec);
  if (rename_ec) {
    std::error_code cleanup_ec;
    std::filesystem::remove(temp_path, cleanup_ec);
    return absl::FailedPreconditionError(absl::StrCat(
        "Could not finalize required ROM backup: ", backup_path.string(), ": ",
        rename_ec.message()));
  }

#if !defined(__EMSCRIPTEN__)
  BestEffortFsyncParentDir(backup_path);
#endif

  return absl::OkStatus();
}

// Match the exclusive staging contract used by CLI artifact publication.
// Keep the name independent of the destination basename so long ROM filenames
// still fit the filesystem's component limit. Exclusive creation, rather than
// the nonce alone, establishes ownership; never reopen this path to write it.
std::mutex g_rom_staging_failure_mutex;
bool g_fail_rom_staging_for_testing = false;
std::function<void()> g_before_rom_staging_failure;

absl::StatusOr<std::filesystem::path> WriteExclusiveRomTemp(
    const std::filesystem::path& target_path,
    const std::vector<uint8_t>& bytes) {
  bool fail_staging = false;
  std::function<void()> before_failure;
  {
    std::lock_guard<std::mutex> guard(g_rom_staging_failure_mutex);
    fail_staging = g_fail_rom_staging_for_testing;
    before_failure = g_before_rom_staging_failure;
  }
  if (fail_staging) {
    // Copy the callback while locked, then run it unlocked so a callback may
    // reset the fault without deadlocking the save worker.
    if (before_failure)
      before_failure();
    return absl::InternalError(
        "Could not create temp ROM file: staging failure injected for testing");
  }
  static std::atomic<uint64_t> sequence{0};
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto tick = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto id = sequence.fetch_add(1, std::memory_order_relaxed);
    const auto temp_path =
        target_path.parent_path() /
        absl::StrFormat(".yaze-rom-%016x-%016x.tmp", tick, id);
#if defined(_WIN32)
    HANDLE file =
        CreateFileW(temp_path.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
        continue;
      }
      return absl::InternalError(absl::StrCat(
          "Could not create temp ROM file: ", temp_path.string(), ": ",
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
#else
    int file = open(temp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (file < 0) {
      const int error = errno;
      if (error == EEXIST) {
        continue;
      }
      return absl::InternalError(absl::StrCat(
          "Could not create temp ROM file: ", temp_path.string(), ": ",
          std::error_code(error, std::generic_category()).message()));
    }
#endif
    // Only installed after successful exclusive creation. On failure we may
    // remove our staging file, never a pre-existing destination or .tmp file.
    absl::Cleanup cleanup = [&] {
#if defined(_WIN32)
      if (file != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(file);
      }
#else
      if (file >= 0) {
        (void)close(file);
      }
#endif
      std::error_code ec;
      std::filesystem::remove(temp_path, ec);
    };

    size_t offset = 0;
    while (offset < bytes.size()) {
      const size_t chunk = std::min<size_t>(bytes.size() - offset, 1024 * 1024);
#if defined(_WIN32)
      DWORD written = 0;
      const bool ok = WriteFile(file, bytes.data() + offset,
                                static_cast<DWORD>(chunk), &written, nullptr);
      const int error =
          ok ? ERROR_WRITE_FAULT : static_cast<int>(GetLastError());
      if (!ok || written == 0) {
        return absl::InternalError(absl::StrCat(
            "Error while writing ROM file: ", temp_path.string(), ": ",
            std::error_code(error, std::system_category()).message()));
      }
#else
      const ssize_t written = write(file, bytes.data() + offset, chunk);
      if (written < 0 && errno == EINTR) {
        continue;
      }
      if (written <= 0) {
        const int error = written < 0 ? errno : EIO;
        return absl::InternalError(absl::StrCat(
            "Error while writing ROM file: ", temp_path.string(), ": ",
            std::error_code(error, std::generic_category()).message()));
      }
#endif
      offset += static_cast<size_t>(written);
    }

#if defined(_WIN32)
    if (!FlushFileBuffers(file)) {
      const int error = static_cast<int>(GetLastError());
      return absl::InternalError(absl::StrCat(
          "Could not flush temp ROM file: ", temp_path.string(), ": ",
          std::error_code(error, std::system_category()).message()));
    }
    const bool closed = CloseHandle(file);
    const int close_error = closed ? 0 : static_cast<int>(GetLastError());
    file = INVALID_HANDLE_VALUE;
    const auto close_category = &std::system_category();
#else
#if !defined(__EMSCRIPTEN__)
    int flush_result;
    do {
      flush_result = fsync(file);
    } while (flush_result != 0 && errno == EINTR);
    if (flush_result != 0) {
      const int error = errno;
      return absl::InternalError(absl::StrCat(
          "Could not flush temp ROM file: ", temp_path.string(), ": ",
          std::error_code(error, std::generic_category()).message()));
    }
#endif
    const int close_result = close(file);
    const int close_error = close_result == 0 ? 0 : errno;
    file = -1;
    const auto close_category = &std::generic_category();
#endif
    if (close_error != 0) {
      return absl::InternalError(absl::StrCat(
          "Could not close temp ROM file: ", temp_path.string(), ": ",
          std::error_code(close_error, *close_category).message()));
    }
    std::move(cleanup).Cancel();
    return temp_path;
  }
  return absl::ResourceExhaustedError(
      "Could not allocate a unique temp ROM file");
}

#ifdef __EMSCRIPTEN__
inline void MaybeBroadcastChange(uint32_t offset,
                                 const std::vector<uint8_t>& old_bytes,
                                 const std::vector<uint8_t>& new_bytes) {
  if (new_bytes.empty())
    return;
  auto& collab = app::platform::GetWasmCollaborationInstance();
  if (!collab.IsConnected() || collab.IsApplyingRemoteChange()) {
    return;
  }
  (void)collab.BroadcastChange(offset, old_bytes, new_bytes);
}
#endif

#if !defined(__EMSCRIPTEN__)
void BestEffortFsyncFile(const std::filesystem::path& path) {
#if defined(_WIN32)
  // FlushFileBuffers requires GENERIC_WRITE access.
  HANDLE handle =
      CreateFileW(path.wstring().c_str(), GENERIC_WRITE,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  (void)FlushFileBuffers(handle);
  (void)CloseHandle(handle);
#else
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return;
  }
  (void)fsync(fd);
  (void)close(fd);
#endif
}

void BestEffortFsyncParentDir(const std::filesystem::path& file_path) {
#if defined(_WIN32)
  (void)file_path;
  // Best-effort only; Windows directory fsync is not portable here.
#else
  std::filesystem::path dir_path = file_path.parent_path();
  if (dir_path.empty()) {
    dir_path = ".";
  }
  int fd = open(dir_path.c_str(), O_RDONLY);
  if (fd < 0) {
    return;
  }
  (void)fsync(fd);
  (void)close(fd);
#endif
}
#endif  // !defined(__EMSCRIPTEN__)

}  // namespace

CopierHeaderReason DetectCopierHeader(const uint8_t* data, size_t size) {
  if (data == nullptr || size <= kCopierHeaderSize) {
    return CopierHeaderReason::kNone;
  }
  const size_t remainder = size % kCopierHeaderAlignment;
  if (remainder == 0) {
    return CopierHeaderReason::kNone;
  }
  const bool header_in_place = HasInternalHeader(data, size, 0);
  const bool header_after_skip =
      HasInternalHeader(data, size, kCopierHeaderSize);
  if (remainder == kCopierHeaderSize) {
    // Standard layout. Still keep a headerless image with 0x200 trailing
    // bytes, recognizable by an internal header valid only in place.
    return header_in_place && !header_after_skip
               ? CopierHeaderReason::kNone
               : CopierHeaderReason::kSizeAlignment;
  }
  // Nonstandard sizes (trailing data after the ROM) need positive evidence.
  return header_after_skip && !header_in_place
             ? CopierHeaderReason::kInternalHeaderOffset
             : CopierHeaderReason::kNone;
}

const char* CopierHeaderReasonName(CopierHeaderReason reason) {
  switch (reason) {
    case CopierHeaderReason::kNone:
      return "none";
    case CopierHeaderReason::kSizeAlignment:
      return "size_alignment";
    case CopierHeaderReason::kInternalHeaderOffset:
      return "internal_header_offset";
  }
  return "none";
}

void Rom::SetStagingFailureForTesting(bool fail,
                                      std::function<void()> before_failure) {
  std::lock_guard<std::mutex> guard(g_rom_staging_failure_mutex);
  g_fail_rom_staging_for_testing = fail;
  if (fail)
    g_before_rom_staging_failure = std::move(before_failure);
  else
    g_before_rom_staging_failure = {};
}

rom::LyraLayoutInspection Rom::InspectLyraLayout() const {
  auto result = rom::InspectLyraLayout(rom_data_.data(), rom_data_.size());
  if (lyra_attributed_at_load_ &&
      result.presence == rom::LyraLayoutPresence::kAbsent) {
    result.presence = rom::LyraLayoutPresence::kInvalid;
    result.write_supported = false;
    result.error = "Loaded Lyra descriptor was removed or damaged";
  }
  return result;
}

absl::Status Rom::CheckLayoutWritable() const {
  if (read_only_scope_depth_ != 0) {
    return absl::PermissionDeniedError(
        "ROM write refused by read_only project policy");
  }
  const auto layout = InspectLyraLayout();
  if (layout.presence == rom::LyraLayoutPresence::kAbsent) {
    return absl::OkStatus();
  }
  return absl::FailedPreconditionError(
      layout.presence == rom::LyraLayoutPresence::kValid
          ? (layout.a0_terrain_write_supported
                 ? "Lyra A0 terrain requires dedicated Save As; generic save "
                   "refused"
                 : "Lyra overworld ROM layout is inspection-only; save refused")
          : absl::StrCat("Invalid Lyra overworld ROM layout: ", layout.error,
                         "; save refused"));
}

absl::Status Rom::CapturePrivateLyraReservation() {
  if (private_lyra_attached_)
    return ValidateLyraPrivateReservation();
  const auto contract = core::PrivateLyraExactSaveReservation();
  const auto hash = core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(rom_data_.data()), rom_data_.size()));
  if (hash != contract.source_rom_sha256)
    return absl::OkStatus();
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  for (uint32_t base : contract.dictionary_bases_pc)
    ranges.emplace_back(base, base + 0x67E0);
  // Native Tile16 definitions, native pointer rows, the supplemental stream
  // transfer and old A0 diagnostic slots are all immutable in this phase.
  // Native full-load, packed decoder/relocation operands, and sub-overlay
  // consumers pinned by the exact source ownership proof.
  ranges.emplace_back(0x1759D, 0x175D2);
  ranges.emplace_back(0x17691, 0x177CB);
  ranges.emplace_back(0x177E3, 0x17818);
  ranges.emplace_back(0x78000, 0x80000);
  ranges.emplace_back(0x1E8000, 0x1F0000);
  ranges.emplace_back(0x132000, 0x132400);
  ranges.emplace_back(contract.stream_pc_range[0], contract.stream_pc_range[1]);
  for (uint32_t table : {0x1794Du, 0x17B2Du}) {
    ranges.emplace_back(table, table + 160 * 3);
    for (int map = 0; map < 160; ++map) {
      ASSIGN_OR_RETURN(uint32_t pointer, ReadLong(table + map * 3));
      const uint32_t pc = SnesToPc(pointer);
      ASSIGN_OR_RETURN(auto decoded,
                       gfx::lc_lz2::DecompressExact(
                           rom_data_.data(), rom_data_.size(), pc, 256, true));
      if (decoded.data.size() != 256 ||
          pc + decoded.compressed_size > rom_data_.size())
        return absl::FailedPreconditionError(
            "Private Lyra native stream is invalid");
      ranges.emplace_back(pc, pc + decoded.compressed_size);
    }
  }
  std::sort(ranges.begin(), ranges.end());
  std::vector<std::pair<uint32_t, uint32_t>> merged;
  for (auto range : ranges) {
    if (!merged.empty() && range.first <= merged.back().second)
      merged.back().second = std::max(merged.back().second, range.second);
    else
      merged.push_back(range);
  }
  auto fence = std::make_shared<rom::WriteFence>();
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> preimages;
  uint32_t cursor = 0;
  for (auto [begin, end] : merged) {
    if (end > rom_data_.size())
      return absl::FailedPreconditionError("Private Lyra range outside ROM");
    if (cursor < begin)
      RETURN_IF_ERROR(fence->Allow(cursor, begin, "outside private terrain"));
    preimages.emplace_back(begin,
                           std::vector<uint8_t>(rom_data_.begin() + begin,
                                                rom_data_.begin() + end));
    cursor = end;
  }
  if (cursor < rom_data_.size())
    RETURN_IF_ERROR(
        fence->Allow(cursor, rom_data_.size(), "outside private terrain"));
  private_lyra_preimages_ = std::move(preimages);
  private_lyra_fence_ = std::move(fence);
  private_lyra_attached_ = true;
  return absl::OkStatus();
}

absl::Status Rom::AttachLyraPrivateReservation(
    const core::HackManifest& manifest) {
  if (!manifest.lyra_private_reservation())
    return ValidateLyraPrivateReservation();
  if (!manifest.loaded())
    return absl::FailedPreconditionError("Private Lyra manifest is not loaded");
  if (*manifest.lyra_private_reservation() !=
      core::PrivateLyraExactSaveReservation())
    return absl::FailedPreconditionError(
        "Private Lyra reservation contract mismatch");
  if (private_lyra_attached_)
    return ValidateLyraPrivateReservation();
  RETURN_IF_ERROR(CapturePrivateLyraReservation());
  if (!private_lyra_attached_)
    return absl::FailedPreconditionError(
        "Private Lyra reservation source ROM hash mismatch");
  return absl::OkStatus();
}

absl::Status Rom::ValidatePrivateLyraImage(
    const std::vector<uint8_t>& image) const {
  if (!private_lyra_attached_)
    return absl::OkStatus();
  if (image.size() != 0x400000) {
    return absl::FailedPreconditionError(
        "Private Lyra exact-save ROM size changed");
  }
  for (const auto& [pc, bytes] : private_lyra_preimages_) {
    if (pc > image.size() || bytes.size() > image.size() - pc ||
        !std::equal(bytes.begin(), bytes.end(), image.begin() + pc))
      return absl::FailedPreconditionError(
          "Private Lyra reservation protected terrain changed; native save "
          "refused");
  }
  return absl::OkStatus();
}

absl::Status Rom::ValidateLyraPrivateReservation() const {
  return ValidatePrivateLyraImage(rom_data_);
}

absl::Status Rom::CheckPrivateLyraWrite(uint32_t pc, const uint8_t* bytes,
                                        size_t size) const {
  if (!private_lyra_attached_ || size == 0)
    return absl::OkStatus();
  // Check only changed bytes. Exact no-op serializers stay compatible with
  // existing fences; shared packed nibbles cannot change a protected neighbor.
  for (size_t i = 0; i < size; ++i) {
    if (rom_data_[pc + i] != bytes[i])
      RETURN_IF_ERROR(
          private_lyra_fence_->Check(pc + i, 1, "private native terrain"));
  }
  return absl::OkStatus();
}

Rom::Rom(const Rom& other)
    : private_lyra_attached_(other.private_lyra_attached_),
      private_lyra_preimages_(other.private_lyra_preimages_),
      private_lyra_fence_(other.private_lyra_fence_),
      size_(other.size_),
      title_(other.title_),
      filename_(other.filename_),
      short_name_(other.short_name_),
      rom_data_(other.rom_data_),
      lyra_terrain_baseline_(other.lyra_terrain_baseline_),
      copier_header_(other.copier_header_),
      copier_header_reason_(other.copier_header_reason_),
      lyra_attributed_at_load_(other.lyra_attributed_at_load_),
      resource_label_manager_(other.resource_label_manager_),
      dirty_(other.dirty_),
      object_tile_revision_(other.object_tile_revision_) {
  // Write fences and read-only guards are instance-local scope state. A new
  // Rom must not inherit active scopes on `other`, including dry-run copies.
}

Rom& Rom::operator=(const Rom& other) {
  if (this == &other) {
    return *this;
  }

  if (!private_lyra_attached_ && other.private_lyra_attached_) {
    private_lyra_attached_ = true;
    private_lyra_preimages_ = other.private_lyra_preimages_;
    private_lyra_fence_ = other.private_lyra_fence_;
  }
  const uint64_t previous_revision = object_tile_revision_;
  size_ = other.size_;
  title_ = other.title_;
  filename_ = other.filename_;
  short_name_ = other.short_name_;
  rom_data_ = other.rom_data_;
  lyra_terrain_baseline_ = other.lyra_terrain_baseline_;
  copier_header_ = other.copier_header_;
  copier_header_reason_ = other.copier_header_reason_;
  lyra_attributed_at_load_ = other.lyra_attributed_at_load_;
  resource_label_manager_ = other.resource_label_manager_;
  dirty_ = other.dirty_;

  // Preserve this Rom's active write fences and read-only scopes rather than
  // importing instance-local scope state from `other`.

  object_tile_revision_ =
      std::max(previous_revision, other.object_tile_revision_);
  AdvanceObjectTileRevision();
  return *this;
}

Rom& Rom::operator=(Rom&& other) {
  // Rom historically has copy-on-rvalue assignment semantics. Keep the source
  // intact while ensuring in-place session replacement uses the same strictly
  // monotonic destination generation as lvalue assignment.
  return operator=(static_cast<const Rom&>(other));
}

absl::Status Rom::LoadFromFile(const std::string& filename,
                               const LoadOptions& options) {
  if (filename.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `filename` is empty.");
  }

  // Attached reloads validate and load one bounded snapshot. Never reopen a
  // second image after validating the first, and commit file identity last.
  if (private_lyra_attached_) {
    std::ifstream candidate_file(filename, std::ios::binary | std::ios::ate);
    if (!candidate_file)
      return absl::NotFoundError("Could not open ROM: " + filename);
    const auto length = candidate_file.tellg();
    if (length < 32768 || length > 16 * 1024 * 1024) {
      return absl::InvalidArgumentError(
          "Attached ROM file must be between 32KB and 16MB");
    }
    std::vector<uint8_t> candidate(static_cast<size_t>(length));
    candidate_file.seekg(0, std::ios::beg);
    if (!candidate_file.read(reinterpret_cast<char*>(candidate.data()),
                             length)) {
      return absl::DataLossError(
          "Could not read complete attached ROM snapshot");
    }
    RETURN_IF_ERROR(LoadFromData(candidate, options));
#ifdef __EMSCRIPTEN__
    filename_ = filename;
#else
    filename_ = std::filesystem::absolute(filename).string();
#endif
    short_name_ = filename_.substr(filename_.find_last_of("/\\") + 1);
    if (options.load_resource_labels) {
      resource_label_manager_.LoadLabels(
          absl::StrFormat("%s.labels", filename));
    }
    return absl::OkStatus();
  }

#ifdef __EMSCRIPTEN__
  filename_ = filename;
  std::ifstream test_file(filename_, std::ios::binary);
  if (!test_file.is_open()) {
    return absl::NotFoundError(absl::StrCat(
        "ROM file does not exist or cannot be opened: ", filename_));
  }
  test_file.seekg(0, std::ios::end);
  size_ = test_file.tellg();
  test_file.close();

  if (size_ < 32768) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "ROM file too small (%zu bytes), minimum is 32KB", size_));
  }
#else
  if (!std::filesystem::exists(filename)) {
    return absl::NotFoundError(
        absl::StrCat("ROM file does not exist: ", filename));
  }
  filename_ = std::filesystem::absolute(filename).string();
#endif
  short_name_ = filename_.substr(filename_.find_last_of("/\\") + 1);

  std::ifstream file(filename_, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open ROM file: ", filename_));
  }

#ifndef __EMSCRIPTEN__
  try {
    size_ = std::filesystem::file_size(filename_);
    if (size_ < 32768) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "ROM file too small (%zu bytes), minimum is 32KB", size_));
    }
  } catch (...) {
    file.seekg(0, std::ios::end);
    size_ = file.tellg();
  }
#endif

  // ALttP ROMs are <= 4MB (with an SMC header ~4MB+512); reject anything far
  // larger so a mis-selected file cannot force a huge up-front allocation.
  constexpr size_t kMaxRomSize = 16 * 1024 * 1024;  // 16 MB
  if (size_ > kMaxRomSize) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "ROM file too large (%zu bytes), maximum is 16MB", size_));
  }

  try {
    rom_data_.resize(size_);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(rom_data_.data()), size_);
  } catch (const std::bad_alloc& e) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Failed to allocate memory for ROM (%zu bytes)", size_));
  }

  file.close();

  copier_header_.clear();
  copier_header_reason_ = CopierHeaderReason::kNone;
  if (options.strip_header) {
    MaybeStripCopierHeader(rom_data_, copier_header_, copier_header_reason_);
  }
  size_ = rom_data_.size();

  lyra_attributed_at_load_ =
      rom::InspectLyraLayout(rom_data_.data(), rom_data_.size()).presence !=
      rom::LyraLayoutPresence::kAbsent;
  CaptureLyraTerrainBaseline();

  if (options.load_resource_labels) {
    resource_label_manager_.LoadLabels(absl::StrFormat("%s.labels", filename));
  }

  // Parse SNES Header for Title
  if (rom_data_.size() >= 0x8000) {
    // Check LoROM (0x7FC0) vs HiROM (0xFFC0)
    // Simple heuristic: Z3 is LoROM
    size_t header_offset = 0x7FC0;
    if (rom_data_.size() >= 0x10000) {
      // Compute checksums to verify?
      // For now default to LoROM
    }

    if (header_offset + 21 <= rom_data_.size()) {
      char buffer[22] = {0};
      for (int i = 0; i < 21; ++i) {
        uint8_t c = rom_data_[header_offset + i];
        buffer[i] = (c >= 32 && c <= 126) ? c : ' ';
      }
      title_ = std::string(buffer);
      // Trim trailing spaces safely
      auto last_non_space = title_.find_last_not_of(' ');
      if (last_non_space == std::string::npos) {
        title_.clear();
      } else {
        title_.erase(last_non_space + 1);
      }
    }
  }

  AdvanceObjectTileRevision();
  RETURN_IF_ERROR(CapturePrivateLyraReservation());
  return absl::OkStatus();
}

absl::Status Rom::LoadFromData(const std::vector<uint8_t>& data,
                               const LoadOptions& options) {
  if (data.empty()) {
    return absl::InvalidArgumentError(
        "Could not load ROM: parameter `data` is empty.");
  }
  if (private_lyra_attached_) {
    auto candidate = data;
    std::vector<uint8_t> header;
    CopierHeaderReason reason = CopierHeaderReason::kNone;
    if (options.strip_header)
      MaybeStripCopierHeader(candidate, header, reason);
    RETURN_IF_ERROR(ValidatePrivateLyraImage(candidate));
  }
  rom_data_ = data;

  copier_header_.clear();
  copier_header_reason_ = CopierHeaderReason::kNone;
  if (options.strip_header) {
    MaybeStripCopierHeader(rom_data_, copier_header_, copier_header_reason_);
  }
  size_ = rom_data_.size();

  lyra_attributed_at_load_ =
      rom::InspectLyraLayout(rom_data_.data(), rom_data_.size()).presence !=
      rom::LyraLayoutPresence::kAbsent;
  CaptureLyraTerrainBaseline();

  // Parse SNES Header for Title
  if (rom_data_.size() >= 0x8000) {
    size_t header_offset = 0x7FC0;
    if (header_offset + 21 <= rom_data_.size()) {
      char buffer[22] = {0};
      for (int i = 0; i < 21; ++i) {
        uint8_t c = rom_data_[header_offset + i];
        buffer[i] = (c >= 32 && c <= 126) ? c : ' ';
      }
      title_ = std::string(buffer);
      auto last_non_space = title_.find_last_not_of(' ');
      if (last_non_space == std::string::npos) {
        title_.clear();
      } else {
        title_.erase(last_non_space + 1);
      }
    }
  }

  AdvanceObjectTileRevision();
  RETURN_IF_ERROR(CapturePrivateLyraReservation());
  return absl::OkStatus();
}

void Rom::CaptureLyraTerrainBaseline() {
  lyra_terrain_baseline_.clear();
  const auto layout = InspectLyraLayout();
  if (layout.presence == rom::LyraLayoutPresence::kValid &&
      layout.a0_terrain_write_supported) {
    lyra_terrain_baseline_ = rom_data_;
  }
}

absl::Status Rom::SaveLyraA0TerrainAs(const std::array<uint16_t, 256>& tiles,
                                      const std::string& filename) {
  if (private_lyra_attached_)
    return absl::FailedPreconditionError(
        "Private Lyra reservation forbids alternate A0 save route");
  if (read_only_scope_depth_ != 0) {
    return absl::PermissionDeniedError(
        "ROM write refused by read_only project policy");
  }
#ifdef __EMSCRIPTEN__
  return absl::UnimplementedError(
      "Lyra terrain Save As requires native storage");
#else
  if (filename.empty() || lyra_terrain_baseline_.empty() ||
      rom_data_ != lyra_terrain_baseline_ || !write_fence_stack_.empty()) {
    return absl::FailedPreconditionError(
        "Lyra terrain Save As requires the unchanged loaded ROM snapshot");
  }
  std::error_code path_ec;
  const auto target = std::filesystem::weakly_canonical(filename, path_ec);
  if (path_ec) {
    return absl::InvalidArgumentError("Cannot resolve Lyra Save As target");
  }
  if (!filename_.empty()) {
    const auto source = std::filesystem::weakly_canonical(filename_, path_ec);
    if (path_ec) {
      return absl::InvalidArgumentError("Cannot resolve Lyra source ROM path");
    }
    const bool equivalent =
        std::filesystem::equivalent(source, target, path_ec);
    if (source == target || (!path_ec && equivalent)) {
      return absl::FailedPreconditionError(
          "Lyra terrain requires Save As to a different backing file");
    }
  }
  ASSIGN_OR_RETURN(auto staged,
                   rom::SerializeLyraA0Terrain(lyra_terrain_baseline_, tiles));
  Rom candidate(*this);
  candidate.rom_data_ = std::move(staged);
  SaveSettings settings;
  settings.filename = filename;
  settings.require_backup = true;
  RETURN_IF_ERROR(candidate.SaveToFileUnchecked(settings));
  candidate.filename_ = filename;
  candidate.short_name_ = std::filesystem::path(filename).filename().string();
  candidate.CaptureLyraTerrainBaseline();
  *this = candidate;
  return absl::OkStatus();
#endif
}

absl::Status Rom::SaveToFile(const SaveSettings& settings) {
  RETURN_IF_ERROR(CheckLayoutWritable());
  RETURN_IF_ERROR(ValidateLyraPrivateReservation());
  return SaveToFileUnchecked(settings);
}

absl::Status Rom::SaveToFileUnchecked(const SaveSettings& settings) {
  RETURN_IF_ERROR(ValidateLyraPrivateReservation());
  if (rom_data_.empty()) {
    return absl::InternalError("ROM data is empty.");
  }

  std::string filename = settings.filename;
  if (filename.empty()) {
    filename = filename_;
  }

  // Backup modes must reason about the actual destination, including the
  // timestamped path selected by save_new.
  if (settings.save_new) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto filename_no_ext = filename.substr(0, filename.find_last_of("."));
    filename =
        absl::StrCat(filename_no_ext, "_", MakeSafeTimestamp(now_c), ".sfc");
  }

  if (settings.require_backup) {
    const std::filesystem::path target_path(filename);
    std::error_code exists_ec;
    const bool target_exists = std::filesystem::exists(target_path, exists_ec);
    if (exists_ec) {
      return absl::FailedPreconditionError(absl::StrCat(
          "Could not inspect required ROM backup target: ", filename, ": ",
          exists_ec.message()));
    }

    // A strict backup protects the file that is about to be replaced, not the
    // ROM's original load path. A new target has no previous bytes to protect.
    if (target_exists) {
      auto now = std::chrono::system_clock::now();
      auto now_c = std::chrono::system_clock::to_time_t(now);
      std::string backup_filename =
          absl::StrCat(filename, "_backup_", MakeSafeTimestamp(now_c));
      RETURN_IF_ERROR(CreateRequiredBackup(target_path, backup_filename));
    }
  } else if (settings.backup) {
    try {
      const std::filesystem::path target_path(filename);
      // Best-effort backups protect the file about to be replaced. A new
      // destination has no previous bytes to preserve.
      if (std::filesystem::exists(target_path)) {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::string backup_filename =
            absl::StrCat(filename, "_backup_", MakeSafeTimestamp(now_c));
        std::filesystem::copy(
            target_path, backup_filename,
            std::filesystem::copy_options::overwrite_existing);
      }
    } catch (const std::filesystem::filesystem_error& e) {
      LOG_WARN("Rom", "Could not create backup: %s", e.what());
    }
  }

  // A copier header stripped at load goes back in front of the data, so the
  // file keeps its layout (dropping it would shift every file offset).
  std::vector<uint8_t> headered;
  if (!copier_header_.empty()) {
    headered.reserve(copier_header_.size() + rom_data_.size());
    headered.insert(headered.end(), copier_header_.begin(),
                    copier_header_.end());
    headered.insert(headered.end(), rom_data_.begin(), rom_data_.end());
  }
  const std::vector<uint8_t>& file_bytes =
      copier_header_.empty() ? rom_data_ : headered;

  // Write only through an exclusively created staging handle, then replace
  // the destination. A pre-existing .tmp file/link must never be truncated.
  const std::filesystem::path target_path(filename);
  ASSIGN_OR_RETURN(const std::filesystem::path temp_path,
                   WriteExclusiveRomTemp(target_path, file_bytes));

  std::error_code rename_ec;
  std::filesystem::rename(temp_path, target_path, rename_ec);
#if defined(_WIN32)
  // Windows may reject std::filesystem::rename when the destination exists.
  // Replace it without deleting the original first, so a failed replacement
  // does not leave the target missing.
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
    std::error_code rm_ec;
    std::filesystem::remove(temp_path, rm_ec);
    return absl::InternalError(absl::StrCat(
        "Failed to move temp ROM into place: ", rename_ec.message()));
  }

#if !defined(__EMSCRIPTEN__)
  // Best-effort fsync the parent dir so the rename is durable.
  BestEffortFsyncParentDir(target_path);
#endif

  dirty_ = false;
  return absl::OkStatus();
}

void Rom::PushWriteFence(rom::WriteFence* fence) {
  if (fence == nullptr) {
    return;
  }
  write_fence_stack_.push_back(fence);
}

void Rom::PopWriteFence(rom::WriteFence* fence) {
  if (fence == nullptr) {
    return;
  }
  if (!write_fence_stack_.empty() && write_fence_stack_.back() == fence) {
    write_fence_stack_.pop_back();
    return;
  }

  // Defensive: avoid leaving a stale fence active if call sites mismatch.
  for (auto it = write_fence_stack_.rbegin(); it != write_fence_stack_.rend();
       ++it) {
    if (*it == fence) {
      write_fence_stack_.erase(std::next(it).base());
      LOG_WARN("Rom", "Popped non-top write fence (mismatched scope)");
      return;
    }
  }
  LOG_WARN("Rom", "PopWriteFence called for unknown fence");
}

absl::StatusOr<uint8_t> Rom::ReadByte(int offset) const {
  if (offset < 0 || offset >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Offset %d out of range (size: %d)", offset, rom_data_.size()));
  }
  return rom_data_[offset];
}

absl::StatusOr<uint16_t> Rom::ReadWord(int offset) const {
  if (offset < 0 || offset + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  return (uint16_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8));
}

absl::StatusOr<uint32_t> Rom::ReadLong(int offset) const {
  if (offset < 0 || offset + 2 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset out of range");
  }
  return (uint32_t)(rom_data_[offset] | (rom_data_[offset + 1] << 8) |
                    (rom_data_[offset + 2] << 16));
}

absl::StatusOr<std::vector<uint8_t>> Rom::ReadByteVector(
    uint32_t offset, uint32_t length) const {
  if (offset + length > static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Offset and length out of range");
  }
  std::vector<uint8_t> result;
  result.reserve(length);
  for (uint32_t i = offset; i < offset + length; i++) {
    result.push_back(rom_data_[i]);
  }
  return result;
}

absl::StatusOr<gfx::Tile16> Rom::ReadTile16(uint32_t tile16_id,
                                            uint32_t tile16_ptr) {
  // Skip 8 bytes per tile.
  auto tpos = tile16_ptr + (tile16_id * 0x08);
  gfx::Tile16 tile16 = {};
  ASSIGN_OR_RETURN(auto new_tile0, ReadWord(tpos));
  tile16.tile0_ = gfx::WordToTileInfo(new_tile0);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile1, ReadWord(tpos));
  tile16.tile1_ = gfx::WordToTileInfo(new_tile1);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile2, ReadWord(tpos));
  tile16.tile2_ = gfx::WordToTileInfo(new_tile2);
  tpos += 2;
  ASSIGN_OR_RETURN(auto new_tile3, ReadWord(tpos));
  tile16.tile3_ = gfx::WordToTileInfo(new_tile3);
  return tile16;
}

absl::Status Rom::WriteTile16(int tile16_id, uint32_t tile16_ptr,
                              const gfx::Tile16& tile) {
  auto tpos = tile16_ptr + (tile16_id * 0x08);
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile0_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile1_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile2_)));
  tpos += 2;
  RETURN_IF_ERROR(WriteShort(tpos, gfx::TileInfoToWord(tile.tile3_)));
  return absl::OkStatus();
}

absl::Status Rom::WriteByte(int addr, uint8_t value) {
  RETURN_IF_ERROR(CheckLayoutWritable());
  if (addr < 0 || addr >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  for (auto* fence : write_fence_stack_) {
    RETURN_IF_ERROR(fence->Check(static_cast<uint32_t>(addr), 1, "WriteByte"));
  }
  RETURN_IF_ERROR(CheckPrivateLyraWrite(addr, &value, 1));
  const uint8_t old_val = rom_data_[addr];
  rom_data_[addr] = value;
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old_val}, {value});
#endif
  for (auto* fence : write_fence_stack_) {
    fence->RecordWrite(static_cast<uint32_t>(addr), 1);
  }
  return absl::OkStatus();
}

absl::Status Rom::WriteWord(int addr, uint16_t value) {
  RETURN_IF_ERROR(CheckLayoutWritable());
  if (addr < 0 || addr + 1 >= static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  for (auto* fence : write_fence_stack_) {
    RETURN_IF_ERROR(fence->Check(static_cast<uint32_t>(addr), 2, "WriteWord"));
  }
  const uint8_t private_bytes[] = {static_cast<uint8_t>(value),
                                   static_cast<uint8_t>(value >> 8),
                                   static_cast<uint8_t>(value >> 16)};
  RETURN_IF_ERROR(CheckPrivateLyraWrite(addr, private_bytes, 2));
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF)});
#endif
  for (auto* fence : write_fence_stack_) {
    fence->RecordWrite(static_cast<uint32_t>(addr), 2);
  }
  return absl::OkStatus();
}

absl::Status Rom::WriteShort(int addr, uint16_t value) {
  return WriteWord(addr, value);
}

absl::Status Rom::WriteLong(uint32_t addr, uint32_t value) {
  RETURN_IF_ERROR(CheckLayoutWritable());
  if (addr + 2 >= static_cast<uint32_t>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  for (auto* fence : write_fence_stack_) {
    RETURN_IF_ERROR(fence->Check(addr, 3, "WriteLong"));
  }
  const uint8_t private_bytes[] = {static_cast<uint8_t>(value),
                                   static_cast<uint8_t>(value >> 8),
                                   static_cast<uint8_t>(value >> 16)};
  RETURN_IF_ERROR(CheckPrivateLyraWrite(addr, private_bytes, 3));
  const uint8_t old0 = rom_data_[addr];
  const uint8_t old1 = rom_data_[addr + 1];
  const uint8_t old2 = rom_data_[addr + 2];
  rom_data_[addr] = (uint8_t)(value & 0xFF);
  rom_data_[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
  rom_data_[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, {old0, old1, old2},
                       {static_cast<uint8_t>(value & 0xFF),
                        static_cast<uint8_t>((value >> 8) & 0xFF),
                        static_cast<uint8_t>((value >> 16) & 0xFF)});
#endif
  for (auto* fence : write_fence_stack_) {
    fence->RecordWrite(addr, 3);
  }
  return absl::OkStatus();
}

absl::Status Rom::WriteVector(int addr, std::vector<uint8_t> data) {
  RETURN_IF_ERROR(CheckLayoutWritable());
  if (addr < 0) {
    return absl::OutOfRangeError("Address out of range");
  }
  if (addr + static_cast<int>(data.size()) >
      static_cast<int>(rom_data_.size())) {
    return absl::OutOfRangeError("Address out of range");
  }
  for (auto* fence : write_fence_stack_) {
    RETURN_IF_ERROR(fence->Check(static_cast<uint32_t>(addr),
                                 static_cast<uint32_t>(data.size()),
                                 "WriteVector"));
  }
  RETURN_IF_ERROR(CheckPrivateLyraWrite(addr, data.data(), data.size()));
  std::vector<uint8_t> old_data;
  old_data.reserve(data.size());
  for (int i = 0; i < static_cast<int>(data.size()); i++) {
    old_data.push_back(rom_data_[addr + i]);
    rom_data_[addr + i] = data[i];
  }
  dirty_ = true;
#ifdef __EMSCRIPTEN__
  MaybeBroadcastChange(addr, old_data, data);
#endif
  for (auto* fence : write_fence_stack_) {
    fence->RecordWrite(static_cast<uint32_t>(addr),
                       static_cast<uint32_t>(data.size()));
  }
  return absl::OkStatus();
}

absl::Status Rom::WriteColor(uint32_t address, const gfx::SnesColor& color) {
  uint16_t bgr = ((color.snes() >> 10) & 0x1F) | ((color.snes() & 0x1F) << 10) |
                 (color.snes() & 0x7C00);
  return WriteWord(address, bgr);
}

absl::Status Rom::WriteHelper(const WriteAction& action) {
  if (std::holds_alternative<uint8_t>(action.value)) {
    return WriteByte(action.address, std::get<uint8_t>(action.value));
  } else if (std::holds_alternative<uint16_t>(action.value) ||
             std::holds_alternative<short>(action.value)) {
    return WriteShort(action.address, std::get<uint16_t>(action.value));
  } else if (std::holds_alternative<std::vector<uint8_t>>(action.value)) {
    return WriteVector(action.address,
                       std::get<std::vector<uint8_t>>(action.value));
  } else if (std::holds_alternative<gfx::SnesColor>(action.value)) {
    return WriteColor(action.address, std::get<gfx::SnesColor>(action.value));
  }
  return absl::InvalidArgumentError("Invalid write argument type");
}

}  // namespace yaze
