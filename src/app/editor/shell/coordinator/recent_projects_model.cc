#include "app/editor/shell/coordinator/recent_projects_model.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "util/log.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"

namespace yaze {
namespace editor {

namespace {

// Display cap. Matches the previous welcome_screen constant; kept here because
// the model owns the truncation decision.
constexpr std::size_t kMaxRecentEntries = 6;

std::string ToLowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string TrimAscii(const std::string& value) {
  const auto not_space = [](unsigned char c) {
    return !std::isspace(c);
  };
  auto begin = std::find_if(value.begin(), value.end(), not_space);
  if (begin == value.end())
    return "";
  auto end = std::find_if(value.rbegin(), value.rend(), not_space).base();
  return std::string(begin, end);
}

bool IsRomPath(const std::filesystem::path& path) {
  const std::string ext = ToLowerAscii(path.extension().string());
  return ext == ".sfc" || ext == ".smc";
}

bool IsProjectPath(const std::filesystem::path& path) {
  const std::string ext = ToLowerAscii(path.extension().string());
  return ext == ".yaze" || ext == ".yazeproj" || ext == ".zsproj";
}

std::string FormatFileSize(std::uintmax_t bytes) {
  static constexpr std::array<const char*, 4> kUnits = {"B", "KB", "MB", "GB"};
  double value = static_cast<double>(bytes);
  std::size_t unit = 0;
  while (value >= 1024.0 && unit + 1 < kUnits.size()) {
    value /= 1024.0;
    ++unit;
  }
  if (unit == 0) {
    return absl::StrFormat("%llu %s", static_cast<unsigned long long>(bytes),
                           kUnits[unit]);
  }
  return absl::StrFormat("%.1f %s", value, kUnits[unit]);
}

std::string GetRelativeTimeString(
    const std::filesystem::file_time_type& ftime) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  auto now = std::chrono::system_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::hours>(now - sctp);

  int hours = diff.count();
  if (hours < 24)
    return "Today";
  if (hours < 48)
    return "Yesterday";
  if (hours < 168) {
    int days = hours / 24;
    return absl::StrFormat("%d days ago", days);
  }
  if (hours < 720) {
    int weeks = hours / 168;
    return absl::StrFormat("%d week%s ago", weeks, weeks > 1 ? "s" : "");
  }
  int months = hours / 720;
  return absl::StrFormat("%d month%s ago", months, months > 1 ? "s" : "");
}

std::string DecodeSnesRegion(std::uint8_t code) {
  switch (code) {
    case 0x00:
      return "Japan";
    case 0x01:
      return "USA";
    case 0x02:
      return "Europe";
    case 0x03:
      return "Sweden";
    case 0x06:
      return "France";
    case 0x07:
      return "Netherlands";
    case 0x08:
      return "Spain";
    case 0x09:
      return "Germany";
    case 0x0A:
      return "Italy";
    case 0x0B:
      return "China";
    case 0x0D:
      return "Korea";
    default:
      return "Unknown region";
  }
}

std::string DecodeSnesMapMode(std::uint8_t code) {
  switch (code & 0x3F) {
    case 0x20:
      return "LoROM";
    case 0x21:
      return "HiROM";
    case 0x22:
      return "ExLoROM";
    case 0x25:
      return "ExHiROM";
    case 0x30:
      return "Fast LoROM";
    case 0x31:
      return "Fast HiROM";
    default:
      return absl::StrFormat("Mode %02X", code);
  }
}

struct SnesHeaderMetadata {
  std::string title;
  std::string region;
  std::string map_mode;
  bool valid = false;
};

bool ReadFileBlock(std::ifstream* file, std::streamoff offset, char* out,
                   std::size_t size) {
  if (!file)
    return false;
  file->clear();
  file->seekg(offset, std::ios::beg);
  if (!file->good())
    return false;
  file->read(out, static_cast<std::streamsize>(size));
  return file->good() && file->gcount() == static_cast<std::streamsize>(size);
}

bool LooksLikeSnesTitle(const std::string& title) {
  if (title.empty())
    return false;
  int printable = 0;
  for (unsigned char c : title) {
    if (c >= 32 && c <= 126)
      ++printable;
  }
  return printable >= std::max(6, static_cast<int>(title.size()) / 2);
}

SnesHeaderMetadata ReadSnesHeaderMetadata(const std::filesystem::path& path) {
  std::error_code size_ec;
  const std::uintmax_t file_size = std::filesystem::file_size(path, size_ec);
  if (size_ec || file_size < 0x8020)
    return {};

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return {};

  static constexpr std::array<std::streamoff, 2> kHeaderBases = {0x7FC0,
                                                                 0xFFC0};
  static constexpr std::array<std::streamoff, 2> kHeaderBiases = {0, 512};

  for (std::streamoff bias : kHeaderBiases) {
    for (std::streamoff base : kHeaderBases) {
      const std::streamoff offset = base + bias;
      if (static_cast<std::uintmax_t>(offset + 0x20) > file_size)
        continue;

      char header[0x20] = {};
      if (!ReadFileBlock(&input, offset, header, sizeof(header)))
        continue;

      std::string raw_title(header, header + 21);
      for (char& c : raw_title) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126)
          c = ' ';
      }
      const std::string title = TrimAscii(raw_title);
      if (!LooksLikeSnesTitle(title))
        continue;

      const std::uint8_t map_mode_code =
          static_cast<std::uint8_t>(header[0x15]);
      const std::uint8_t region_code = static_cast<std::uint8_t>(header[0x19]);
      return {title, DecodeSnesRegion(region_code),
              DecodeSnesMapMode(map_mode_code), true};
    }
  }

