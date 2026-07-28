#include "app/editor/message/message_source_sync.h"

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
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/message/message_data.h"
#include "nlohmann/json.hpp"
#include "rom/snes.h"
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

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

struct ExpandedSourceMessage {
  std::string text;
  std::vector<uint8_t> bytes;
};

using ExpandedSourceBank = std::map<int, ExpandedSourceMessage>;

constexpr char kSourceSyncLockFilename[] = ".yaze-message-source-sync.lock";

struct PublicationArtifact {
  fs::path target;
  std::string content;
  std::optional<std::string> original_content;
  fs::path temp;
  std::optional<fs::path> backup;
  bool published = false;
};

std::mutex& MessageSourceWriteMutex() {
  static std::mutex mutex;
  return mutex;
}

class MessageSourceWriteLock {
 public:
  static absl::StatusOr<std::unique_ptr<MessageSourceWriteLock>> Acquire(
      const fs::path& project_root) {
    auto lock =
        std::unique_ptr<MessageSourceWriteLock>(new MessageSourceWriteLock());
    lock->process_lock_ =
        std::unique_lock<std::mutex>(MessageSourceWriteMutex());
    lock->path_ = project_root / kSourceSyncLockFilename;

#if defined(_WIN32)
    lock->handle_ =
        CreateFileW(lock->path_.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock->handle_ == INVALID_HANDLE_VALUE) {
      const DWORD error = GetLastError();
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot open message source lock %s: %s", lock->path_.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
    if (!LockFileEx(lock->handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD,
                    MAXDWORD, &lock->overlapped_)) {
      const DWORD error = GetLastError();
      CloseHandle(lock->handle_);
      lock->handle_ = INVALID_HANDLE_VALUE;
      return absl::UnavailableError(absl::StrFormat(
          "Cannot acquire message source lock %s: %s", lock->path_.string(),
          std::error_code(static_cast<int>(error), std::system_category())
              .message()));
    }
#elif defined(__EMSCRIPTEN__)
    return absl::FailedPreconditionError(
        "Durable message source locking is unavailable in browser builds");
#else
    int open_flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
    open_flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    open_flags |= O_NOFOLLOW;
#endif
    lock->fd_ = open(lock->path_.c_str(), open_flags, S_IRUSR | S_IWUSR);
    if (lock->fd_ < 0) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot open message source lock %s: %s", lock->path_.string(),
          std::error_code(errno, std::generic_category()).message()));
    }
    struct stat lock_stat{};
    if (fstat(lock->fd_, &lock_stat) != 0) {
      const int error = errno;
      close(lock->fd_);
      lock->fd_ = -1;
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot inspect message source lock %s: %s", lock->path_.string(),
          std::error_code(error, std::generic_category()).message()));
    }
    if (!S_ISREG(lock_stat.st_mode)) {
      close(lock->fd_);
      lock->fd_ = -1;
      return absl::FailedPreconditionError(
          absl::StrFormat("Message source lock must be a regular file: %s",
                          lock->path_.string()));
    }
    if (fchmod(lock->fd_, S_IRUSR | S_IWUSR) != 0) {
      const int error = errno;
      close(lock->fd_);
      lock->fd_ = -1;
      return absl::PermissionDeniedError(absl::StrFormat(
          "Cannot secure message source lock %s: %s", lock->path_.string(),
          std::error_code(error, std::generic_category()).message()));
    }
    while (flock(lock->fd_, LOCK_EX) != 0) {
      if (errno == EINTR) {
        continue;
      }
      const int error = errno;
      close(lock->fd_);
      lock->fd_ = -1;
      return absl::UnavailableError(absl::StrFormat(
          "Cannot acquire message source lock %s: %s", lock->path_.string(),
          std::error_code(error, std::generic_category()).message()));
    }
