#ifndef YAZE_APP_EDITOR_SHELL_COORDINATOR_RECENT_PROJECTS_MODEL_H_
#define YAZE_APP_EDITOR_SHELL_COORDINATOR_RECENT_PROJECTS_MODEL_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace editor {

// Display-ready record of a single recent project or ROM.
//
// The first block of fields is what the welcome screen needs to render a card.
// The second block is carried forward for features landing in subsequent
// commits — relink (is_missing), pin/rename/notes (display_name_override,
// pinned, notes), and the metadata+CRC cache (size_bytes, mtime_epoch_ns,
// crc32, snes_region, snes_map_mode). Defaults keep today's behavior.
struct RecentProject {
  // Display fields consumed by DrawProjectPanel.
  std::string name;
  std::string filepath;
  std::string rom_title;
  std::string metadata_summary;
  std::string last_modified;
  std::string item_type;  // "ROM" / "Project" / "File" / "Unavailable"
  std::string item_icon;
  std::string thumbnail_path;  // Optional screenshot
  bool unavailable = false;    // platform permission gate (iOS)
  int days_ago = 0;

  // --- Forward-looking (populated in later tasks) ---
  bool is_missing = false;
  bool pinned = false;
  std::string display_name_override;
  std::string notes;
  std::uint64_t size_bytes = 0;
  std::int64_t mtime_epoch_ns = 0;
  std::string crc32;
  std::string snes_region;
  std::string snes_map_mode;
};

// Observable, mutable list of recent projects.
//
// Owns no global state of its own; under the hood it reads and writes
// project::RecentFilesManager::GetInstance() so existing persistence keeps
// working. This seam lets the welcome screen, command palette, and tests
// consume the same list without each of them touching the singleton.
//
// Refresh() is gated by the manager's generation counter — cheap no-op when
// nothing changed, which is the common per-frame case.
class RecentProjectsModel {
 public:
  RecentProjectsModel();
  ~RecentProjectsModel();

  // Rebuild the entry vector from RecentFilesManager, resolving file
  // metadata. Fast path: returns early if the manager's generation counter
  // has not advanced since the last Refresh. Pass force=true to override.
  //
  // Expensive ROM metadata (SNES header parse + full-ROM CRC32) runs on a
  // detached worker for cache-miss entries; the display-ready entry carries
  // a "Scanning…" placeholder until the worker's result is drained on a
  // later Refresh. The UI doesn't need to do anything special — the welcome
  // screen already calls Refresh every frame.
  void Refresh(bool force = false);

  const std::vector<RecentProject>& entries() const { return entries_; }
  std::uint64_t generation() const { return cached_generation_; }

  // Mutations pass through to RecentFilesManager + Save(). They bump the
  // manager's generation counter so the next Refresh() picks up the change
  // automatically on a subsequent frame.
  void AddRecent(const std::string& path);
  void RemoveRecent(const std::string& path);
  void ClearAll();

  // Replace old_path with new_path in the recents list, preserving any
  // cached extras (pin/rename/notes carry over). Use when a recent file has
  // been moved on disk — the user points at its new location via a file
  // dialog, we swap the entry without losing its annotations.
  void RelinkRecent(const std::string& old_path, const std::string& new_path);

  // Per-entry annotations. All three persist across restarts via the
  // sidecar cache. Pinned entries sort first. Passing an empty name or notes
  // string clears the override.
  void SetPinned(const std::string& path, bool pinned);
  void SetDisplayName(const std::string& path, std::string display_name);
  void SetNotes(const std::string& path, std::string notes);

  // Undo support for RemoveRecent. When a user removes a recent entry we
  // stash its path + cached extras for a short window so the welcome screen
  // can surface an "Undo" affordance. Restoring re-adds the path to
  // RecentFilesManager (which puts it at the front — we don't try to recover
  // its original position) and re-attaches pin/rename/notes/crc so no user
  // annotations are lost. The window is `kUndoWindowSeconds`; after that the
  // removal is permanent and HasUndoableRemoval() returns false.
  static constexpr float kUndoWindowSeconds = 8.0f;
  struct PendingUndo {
    std::string path;
    std::string display_name;  // For the toast text.
  };
  bool HasUndoableRemoval() const;
  PendingUndo PeekLastRemoval() const;
  bool UndoLastRemoval();
  void DismissLastRemoval();  // Explicitly drop the pending undo.

 private:
  // Persisted extras per path. Stored in a sidecar JSON alongside the
  // recent-files list. The (size_bytes, mtime_epoch_ns) pair acts as a cache
  // key for the expensive reads (SNES header parse + full-ROM CRC32); when
  // neither has changed we skip the I/O entirely.
  //
  // The pinned / display_name_override / notes fields are wired up by later
  // commits but live here so the sidecar format is stable across the feature
  // set rolling out in this initiative.
  struct CachedExtras {
    std::uint64_t size_bytes = 0;
    std::int64_t mtime_epoch_ns = 0;
    std::string crc32;
    std::string snes_title;
    std::string snes_region;
    std::string snes_map_mode;
    bool pinned = false;
    std::string display_name_override;
    std::string notes;
  };

  RecentProject BuildEntry(const std::string& filepath);
  void DispatchBackgroundRomScan(const std::string& filepath,
                                 std::uint64_t size_bytes,
                                 std::int64_t mtime_epoch_ns);
  bool DrainAsyncResults();  // true iff state changed; bumps generation.

  void LoadCache();
  void SaveCache();
  std::filesystem::path CachePath() const;

  std::vector<RecentProject> entries_;
  std::uint64_t cached_generation_ = 0;
  bool loaded_once_ = false;

  std::unordered_map<std::string, CachedExtras> cache_;
  bool cache_dirty_ = false;
  bool cache_loaded_ = false;

  // Bumped by annotation mutations that RecentFilesManager doesn't see
  // (Pin/Rename/Notes). Combined with the manager's generation so the
  // Refresh fast-path still short-circuits correctly.
  std::uint64_t annotation_generation_ = 0;

  // Single-slot undo buffer for RemoveRecent. Keeps the path + cached extras
  // so a restore re-attaches pin/rename/notes. The expiry is a steady_clock
  // deadline; HasUndoableRemoval() returns false past it.
  struct RemovedRecent {
    std::string path;
    std::string display_name;
    CachedExtras extras;
    std::chrono::steady_clock::time_point expires_at;
  };
  std::deque<RemovedRecent> undo_buffer_;

  // Shared async-scan state. Workers hold their own shared_ptr copy, so
  // destroying the model mid-scan is safe: we flip `cancelled` and release
  // our reference; workers check `cancelled` before posting their result
  // and then drop the state when their ref dies. The mutex guards the
  // `ready` queue and the `in_flight` set.
  struct AsyncScanResult {
    std::string path;
    std::uint64_t size_bytes = 0;
    std::int64_t mtime_epoch_ns = 0;
    std::string crc32;
    std::string snes_title;
    std::string snes_region;
    std::string snes_map_mode;
  };
  struct AsyncScanState {
    std::mutex mu;
    std::vector<AsyncScanResult> ready;
    std::unordered_map<std::string, bool> in_flight;  // path -> true
    std::atomic<bool> cancelled{false};
  };
  std::shared_ptr<AsyncScanState> scan_state_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_COORDINATOR_RECENT_PROJECTS_MODEL_H_