  return {};
}

std::string ReadFileCrc32(const std::filesystem::path& path) {
  std::error_code size_ec;
  const std::uintmax_t file_size = std::filesystem::file_size(path, size_ec);
  if (size_ec || file_size == 0 ||
      file_size > static_cast<std::uintmax_t>(
                      std::numeric_limits<std::size_t>::max())) {
    return "";
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return "";

  std::vector<std::uint8_t> data(static_cast<std::size_t>(file_size));
  input.read(reinterpret_cast<char*>(data.data()),
             static_cast<std::streamsize>(data.size()));
  if (input.gcount() != static_cast<std::streamsize>(data.size()))
    return "";

  return absl::StrFormat("%08X",
                         util::CalculateCrc32(data.data(), data.size()));
}

std::string ParseConfigValue(const std::string& line) {
  if (line.empty())
    return "";
  std::size_t sep = line.find('=');
  if (sep == std::string::npos)
    sep = line.find(':');
  if (sep == std::string::npos || sep + 1 >= line.size())
    return "";
  std::string value = line.substr(sep + 1);
  const std::size_t comment_pos = value.find('#');
  if (comment_pos != std::string::npos)
    value = value.substr(0, comment_pos);
  value = TrimAscii(value);
  if (value.empty())
    return "";
  if (!value.empty() && value.back() == ',')
    value.pop_back();
  value = TrimAscii(value);
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1, value.size() - 2);
  }
  return TrimAscii(value);
}

std::string ExtractLinkedProjectRomName(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input.is_open())
    return "";

  static constexpr std::array<const char*, 3> kKeys = {"rom_filename",
                                                       "rom_file", "rom_path"};
  std::string line;
  while (std::getline(input, line)) {
    const std::string lowered = ToLowerAscii(line);
    for (const char* key : kKeys) {
      if (lowered.find(key) == std::string::npos)
        continue;
      const std::string value = ParseConfigValue(line);
      if (!value.empty()) {
        return std::filesystem::path(value).filename().string();
      }
    }
  }
  return "";
}

}  // namespace

RecentProjectsModel::RecentProjectsModel()
    : scan_state_(std::make_shared<AsyncScanState>()) {}