#endif
    return lock;
  }

  ~MessageSourceWriteLock() {
#if defined(_WIN32)
    if (handle_ != INVALID_HANDLE_VALUE) {
      UnlockFileEx(handle_, 0, MAXDWORD, MAXDWORD, &overlapped_);
      CloseHandle(handle_);
    }
#elif !defined(__EMSCRIPTEN__)
    if (fd_ >= 0) {
      while (flock(fd_, LOCK_UN) != 0 && errno == EINTR) {}
      close(fd_);
    }
#endif
  }

  MessageSourceWriteLock(const MessageSourceWriteLock&) = delete;
  MessageSourceWriteLock& operator=(const MessageSourceWriteLock&) = delete;

 private:
  MessageSourceWriteLock() = default;

  fs::path path_;
  std::unique_lock<std::mutex> process_lock_;
#if defined(_WIN32)
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  OVERLAPPED overlapped_ = {};
#elif !defined(__EMSCRIPTEN__)
  int fd_ = -1;
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

absl::StatusOr<Json> ParseBundleDocument(std::string_view content,
                                         absl::string_view label) {
  Json document;
  try {
    document = Json::parse(content);
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s is not valid JSON: %s", label, error.what()));
  }
  if (!document.is_object()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must be a JSON object", label));
  }
  if (!document.contains("format") || !document["format"].is_string() ||
      document["format"].get<std::string>() != "yaze-message-bundle") {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s format must be 'yaze-message-bundle'", label));
  }
  if (!document.contains("version") ||
      !document["version"].is_number_integer() ||
      document["version"].get<int>() != kMessageBundleVersion) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s version must be integer %d", label, kMessageBundleVersion));
  }
  if (!document.contains("messages") || !document["messages"].is_array()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must contain a messages array", label));
  }
  return document;
}

absl::StatusOr<std::string> SelectEntryText(const Json& entry, int id,
                                            absl::string_view label) {
  // `text` is the editable canonical field. Rich exports may retain stale
  // raw/parsed diagnostics beside a deliberately changed text value.
  for (absl::string_view key : {"text", "raw", "parsed"}) {
    const std::string key_string(key);
    if (!entry.contains(key_string)) {
      continue;
    }
    if (!entry[key_string].is_string()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d field '%s' must be a string",
                          label, id, key));
    }
    return entry[key_string].get<std::string>();
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "%s expanded message %d has no raw, text, or parsed string", label, id));
}

std::string NormalizeLegacyDictionaryTokens(std::string text) {
  size_t search_from = 0;
  while (true) {
    const size_t token_start = text.find("[D:", search_from);
    if (token_start == std::string::npos) {
      break;
    }
    size_t value_start = token_start + 3;
    if (value_start < text.size() && text[value_start] == '$') {
      ++value_start;
    }
    const size_t token_end = text.find(']', value_start);
    if (token_end == std::string::npos) {
      break;
    }
    const size_t digit_count = token_end - value_start;
    const bool valid_digits =
        digit_count >= 1 && digit_count <= 2 &&
        std::all_of(text.begin() + static_cast<std::ptrdiff_t>(value_start),
                    text.begin() + static_cast<std::ptrdiff_t>(token_end),
                    [](unsigned char c) { return std::isxdigit(c) != 0; });
    if (!valid_digits) {
      search_from = token_end + 1;
      continue;
    }
    const int dictionary_id =
        std::stoi(text.substr(value_start, digit_count), nullptr, 16);
    const std::string normalized = absl::StrFormat("[D:%02X]", dictionary_id);
    text.replace(token_start, token_end - token_start + 1, normalized);
    search_from = token_start + normalized.size();
  }
  return text;
}

