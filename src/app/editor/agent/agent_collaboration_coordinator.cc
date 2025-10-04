#include "app/editor/agent/agent_collaboration_coordinator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "app/core/platform/file_dialog.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

namespace {

std::filesystem::path ExpandUserPath(std::string path) {
  if (!path.empty() && path.front() == '~') {
    const char* home = nullptr;
#ifdef _WIN32
    home = std::getenv("USERPROFILE");
#else
    home = std::getenv("HOME");
#endif
    if (home != nullptr) {
      path.replace(0, 1, home);
    }
  }
  return std::filesystem::path(path);
}

std::string Trimmed(const std::string& value) {
  return std::string(absl::StripAsciiWhitespace(value));
}

}  // namespace

AgentCollaborationCoordinator::AgentCollaborationCoordinator()
    : local_user_(LocalUserName()) {}

absl::StatusOr<AgentCollaborationCoordinator::SessionInfo>
AgentCollaborationCoordinator::HostSession(const std::string& session_name) {
  const std::string trimmed = Trimmed(session_name);
  if (trimmed.empty()) {
    return absl::InvalidArgumentError("Session name cannot be empty");
  }

  RETURN_IF_ERROR(EnsureDirectory());

  SessionFileData data;
  data.session_name = trimmed;
  data.session_code = GenerateSessionCode();
  data.host = local_user_;
  data.participants.push_back(local_user_);

  std::filesystem::path path = SessionFilePath(data.session_code);

  // Collision avoidance (extremely unlikely but cheap to guard against).
  int attempts = 0;
  while (std::filesystem::exists(path) && attempts++ < 5) {
    data.session_code = GenerateSessionCode();
    path = SessionFilePath(data.session_code);
  }
  if (std::filesystem::exists(path)) {
    return absl::InternalError(
        "Unable to allocate a new collaboration session code");
  }

  RETURN_IF_ERROR(WriteSessionFile(path, data));

  active_ = true;
  hosting_ = true;
  session_id_ = data.session_code;
  session_name_ = data.session_name;

  SessionInfo info;
  info.session_id = data.session_code;
  info.session_name = data.session_name;
  info.participants = data.participants;
  return info;
}

absl::StatusOr<AgentCollaborationCoordinator::SessionInfo>
AgentCollaborationCoordinator::JoinSession(const std::string& session_code) {
  const std::string normalized = NormalizeSessionCode(session_code);
  if (normalized.empty()) {
    return absl::InvalidArgumentError("Session code cannot be empty");
  }

  RETURN_IF_ERROR(EnsureDirectory());

  std::filesystem::path path = SessionFilePath(normalized);
  ASSIGN_OR_RETURN(SessionFileData data, LoadSessionFile(path));

  const auto already_joined = std::find(data.participants.begin(),
                                        data.participants.end(), local_user_);
  if (already_joined == data.participants.end()) {
    data.participants.push_back(local_user_);
    RETURN_IF_ERROR(WriteSessionFile(path, data));
  }

  active_ = true;
  hosting_ = false;
  session_id_ = data.session_code.empty() ? normalized : data.session_code;
  session_name_ = data.session_name.empty() ? session_id_ : data.session_name;

  SessionInfo info;
  info.session_id = session_id_;
  info.session_name = session_name_;
  info.participants = data.participants;
  return info;
}

absl::Status AgentCollaborationCoordinator::LeaveSession() {
  if (!active_) {
    return absl::FailedPreconditionError("No collaborative session active");
  }

  const std::filesystem::path path = SessionFilePath(session_id_);
  absl::Status status = absl::OkStatus();

  if (hosting_) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
      status = absl::InternalError(
          absl::StrFormat("Failed to clean up session file: %s", ec.message()));
    }
  } else {
    auto data_or = LoadSessionFile(path);
    if (data_or.ok()) {
      SessionFileData data = std::move(data_or.value());
      auto end = std::remove(data.participants.begin(), data.participants.end(),
                             local_user_);
      data.participants.erase(end, data.participants.end());

      if (data.participants.empty()) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) {
          status = absl::InternalError(absl::StrFormat(
              "Failed to remove empty session file: %s", ec.message()));
        }
      } else {
        status = WriteSessionFile(path, data);
      }
    } else {
      // If the session file has already disappeared, treat it as success.
      status = absl::OkStatus();
    }
  }

  active_ = false;
  hosting_ = false;
  session_id_.clear();
  session_name_.clear();

  return status;
}