RecentProjectsModel::~RecentProjectsModel() {
  // Flip cancel so any in-flight workers stop touching their result slot
  // after the expensive I/O finishes. They still release their shared_ptr
  // ref naturally; the state outlives the model only long enough to let
  // them exit cleanly.
  if (scan_state_)
    scan_state_->cancelled.store(true);
}

// Build a display-ready entry for a single filepath. Populates all fields the
// welcome card needs, and consults / updates the persisted cache so we skip
// the full-ROM CRC32 hash and header parse whenever (size_bytes, mtime) still
// match what we saw last time.
RecentProject RecentProjectsModel::BuildEntry(const std::string& filepath) {
  std::filesystem::path path(filepath);

  RecentProject entry;
  entry.filepath = filepath;
  entry.name = path.filename().string();
  if (entry.name.empty())
    entry.name = filepath;
  entry.item_type = "File";
  entry.item_icon = ICON_MD_INSERT_DRIVE_FILE;
  entry.rom_title = "Local file";

  // Pull any persisted overrides. display_name_override takes precedence over
  // the filename for display purposes; the raw filename stays in filepath.
  auto cache_it = cache_.find(filepath);
  if (cache_it != cache_.end()) {
    entry.pinned = cache_it->second.pinned;
    entry.display_name_override = cache_it->second.display_name_override;
    entry.notes = cache_it->second.notes;
    if (!entry.display_name_override.empty()) {
      entry.name = entry.display_name_override;
    }
  }

  // iOS note: `filesystem::exists` without an error_code may throw when the
  // app lost security-scoped access to an iCloud/document-picker URL. Always
  // use the error_code overload.
  std::error_code exists_ec;
  const bool exists = std::filesystem::exists(path, exists_ec);
  if (exists_ec) {
    entry.unavailable = true;
    entry.last_modified = "Unavailable";
    entry.item_type = "Unavailable";
    entry.item_icon = ICON_MD_WARNING;
    entry.rom_title = "Re-open required";
    entry.metadata_summary = "Permission expired for this location";
    return entry;
  }
  if (!exists) {
    // Task #4: keep missing entries visible with a warning badge instead of
    // silently dropping them. User decides whether to "Locate..." (triggers
    // RelinkRecent) or "Forget" (RemoveRecent).
    entry.is_missing = true;
    entry.item_type = "Missing";
    entry.item_icon = ICON_MD_WARNING;
    entry.rom_title = "File not found";
    entry.last_modified = "Missing";
    entry.metadata_summary = "Choose Locate... to point at the new location";
    return entry;
  }

  std::error_code time_ec;
  auto ftime = std::filesystem::last_write_time(path, time_ec);
  const std::int64_t mtime_ns =
      time_ec ? 0
              : std::chrono::duration_cast<std::chrono::nanoseconds>(
                    ftime.time_since_epoch())
                    .count();
  if (!time_ec) {
    entry.last_modified = GetRelativeTimeString(ftime);
    entry.mtime_epoch_ns = mtime_ns;
  } else {
    entry.last_modified = "Unknown";
  }

  std::error_code size_ec;
  const std::uintmax_t size_bytes = std::filesystem::file_size(path, size_ec);
  entry.size_bytes = size_ec ? 0 : size_bytes;
  const std::string size_text =
      size_ec ? "Unknown size" : FormatFileSize(size_bytes);

  // Cache hit: size and mtime both unchanged. Reuse expensive fields.
  const bool cache_hit =
      cache_it != cache_.end() &&
      cache_it->second.size_bytes == entry.size_bytes &&
      cache_it->second.mtime_epoch_ns == entry.mtime_epoch_ns;

  std::string crc32;
  SnesHeaderMetadata rom_metadata;
  bool need_write_back = false;

  if (IsRomPath(path)) {
    entry.item_type = "ROM";
    entry.item_icon = ICON_MD_MEMORY;

    if (cache_hit) {
      crc32 = cache_it->second.crc32;
      rom_metadata.title = cache_it->second.snes_title;
      rom_metadata.region = cache_it->second.snes_region;
      rom_metadata.map_mode = cache_it->second.snes_map_mode;
      rom_metadata.valid = !rom_metadata.title.empty();
    } else {
      // Cache miss: don't block the UI thread on a full-ROM CRC32 + header
      // parse. Dispatch a detached worker; the result is drained on the next
      // Refresh call. In the meantime the card shows a "Scanning…" summary.
      DispatchBackgroundRomScan(filepath, entry.size_bytes,
                                entry.mtime_epoch_ns);
    }

    entry.crc32 = crc32;
    const std::string crc32_summary =
        crc32.empty() ? "Scanning…" : absl::StrFormat("CRC %s", crc32.c_str());
    if (rom_metadata.valid && !rom_metadata.title.empty()) {
      entry.rom_title = rom_metadata.title;
      entry.snes_region = rom_metadata.region;
      entry.snes_map_mode = rom_metadata.map_mode;
      entry.metadata_summary =
          absl::StrFormat("%s • %s • %s • %s", rom_metadata.region.c_str(),
                          rom_metadata.map_mode.c_str(), size_text.c_str(),
                          crc32_summary.c_str());
    } else if (cache_hit) {
      entry.rom_title = "SNES ROM";
      entry.metadata_summary =
          absl::StrFormat("%s • %s", size_text.c_str(), crc32_summary.c_str());
    } else {
      // Still waiting on the background worker. Show size immediately so the
      // card has something other than a blank line.
      entry.rom_title = "SNES ROM (scanning…)";
      entry.metadata_summary =
          absl::StrFormat("%s • Scanning…", size_text.c_str());
    }
  } else if (IsProjectPath(path)) {
    entry.item_type = "Project";
    entry.item_icon = ICON_MD_FOLDER_SPECIAL;

    const std::string linked_rom = ExtractLinkedProjectRomName(path);
    entry.rom_title = linked_rom.empty()
                          ? "Project metadata + settings"
                          : absl::StrFormat("ROM: %s", linked_rom.c_str());
    entry.metadata_summary = absl::StrFormat("%s • %s", size_text.c_str(),
                                             entry.last_modified.c_str());
  } else {
    entry.item_type = "File";
    entry.item_icon = ICON_MD_INSERT_DRIVE_FILE;
    entry.rom_title = "Imported file";
    entry.metadata_summary = absl::StrFormat("%s • %s", size_text.c_str(),
                                             entry.last_modified.c_str());
  }

  // Always keep the cache's (size, mtime) fresh; only write back the expensive
  // fields for ROMs that we actually recomputed.
  CachedExtras& cached = cache_[filepath];
  if (cached.size_bytes != entry.size_bytes ||
      cached.mtime_epoch_ns != entry.mtime_epoch_ns) {
    cached.size_bytes = entry.size_bytes;
    cached.mtime_epoch_ns = entry.mtime_epoch_ns;
    cache_dirty_ = true;
  }
  if (need_write_back) {
    cached.crc32 = crc32;
    cached.snes_title = rom_metadata.title;
    cached.snes_region = rom_metadata.region;
    cached.snes_map_mode = rom_metadata.map_mode;
    cache_dirty_ = true;
  }

  return entry;
}

