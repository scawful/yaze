#include "core/version_manager.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace core {

namespace fs = std::filesystem;

VersionManager::VersionManager(project::YazeProject* project)
    : project_(project) {}

bool VersionManager::IsGitInitialized() const {
  if (!project_ || project_->git_repository.empty())
    return false;

  fs::path git_dir = fs::path(project_->git_repository) / ".git";
  return fs::exists(git_dir);
}

absl::Status VersionManager::InitializeGit() {
  if (!project_)
    return absl::InvalidArgumentError("No project context");

  // Ensure git repository path is set (default to project root)
  if (project_->git_repository.empty()) {
    project_->git_repository = ".";  // Default to current dir
  }

  if (IsGitInitialized())
    return absl::OkStatus();

  return GitInit();
}

absl::StatusOr<SnapshotResult> VersionManager::CreateSnapshot(
    const std::string& message) {
  if (!project_)
    return absl::InvalidArgumentError("No project context");

  SnapshotResult result;
  result.success = false;

  // 1. Ensure Git is ready
  if (!IsGitInitialized()) {
    auto status = InitializeGit();
    if (!status.ok())
      return status;
  }

  // 2. Commit Code
  auto add_status = GitAddAll();
  if (!add_status.ok())
    return add_status;

  auto commit_status = GitCommit(message);
  if (!commit_status.ok()) {
    // It's possible there were no changes to commit, which is fine.
    // We continue to ROM backup.
    LOG_INFO("VersionManager", "Git commit skipped (no changes?)");
  }

  result.commit_hash = GitRevParseHead();

  // 3. Backup ROM Artifact
  // Generate timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  std::string timestamp = ss.str();

  auto backup_status = BackupRomArtifact(timestamp);
  if (backup_status.ok()) {
    result.rom_backup_path = "backups/snapshots/rom_" + timestamp + ".sfc";
  } else {
    LOG_ERROR("VersionManager", "Failed to backup ROM: %s",
              backup_status.ToString().c_str());
  }

  result.success = true;
  result.message = "Snapshot created successfully.";

  // Update project build hash
  project_->last_build_hash = result.commit_hash;

  return result;
}

std::string VersionManager::GetCurrentHash() const {
  return GitRevParseHead();
}

std::vector<std::string> VersionManager::GetHistory(int limit) const {
  std::string cmd =
      absl::StrFormat("git log -n %d --pretty=format:\"%%h %%s\"", limit);
  auto output = RunCommandOutput(cmd);
  if (!output.ok())
    return {};

  // Split lines (basic)
  std::vector<std::string> history;
  std::istringstream stream(*output);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty())
      history.push_back(line);
  }
  return history;
}

// ============================================================================
// Git Implementation
// ============================================================================

absl::Status VersionManager::GitInit() {
  return RunCommand("git init");
}

absl::Status VersionManager::GitAddAll() {
  // We specifically want to avoid adding the ROMs if they aren't ignored.
  // But assuming user has .gitignore setup properly for Roms/*.sfc
  return RunCommand("git add .");
}

absl::Status VersionManager::GitCommit(const std::string& message) {
  std::string sanitized_msg = message;
  // Basic sanitization for shell command (replace quotes)
  std::replace(sanitized_msg.begin(), sanitized_msg.end(), '"', '\'');

  return RunCommand(absl::StrFormat("git commit -m \"%s\"", sanitized_msg));
}

std::string VersionManager::GitRevParseHead() const {
  auto res = RunCommandOutput("git rev-parse --short HEAD");
  if (res.ok()) {
    // Trim newline
    std::string hash = *res;
    hash.erase(hash.find_last_not_of(" \n\r\t") + 1);
    return hash;
  }
  return "";
}

// ============================================================================
// ROM Backup Implementation
// ============================================================================

absl::Status VersionManager::BackupRomArtifact(
    const std::string& timestamp_str) {
  if (project_->output_folder.empty() || project_->build_target.empty()) {
    return absl::FailedPreconditionError(
        "Project output folder or build target not defined");
  }

  fs::path rom_path(project_->build_target);

  // Resolve relative paths against project directory
  fs::path project_dir = fs::path(project_->filepath).parent_path();
  fs::path abs_rom_path = project_dir / rom_path;

  if (!fs::exists(abs_rom_path)) {
    return absl::NotFoundError(
        absl::StrFormat("Built ROM not found at %s", abs_rom_path.string()));
  }

  // Determine backup location
  fs::path backup_dir = project_dir / "backups" / "snapshots";
  std::error_code ec;
  fs::create_directories(backup_dir, ec);
  if (ec)
    return absl::InternalError("Could not create backup directory");

  std::string filename = absl::StrFormat("rom_%s.sfc", timestamp_str);
  fs::path backup_path = backup_dir / filename;

  // Copy file
  fs::copy_file(abs_rom_path, backup_path, fs::copy_options::overwrite_existing,
                ec);
  if (ec) {
    return absl::InternalError(
        absl::StrFormat("Failed to copy ROM: %s", ec.message()));
  }

  return absl::OkStatus();
}

// ============================================================================
// System Helpers
// ============================================================================

absl::Status VersionManager::RunCommand(const std::string& cmd) {
#if defined(YAZE_IOS)
  (void)cmd;
  return absl::FailedPreconditionError(
      "Command execution is not available on iOS");
#else
  // Execute command in project root
  std::string full_cmd;

  fs::path project_dir = fs::path(project_->filepath).parent_path();
  if (!project_dir.empty()) {
    full_cmd = absl::StrFormat("cd \"%s\" && %s", project_dir.string(), cmd);
  } else {
    full_cmd = cmd;
  }

  int ret = std::system(full_cmd.c_str());
  if (ret != 0) {
    return absl::InternalError(
        absl::StrFormat("Command failed: %s (Exit code %d)", cmd, ret));
  }
  return absl::OkStatus();
#endif
}

absl::StatusOr<std::string> VersionManager::RunCommandOutput(
    const std::string& cmd) const {
#if defined(YAZE_IOS)
  (void)cmd;
  return absl::FailedPreconditionError(
      "Command execution is not available on iOS");
#else
  std::string full_cmd;
  fs::path project_dir = fs::path(project_->filepath).parent_path();
  if (!project_dir.empty()) {
    full_cmd = absl::StrFormat("cd \"%s\" && %s", project_dir.string(), cmd);
  } else {
    full_cmd = cmd;
  }

  std::array<char, 128> buffer;
  std::string result;
#ifdef _WIN32
  std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(full_cmd.c_str(), "r"),
                                                 _pclose);
#else
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(full_cmd.c_str(), "r"),
                                                pclose);
#endif
  if (!pipe) {
    return absl::InternalError("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
#endif
}

}  // namespace core
}  // namespace yaze
