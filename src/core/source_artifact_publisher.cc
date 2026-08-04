#include "core/source_artifact_publisher.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <semaphore>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/macro.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace yaze::core {
namespace {

namespace fs = std::filesystem;

std::string Capitalized(std::string value) {
  if (!value.empty()) {
    value.front() = static_cast<char>(
        std::toupper(static_cast<unsigned char>(value.front())));
  }
  return value;
}

struct PublicationArtifact {
  fs::path target;
  std::string content;
  std::optional<std::string> original_content;
  fs::path temp;
  std::optional<fs::path> backup;
  bool published = false;
};

std::binary_semaphore& SourceArtifactWriteSemaphore() {
  static std::binary_semaphore semaphore{1};
  return semaphore;
}

class SourceArtifactWriteLocks {
 public:
  static absl::StatusOr<std::unique_ptr<SourceArtifactWriteLocks>> Acquire(
      const std::vector<fs::path>& lock_paths,
      const SourceArtifactPublisherLabels& labels) {
    if (lock_paths.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s publication requires at least one lock",
                          Capitalized(labels.subject)));
    }
    auto lock = std::unique_ptr<SourceArtifactWriteLocks>(
        new SourceArtifactWriteLocks());
    SourceArtifactWriteSemaphore().acquire();
    lock->process_lock_acquired_ = true;

#if defined(_WIN32)
    lock->handles_.reserve(lock_paths.size());
    struct WindowsFileIdentity {
      DWORD volume_serial;
      DWORD file_index_high;
      DWORD file_index_low;
    };
    std::vector<WindowsFileIdentity> lock_identities;
    lock_identities.reserve(lock_paths.size());
    for (const fs::path& path : lock_paths) {
      HANDLE handle = CreateFileW(
          path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
      if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return absl::PermissionDeniedError(absl::StrFormat(
            "Cannot open %s lock %s: %s", labels.subject, path.string(),
            std::error_code(static_cast<int>(error), std::system_category())
                .message()));
      }
      BY_HANDLE_FILE_INFORMATION file_info = {};
      if (!GetFileInformationByHandle(handle, &file_info)) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot inspect %s lock %s: %s", labels.subject, path.string(),
            std::error_code(static_cast<int>(error), std::system_category())
                .message()));
      }
      if ((file_info.dwFileAttributes &
           (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0) {
        CloseHandle(handle);
        return absl::FailedPreconditionError(
            absl::StrFormat("%s lock must be a regular file: %s",
                            Capitalized(labels.subject), path.string()));
      }
      if (file_info.nNumberOfLinks != 1) {
        CloseHandle(handle);
        return absl::FailedPreconditionError(
            absl::StrFormat("%s lock must have exactly one hard link: %s",
                            Capitalized(labels.subject), path.string()));
      }
      const WindowsFileIdentity identity = {file_info.dwVolumeSerialNumber,
                                            file_info.nFileIndexHigh,
                                            file_info.nFileIndexLow};
      const bool duplicate_identity = std::any_of(
          lock_identities.begin(), lock_identities.end(),
          [&](const WindowsFileIdentity& existing) {
            return existing.volume_serial == identity.volume_serial &&
                   existing.file_index_high == identity.file_index_high &&
                   existing.file_index_low == identity.file_index_low;
          });
      if (duplicate_identity) {
        CloseHandle(handle);
        return absl::InvalidArgumentError(
            absl::StrFormat("%s lock aliases another lock path: %s",
                            Capitalized(labels.subject), path.string()));
      }
      OVERLAPPED overlapped = {};
      if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD,
                      &overlapped)) {
        const DWORD error = GetLastError();
        CloseHandle(handle);
        return absl::UnavailableError(absl::StrFormat(
            "Cannot acquire %s lock %s: %s", labels.subject, path.string(),
            std::error_code(static_cast<int>(error), std::system_category())
                .message()));
      }
      lock_identities.push_back(identity);
      lock->handles_.push_back(handle);
    }
#elif defined(__EMSCRIPTEN__)
    return absl::FailedPreconditionError(absl::StrFormat(
        "Durable %s locking is unavailable in browser builds", labels.subject));