void RecentProjectsModel::Refresh(bool force) {
  // Lazy-load the sidecar cache on the first call. Cheap; small file, once
  // per process. Done here instead of the constructor so PlatformPaths
  // initialization errors don't crash global-ctor order.
  if (!cache_loaded_) {
    LoadCache();
    cache_loaded_ = true;
  }

  // Fold in any background-scan results before we decide whether to rebuild.
  // DrainAsyncResults bumps annotation_generation_ on hit, so the combined
  // counter below catches it naturally.
  DrainAsyncResults();

  auto& manager = project::RecentFilesManager::GetInstance();

  // The manager's counter covers add/remove/clear; our annotation counter
  // covers pin/rename/notes mutations that the manager doesn't observe.
  // Combining lets the fast path skip work only when *neither* has changed.
  const std::uint64_t combined_generation =
      manager.GetGeneration() + annotation_generation_;
  if (!force && loaded_once_ && combined_generation == cached_generation_) {
    return;
  }
  cached_generation_ = combined_generation;
  loaded_once_ = true;

  entries_.clear();

  auto recent_files = manager.GetRecentFiles();

  for (const auto& filepath : recent_files) {
    if (entries_.size() >= kMaxRecentEntries)
      break;
    entries_.push_back(BuildEntry(filepath));
  }

  // Pinned entries float to the top; otherwise preserve RecentFilesManager
  // order (most recent first). std::stable_sort keeps ties deterministic.
  std::stable_sort(entries_.begin(), entries_.end(),
                   [](const RecentProject& a, const RecentProject& b) {
                     return a.pinned && !b.pinned;
                   });

  if (cache_dirty_) {
    SaveCache();
    cache_dirty_ = false;
  }
}