absl::StatusOr<ExpandedSourceBank> ParseExpandedBank(const Json& document,
                                                     int expected_count,
                                                     bool require_complete,
                                                     absl::string_view label) {
  ExpandedSourceBank bank;
  for (const Json& entry : document["messages"]) {
    if (!entry.is_object()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s message entry must be an object", label));
    }
    if (!entry.contains("id") || !entry["id"].is_number_integer()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s message entry has no integer id", label));
    }
    const int64_t id64 = entry["id"].get<int64_t>();
    if (id64 < 0 || id64 >= expected_count) {
      return absl::OutOfRangeError(absl::StrFormat(
          "%s expanded message ID %d is outside bank-local range [0, %d)",
          label, id64, expected_count));
    }
    const int id = static_cast<int>(id64);
    if (!entry.contains("bank") || !entry["bank"].is_string() ||
        entry["bank"].get<std::string>() != "expanded") {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s message %d must declare bank 'expanded'", label, id));
    }
    if (bank.contains(id)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s contains duplicate expanded message ID %d", label, id));
    }

    std::string text;
    ASSIGN_OR_RETURN(text, SelectEntryText(entry, id, label));
    text = NormalizeLegacyDictionaryTokens(std::move(text));
    if (absl::StrContains(text, "[BANK]")) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s expanded message %d contains [BANK], which is only valid in "
          "the vanilla message stream",
          label, id));
    }
    MessageParseResult parsed;
    try {
      parsed = ParseMessageToDataWithDiagnostics(text);
    } catch (const std::exception& error) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d cannot be parsed safely: %s",
                          label, id, error.what()));
    } catch (...) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s expanded message %d cannot be parsed safely", label, id));
    }
    if (!parsed.ok()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d is invalid: %s", label, id,
                          parsed.errors.front()));
    }
    if (!parsed.warnings.empty()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s expanded message %d is not source-stable: %s",
                          label, id, parsed.warnings.front()));
    }
    bank.emplace(id, ExpandedSourceMessage{
                         .text = std::move(text),
                         .bytes = std::move(parsed.bytes),
                     });
  }

  if (bank.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s has no expanded messages", label));
  }
  if (require_complete) {
    if (static_cast<int>(bank.size()) != expected_count) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s must contain the complete expanded bank: expected %d entries, "
          "found %zu",
          label, expected_count, bank.size()));
    }
    for (int id = 0; id < expected_count; ++id) {
      if (!bank.contains(id)) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "%s is missing bank-local expanded message ID %d", label, id));
      }
    }
    if (!document.contains("counts") || !document["counts"].is_object() ||
        !document["counts"].contains("expanded") ||
        !document["counts"]["expanded"].is_number_integer() ||
        document["counts"]["expanded"].get<int>() != expected_count ||
        !document["counts"].contains("vanilla") ||
        !document["counts"]["vanilla"].is_number_integer() ||
        document["counts"]["vanilla"].get<int>() != 0) {
      return absl::FailedPreconditionError(
          absl::StrFormat("%s counts must declare vanilla=0 and expanded=%d",
                          label, expected_count));
    }
  }
  return bank;
}

std::string SerializeCanonicalBundle(const ExpandedSourceBank& bank) {
  Json document;
  document["format"] = "yaze-message-bundle";
  document["version"] = kMessageBundleVersion;
  document["counts"] = {{"vanilla", 0}, {"expanded", bank.size()}};
  document["messages"] = Json::array();
  for (const auto& [id, message] : bank) {
    document["messages"].push_back(
        {{"id", id}, {"bank", "expanded"}, {"text", message.text}});
  }
  return document.dump(2) + "\n";
}

std::string RenderAsmBody(const ExpandedSourceBank& bank,
                          int first_expanded_id) {
  std::string output;
  for (const auto& [local_id, message] : bank) {
    absl::StrAppend(&output, absl::StrFormat("Message_%03X:\n",
                                             first_expanded_id + local_id));
    std::vector<uint8_t> terminated = message.bytes;
    terminated.push_back(kMessageTerminator);
    for (size_t offset = 0; offset < terminated.size(); offset += 16) {
      absl::StrAppend(&output, "  db ");
      const size_t line_end = std::min(terminated.size(), offset + 16);
      for (size_t index = offset; index < line_end; ++index) {
        if (index != offset) {
          absl::StrAppend(&output, ", ");
        }
        absl::StrAppend(&output, absl::StrFormat("$%02X", terminated[index]));
      }
      absl::StrAppend(&output, "\n");
    }
    absl::StrAppend(&output, "\n");
  }
  absl::StrAppend(&output, "db $FF\n");
  return output;
}

std::string RenderAsmInclude(const ExpandedSourceBank& bank,
                             int first_expanded_id,
                             std::string_view source_sha256) {
  const std::string body = RenderAsmBody(bank, first_expanded_id);
  return absl::StrFormat(
             "; Source bundle SHA-256: %s\n"
             "; Generated ASM body SHA-256: %s\n"
             "; Generated by yaze message-source-sync. Do not edit.\n\n",
             source_sha256, Sha256Hex(body)) +
         body;
}