#else
    lock->fds_.reserve(lock_paths.size());
    struct PosixFileIdentity {
      dev_t device;
      ino_t inode;
    };
    std::vector<PosixFileIdentity> lock_identities;
    lock_identities.reserve(lock_paths.size());
    for (const fs::path& path : lock_paths) {
      int open_flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
      open_flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
      open_flags |= O_NOFOLLOW;
#endif
      const int fd = open(path.c_str(), open_flags, S_IRUSR | S_IWUSR);
      if (fd < 0) {
        return absl::PermissionDeniedError(absl::StrFormat(
            "Cannot open %s lock %s: %s", labels.subject, path.string(),
            std::error_code(errno, std::generic_category()).message()));
      }
      struct stat lock_stat{};
      if (fstat(fd, &lock_stat) != 0) {
        const int error = errno;
        close(fd);
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot inspect %s lock %s: %s", labels.subject, path.string(),
            std::error_code(error, std::generic_category()).message()));
      }
      if (!S_ISREG(lock_stat.st_mode)) {
        close(fd);
        return absl::FailedPreconditionError(
            absl::StrFormat("%s lock must be a regular file: %s",
                            Capitalized(labels.subject), path.string()));
      }
      if (lock_stat.st_nlink != 1) {
        close(fd);
        return absl::FailedPreconditionError(
            absl::StrFormat("%s lock must have exactly one hard link: %s",
                            Capitalized(labels.subject), path.string()));
      }
      const PosixFileIdentity identity = {lock_stat.st_dev, lock_stat.st_ino};
      const bool duplicate_identity =
          std::any_of(lock_identities.begin(), lock_identities.end(),
                      [&](const PosixFileIdentity& existing) {
                        return existing.device == identity.device &&
                               existing.inode == identity.inode;
                      });
      if (duplicate_identity) {
        close(fd);
        return absl::InvalidArgumentError(
            absl::StrFormat("%s lock aliases another lock path: %s",
                            Capitalized(labels.subject), path.string()));
      }
      if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        const int error = errno;
        close(fd);
        return absl::PermissionDeniedError(absl::StrFormat(
            "Cannot secure %s lock %s: %s", labels.subject, path.string(),
            std::error_code(error, std::generic_category()).message()));
      }
      while (flock(fd, LOCK_EX) != 0) {
        if (errno == EINTR) {
          continue;
        }
        const int error = errno;
        close(fd);
        return absl::UnavailableError(absl::StrFormat(
            "Cannot acquire %s lock %s: %s", labels.subject, path.string(),
            std::error_code(error, std::generic_category()).message()));
      }
      lock_identities.push_back(identity);
      lock->fds_.push_back(fd);
    }
#endif
    return lock;
  }

  ~SourceArtifactWriteLocks() {
#if defined(_WIN32)
    for (auto it = handles_.rbegin(); it != handles_.rend(); ++it) {
      OVERLAPPED overlapped = {};
      UnlockFileEx(*it, 0, MAXDWORD, MAXDWORD, &overlapped);
      CloseHandle(*it);
    }
#elif !defined(__EMSCRIPTEN__)
    for (auto it = fds_.rbegin(); it != fds_.rend(); ++it) {
      while (flock(*it, LOCK_UN) != 0 && errno == EINTR) {}
      close(*it);
    }
#endif
    if (process_lock_acquired_) {
      SourceArtifactWriteSemaphore().release();
    }
  }

  SourceArtifactWriteLocks(const SourceArtifactWriteLocks&) = delete;
  SourceArtifactWriteLocks& operator=(const SourceArtifactWriteLocks&) = delete;

 private:
  SourceArtifactWriteLocks() = default;

  bool process_lock_acquired_ = false;
#if defined(_WIN32)
  std::vector<HANDLE> handles_;
#elif !defined(__EMSCRIPTEN__)
  std::vector<int> fds_;
#endif
};

constexpr uint32_t kSha256Constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr uint32_t RotateRight(uint32_t value, uint32_t count) {
  return (value >> count) | (value << (32 - count));
}

void TransformSha256(uint32_t state[8], const uint8_t block[64]) {
  uint32_t schedule[64];
  for (int index = 0; index < 16; ++index) {
    schedule[index] = (static_cast<uint32_t>(block[index * 4]) << 24) |
                      (static_cast<uint32_t>(block[index * 4 + 1]) << 16) |
                      (static_cast<uint32_t>(block[index * 4 + 2]) << 8) |
                      static_cast<uint32_t>(block[index * 4 + 3]);
  }
  for (int index = 16; index < 64; ++index) {
    const uint32_t low = RotateRight(schedule[index - 15], 7) ^
                         RotateRight(schedule[index - 15], 18) ^
                         (schedule[index - 15] >> 3);
    const uint32_t high = RotateRight(schedule[index - 2], 17) ^
                          RotateRight(schedule[index - 2], 19) ^
                          (schedule[index - 2] >> 10);
    schedule[index] = schedule[index - 16] + low + schedule[index - 7] + high;
  }

  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];
  for (int index = 0; index < 64; ++index) {
    const uint32_t sigma1 =
        RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
    const uint32_t choose = (e & f) ^ ((~e) & g);
    const uint32_t temp1 =
        h + sigma1 + choose + kSha256Constants[index] + schedule[index];
    const uint32_t sigma0 =
        RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = sigma0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

std::string Sha256Hex(std::string_view content) {
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const auto* cursor = reinterpret_cast<const uint8_t*>(content.data());
  size_t remaining = content.size();
  while (remaining >= 64) {
    TransformSha256(state, cursor);
    cursor += 64;
    remaining -= 64;
  }

  uint8_t block[64] = {};
  if (remaining > 0) {
    std::memcpy(block, cursor, remaining);
  }
  block[remaining] = 0x80;
  if (remaining >= 56) {
    TransformSha256(state, block);
    std::memset(block, 0, sizeof(block));
  }
  const uint64_t bit_length = static_cast<uint64_t>(content.size()) * 8;
  for (int index = 0; index < 8; ++index) {
    block[63 - index] =
        static_cast<uint8_t>(bit_length >> (static_cast<uint64_t>(index) * 8));
  }
  TransformSha256(state, block);

  std::string digest;
  digest.reserve(64);
  for (uint32_t value : state) {
    absl::StrAppend(&digest, absl::StrFormat("%08x", value));
  }
  return digest;
}

absl::StatusOr<std::string> ReadTextFile(const fs::path& path,
                                         absl::string_view label) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open %s: %s", label, path.string()));
  }
  std::string content{std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>()};
  if (!input.good() && !input.eof()) {
    return absl::DataLossError(
        absl::StrFormat("Failed while reading %s: %s", label, path.string()));
  }
  return content;
}

