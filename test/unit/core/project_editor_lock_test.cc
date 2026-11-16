// The project editor lock must stay readable by Oracle of Secrets'
// Scripts/Agents/project_rom_lock.py: same JSON fields, and a `start` string
// equal to `/bin/ps -o lstart= -p <pid>` so the script does not treat a live
// yaze lock as stale.

#include "core/project_editor_lock.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "unique_temp_path.h"

#if defined(__APPLE__) || defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

#if defined(__APPLE__) || defined(__linux__)

class ScopedDir {
 public:
  explicit ScopedDir(fs::path path) : path_(std::move(path)) {
    fs::create_directories(path_);
  }
  ~ScopedDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << content;
}

std::string PsLstart(long pid) {
  const std::string command =
      absl::StrFormat("/bin/ps -o lstart= -p %d 2>/dev/null", pid);
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return "";
  }
  char buffer[256];
  std::string out;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    out += buffer;
  }
  pclose(pipe);
  return std::string(absl::StripAsciiWhitespace(out));
}

long DeadPid() {
  const pid_t child = fork();
  if (child == 0) {
    _exit(0);
  }
  int status = 0;
  waitpid(child, &status, 0);
  return static_cast<long>(child);
}

EditorLockRecord RecordFor(long pid, const std::string& owner) {
  EditorLockRecord record;
  record.owner = owner;
  record.pid = pid;
  auto start = ProcessStartString(pid);
  record.start = start.ok() ? *start : "";
  record.command = {"/bin/zsh", "Open Oracle.command"};
  record.since = "2026-11-16T17:44:54+00:00";
  return record;
}

TEST(ProjectEditorLockTest, ProcessStartMatchesPsLstart) {
  const long self = static_cast<long>(getpid());
  auto start = ProcessStartString(self);
  ASSERT_TRUE(start.ok()) << start.status();
  EXPECT_EQ(*start, PsLstart(self));

  const long parent = static_cast<long>(getppid());
  auto parent_start = ProcessStartString(parent);
  ASSERT_TRUE(parent_start.ok()) << parent_start.status();
  EXPECT_EQ(*parent_start, PsLstart(parent));
}

TEST(ProjectEditorLockTest, ProcessStartOfDeadPidIsNotFound) {
  EXPECT_FALSE(ProcessStartString(DeadPid()).ok());
}

TEST(ProjectEditorLockTest, AcquireWritesScriptFormatAndReleaseRemoves) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_acquire"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  {
    ProjectEditorLock lock;
    ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
    EXPECT_TRUE(lock.owns_file());
    ASSERT_TRUE(fs::exists(path));

    const std::string text = ReadFile(path);
    EXPECT_THAT(text, HasSubstr("\"owner\": \"scawful\""));
    auto record = ParseEditorLock(text);
    ASSERT_TRUE(record.ok());
    EXPECT_EQ(record->pid, static_cast<long>(getpid()));
    EXPECT_EQ(record->start, PsLstart(getpid()));
    ASSERT_EQ(record->command.size(), 1u);
    EXPECT_FALSE(record->command[0].empty());
    EXPECT_EQ(record->since.size(), 25u) << record->since;
    EXPECT_EQ(record->since.substr(19), "+00:00");

    lock.Release();
    EXPECT_FALSE(fs::exists(path));
  }
  // Destructor path.
  {
    ProjectEditorLock lock;
    ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
  }
  EXPECT_FALSE(fs::exists(path));
}

TEST(ProjectEditorLockTest, StaleLockFromDeadPidIsReplaced) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_dead"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  EditorLockRecord dead;
  dead.owner = "old";
  dead.pid = DeadPid();
  dead.start = "Mon Nov 16 01:02:03 2026";
  WriteFile(path, SerializeEditorLock(dead));

  ProjectEditorLock lock;
  ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
  EXPECT_TRUE(lock.owns_file());
  auto record = ParseEditorLock(ReadFile(path));
  ASSERT_TRUE(record.ok());
  EXPECT_EQ(record->pid, static_cast<long>(getpid()));
}

TEST(ProjectEditorLockTest, ReusedPidWithOtherStartIsStale) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_reused"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  EditorLockRecord reused = RecordFor(getppid(), "old");
  reused.start = "Thu Jan  1 00:00:00 1970";
  WriteFile(path, SerializeEditorLock(reused));

  ProjectEditorLock lock;
  ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
  EXPECT_TRUE(lock.owns_file());
}

TEST(ProjectEditorLockTest, LiveLockOfAnotherProcessRefuses) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_live"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  const std::string original =
      SerializeEditorLock(RecordFor(getppid(), "agent"));
  WriteFile(path, original);

  ProjectEditorLock lock;
  const auto status = lock.Acquire(path, "scawful");
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(status.message()), HasSubstr("held by agent (pid "));
  EXPECT_THAT(std::string(status.message()), HasSubstr("since 17:44Z"));
  EXPECT_EQ(ReadFile(path), original);
}

TEST(ProjectEditorLockTest, LauncherLockForThisPidIsAcceptedNotOwned) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_launcher"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  const std::string original =
      SerializeEditorLock(RecordFor(getpid(), "scawful"));
  WriteFile(path, original);
  {
    ProjectEditorLock lock;
    ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
    EXPECT_FALSE(lock.owns_file());
  }
  // The launcher that wrote it removes it, not yaze.
  EXPECT_EQ(ReadFile(path), original);
}

TEST(ProjectEditorLockTest, UnreadableLockRefuses) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_unreadable"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  WriteFile(path, "{not json");
  ProjectEditorLock lock;
  const auto status = lock.Acquire(path, "scawful");
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(std::string(status.message()), HasSubstr("unreadable"));
  EXPECT_TRUE(fs::exists(path));
}

TEST(ProjectEditorLockTest, ReleaseKeepsALockSomeoneElseRewrote) {
  ScopedDir dir(yaze::test::UniqueTempPath("editor_lock_rewritten"));
  const fs::path path = dir.path() / ".rom-editor.lock";
  ProjectEditorLock lock;
  ASSERT_TRUE(lock.Acquire(path, "scawful").ok());
  const std::string other = SerializeEditorLock(RecordFor(getppid(), "agent"));
  WriteFile(path, other);
  lock.Release();
  EXPECT_EQ(ReadFile(path), other);
}

#endif  // __APPLE__ || __linux__

TEST(ProjectEditorLockTest, DescribeMatchesScript) {
  EditorLockRecord record;
  record.owner = "scawful";
  record.pid = 4242;
  record.since = "2026-11-16T17:44:54+00:00";
  EXPECT_EQ(DescribeEditorLock(record), "scawful (pid 4242, since 17:44Z)");
}

}  // namespace
}  // namespace yaze::core
