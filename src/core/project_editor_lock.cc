#include "core/project_editor_lock.h"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

#if defined(__APPLE__)
#include <fcntl.h>
#include <libproc.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace yaze::core {
namespace {

namespace fs = std::filesystem;

#if defined(__APPLE__) || defined(__linux__)
long CurrentPid() {
  return static_cast<long>(::getpid());
}
#else
long CurrentPid() {
  return 0;
}
#endif

// `ps -o lstart=` prints strftime("%c") in the C locale.
std::string FormatLocalStart(time_t seconds) {
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &seconds);
#else
  localtime_r(&seconds, &local);
#endif
  char buffer[64];
  const size_t n =
      std::strftime(buffer, sizeof(buffer), "%a %b %e %H:%M:%S %Y", &local);
  return std::string(buffer, n);
}

std::string UtcNowIso() {
  const time_t now = std::time(nullptr);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  char buffer[64];
  const size_t n =
      std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+00:00", &utc);
  return std::string(buffer, n);
}

std::string CurrentExecutable() {
#if defined(__APPLE__)
  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(::getpid(), buffer, sizeof(buffer)) > 0) {
    return buffer;
  }
#elif defined(__linux__)
  std::error_code ec;
  auto exe = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) {
    return exe.string();
  }
#endif
  return "yaze";
}

std::string CurrentOwner() {
  const char* user = std::getenv("USER");
  return (user != nullptr && user[0] != '\0') ? user : "yaze";
}

absl::StatusOr<std::string> ReadText(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return absl::NotFoundError(path.string());
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// True when `record` names a process that is still running with the same start
// time. Mirrors project_rom_lock.py read_lock(): only an int pid > 0 can be
// proven stale; anything else counts as held.
bool RecordIsLive(const EditorLockRecord& record) {
  if (record.pid <= 0) {
    return true;
  }
  auto start = ProcessStartString(record.pid);
  return start.ok() && *start == record.start;
}

}  // namespace

absl::StatusOr<std::string> ProcessStartString(long pid) {
  if (pid <= 0) {
    return absl::InvalidArgumentError("pid must be positive");
  }
#if defined(__APPLE__)
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  struct kinfo_proc info{};
  size_t size = sizeof(info);
  if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0 || size == 0) {
    return absl::NotFoundError(absl::StrFormat("no process %d", pid));
  }
  return FormatLocalStart(info.kp_proc.p_starttime.tv_sec);
#elif defined(__linux__)
  std::ifstream stat_file(absl::StrFormat("/proc/%d/stat", pid));
  if (!stat_file) {
    return absl::NotFoundError(absl::StrFormat("no process %d", pid));
  }
  std::string stat;
  std::getline(stat_file, stat);
  // Field 2 (comm) may contain spaces; parse after its closing paren.
  const size_t close = stat.rfind(')');
  if (close == std::string::npos) {
    return absl::InternalError("unexpected /proc stat format");
  }
  std::istringstream fields(stat.substr(close + 2));
  std::string field;
  unsigned long long start_ticks = 0;
  // Fields after comm start at 3 (state); starttime is field 22.
  for (int index = 3; index <= 22 && (fields >> field); ++index) {
    if (index == 22) {
      start_ticks = std::stoull(field);
    }
  }
  std::ifstream proc_stat("/proc/stat");
  std::string line;
  long long boot_time = 0;
  while (std::getline(proc_stat, line)) {
    if (line.rfind("btime ", 0) == 0) {
      boot_time = std::stoll(line.substr(6));
      break;
    }
  }
  const long ticks = sysconf(_SC_CLK_TCK);
  return FormatLocalStart(
      static_cast<time_t>(boot_time + start_ticks / (ticks > 0 ? ticks : 100)));
#else
  return absl::UnimplementedError("process start time not available");
#endif
}

std::string SerializeEditorLock(const EditorLockRecord& record) {
  nlohmann::ordered_json json;
  json["owner"] = record.owner;
  json["pid"] = record.pid;
  json["start"] = record.start;
  json["command"] = record.command;
  json["since"] = record.since;
  return json.dump(2);
}

