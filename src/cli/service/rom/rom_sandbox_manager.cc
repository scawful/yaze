#include "cli/service/rom/rom_sandbox_manager.h"

#include <algorithm>
#include <cstdlib>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/macro.h"
#include "util/platform_paths.h"

namespace yaze {
namespace cli {

namespace {

std::filesystem::path DetermineDefaultRoot() {
  if (const char* env_root = std::getenv("YAZE_SANDBOX_ROOT")) {
    return std::filesystem::path(env_root);
  }
  auto app_data = util::PlatformPaths::GetAppDataSubdirectory("sandboxes");
  if (app_data.ok()) {
    return *app_data;
  }
  std::error_code ec;
  auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    // Fallback to current working directory if temp is unavailable.
    return std::filesystem::current_path() / "yaze" / "sandboxes";
  }
  return temp_dir / "yaze" / "sandboxes";
}

std::filesystem::path ResolveUniqueDirectory(const std::filesystem::path& root,
                                             absl::string_view id) {
  return root / std::string(id);
}

}  // namespace

RomSandboxManager& RomSandboxManager::Instance() {
  static RomSandboxManager* instance = new RomSandboxManager();
  return *instance;
}

RomSandboxManager::RomSandboxManager()
    : root_directory_(DetermineDefaultRoot()) {}

void RomSandboxManager::SetRootDirectory(const std::filesystem::path& root) {
  std::lock_guard<std::mutex> lock(mutex_);
  root_directory_ = root;
  (void)EnsureRootExistsLocked();
}

const std::filesystem::path& RomSandboxManager::RootDirectory() const {
  return root_directory_;
}

absl::Status RomSandboxManager::EnsureRootExistsLocked() {
  std::error_code ec;
  if (!std::filesystem::exists(root_directory_, ec)) {
    if (!std::filesystem::create_directories(root_directory_, ec) && ec) {
      return absl::InternalError(
          absl::StrCat("Failed to create sandbox root at ",
                       root_directory_.string(), ": ", ec.message()));
    }
  }
  return absl::OkStatus();
}

std::string RomSandboxManager::GenerateSandboxIdLocked() {
  absl::Time now = absl::Now();
  std::string time_component =
      absl::FormatTime("%Y%m%dT%H%M%S", now, absl::LocalTimeZone());
  ++sequence_;
  return absl::StrCat(time_component, "-", sequence_);
}

absl::StatusOr<RomSandboxManager::SandboxMetadata>
RomSandboxManager::CreateSandbox(Rom& rom, absl::string_view description) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError(
        "Cannot create sandbox: ROM is not loaded");
  }

  std::filesystem::path source_path(rom.filename());
  if (source_path.empty()) {
    return absl::FailedPreconditionError(
        "Cannot create sandbox: ROM filename is empty");
  }

  std::unique_lock<std::mutex> lock(mutex_);
  RETURN_IF_ERROR(EnsureRootExistsLocked());

  std::string id = GenerateSandboxIdLocked();
  std::filesystem::path sandbox_dir =
      ResolveUniqueDirectory(root_directory_, id);
  lock.unlock();

  std::error_code ec;
  if (!std::filesystem::create_directories(sandbox_dir, ec) && ec) {
    return absl::InternalError(
        absl::StrCat("Failed to create sandbox directory at ",
                     sandbox_dir.string(), ": ", ec.message()));
  }

  std::filesystem::path sandbox_rom_path = sandbox_dir / source_path.filename();

  Rom::SaveSettings settings;
  settings.filename = sandbox_rom_path.string();
  settings.save_new = false;
  settings.backup = false;

  absl::Status save_status = rom.SaveToFile(settings);
  if (!save_status.ok()) {
    std::error_code cleanup_ec;
    std::filesystem::remove_all(sandbox_dir, cleanup_ec);
    return save_status;
  }

  lock.lock();
  sandboxes_[id] = SandboxMetadata{
      .id = id,
      .directory = sandbox_dir,
      .rom_path = sandbox_rom_path,
      .source_rom = source_path.string(),
      .description = std::string(description),
      .created_at = absl::Now(),
  };
  active_sandbox_id_ = id;

  return sandboxes_.at(id);
}

absl::StatusOr<RomSandboxManager::SandboxMetadata>
RomSandboxManager::ActiveSandbox() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!active_sandbox_id_.has_value()) {
    return absl::NotFoundError("No active sandbox");
  }
  auto it = sandboxes_.find(*active_sandbox_id_);
  if (it == sandboxes_.end()) {
    return absl::NotFoundError("Active sandbox metadata missing");
  }
  return it->second;
}

absl::StatusOr<std::filesystem::path> RomSandboxManager::ActiveSandboxRomPath()
    const {
  ASSIGN_OR_RETURN(auto meta, ActiveSandbox());
  return meta.rom_path;
}

std::vector<RomSandboxManager::SandboxMetadata>
RomSandboxManager::ListSandboxes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<SandboxMetadata> list;
  list.reserve(sandboxes_.size());
  for (const auto& [_, metadata] : sandboxes_) {
    list.push_back(metadata);
  }
  std::sort(list.begin(), list.end(),
            [](const SandboxMetadata& a, const SandboxMetadata& b) {
              return a.created_at < b.created_at;
            });
  return list;
}

absl::Status RomSandboxManager::RemoveSandbox(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = sandboxes_.find(id);
  if (it == sandboxes_.end()) {
    return absl::NotFoundError("Sandbox not found");
  }
  std::error_code ec;
  std::filesystem::remove_all(it->second.directory, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrCat("Failed to remove sandbox directory: ", ec.message()));
  }
  sandboxes_.erase(it);
  if (active_sandbox_id_.has_value() && *active_sandbox_id_ == id) {
    active_sandbox_id_.reset();
  }
  return absl::OkStatus();
}

absl::StatusOr<int> RomSandboxManager::CleanupOlderThan(
    absl::Duration max_age) {
  std::vector<std::string> to_remove;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    absl::Time threshold = absl::Now() - max_age;
    for (const auto& [id, metadata] : sandboxes_) {
      if (metadata.created_at < threshold) {
        to_remove.push_back(id);
      }
    }
  }

  int removed = 0;
  for (const auto& id : to_remove) {
    absl::Status status = RemoveSandbox(id);
    if (!status.ok()) {
      return status;
    }
    ++removed;
  }
  return removed;
}

}  // namespace cli
}  // namespace yaze