bool IsCanonicalMappedLoRomAddress(uint32_t address) {
  const uint8_t bank = static_cast<uint8_t>((address >> 16) & 0xFFu);
  return bank != 0x7E && bank != 0x7F && (address & 0xFFFFu) >= 0x8000u &&
         PcToSnes(SnesToPc(address)) == address;
}

absl::StatusOr<size_t> ValidateLayoutAndGetCapacity(
    const core::MessageLayout& layout) {
  if (layout.expanded_count <= 0 ||
      layout.last_expanded_id < layout.first_expanded_id) {
    return absl::FailedPreconditionError(
        "Hack manifest must define a non-empty expanded message range");
  }
  const int range_count =
      static_cast<int>(layout.last_expanded_id - layout.first_expanded_id) + 1;
  if (layout.expanded_count != range_count) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Hack manifest expanded range is not contiguous: first=0x%03X, "
        "last=0x%03X implies %d messages, but count=%d",
        layout.first_expanded_id, layout.last_expanded_id, range_count,
        layout.expanded_count));
  }
  if (!IsCanonicalMappedLoRomAddress(layout.data_start) ||
      !IsCanonicalMappedLoRomAddress(layout.data_end)) {
    return absl::FailedPreconditionError(
        "Hack manifest expanded data range must use canonical mapped LoROM "
        "addresses");
  }
  const uint32_t start_pc = SnesToPc(layout.data_start);
  const uint32_t end_pc = SnesToPc(layout.data_end);
  if (end_pc < start_pc) {
    return absl::FailedPreconditionError(
        "Hack manifest expanded data range ends before it starts");
  }
  return static_cast<size_t>(end_pc - start_pc) + 1;
}

bool IsWithinRoot(const fs::path& root, const fs::path& candidate) {
  const fs::path relative = candidate.lexically_relative(root);
  if (relative.empty() || relative.is_absolute()) {
    return false;
  }
  const auto first = relative.begin();
  return first != relative.end() && *first != "..";
}

absl::StatusOr<fs::path> ResolveProjectTarget(
    const fs::path& project_root, const std::string& configured_path,
    bool must_exist, absl::string_view label) {
  const fs::path relative(configured_path);
  if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s must be a non-empty project-relative path", label));
  }

  const fs::path joined = (project_root / relative).lexically_normal();
  std::error_code status_ec;
  const auto link_status = fs::symlink_status(joined, status_ec);
  if (status_ec != std::errc::no_such_file_or_directory && status_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect %s path %s: %s", label, joined.string(),
                        status_ec.message()));
  }
  if (!status_ec && fs::is_symlink(link_status)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "%s may not be a symbolic link: %s", label, joined.string()));
  }

  std::error_code canonical_ec;
  fs::path resolved = must_exist ? fs::canonical(joined, canonical_ec)
                                 : fs::weakly_canonical(joined, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve %s path %s: %s", label, joined.string(),
                        canonical_ec.message()));
  }
  resolved = resolved.lexically_normal();
  if (!IsWithinRoot(project_root, resolved)) {
    return absl::PermissionDeniedError(
        absl::StrFormat("%s escapes project root %s: %s", label,
                        project_root.string(), resolved.string()));
  }

  std::error_code parent_ec;
  const auto parent_status = fs::status(resolved.parent_path(), parent_ec);
  if (parent_ec || !fs::is_directory(parent_status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s parent directory must already exist: %s", label,
                        resolved.parent_path().string()));
  }
  if (must_exist) {
    std::error_code file_ec;
    if (!fs::is_regular_file(resolved, file_ec) || file_ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s must be an existing regular file: %s", label, resolved.string()));
    }
  } else if (!status_ec && fs::exists(link_status) &&
             !fs::is_regular_file(link_status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s must be a regular file or a new path: %s", label,
                        resolved.string()));
  }
  return resolved;
}