std::string LowercasePath(const fs::path& path) {
  return absl::AsciiStrToLower(path.generic_string());
}

absl::StatusOr<fs::path> ResolvePublicationTarget(
    const fs::path& target, const SourceArtifactPublisherLabels& labels) {
  if (target.empty() || !target.is_absolute()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s target must be an absolute path: %s",
                        Capitalized(labels.subject), target.string()));
  }

  std::error_code status_ec;
  const fs::file_status link_status = fs::symlink_status(target, status_ec);
  if (status_ec != std::errc::no_such_file_or_directory && status_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect %s target %s: %s", labels.subject,
                        target.string(), status_ec.message()));
  }
  if (!status_ec && fs::is_symlink(link_status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s target may not be a symbolic link: %s",
                        Capitalized(labels.subject), target.string()));
  }

  std::error_code canonical_ec;
  fs::path resolved = fs::weakly_canonical(target, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve %s target %s: %s", labels.subject,
                        target.string(), canonical_ec.message()));
  }
  return resolved.lexically_normal();
}

absl::StatusOr<std::vector<fs::path>> ResolvePublicationTargets(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  if (targets.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s publication requires at least one target",
                        Capitalized(labels.subject)));
  }
  std::vector<fs::path> resolved_targets;
  resolved_targets.reserve(targets.size());
  for (const fs::path& target : targets) {
    fs::path resolved;
    ASSIGN_OR_RETURN(resolved, ResolvePublicationTarget(target, labels));
    resolved_targets.push_back(std::move(resolved));
  }
  return resolved_targets;
}

absl::Status ValidateDistinctPublicationTargets(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  for (size_t left_index = 0; left_index < targets.size(); ++left_index) {
    for (size_t right_index = left_index + 1; right_index < targets.size();
         ++right_index) {
      const fs::path& left = targets[left_index];
      const fs::path& right = targets[right_index];
      if (left == right || LowercasePath(left) == LowercasePath(right)) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "%s publication targets must be distinct: %s and %s",
            Capitalized(labels.subject), left.string(), right.string()));
      }

      std::error_code left_exists_ec;
      const bool left_exists = fs::exists(left, left_exists_ec);
      std::error_code right_exists_ec;
      const bool right_exists = fs::exists(right, right_exists_ec);
      if (left_exists_ec || right_exists_ec) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot inspect %s publication targets %s and %s: %s",
            labels.subject, left.string(), right.string(),
            left_exists_ec ? left_exists_ec.message()
                           : right_exists_ec.message()));
      }
      if (!left_exists || !right_exists) {
        continue;
      }

      std::error_code equivalent_ec;
      const bool equivalent = fs::equivalent(left, right, equivalent_ec);
      if (equivalent_ec) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot compare %s publication targets %s and %s: %s",
            labels.subject, left.string(), right.string(),
            equivalent_ec.message()));
      }
      if (equivalent) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "%s publication targets alias each other: %s and %s",
            Capitalized(labels.subject), left.string(), right.string()));
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<fs::path>> PublicationLockPaths(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  if (targets.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s publication requires at least one target",
                        Capitalized(labels.subject)));
  }
  std::vector<fs::path> lock_paths;
  for (const fs::path& target : targets) {
    std::error_code canonical_ec;
    const fs::path parent = fs::canonical(target.parent_path(), canonical_ec);
    if (canonical_ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot resolve %s target directory %s: %s", labels.subject,
          target.parent_path().string(), canonical_ec.message()));
    }

    bool duplicate = false;
    for (const fs::path& existing_lock : lock_paths) {
      std::error_code equivalent_ec;
      const bool equivalent =
          fs::equivalent(parent, existing_lock.parent_path(), equivalent_ec);
      if (equivalent_ec) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot compare %s target directories %s and %s: %s",
            labels.subject, parent.string(),
            existing_lock.parent_path().string(), equivalent_ec.message()));
      }
      if (equivalent) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      lock_paths.push_back(parent / kSourceArtifactPublicationLockBasename);
    }
  }

  std::sort(lock_paths.begin(), lock_paths.end(),
            [](const fs::path& left, const fs::path& right) {
              const std::string left_lower = LowercasePath(left);
              const std::string right_lower = LowercasePath(right);
              return left_lower == right_lower
                         ? left.generic_string() < right.generic_string()
                         : left_lower < right_lower;
            });
  return lock_paths;
}