void RecentProjectsModel::AddRecent(const std::string& path) {
  auto& manager = project::RecentFilesManager::GetInstance();
  manager.AddFile(path);
  manager.Save();
}

void RecentProjectsModel::RemoveRecent(const std::string& path) {
  // Capture the display name + cached extras *before* we tear them down so
  // the undo buffer has everything it needs to re-attach annotations on
  // restore. Cheap: one hashmap lookup.
  RemovedRecent pending;
  pending.path = path;
  if (auto it = cache_.find(path); it != cache_.end()) {
    pending.extras = it->second;
    if (!it->second.display_name_override.empty()) {
      pending.display_name = it->second.display_name_override;
    }
  }
  if (pending.display_name.empty()) {
    std::error_code ec;
    pending.display_name = std::filesystem::path(path).filename().string();
    if (pending.display_name.empty())
      pending.display_name = path;
  }
  pending.expires_at =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(static_cast<int>(kUndoWindowSeconds * 1000.0f));

  auto& manager = project::RecentFilesManager::GetInstance();
  manager.RemoveFile(path);
  manager.Save();
  if (cache_.erase(path) > 0) {
    cache_dirty_ = true;
    SaveCache();
    cache_dirty_ = false;
  }

  // Single-slot buffer: a newer removal displaces an older pending one. That
  // matches how "Undo" UX tends to feel — rapid successive removals shouldn't
  // pile up stacked toasts.
  undo_buffer_.clear();
  undo_buffer_.push_back(std::move(pending));
}

bool RecentProjectsModel::HasUndoableRemoval() const {
  if (undo_buffer_.empty())
    return false;
  return std::chrono::steady_clock::now() < undo_buffer_.front().expires_at;
}

RecentProjectsModel::PendingUndo RecentProjectsModel::PeekLastRemoval() const {
  if (!HasUndoableRemoval())
    return {};
  const auto& front = undo_buffer_.front();
  return {front.path, front.display_name};
}

bool RecentProjectsModel::UndoLastRemoval() {
  if (!HasUndoableRemoval()) {
    undo_buffer_.clear();
    return false;
  }
  RemovedRecent entry = std::move(undo_buffer_.front());
  undo_buffer_.clear();

  auto& manager = project::RecentFilesManager::GetInstance();
  manager.AddFile(entry.path);  // Lands at the front of the MRU list.
  manager.Save();

  // Only restore extras if they carry user annotations worth keeping; the
  // size/mtime/CRC will be recomputed on next Refresh. Avoid writing an empty
  // CachedExtras record for no reason.
  const bool has_annotations = entry.extras.pinned ||
                               !entry.extras.display_name_override.empty() ||
                               !entry.extras.notes.empty();
  if (has_annotations) {
    cache_[entry.path] = std::move(entry.extras);
    cache_dirty_ = true;
    ++annotation_generation_;
    SaveCache();
    cache_dirty_ = false;
  }
  return true;
}

void RecentProjectsModel::DismissLastRemoval() {
  undo_buffer_.clear();
}