absl::StatusOr<fs::path> CanonicalProjectRoot(
    const project::YazeProject& project) {
  if (project.filepath.empty()) {
    return absl::FailedPreconditionError(
        "Project has no descriptor path; open the project before source sync");
  }
  std::error_code ec;
  fs::path descriptor = fs::absolute(project.filepath, ec);
  if (ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project descriptor %s: %s",
                        project.filepath, ec.message()));
  }
  fs::path root = fs::canonical(descriptor.parent_path(), ec);
  if (ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot resolve project root for %s: %s",
                        project.filepath, ec.message()));
  }
  return root.lexically_normal();
}

std::string LowercasePath(const fs::path& path) {
  return absl::AsciiStrToLower(path.generic_string());
}

absl::Status ValidateDistinctTargets(const fs::path& bundle_path,
                                     const fs::path& include_path) {
  if (bundle_path == include_path ||
      LowercasePath(bundle_path) == LowercasePath(include_path)) {
    return absl::InvalidArgumentError(
        "Canonical bundle and generated ASM include paths must be distinct");
  }
  std::error_code exists_ec;
  const bool include_exists = fs::exists(include_path, exists_ec);
  if (exists_ec) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Cannot inspect generated ASM include path: %s", exists_ec.message()));
  }
  if (include_exists) {
    std::error_code equivalent_ec;
    if (fs::equivalent(bundle_path, include_path, equivalent_ec)) {
      return absl::InvalidArgumentError(
          "Canonical bundle and generated ASM include paths alias each other");
    }
    if (equivalent_ec) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Cannot compare source publication paths: %s",
                          equivalent_ec.message()));
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateTargetsDoNotAliasLock(const fs::path& lock_path,
                                           const fs::path& bundle_path,
                                           const fs::path& include_path) {
  for (const fs::path* target : {&bundle_path, &include_path}) {
    if (*target == lock_path ||
        LowercasePath(*target) == LowercasePath(lock_path)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Message source targets may not use the persistent lock path: %s",
          lock_path.string()));
    }
  }

  std::error_code lock_exists_ec;
  const bool lock_exists = fs::exists(lock_path, lock_exists_ec);
  if (lock_exists_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect persistent message source lock %s: %s",
                        lock_path.string(), lock_exists_ec.message()));
  }
  if (!lock_exists) {
    return absl::OkStatus();
  }

  for (const fs::path* target : {&bundle_path, &include_path}) {
    std::error_code target_exists_ec;
    const bool target_exists = fs::exists(*target, target_exists_ec);
    if (target_exists_ec) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Cannot inspect message source target %s: %s",
                          target->string(), target_exists_ec.message()));
    }
    if (!target_exists) {
      continue;
    }
    std::error_code equivalent_ec;
    const bool equivalent = fs::equivalent(lock_path, *target, equivalent_ec);
    if (equivalent_ec) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot compare message source target %s with persistent lock %s: %s",
          target->string(), lock_path.string(), equivalent_ec.message()));
    }
    if (equivalent) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Message source target aliases the persistent lock path: %s",
          target->string()));
    }
  }
  return absl::OkStatus();
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

absl::Status CleanupPublicationPaths(const std::vector<fs::path>& paths,
                                     absl::string_view artifact_kind) {
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
        "Could not remove message source %s: %s", artifact_kind, failures));
  }
  return absl::OkStatus();
}

absl::Status CleanupTemps(const std::vector<PublicationArtifact>& artifacts) {
  std::vector<fs::path> paths;
  paths.reserve(artifacts.size());
  for (const auto& artifact : artifacts) {
    if (!artifact.temp.empty()) {
      paths.push_back(artifact.temp);
    }
  }
  return CleanupPublicationPaths(paths, "temporary files");
}