absl::Status ValidateTargetsDoNotAliasLocks(
    const std::vector<fs::path>& lock_paths,
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  for (const fs::path& lock_path : lock_paths) {
    for (const fs::path& target : targets) {
      if (target == lock_path ||
          LowercasePath(target) == LowercasePath(lock_path)) {
        return absl::InvalidArgumentError(
            absl::StrFormat("%s targets may not use a persistent lock path: %s",
                            Capitalized(labels.subject), lock_path.string()));
      }
    }

    std::error_code lock_exists_ec;
    const bool lock_exists = fs::exists(lock_path, lock_exists_ec);
    if (lock_exists_ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot inspect persistent %s lock %s: %s", labels.subject,
          lock_path.string(), lock_exists_ec.message()));
    }
    if (!lock_exists) {
      continue;
    }

    for (const fs::path& target : targets) {
      std::error_code target_exists_ec;
      const bool target_exists = fs::exists(target, target_exists_ec);
      if (target_exists_ec) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Cannot inspect %s target %s: %s", labels.subject,
                            target.string(), target_exists_ec.message()));
      }
      if (!target_exists) {
        continue;
      }
      std::error_code equivalent_ec;
      const bool equivalent = fs::equivalent(lock_path, target, equivalent_ec);
      if (equivalent_ec) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Cannot compare %s target %s with persistent lock %s: %s",
            labels.subject, target.string(), lock_path.string(),
            equivalent_ec.message()));
      }
      if (equivalent) {
        return absl::InvalidArgumentError(
            absl::StrFormat("%s target aliases a persistent lock path: %s",
                            Capitalized(labels.subject), target.string()));
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<fs::path>> PreparePublicationTargets(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  std::vector<fs::path> resolved_targets;
  ASSIGN_OR_RETURN(resolved_targets,
                   ResolvePublicationTargets(targets, labels));
  RETURN_IF_ERROR(ValidateDistinctPublicationTargets(resolved_targets, labels));

  std::vector<fs::path> lock_paths;
  ASSIGN_OR_RETURN(lock_paths, PublicationLockPaths(resolved_targets, labels));
  RETURN_IF_ERROR(
      ValidateTargetsDoNotAliasLocks(lock_paths, resolved_targets, labels));
  return resolved_targets;
}

fs::path NextSiblingPath(const fs::path& target, absl::string_view purpose) {
  static std::atomic<uint64_t> sequence{0};
  const uint64_t tick = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const uint64_t id = sequence.fetch_add(1, std::memory_order_relaxed);
  fs::path name = target.filename();
  name += absl::StrFormat(".yaze-%s-%016x-%016x", purpose, tick, id);
  return target.parent_path() / name;
}

absl::StatusOr<fs::path> WriteExclusiveTemp(const fs::path& target,
                                            std::string_view content) {
  constexpr int kMaxAttempts = 100;
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
  mode_t create_mode =
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  bool preserve_mode = false;
  struct stat target_stat{};
  if (stat(target.c_str(), &target_stat) == 0) {
    if (!S_ISREG(target_stat.st_mode)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Publication target must be a regular file: %s", target.string()));
    }
    create_mode = target_stat.st_mode & 07777;
    preserve_mode = true;
  } else if (errno != ENOENT) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot inspect publication target mode %s: %s", target.string(),
        std::error_code(errno, std::generic_category()).message()));
  }
#endif
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const fs::path temp = NextSiblingPath(target, "tmp");
#if defined(_WIN32)
    HANDLE file = CreateFileW(
        temp.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
        continue;
      }
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot create temporary file for %s: %s", target.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
    size_t written_total = 0;
    while (written_total < content.size()) {
      const DWORD chunk = static_cast<DWORD>(std::min<size_t>(
          content.size() - written_total, std::numeric_limits<DWORD>::max()));
      DWORD written = 0;
      if (!WriteFile(file, content.data() + written_total, chunk, &written,
                     nullptr) ||
          written == 0) {
        const DWORD error = GetLastError();
        CloseHandle(file);
        std::error_code cleanup_ec;
        fs::remove(temp, cleanup_ec);
        return absl::InternalError(absl::StrFormat(
            "Failed to write temporary file for %s: %s", target.string(),
            std::error_code(static_cast<int>(error), std::system_category())
                .message()));
      }
      written_total += written;
    }
    if (!FlushFileBuffers(file)) {
      const DWORD error = GetLastError();
      CloseHandle(file);
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary file for %s: %s", target.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
    if (!CloseHandle(file)) {
      const DWORD error = GetLastError();
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary file for %s: %s", target.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
#elif defined(__EMSCRIPTEN__)
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot create temporary file for %s", target.string()));
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output.good()) {
      output.close();
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to write temporary file for %s", target.string()));
    }
    output.close();
#else
    const int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, create_mode);
    if (fd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot create temporary file for %s: %s", target.string(),
          std::error_code(errno, std::generic_category()).message()));
    }
    if (preserve_mode && fchmod(fd, create_mode) != 0) {
      const int error = errno;
      close(fd);
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Cannot preserve publication target mode for %s: %s", target.string(),
          std::error_code(error, std::generic_category()).message()));
    }
    size_t written_total = 0;
    while (written_total < content.size()) {
      const ssize_t written =
          write(fd, content.data() + written_total,
                std::min<size_t>(content.size() - written_total, 1024 * 1024));
      if (written < 0 && errno == EINTR) {
        continue;
      }
      if (written <= 0) {
        const int error = written < 0 ? errno : EIO;
        close(fd);
        std::error_code cleanup_ec;
        fs::remove(temp, cleanup_ec);
        return absl::InternalError(absl::StrFormat(
            "Failed to write temporary file for %s: %s", target.string(),
            std::error_code(error, std::generic_category()).message()));
      }
      written_total += static_cast<size_t>(written);
    }
    if (fsync(fd) != 0) {
      const int error = errno;
      close(fd);
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary file for %s: %s", target.string(),
          std::error_code(error, std::generic_category()).message()));
    }
    if (close(fd) != 0) {
      const int error = errno;
      std::error_code cleanup_ec;
      fs::remove(temp, cleanup_ec);
      return absl::InternalError(absl::StrFormat(
          "Failed to flush temporary file for %s: %s", target.string(),
          std::error_code(error, std::generic_category()).message()));
    }