absl::StatusOr<AgentCollaborationCoordinator::SessionInfo>
AgentCollaborationCoordinator::RefreshSession() {
  if (!active_) {
    return absl::FailedPreconditionError("No collaborative session active");
  }

  const std::filesystem::path path = SessionFilePath(session_id_);
  auto data_or = LoadSessionFile(path);
  if (!data_or.ok()) {
    absl::Status status = data_or.status();
    if (absl::IsNotFound(status)) {
      active_ = false;
      hosting_ = false;
      session_id_.clear();
      session_name_.clear();
    }
    return status;
  }

  SessionFileData data = std::move(data_or.value());
  session_name_ = data.session_name.empty() ? session_id_ : data.session_name;
  SessionInfo info;
  info.session_id = session_id_;
  info.session_name = session_name_;
  info.participants = data.participants;
  return info;
}

absl::Status AgentCollaborationCoordinator::EnsureDirectory() const {
  std::error_code ec;
  std::filesystem::create_directories(SessionsDirectory(), ec);
  if (ec) {
    return absl::InternalError(absl::StrFormat(
        "Failed to create collaboration directory: %s", ec.message()));
  }
  return absl::OkStatus();
}

std::string AgentCollaborationCoordinator::LocalUserName() const {
  const char* override_name = std::getenv("YAZE_USER_NAME");
  const char* user = override_name != nullptr ? override_name : std::getenv("USER");
  if (user == nullptr) {
    user = std::getenv("USERNAME");
  }
  std::string base = (user != nullptr && std::strlen(user) > 0)
                         ? std::string(user)
                         : std::string("Player");

  const char* host = std::getenv("HOSTNAME");
#if defined(_WIN32)
  if (host == nullptr) {
    host = std::getenv("COMPUTERNAME");
  }
#endif
  if (host != nullptr && std::strlen(host) > 0) {
    return absl::StrCat(base, "@", host);
  }
  return base;
}

std::string AgentCollaborationCoordinator::NormalizeSessionCode(
    const std::string& input) const {
  std::string normalized = Trimmed(input);
  normalized.erase(std::remove_if(normalized.begin(), normalized.end(),
                                  [](unsigned char c) {
                                    return !std::isalnum(
                                        static_cast<unsigned char>(c));
                                  }),
                   normalized.end());
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(
                       std::toupper(static_cast<unsigned char>(c)));
                 });
  return normalized;
}

std::string AgentCollaborationCoordinator::GenerateSessionCode() const {
  static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<size_t> dist(0, sizeof(kAlphabet) - 2);

  std::string code(6, '0');
  for (char& ch : code) {
    ch = kAlphabet[dist(rng)];
  }
  return code;
}

std::filesystem::path AgentCollaborationCoordinator::SessionsDirectory() const {
  std::filesystem::path base = ExpandUserPath(core::GetConfigDirectory());
  if (base.empty()) {
    base = ExpandUserPath(".yaze");
  }
  return base / "agent" / "sessions";
}

std::filesystem::path AgentCollaborationCoordinator::SessionFilePath(
    const std::string& code) const {
  return SessionsDirectory() / (code + ".session");
}

absl::StatusOr<AgentCollaborationCoordinator::SessionFileData>
AgentCollaborationCoordinator::LoadSessionFile(
    const std::filesystem::path& path) const {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Session %s does not exist", path.string()));
  }

  SessionFileData data;
  data.session_code = path.stem().string();

  std::string line;
  while (std::getline(file, line)) {
    auto pos = line.find(':');
    if (pos == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, pos);
    std::string value = Trimmed(line.substr(pos + 1));
    if (key == "name") {
      data.session_name = value;
    } else if (key == "code") {
      data.session_code = NormalizeSessionCode(value);
    } else if (key == "host") {
      data.host = value;
      data.participants.push_back(value);
    } else if (key == "participant") {
      if (std::find(data.participants.begin(), data.participants.end(), value) ==
          data.participants.end()) {
        data.participants.push_back(value);
      }
    }
  }

  if (data.session_name.empty()) {
    data.session_name = data.session_code;
  }
  if (!data.host.empty()) {
    auto host_it = std::find(data.participants.begin(), data.participants.end(),
                             data.host);
    if (host_it == data.participants.end()) {
      data.participants.insert(data.participants.begin(), data.host);
    } else if (host_it != data.participants.begin()) {
      std::rotate(data.participants.begin(), host_it,
                  std::next(host_it));
    }
  }

  return data;
}

absl::Status AgentCollaborationCoordinator::WriteSessionFile(
    const std::filesystem::path& path, const SessionFileData& data) const {
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to write session file: %s", path.string()));
  }

  file << "name:" << data.session_name << "\n";
  file << "code:" << data.session_code << "\n";
  file << "host:" << data.host << "\n";

  std::set<std::string> seen;
  seen.insert(data.host);
  for (const auto& participant : data.participants) {
    if (seen.insert(participant).second) {
      file << "participant:" << participant << "\n";
    }
  }

  file.flush();
  if (!file.good()) {
    return absl::InternalError(
        absl::StrFormat("Failed to flush session file: %s", path.string()));
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