void RecentProjectsModel::RelinkRecent(const std::string& old_path,
                                       const std::string& new_path) {
  if (old_path == new_path || new_path.empty())
    return;

  auto& manager = project::RecentFilesManager::GetInstance();
  manager.RemoveFile(old_path);
  manager.AddFile(new_path);
  manager.Save();

  // Carry over user annotations (pin/rename/notes). The size/mtime cache is
  // path-agnostic but size/mtime on disk may differ if this is a moved copy,
  // so we drop the CRC + header snapshot; Refresh() will re-hash on demand.
  auto it = cache_.find(old_path);
  if (it != cache_.end()) {
    CachedExtras carried = std::move(it->second);
    carried.size_bytes = 0;
    carried.mtime_epoch_ns = 0;
    carried.crc32.clear();
    carried.snes_title.clear();
    carried.snes_region.clear();
    carried.snes_map_mode.clear();
    cache_.erase(it);
    cache_.emplace(new_path, std::move(carried));
    cache_dirty_ = true;
    SaveCache();
    cache_dirty_ = false;
  }
}

void RecentProjectsModel::ClearAll() {
  auto& manager = project::RecentFilesManager::GetInstance();
  manager.Clear();
  manager.Save();
  if (!cache_.empty()) {
    cache_.clear();
    SaveCache();
    cache_dirty_ = false;
  }
}

void RecentProjectsModel::SetPinned(const std::string& path, bool pinned) {
  auto& extras = cache_[path];
  if (extras.pinned == pinned)
    return;
  extras.pinned = pinned;
  cache_dirty_ = true;
  ++annotation_generation_;
  SaveCache();
  cache_dirty_ = false;
}

void RecentProjectsModel::SetDisplayName(const std::string& path,
                                         std::string display_name) {
  auto& extras = cache_[path];
  if (extras.display_name_override == display_name)
    return;
  extras.display_name_override = std::move(display_name);
  cache_dirty_ = true;
  ++annotation_generation_;
  SaveCache();
  cache_dirty_ = false;
}

void RecentProjectsModel::SetNotes(const std::string& path, std::string notes) {
  auto& extras = cache_[path];
  if (extras.notes == notes)
    return;
  extras.notes = std::move(notes);
  cache_dirty_ = true;
  ++annotation_generation_;
  SaveCache();
  cache_dirty_ = false;
}

void RecentProjectsModel::DispatchBackgroundRomScan(
    const std::string& filepath, std::uint64_t size_bytes,
    std::int64_t mtime_epoch_ns) {
  if (!scan_state_)
    return;
  {
    std::lock_guard<std::mutex> lock(scan_state_->mu);
    // De-dupe: a single in-flight worker per path is enough. The welcome
    // screen calls Refresh every frame, so without this guard we'd kick off
    // a new thread per frame until the first one finishes.
    if (scan_state_->in_flight[filepath])
      return;
    scan_state_->in_flight[filepath] = true;
  }

  std::thread worker(
      [state = scan_state_, filepath, size_bytes, mtime_epoch_ns]() {
        AsyncScanResult result;
        result.path = filepath;
        result.size_bytes = size_bytes;
        result.mtime_epoch_ns = mtime_epoch_ns;

        // Check cancel at entry/exit boundaries. The cost of the work itself
        // is dominated by the CRC32 read — if the model is destroyed mid-scan
        // we simply drop the result on the floor.
        if (state->cancelled.load())
          return;

        std::filesystem::path path(filepath);
        SnesHeaderMetadata header = ReadSnesHeaderMetadata(path);
        if (state->cancelled.load())
          return;
        std::string crc32 = ReadFileCrc32(path);
        if (state->cancelled.load())
          return;

        result.crc32 = std::move(crc32);
        if (header.valid) {
          result.snes_title = std::move(header.title);
          result.snes_region = std::move(header.region);
          result.snes_map_mode = std::move(header.map_mode);
        }

        std::lock_guard<std::mutex> lock(state->mu);
        state->in_flight.erase(filepath);
        if (state->cancelled.load())
          return;
        state->ready.push_back(std::move(result));
      });
  worker.detach();
}