#endif
    return temp;
  }
  return absl::ResourceExhaustedError(absl::StrFormat(
      "Could not allocate a unique temporary file for %s", target.string()));
}

absl::Status SyncParentDirectory(const fs::path& target) {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
  return absl::OkStatus();
#else
  int open_flags = O_RDONLY;
#ifdef O_CLOEXEC
  open_flags |= O_CLOEXEC;
#endif
#ifdef O_DIRECTORY
  open_flags |= O_DIRECTORY;
#endif
  const fs::path parent = target.parent_path();
  const int fd = open(parent.c_str(), open_flags);
  if (fd < 0) {
    return absl::InternalError(absl::StrFormat(
        "Cannot open parent directory for durable publication %s: %s",
        parent.string(),
        std::error_code(errno, std::generic_category()).message()));
  }
  int sync_result = 0;
  do {
    sync_result = fsync(fd);
  } while (sync_result != 0 && errno == EINTR);
  const int sync_error = sync_result == 0 ? 0 : errno;
  const int close_result = close(fd);
  const int close_error = close_result == 0 ? 0 : errno;
  if (sync_error != 0) {
    return absl::InternalError(absl::StrFormat(
        "Cannot sync parent directory after publishing %s: %s", target.string(),
        std::error_code(sync_error, std::generic_category()).message()));
  }
  if (close_result != 0) {
    return absl::InternalError(absl::StrFormat(
        "Cannot close parent directory after publishing %s: %s",
        target.string(),
        std::error_code(close_error, std::generic_category()).message()));
  }
  return absl::OkStatus();
#endif
}

absl::Status ReplaceFromTemp(const fs::path& temp, const fs::path& target,
                             bool* replaced = nullptr) {
  if (replaced != nullptr) {
    *replaced = false;
  }
#if defined(_WIN32)
  if (!MoveFileExW(temp.wstring().c_str(), target.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    const DWORD error = GetLastError();
    return absl::InternalError(absl::StrFormat(
        "Failed to publish %s: %s", target.string(),
        std::error_code(static_cast<int>(error), std::system_category())
            .message()));
  }
  if (replaced != nullptr) {
    *replaced = true;
  }
#else
  std::error_code rename_ec;
  fs::rename(temp, target, rename_ec);
  if (rename_ec) {
    return absl::InternalError(absl::StrFormat(
        "Failed to publish %s: %s", target.string(), rename_ec.message()));
  }
  if (replaced != nullptr) {
    *replaced = true;
  }
#endif
  return SyncParentDirectory(target);
}

absl::StatusOr<fs::path> CreateBackup(const fs::path& target) {
  constexpr int kMaxAttempts = 100;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const fs::path backup = NextSiblingPath(target, "backup");
    std::error_code copy_ec;
    fs::copy_file(target, backup, fs::copy_options::none, copy_ec);
    if (!copy_ec) {
      return backup;
    }
    if (copy_ec != std::errc::file_exists) {
      std::error_code cleanup_ec;
      fs::remove(backup, cleanup_ec);
      if (cleanup_ec) {
        return absl::DataLossError(absl::StrFormat(
            "Cannot back up %s (%s) or remove incomplete backup %s (%s)",
            target.string(), copy_ec.message(), backup.string(),
            cleanup_ec.message()));
      }
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot back up %s: %s", target.string(), copy_ec.message()));
    }
  }
  return absl::ResourceExhaustedError(absl::StrFormat(
      "Could not allocate a backup path for %s", target.string()));
}

absl::Status CleanupPublicationPaths(
    const std::vector<fs::path>& paths, absl::string_view artifact_kind,
    const SourceArtifactPublisherLabels& labels) {
  std::string failures;
  for (const fs::path& path : paths) {
    if (path.empty()) {
      continue;
    }
    std::error_code ec;
    fs::remove(path, ec);
    if (!ec) {
      continue;
    }
    if (!failures.empty()) {
      absl::StrAppend(&failures, "; ");
    }
    absl::StrAppend(&failures, path.string(), " (", ec.message(), ")");
  }
  if (!failures.empty()) {
    return absl::InternalError(absl::StrFormat(
        "Could not remove %s %s: %s", labels.subject, artifact_kind, failures));
  }
  return absl::OkStatus();
}