absl::StatusOr<EditorLockRecord> ParseEditorLock(const std::string& text) {
  nlohmann::json json = nlohmann::json::parse(text, nullptr,
                                              /*allow_exceptions=*/false);
  if (json.is_discarded() || !json.is_object()) {
    return absl::DataLossError("unreadable lock file");
  }
  EditorLockRecord record;
  if (auto it = json.find("owner"); it != json.end() && it->is_string()) {
    record.owner = it->get<std::string>();
  }
  if (auto it = json.find("pid"); it != json.end() && it->is_number_integer()) {
    record.pid = it->get<long>();
  }
  if (auto it = json.find("start"); it != json.end() && it->is_string()) {
    record.start = it->get<std::string>();
  }
  if (auto it = json.find("command"); it != json.end() && it->is_array()) {
    for (const auto& part : *it) {
      if (part.is_string()) {
        record.command.push_back(part.get<std::string>());
      }
    }
  }
  if (auto it = json.find("since"); it != json.end() && it->is_string()) {
    record.since = it->get<std::string>();
  }
  return record;
}

std::string DescribeEditorLock(const EditorLockRecord& record) {
  const std::string since =
      record.since.size() >= 16 ? record.since.substr(11, 5) : "";
  return absl::StrFormat("%s (pid %d, since %sZ)",
                         record.owner.empty() ? "?" : record.owner, record.pid,
                         since);
}

ProjectEditorLock::~ProjectEditorLock() {
  Release();
}

absl::Status ProjectEditorLock::Acquire(const fs::path& path,
                                        const std::string& owner) {
#if !defined(__APPLE__) && !defined(__linux__)
  (void)path;
  (void)owner;
  return absl::UnimplementedError("editor lock needs macOS or Linux");
#else
  Release();
  const long self = CurrentPid();

  if (auto text = ReadText(path); text.ok()) {
    auto record = ParseEditorLock(*text);
    if (!record.ok()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "project is locked: unreadable lock file %s", path.string()));
    }
    if (RecordIsLive(*record)) {
      if (record->pid == self) {
        // A launcher took the lock for this pid and exec'd yaze; keep it and
        // leave its removal to the launcher.
        path_ = path;
        pid_ = self;
        owns_file_ = false;
        return absl::OkStatus();
      }
      return absl::FailedPreconditionError(absl::StrFormat(
          "project is locked: held by %s", DescribeEditorLock(*record)));
    }
    std::error_code ec;
    fs::remove(path, ec);
  }

  auto start = ProcessStartString(self);
  if (!start.ok()) {
    return start.status();
  }
  EditorLockRecord record;
  record.owner = owner.empty() ? CurrentOwner() : owner;
  record.pid = self;
  record.start = *start;
  record.command = {CurrentExecutable()};
  record.since = UtcNowIso();
  const std::string body = SerializeEditorLock(record);

  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
  }
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
  if (fd < 0) {
    if (errno == EEXIST) {
      return absl::FailedPreconditionError(
          "project is locked: lock was taken at the same moment");
    }
    return absl::PermissionDeniedError(absl::StrFormat(
        "cannot create lock %s: %s", path.string(), std::strerror(errno)));
  }
  size_t written = 0;
  while (written < body.size()) {
    const ssize_t n = ::write(fd, body.data() + written, body.size() - written);
    if (n <= 0) {
      if (n < 0 && errno == EINTR) {
        continue;
      }
      break;
    }
    written += static_cast<size_t>(n);
  }
  ::close(fd);
  path_ = path;
  pid_ = self;
  owns_file_ = true;
  if (written != body.size()) {
    Release();
    return absl::DataLossError(
        absl::StrFormat("short write to lock %s", path.string()));
  }
  return absl::OkStatus();
#endif
}

void ProjectEditorLock::Release() {
  if (owns_file_ && !path_.empty()) {
    if (auto text = ReadText(path_); text.ok()) {
      auto record = ParseEditorLock(*text);
      if (record.ok() && record->pid == pid_) {
        std::error_code ec;
        fs::remove(path_, ec);
      }
    }
  }
  path_.clear();
  pid_ = 0;
  owns_file_ = false;
}

}  // namespace yaze::core
