#ifndef YAZE_CORE_PROJECT_EDITOR_LOCK_H_
#define YAZE_CORE_PROJECT_EDITOR_LOCK_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::core {

// A project editor lock file tells other tools (agents, scripts) that a person
// has this project open in yaze and its ROM must not be written underneath
// them. The JSON format matches Oracle of Secrets'
// Scripts/Agents/project_rom_lock.py so its `check` command reads yaze's lock:
//   {"owner": ..., "pid": N, "start": "<ps -o lstart=>", "command": [...],
//    "since": "<UTC ISO-8601>"}
// A lock whose pid is gone, or whose pid now belongs to a process with a
// different start time, is stale and is removed.
struct EditorLockRecord {
  std::string owner;
  long pid = 0;
  std::string start;
  std::vector<std::string> command;
  std::string since;
};

// The process start time formatted like `/bin/ps -o lstart=` in the C locale
// ("Mon Nov 16 12:40:50 2026"). NotFound when the pid does not exist.
absl::StatusOr<std::string> ProcessStartString(long pid);

std::string SerializeEditorLock(const EditorLockRecord& record);
absl::StatusOr<EditorLockRecord> ParseEditorLock(const std::string& text);

// "owner (pid N, since HH:MMZ)", as project_rom_lock.py describes a holder.
std::string DescribeEditorLock(const EditorLockRecord& record);

class ProjectEditorLock {
 public:
  ProjectEditorLock() = default;
  ~ProjectEditorLock();
  ProjectEditorLock(const ProjectEditorLock&) = delete;
  ProjectEditorLock& operator=(const ProjectEditorLock&) = delete;

  // Take the lock at `path` for this process. A stale lock is removed first.
  // A live lock held by this same process (a launcher that took the lock and
  // exec'd yaze) is accepted without taking ownership. A live lock held by any
  // other process, or an unreadable lock file, returns FailedPrecondition
  // naming the holder.
  absl::Status Acquire(const std::filesystem::path& path,
                       const std::string& owner);

  // Remove the lock file if this object created it and it still names this
  // process. Safe to call more than once.
  void Release();

  bool owns_file() const { return owns_file_; }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
  long pid_ = 0;
  bool owns_file_ = false;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_PROJECT_EDITOR_LOCK_H_