absl::Status CleanupTemps(const std::vector<PublicationArtifact>& artifacts,
                          const SourceArtifactPublisherLabels& labels) {
  std::vector<fs::path> paths;
  paths.reserve(artifacts.size());
  for (const auto& artifact : artifacts) {
    if (!artifact.temp.empty()) {
      paths.push_back(artifact.temp);
    }
  }
  return CleanupPublicationPaths(paths, "temporary files", labels);
}

absl::Status CleanupBackups(std::vector<PublicationArtifact>* artifacts,
                            const SourceArtifactPublisherLabels& labels) {
  std::string failures;
  for (auto& artifact : *artifacts) {
    if (!artifact.backup.has_value()) {
      continue;
    }
    std::error_code ec;
    fs::remove(*artifact.backup, ec);
    if (!ec) {
      artifact.backup.reset();
      continue;
    }
    if (!failures.empty()) {
      absl::StrAppend(&failures, "; ");
    }
    absl::StrAppend(&failures, artifact.backup->string(), " (", ec.message(),
                    ")");
  }
  if (!failures.empty()) {
    return absl::InternalError(absl::StrFormat(
        "Could not remove %s backup files: %s", labels.subject, failures));
  }
  return absl::OkStatus();
}

std::string BackupRecoveryPaths(
    const std::vector<PublicationArtifact>& artifacts) {
  std::string paths;
  for (const auto& artifact : artifacts) {
    if (!artifact.backup.has_value()) {
      continue;
    }
    if (!paths.empty()) {
      absl::StrAppend(&paths, ", ");
    }
    absl::StrAppend(&paths, artifact.backup->string());
  }
  return paths.empty() ? "<none>" : paths;
}

std::string CleanupStatusSummary(const absl::Status& status) {
  return status.ok() ? "ok" : std::string(status.message());
}

absl::Status ReturnFailureAfterCleanup(
    std::vector<PublicationArtifact>* artifacts,
    const absl::Status& publication_failure,
    const SourceArtifactPublisherLabels& labels) {
  const absl::Status temp_cleanup = CleanupTemps(*artifacts, labels);
  const absl::Status backup_cleanup = CleanupBackups(artifacts, labels);
  if (temp_cleanup.ok() && backup_cleanup.ok()) {
    return publication_failure;
  }
  return absl::DataLossError(absl::StrFormat(
      "%s publication failed (%s), then artifact cleanup failed "
      "(temporary files: %s; backup files: %s)",
      Capitalized(labels.subject), publication_failure.message(),
      CleanupStatusSummary(temp_cleanup),
      CleanupStatusSummary(backup_cleanup)));
}

absl::Status RestoreArtifact(const PublicationArtifact& artifact) {
  if (artifact.original_content.has_value()) {
    fs::path restore_temp;
    ASSIGN_OR_RETURN(
        restore_temp,
        WriteExclusiveTemp(artifact.target, *artifact.original_content));
    const absl::Status status = ReplaceFromTemp(restore_temp, artifact.target);
    if (!status.ok()) {
      std::error_code cleanup_ec;
      fs::remove(restore_temp, cleanup_ec);
      if (cleanup_ec) {
        return absl::DataLossError(absl::StrFormat(
            "Could not restore %s (%s) or remove restore file %s (%s)",
            artifact.target.string(), status.message(), restore_temp.string(),
            cleanup_ec.message()));
      }
    }
    return status;
  }
  std::error_code remove_ec;
  const bool removed = fs::remove(artifact.target, remove_ec);
  if (remove_ec || !removed) {
    return absl::InternalError(absl::StrFormat(
        "Could not remove newly published file %s during rollback: %s",
        artifact.target.string(),
        remove_ec ? remove_ec.message() : "file was missing"));
  }
  return SyncParentDirectory(artifact.target);
}

absl::Status RollBackPublication(std::vector<PublicationArtifact>* artifacts,
                                 const absl::Status& publication_failure,
                                 const SourceArtifactPublisherLabels& labels) {
  absl::Status rollback_failure = absl::OkStatus();
  for (auto it = artifacts->rbegin(); it != artifacts->rend(); ++it) {
    if (!it->published) {
      continue;
    }
    const absl::Status restore_status = RestoreArtifact(*it);
    if (!restore_status.ok() && rollback_failure.ok()) {
      rollback_failure = restore_status;
    }
    it->published = false;
  }
  const absl::Status temp_cleanup = CleanupTemps(*artifacts, labels);
  if (!rollback_failure.ok()) {
    return absl::DataLossError(absl::StrFormat(
        "%s publication failed (%s) and rollback failed (%s). "
        "Temporary-file cleanup: %s. Preserved recovery backups: %s",
        Capitalized(labels.subject), publication_failure.message(),
        rollback_failure.message(), CleanupStatusSummary(temp_cleanup),
        BackupRecoveryPaths(*artifacts)));
  }
  const absl::Status backup_cleanup = CleanupBackups(artifacts, labels);
  if (!temp_cleanup.ok() || !backup_cleanup.ok()) {
    return absl::DataLossError(absl::StrFormat(
        "%s publication failed (%s); rollback completed, but "
        "artifact cleanup failed (temporary files: %s; backup files: %s)",
        Capitalized(labels.subject), publication_failure.message(),
        CleanupStatusSummary(temp_cleanup),
        CleanupStatusSummary(backup_cleanup)));
  }
  return publication_failure;
}