bool RecentProjectsModel::DrainAsyncResults() {
  if (!scan_state_)
    return false;
  std::vector<AsyncScanResult> drained;
  {
    std::lock_guard<std::mutex> lock(scan_state_->mu);
    if (scan_state_->ready.empty())
      return false;
    drained.swap(scan_state_->ready);
  }

  bool any_applied = false;
  for (auto& r : drained) {
    auto& extras = cache_[r.path];
    // Only apply if the on-disk (size, mtime) the worker saw still matches
    // what's in the cache. If the file has been written to since dispatch,
    // a fresh scan will be kicked off on the next Refresh anyway.
    if (extras.size_bytes != 0 && (extras.size_bytes != r.size_bytes ||
                                   extras.mtime_epoch_ns != r.mtime_epoch_ns)) {
      continue;
    }
    extras.size_bytes = r.size_bytes;
    extras.mtime_epoch_ns = r.mtime_epoch_ns;
    extras.crc32 = std::move(r.crc32);
    extras.snes_title = std::move(r.snes_title);
    extras.snes_region = std::move(r.snes_region);
    extras.snes_map_mode = std::move(r.snes_map_mode);
    cache_dirty_ = true;
    any_applied = true;
  }
  if (any_applied) {
    ++annotation_generation_;  // Force the next Refresh to rebuild entries_.
    SaveCache();
    cache_dirty_ = false;
  }
  return any_applied;
}

std::filesystem::path RecentProjectsModel::CachePath() const {
  auto config_dir = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok())
    return {};
  return *config_dir / "recent_files_cache.json";
}

void RecentProjectsModel::LoadCache() {
  const auto path = CachePath();
  if (path.empty())
    return;

  std::ifstream input(path);
  if (!input.is_open())
    return;  // First run — no cache yet.

  try {
    nlohmann::json root = nlohmann::json::parse(input, nullptr,
                                                /*allow_exceptions=*/true,
                                                /*ignore_comments=*/true);
    if (!root.contains("entries") || !root["entries"].is_object())
      return;

    for (auto& [key, value] : root["entries"].items()) {
      if (!value.is_object())
        continue;
      CachedExtras extras;
      extras.size_bytes = value.value("size", std::uint64_t{0});
      extras.mtime_epoch_ns = value.value("mtime", std::int64_t{0});
      extras.crc32 = value.value("crc32", std::string{});
      extras.snes_title = value.value("snes_title", std::string{});
      extras.snes_region = value.value("snes_region", std::string{});
      extras.snes_map_mode = value.value("snes_map_mode", std::string{});
      extras.pinned = value.value("pinned", false);
      extras.display_name_override =
          value.value("display_name_override", std::string{});
      extras.notes = value.value("notes", std::string{});
      cache_.emplace(key, std::move(extras));
    }
  } catch (const std::exception& e) {
    // A corrupted cache is a cold-start cost, not a correctness problem.
    LOG_WARN("RecentProjectsModel",
             "Failed to parse recent-files cache %s: %s — rebuilding.",
             path.string().c_str(), e.what());
    cache_.clear();
  }
}

void RecentProjectsModel::SaveCache() {
  const auto path = CachePath();
  if (path.empty())
    return;

  nlohmann::json root;
  root["version"] = 1;
  nlohmann::json& entries = root["entries"] = nlohmann::json::object();
  for (const auto& [key, extras] : cache_) {
    entries[key] = {
        {"size", extras.size_bytes},
        {"mtime", extras.mtime_epoch_ns},
        {"crc32", extras.crc32},
        {"snes_title", extras.snes_title},
        {"snes_region", extras.snes_region},
        {"snes_map_mode", extras.snes_map_mode},
        {"pinned", extras.pinned},
        {"display_name_override", extras.display_name_override},
        {"notes", extras.notes},
    };
  }

  std::ofstream output(path);
  if (!output.is_open()) {
    LOG_WARN("RecentProjectsModel", "Could not write recent-files cache to %s",
             path.string().c_str());
    return;
  }
  output << root.dump(2);
}

}  // namespace editor
}  // namespace yaze