absl::Status CleanupBackups(const std::vector<PublicationArtifact>& artifacts) {
  std::vector<fs::path> paths;
  paths.reserve(artifacts.size());
  for (const auto& artifact : artifacts) {
    if (artifact.backup.has_value()) {
      paths.push_back(*artifact.backup);
    }
  }
  return CleanupPublicationPaths(paths, "backup files");
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
    const std::vector<PublicationArtifact>& artifacts,
    const absl::Status& publication_failure) {
  const absl::Status temp_cleanup = CleanupTemps(artifacts);
  const absl::Status backup_cleanup = CleanupBackups(artifacts);
  if (temp_cleanup.ok() && backup_cleanup.ok()) {
    return publication_failure;
  }
  return absl::DataLossError(absl::StrFormat(
      "Message source publication failed (%s), then artifact cleanup failed "
      "(temporary files: %s; backup files: %s)",
      publication_failure.message(), CleanupStatusSummary(temp_cleanup),
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
                                 const absl::Status& publication_failure) {
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
  const absl::Status temp_cleanup = CleanupTemps(*artifacts);
  if (!rollback_failure.ok()) {
    return absl::DataLossError(absl::StrFormat(
        "Message source publication failed (%s) and rollback failed (%s). "
        "Temporary-file cleanup: %s. Preserved recovery backups: %s",
        publication_failure.message(), rollback_failure.message(),
        CleanupStatusSummary(temp_cleanup), BackupRecoveryPaths(*artifacts)));
  }
  const absl::Status backup_cleanup = CleanupBackups(*artifacts);
  if (!temp_cleanup.ok() || !backup_cleanup.ok()) {
    return absl::DataLossError(absl::StrFormat(
        "Message source publication failed (%s); rollback completed, but "
        "artifact cleanup failed (temporary files: %s; backup files: %s)",
        publication_failure.message(), CleanupStatusSummary(temp_cleanup),
        CleanupStatusSummary(backup_cleanup)));
  }
  return publication_failure;
}

absl::Status VerifyUnchangedBeforePublication(
    const std::vector<PublicationArtifact>& artifacts,
    std::string_view expected_source_sha256) {
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
    return absl::AbortedError(
        "Canonical message source changed after SHA-256 preflight");
  }
  return absl::OkStatus();
}

absl::Status PublishArtifactSet(std::vector<PublicationArtifact>* artifacts,
                                std::string_view expected_source_sha256) {
  for (auto& artifact : *artifacts) {
    auto temp_or = WriteExclusiveTemp(artifact.target, artifact.content);
    if (!temp_or.ok()) {
      return ReturnFailureAfterCleanup(*artifacts, temp_or.status());
    }
    artifact.temp = std::move(*temp_or);
    if (artifact.original_content.has_value()) {
      auto backup_or = CreateBackup(artifact.target);
      if (!backup_or.ok()) {
        return ReturnFailureAfterCleanup(*artifacts, backup_or.status());
      }
      artifact.backup = std::move(*backup_or);
    }
  }

  const absl::Status unchanged_status =
      VerifyUnchangedBeforePublication(*artifacts, expected_source_sha256);
  if (!unchanged_status.ok()) {
    return ReturnFailureAfterCleanup(*artifacts, unchanged_status);
  }

  for (auto& artifact : *artifacts) {
    bool replaced = false;
    const absl::Status replace_status =
        ReplaceFromTemp(artifact.temp, artifact.target, &replaced);
    artifact.published = replaced;
    if (!replace_status.ok()) {
      return RollBackPublication(artifacts, replace_status);
    }
    artifact.temp.clear();
  }

  for (const auto& artifact : *artifacts) {
    std::string reopened;
    auto reopened_or = ReadTextFile(artifact.target, "published message file");
    if (!reopened_or.ok()) {
      return RollBackPublication(artifacts, reopened_or.status());
    }
    reopened = std::move(*reopened_or);
    if (reopened != artifact.content) {
      return RollBackPublication(
          artifacts, absl::DataLossError(absl::StrFormat(
                         "Published message file failed exact readback: %s",
                         artifact.target.string())));
    }
  }
  const absl::Status temp_cleanup = CleanupTemps(*artifacts);
  if (!temp_cleanup.ok()) {
    return RollBackPublication(artifacts, temp_cleanup);
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> NormalizeExpectedSha256(std::string hash) {
  if (hash.size() != 64 ||
      !std::all_of(hash.begin(), hash.end(),
                   [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return absl::InvalidArgumentError(
        "--expected-source-sha256 must be exactly 64 hexadecimal characters");
  }
  return absl::AsciiStrToLower(std::move(hash));
}

}  // namespace

std::string ComputeMessageSourceSha256(std::string_view content) {
  return Sha256Hex(content);
}

absl::StatusOr<MessageSourceSyncResult> SyncMessageSource(
    const project::YazeProject& project, const fs::path& incoming_bundle_path,
    const MessageSourceSyncOptions& options) {
  if (!project.hack_manifest.loaded()) {
    return absl::FailedPreconditionError(
        "Project must load a hack manifest before message source sync");
  }
#if defined(__EMSCRIPTEN__)
  if (options.write) {
    return absl::FailedPreconditionError(
        "message-source-sync --write is unavailable in browser builds because "
        "durable atomic filesystem publication cannot be guaranteed");
  }
#endif
  const core::MessageLayout& layout = project.hack_manifest.message_layout();
  if (!layout.source.has_value()) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define messages.source");
  }

  size_t capacity = 0;
  ASSIGN_OR_RETURN(capacity, ValidateLayoutAndGetCapacity(layout));
  fs::path project_root;
  ASSIGN_OR_RETURN(project_root, CanonicalProjectRoot(project));

  fs::path canonical_bundle_path;
  ASSIGN_OR_RETURN(
      canonical_bundle_path,
      ResolveProjectTarget(project_root, layout.source->canonical_bundle_path,
                           /*must_exist=*/true, "Canonical message bundle"));
  fs::path asm_include_path;
  ASSIGN_OR_RETURN(asm_include_path,
                   ResolveProjectTarget(
                       project_root, layout.source->generated_asm_include_path,
                       /*must_exist=*/false, "Generated message ASM include"));
  RETURN_IF_ERROR(
      ValidateDistinctTargets(canonical_bundle_path, asm_include_path));
  const fs::path source_lock_path = project_root / kSourceSyncLockFilename;
  RETURN_IF_ERROR(ValidateTargetsDoNotAliasLock(
      source_lock_path, canonical_bundle_path, asm_include_path));

  std::error_code incoming_ec;
  const fs::path resolved_incoming =
      fs::canonical(incoming_bundle_path, incoming_ec);
  if (incoming_ec || !fs::is_regular_file(resolved_incoming, incoming_ec) ||
      incoming_ec) {
    return absl::NotFoundError(absl::StrFormat(
        "Incoming message bundle must be an existing regular file: %s",
        incoming_bundle_path.string()));
  }

  std::string expected_sha;
  std::unique_ptr<MessageSourceWriteLock> write_lock;
  if (options.write) {
    if (options.expected_source_sha256.empty()) {
      return absl::InvalidArgumentError(
          "--write requires --expected-source-sha256");
    }
    ASSIGN_OR_RETURN(expected_sha,
                     NormalizeExpectedSha256(options.expected_source_sha256));
    ASSIGN_OR_RETURN(write_lock, MessageSourceWriteLock::Acquire(project_root));
  }

  std::string canonical_before;
  ASSIGN_OR_RETURN(canonical_before, ReadTextFile(canonical_bundle_path,
                                                  "canonical message bundle"));
  const std::string source_sha_before = Sha256Hex(canonical_before);
  Json canonical_document;
  ASSIGN_OR_RETURN(
      canonical_document,
      ParseBundleDocument(canonical_before, "Canonical message bundle"));
  ExpandedSourceBank merged_bank;
  ASSIGN_OR_RETURN(
      merged_bank,
      ParseExpandedBank(canonical_document, layout.expanded_count,
                        /*require_complete=*/true, "Canonical message bundle"));
  const std::string expected_current_asm = RenderAsmInclude(
      merged_bank, layout.first_expanded_id, source_sha_before);

  std::string incoming_content;
  ASSIGN_OR_RETURN(incoming_content,
                   ReadTextFile(resolved_incoming, "incoming message bundle"));
  Json incoming_document;
  ASSIGN_OR_RETURN(
      incoming_document,
      ParseBundleDocument(incoming_content, "Incoming message bundle"));
  ExpandedSourceBank incoming_bank;
  ASSIGN_OR_RETURN(
      incoming_bank,
      ParseExpandedBank(incoming_document, layout.expanded_count,
                        /*require_complete=*/false, "Incoming message bundle"));
  for (auto& [id, message] : incoming_bank) {
    merged_bank[id] = std::move(message);
  }

  size_t encoded_size = 1;  // Final $FF.
  for (const auto& entry : merged_bank) {
    encoded_size += entry.second.bytes.size() + 1;  // Per-message $7F.
  }
  if (encoded_size > capacity) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Expanded message source needs %zu bytes including terminators, but "
        "manifest capacity is %zu bytes [0x%06X, 0x%06X]",
        encoded_size, capacity, layout.data_start, layout.data_end));
  }

  const std::string canonical_after = SerializeCanonicalBundle(merged_bank);
  const std::string proposed_source_sha = Sha256Hex(canonical_after);
  const std::string asm_after = RenderAsmInclude(
      merged_bank, layout.first_expanded_id, proposed_source_sha);

  std::optional<std::string> asm_before;
  std::error_code include_exists_ec;
  if (fs::exists(asm_include_path, include_exists_ec)) {
    std::string existing_include;
    ASSIGN_OR_RETURN(
        existing_include,
        ReadTextFile(asm_include_path, "generated message ASM include"));
    asm_before = std::move(existing_include);
  } else if (include_exists_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot inspect generated message ASM include: %s",
                        include_exists_ec.message()));
  }
  if (asm_before.has_value() && *asm_before != expected_current_asm) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Generated message ASM include drifted from the current canonical "
        "bundle: %s",
        asm_include_path.string()));
  }

  MessageSourceSyncResult result{
      .canonical_bundle_path = canonical_bundle_path,
      .generated_asm_include_path = asm_include_path,
      .first_expanded_id = layout.first_expanded_id,
      .last_expanded_id = layout.last_expanded_id,
      .expanded_count = layout.expanded_count,
      .incoming_updates = static_cast<int>(incoming_bank.size()),
      .encoded_size = encoded_size,
      .capacity = capacity,
      .source_sha256_before = source_sha_before,
      .proposed_source_sha256 = proposed_source_sha,
      .changed = canonical_before != canonical_after ||
                 !asm_before.has_value() || *asm_before != asm_after,
  };
  if (!options.write) {
    return result;
  }
  if (expected_sha != source_sha_before) {
    return absl::AbortedError(absl::StrFormat(
        "Canonical message source SHA-256 CAS failed: expected %s, got %s",
        expected_sha, source_sha_before));
  }
  if (!result.changed) {
    return result;
  }

  std::vector<PublicationArtifact> artifacts;
  artifacts.push_back(PublicationArtifact{
      .target = canonical_bundle_path,
      .content = canonical_after,
      .original_content = canonical_before,
  });
  artifacts.push_back(PublicationArtifact{
      .target = asm_include_path,
      .content = asm_after,
      .original_content = asm_before,
  });
  RETURN_IF_ERROR(PublishArtifactSet(&artifacts, expected_sha));

  // Reopen the canonical bundle through the strict source parser, not just as
  // bytes, before reporting a successful two-file publication.
  auto reopened_canonical_or =
      ReadTextFile(canonical_bundle_path, "published canonical message bundle");
  if (!reopened_canonical_or.ok()) {
    return RollBackPublication(&artifacts, reopened_canonical_or.status());
  }
  std::string reopened_canonical = std::move(*reopened_canonical_or);
  auto reopened_document_or = ParseBundleDocument(
      reopened_canonical, "Published canonical message bundle");
  if (!reopened_document_or.ok()) {
    return RollBackPublication(&artifacts, reopened_document_or.status());
  }
  Json reopened_document = std::move(*reopened_document_or);
  auto reopened_bank_or = ParseExpandedBank(
      reopened_document, layout.expanded_count,
      /*require_complete=*/true, "Published canonical message bundle");
  if (!reopened_bank_or.ok()) {
    return RollBackPublication(&artifacts, reopened_bank_or.status());
  }
  if (Sha256Hex(reopened_canonical) != proposed_source_sha) {
    return RollBackPublication(
        &artifacts,
        absl::DataLossError(
            "Published canonical message bundle SHA-256 readback failed"));
  }

  const absl::Status backup_cleanup = CleanupBackups(artifacts);
  if (!backup_cleanup.ok()) {
    return RollBackPublication(&artifacts, backup_cleanup);
  }
  result.wrote = true;
  return result;
}

}  // namespace yaze::editor