absl::Status VerifyUnchangedBeforePublication(
    const std::vector<PublicationArtifact>& artifacts,
    std::string_view expected_source_sha256,
    const SourceArtifactPublisherLabels& labels) {
  for (const auto& artifact : artifacts) {
    std::error_code exists_ec;
    const bool exists = fs::exists(artifact.target, exists_ec);
    if (exists_ec) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Cannot recheck publication target %s: %s",
                          artifact.target.string(), exists_ec.message()));
    }
    if (artifact.original_content.has_value()) {
      if (!exists) {
        return absl::AbortedError(absl::StrFormat(
            "Publication target disappeared after preflight: %s",
            artifact.target.string()));
      }
      std::string current;
      ASSIGN_OR_RETURN(current,
                       ReadTextFile(artifact.target, "publication target"));
      if (current != *artifact.original_content) {
        return absl::AbortedError(
            absl::StrFormat("Publication target changed after preflight: %s",
                            artifact.target.string()));
      }
    } else if (exists) {
      return absl::AbortedError(
          absl::StrFormat("Publication target appeared after preflight: %s",
                          artifact.target.string()));
    }
  }
  if (Sha256Hex(*artifacts.front().original_content) !=
      expected_source_sha256) {
    return absl::AbortedError(absl::StrFormat(
        "Canonical %s changed after SHA-256 preflight", labels.subject));
  }
  return absl::OkStatus();
}

absl::Status PublishArtifactSet(std::vector<PublicationArtifact>* artifacts,
                                std::string_view expected_source_sha256,
                                const SourceArtifactPublisherLabels& labels) {
  for (auto& artifact : *artifacts) {
    auto temp_or = WriteExclusiveTemp(artifact.target, artifact.content);
    if (!temp_or.ok()) {
      return ReturnFailureAfterCleanup(artifacts, temp_or.status(), labels);
    }
    artifact.temp = std::move(*temp_or);
    if (artifact.original_content.has_value()) {
      auto backup_or = CreateBackup(artifact.target);
      if (!backup_or.ok()) {
        return ReturnFailureAfterCleanup(artifacts, backup_or.status(), labels);
      }
      artifact.backup = std::move(*backup_or);
    }
  }

  const absl::Status unchanged_status = VerifyUnchangedBeforePublication(
      *artifacts, expected_source_sha256, labels);
  if (!unchanged_status.ok()) {
    return ReturnFailureAfterCleanup(artifacts, unchanged_status, labels);
  }

  for (auto& artifact : *artifacts) {
    bool replaced = false;
    const absl::Status replace_status =
        ReplaceFromTemp(artifact.temp, artifact.target, &replaced);
    artifact.published = replaced;
    if (!replace_status.ok()) {
      return RollBackPublication(artifacts, replace_status, labels);
    }
    artifact.temp.clear();
  }

  for (const auto& artifact : *artifacts) {
    std::string reopened;
    auto reopened_or = ReadTextFile(artifact.target, labels.published_file);
    if (!reopened_or.ok()) {
      return RollBackPublication(artifacts, reopened_or.status(), labels);
    }
    reopened = std::move(*reopened_or);
    if (reopened != artifact.content) {
      return RollBackPublication(
          artifacts,
          absl::DataLossError(absl::StrFormat(
              "%s failed exact readback: %s",
              Capitalized(labels.published_file), artifact.target.string())),
          labels);
    }
  }
  const absl::Status temp_cleanup = CleanupTemps(*artifacts, labels);
  if (!temp_cleanup.ok()) {
    return RollBackPublication(artifacts, temp_cleanup, labels);
  }
  return absl::OkStatus();
}

}  // namespace

struct SourceArtifactPublicationLock::Impl {
  SourceArtifactPublisherLabels labels;
  std::vector<fs::path> targets;
  std::unique_ptr<SourceArtifactWriteLocks> write_locks;
  std::atomic_flag publication_in_progress = ATOMIC_FLAG_INIT;
};

namespace {

class ScopedPublicationUse {
 public:
  explicit ScopedPublicationUse(
      std::atomic_flag* publication_in_progress) noexcept
      : publication_in_progress_(publication_in_progress) {}

  ~ScopedPublicationUse() noexcept {
    publication_in_progress_->clear(std::memory_order_release);
  }

  ScopedPublicationUse(const ScopedPublicationUse&) = delete;
  ScopedPublicationUse& operator=(const ScopedPublicationUse&) = delete;

 private:
  std::atomic_flag* publication_in_progress_;
};

}  // namespace

SourceArtifactPublicationLock::SourceArtifactPublicationLock(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

SourceArtifactPublicationLock::~SourceArtifactPublicationLock() = default;

absl::Status ValidateSourceArtifactPublicationTargets(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  if (labels.subject.empty() || labels.published_file.empty()) {
    return absl::InvalidArgumentError(
        "Source artifact diagnostic labels must not be empty");
  }
  return PreparePublicationTargets(targets, labels).status();
}

absl::StatusOr<std::unique_ptr<SourceArtifactPublicationLock>>
AcquireSourceArtifactPublicationLock(
    const std::vector<fs::path>& targets,
    const SourceArtifactPublisherLabels& labels) {
  if (labels.subject.empty() || labels.published_file.empty()) {
    return absl::InvalidArgumentError(
        "Source artifact diagnostic labels must not be empty");
  }

  std::vector<fs::path> resolved_targets;
  ASSIGN_OR_RETURN(resolved_targets,
                   PreparePublicationTargets(targets, labels));

  std::vector<fs::path> lock_paths;
  ASSIGN_OR_RETURN(lock_paths, PublicationLockPaths(resolved_targets, labels));
  std::unique_ptr<SourceArtifactWriteLocks> write_locks;
  ASSIGN_OR_RETURN(write_locks,
                   SourceArtifactWriteLocks::Acquire(lock_paths, labels));

  std::vector<fs::path> resolved_after_lock;
  ASSIGN_OR_RETURN(resolved_after_lock,
                   ResolvePublicationTargets(targets, labels));
  if (resolved_after_lock != resolved_targets) {
    return absl::AbortedError(absl::StrFormat(
        "%s publication target changed while acquiring its lock",
        Capitalized(labels.subject)));
  }

  auto impl = std::make_unique<SourceArtifactPublicationLock::Impl>();
  impl->labels = labels;
  impl->targets = std::move(resolved_targets);
  impl->write_locks = std::move(write_locks);
  return std::unique_ptr<SourceArtifactPublicationLock>(
      new SourceArtifactPublicationLock(std::move(impl)));
}

std::string ComputeSourceArtifactSha256(std::string_view content) {
  return Sha256Hex(content);
}

absl::Status PublishSourceArtifacts(
    const SourceArtifactPublicationLock& lock,
    std::vector<SourceArtifactUpdate> updates,
    std::string_view expected_primary_sha256,
    SourceArtifactReadbackValidator readback_validator) {
  if (lock.impl_ == nullptr) {
    return absl::FailedPreconditionError(
        "Source artifact publication lock is not initialized");
  }
  if (lock.impl_->publication_in_progress.test_and_set(
          std::memory_order_acquire)) {
    return absl::FailedPreconditionError(
        "Source artifact publication lock is already in use");
  }
  const ScopedPublicationUse publication_use(
      &lock.impl_->publication_in_progress);
  if (updates.empty()) {
    return absl::InvalidArgumentError(
        "Source artifact publication requires at least one update");
  }
  if (!updates.front().before.has_value()) {
    return absl::InvalidArgumentError(
        "Primary source artifact must contain its preflight bytes");
  }

  std::vector<std::string> seen_targets;
  seen_targets.reserve(updates.size());
  for (SourceArtifactUpdate& update : updates) {
    fs::path resolved_target;
    ASSIGN_OR_RETURN(resolved_target, ResolvePublicationTarget(
                                          update.target, lock.impl_->labels));
    const auto covered = std::find(lock.impl_->targets.begin(),
                                   lock.impl_->targets.end(), resolved_target);
    if (covered == lock.impl_->targets.end()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Publication target is not covered by the acquired lock: %s",
          update.target.string()));
    }
    update.target = *covered;
    const std::string normalized_target = LowercasePath(resolved_target);
    if (std::find(seen_targets.begin(), seen_targets.end(),
                  normalized_target) != seen_targets.end()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Source artifact publication target is duplicated: %s",
          update.target.string()));
    }
    seen_targets.push_back(normalized_target);
  }

  std::vector<PublicationArtifact> artifacts;
  artifacts.reserve(updates.size());
  for (SourceArtifactUpdate& update : updates) {
    artifacts.push_back(PublicationArtifact{
        .target = std::move(update.target),
        .content = std::move(update.after),
        .original_content = std::move(update.before),
    });
  }

  RETURN_IF_ERROR(PublishArtifactSet(&artifacts, expected_primary_sha256,
                                     lock.impl_->labels));
  if (readback_validator) {
    absl::Status validation_status;
    try {
      validation_status = readback_validator();
    } catch (const std::exception& error) {
      validation_status = absl::InternalError(absl::StrFormat(
          "%s readback validator threw an exception: %s",
          Capitalized(lock.impl_->labels.subject), error.what()));
    } catch (...) {
      validation_status = absl::InternalError(
          absl::StrFormat("%s readback validator threw an unknown exception",
                          Capitalized(lock.impl_->labels.subject)));
    }
    if (!validation_status.ok()) {
      return RollBackPublication(&artifacts, validation_status,
                                 lock.impl_->labels);
    }
  }

  const absl::Status backup_cleanup =
      CleanupBackups(&artifacts, lock.impl_->labels);
  if (!backup_cleanup.ok()) {
    return RollBackPublication(&artifacts, backup_cleanup, lock.impl_->labels);
  }
  return absl::OkStatus();
}

}  // namespace yaze::core
