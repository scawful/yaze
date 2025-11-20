#include "cli/service/planning/proposal_registry.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"

#include "nlohmann/json.hpp"
#include "util/macro.h"

namespace yaze {
namespace cli {

namespace {

std::filesystem::path DetermineDefaultRoot() {
  if (const char* env_root = std::getenv("YAZE_PROPOSAL_ROOT")) {
    return std::filesystem::path(env_root);
  }
  std::error_code ec;
  auto temp_dir = std::filesystem::temp_directory_path(ec);
  if (ec) {
    return std::filesystem::current_path() / "yaze" / "proposals";
  }
  return temp_dir / "yaze" / "proposals";
}

std::string StatusToString(ProposalRegistry::ProposalStatus status) {
  switch (status) {
    case ProposalRegistry::ProposalStatus::kAccepted:
      return "accepted";
    case ProposalRegistry::ProposalStatus::kRejected:
      return "rejected";
    case ProposalRegistry::ProposalStatus::kPending:
    default:
      return "pending";
  }
}

ProposalRegistry::ProposalStatus ParseStatus(absl::string_view value) {
  std::string lower = absl::AsciiStrToLower(value);
  if (absl::StartsWith(lower, "accept")) {
    return ProposalRegistry::ProposalStatus::kAccepted;
  }
  if (absl::StartsWith(lower, "reject")) {
    return ProposalRegistry::ProposalStatus::kRejected;
  }
  return ProposalRegistry::ProposalStatus::kPending;
}

int64_t TimeToMillis(absl::Time time) {
  return absl::ToUnixMillis(time);
}

std::optional<absl::Time> OptionalTimeFromMillis(int64_t millis) {
  if (millis <= 0) {
    return std::nullopt;
  }
  return absl::FromUnixMillis(millis);
}

}  // namespace

ProposalRegistry& ProposalRegistry::Instance() {
  static ProposalRegistry* instance = new ProposalRegistry();
  return *instance;
}

ProposalRegistry::ProposalRegistry()
    : root_directory_(DetermineDefaultRoot()) {}

void ProposalRegistry::SetRootDirectory(const std::filesystem::path& root) {
  std::lock_guard<std::mutex> lock(mutex_);
  root_directory_ = root;
  (void)EnsureRootExistsLocked();
}

const std::filesystem::path& ProposalRegistry::RootDirectory() const {
  return root_directory_;
}

absl::Status ProposalRegistry::EnsureRootExistsLocked() {
  std::error_code ec;
  if (!std::filesystem::exists(root_directory_, ec)) {
    if (!std::filesystem::create_directories(root_directory_, ec) && ec) {
      return absl::InternalError(
          absl::StrCat("Failed to create proposal root at ",
                       root_directory_.string(), ": ", ec.message()));
    }
  }
  return absl::OkStatus();
}

absl::Status ProposalRegistry::LoadProposalsFromDiskLocked() {
  std::error_code ec;

  if (!std::filesystem::exists(root_directory_, ec)) {
    return absl::OkStatus();
  }

  for (const auto& entry :
       std::filesystem::directory_iterator(root_directory_, ec)) {
    if (ec) {
      break;
    }

    if (!entry.is_directory()) {
      continue;
    }

    const std::string proposal_id = entry.path().filename().string();
    if (proposals_.find(proposal_id) != proposals_.end()) {
      continue;
    }

    ProposalMetadata metadata;
    bool metadata_loaded = false;
    const std::filesystem::path metadata_path = entry.path() / "metadata.json";

    if (std::filesystem::exists(metadata_path, ec) && !ec) {
      std::ifstream metadata_file(metadata_path);
      if (metadata_file.is_open()) {
        try {
          nlohmann::json metadata_json;
          metadata_file >> metadata_json;

          metadata.id = metadata_json.value("id", proposal_id);
          if (metadata.id.empty()) {
            metadata.id = proposal_id;
          }
          metadata.sandbox_id = metadata_json.value("sandbox_id", "");

          if (metadata_json.contains("sandbox_directory") &&
              metadata_json["sandbox_directory"].is_string()) {
            metadata.sandbox_directory = std::filesystem::path(
                metadata_json["sandbox_directory"].get<std::string>());
          } else {
            metadata.sandbox_directory.clear();
          }

          if (metadata_json.contains("sandbox_rom_path") &&
              metadata_json["sandbox_rom_path"].is_string()) {
            metadata.sandbox_rom_path = std::filesystem::path(
                metadata_json["sandbox_rom_path"].get<std::string>());
          } else {
            metadata.sandbox_rom_path.clear();
          }

          metadata.description = metadata_json.value("description", "");
          metadata.prompt = metadata_json.value("prompt", "");
          metadata.status =
              ParseStatus(metadata_json.value("status", "pending"));

          int64_t created_at_millis = metadata_json.value<int64_t>(
              "created_at_millis", TimeToMillis(absl::Now()));
          metadata.created_at = absl::FromUnixMillis(created_at_millis);

          int64_t reviewed_at_millis =
              metadata_json.value<int64_t>("reviewed_at_millis", 0);
          metadata.reviewed_at = OptionalTimeFromMillis(reviewed_at_millis);

          std::string diff_path =
              metadata_json.value("diff_path", std::string("diff.txt"));
          std::string log_path =
              metadata_json.value("log_path", std::string("execution.log"));
          metadata.diff_path = entry.path() / diff_path;
          metadata.log_path = entry.path() / log_path;

          metadata.bytes_changed = metadata_json.value("bytes_changed", 0);
          metadata.commands_executed =
              metadata_json.value("commands_executed", 0);

          metadata.screenshots.clear();
          if (metadata_json.contains("screenshots") &&
              metadata_json["screenshots"].is_array()) {
            for (const auto& screenshot : metadata_json["screenshots"]) {
              if (screenshot.is_string()) {
                metadata.screenshots.emplace_back(
                    entry.path() / screenshot.get<std::string>());
              }
            }
          }

          if (metadata.sandbox_directory.empty() &&
              !metadata.sandbox_rom_path.empty()) {
            metadata.sandbox_directory =
                metadata.sandbox_rom_path.parent_path();
          }

          metadata_loaded = true;
        } catch (const std::exception& ex) {
          std::cerr << "Warning: Failed to parse metadata for proposal "
                    << proposal_id << ": " << ex.what() << "\n";
        }
      }
    }

    if (!metadata_loaded) {
      std::filesystem::path log_path = entry.path() / "execution.log";
      if (!std::filesystem::exists(log_path, ec) || ec) {
        continue;
      }

      std::filesystem::path diff_path = entry.path() / "diff.txt";

      absl::Time created_at = absl::Now();
      if (proposal_id.starts_with("proposal-")) {
        std::string time_str = proposal_id.substr(9, 15);
        std::string error;
        if (absl::ParseTime("%Y%m%dT%H%M%S", time_str, &created_at, &error)) {
          // Parsed successfully.
        }
      }

      auto ftime = std::filesystem::last_write_time(log_path, ec);
      if (!ec) {
        auto sctp =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        auto time_t_value = std::chrono::system_clock::to_time_t(sctp);
        created_at = absl::FromTimeT(time_t_value);
      }

      metadata = ProposalMetadata{
          .id = proposal_id,
          .sandbox_id = "",
          .sandbox_directory = std::filesystem::path(),
          .sandbox_rom_path = std::filesystem::path(),
          .description = "Loaded from disk",
          .prompt = "",
          .status = ProposalStatus::kPending,
          .created_at = created_at,
          .reviewed_at = std::nullopt,
          .diff_path = diff_path,
          .log_path = log_path,
          .screenshots = {},
          .bytes_changed = 0,
          .commands_executed = 0,
      };

      if (std::filesystem::exists(diff_path, ec) && !ec) {
        metadata.bytes_changed =
            static_cast<int>(std::filesystem::file_size(diff_path, ec));
      }

      for (const auto& file :
           std::filesystem::directory_iterator(entry.path(), ec)) {
        if (ec) {
          break;
        }
        if (file.path().extension() == ".png" ||
            file.path().extension() == ".jpg" ||
            file.path().extension() == ".jpeg") {
          metadata.screenshots.push_back(file.path());
        }
      }

      // Create a metadata file for legacy proposals so future loads are fast.
      absl::Status write_status = WriteMetadataLocked(metadata);
      if (!write_status.ok()) {
        std::cerr << "Warning: Failed to persist metadata for legacy proposal "
                  << proposal_id << ": " << write_status.message() << "\n";
      }
    }

    proposals_[metadata.id] = metadata;
  }

  return absl::OkStatus();
}

std::string ProposalRegistry::GenerateProposalIdLocked() {
  absl::Time now = absl::Now();
  std::string time_component =
      absl::FormatTime("%Y%m%dT%H%M%S", now, absl::LocalTimeZone());
  ++sequence_;
  return absl::StrCat("proposal-", time_component, "-", sequence_);
}

std::filesystem::path ProposalRegistry::ProposalDirectory(
    absl::string_view proposal_id) const {
  return root_directory_ / std::string(proposal_id);
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::CreateProposal(absl::string_view sandbox_id,
                                 const std::filesystem::path& sandbox_rom_path,
                                 absl::string_view prompt,
                                 absl::string_view description) {
  std::unique_lock<std::mutex> lock(mutex_);
  RETURN_IF_ERROR(EnsureRootExistsLocked());

  std::string id = GenerateProposalIdLocked();
  std::filesystem::path proposal_dir = ProposalDirectory(id);
  lock.unlock();

  std::error_code ec;
  if (!std::filesystem::create_directories(proposal_dir, ec) && ec) {
    return absl::InternalError(
        absl::StrCat("Failed to create proposal directory at ",
                     proposal_dir.string(), ": ", ec.message()));
  }

  lock.lock();
  proposals_[id] = ProposalMetadata{
      .id = id,
      .sandbox_id = std::string(sandbox_id),
      .sandbox_directory = sandbox_rom_path.empty()
                               ? std::filesystem::path()
                               : sandbox_rom_path.parent_path(),
      .sandbox_rom_path = sandbox_rom_path,
      .description = std::string(description),
      .prompt = std::string(prompt),
      .status = ProposalStatus::kPending,
      .created_at = absl::Now(),
      .reviewed_at = std::nullopt,
      .diff_path = proposal_dir / "diff.txt",
      .log_path = proposal_dir / "execution.log",
      .screenshots = {},
      .bytes_changed = 0,
      .commands_executed = 0,
  };

  RETURN_IF_ERROR(WriteMetadataLocked(proposals_.at(id)));

  return proposals_.at(id);
}

absl::Status ProposalRegistry::RecordDiff(const std::string& proposal_id,
                                          absl::string_view diff_content) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::ofstream diff_file(it->second.diff_path, std::ios::out);
  if (!diff_file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open diff file: ",
                                            it->second.diff_path.string()));
  }

  diff_file << diff_content;
  diff_file.close();

  // Update bytes_changed metric (rough estimate based on diff size)
  it->second.bytes_changed = static_cast<int>(diff_content.size());

  RETURN_IF_ERROR(WriteMetadataLocked(it->second));

  return absl::OkStatus();
}

absl::Status ProposalRegistry::AppendLog(const std::string& proposal_id,
                                         absl::string_view log_entry) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::ofstream log_file(it->second.log_path, std::ios::out | std::ios::app);
  if (!log_file.is_open()) {
    return absl::InternalError(absl::StrCat("Failed to open log file: ",
                                            it->second.log_path.string()));
  }

  log_file << absl::FormatTime("[%Y-%m-%d %H:%M:%S] ", absl::Now(),
                               absl::LocalTimeZone())
           << log_entry << "\n";
  log_file.close();

  return absl::OkStatus();
}

absl::Status ProposalRegistry::AddScreenshot(
    const std::string& proposal_id,
    const std::filesystem::path& screenshot_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  // Verify screenshot exists
  std::error_code ec;
  if (!std::filesystem::exists(screenshot_path, ec)) {
    return absl::NotFoundError(
        absl::StrCat("Screenshot file not found: ", screenshot_path.string()));
  }

  it->second.screenshots.push_back(screenshot_path);
  RETURN_IF_ERROR(WriteMetadataLocked(it->second));
  return absl::OkStatus();
}

absl::Status ProposalRegistry::UpdateCommandStats(
    const std::string& proposal_id, int commands_executed) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  it->second.commands_executed = commands_executed;
  RETURN_IF_ERROR(WriteMetadataLocked(it->second));
  return absl::OkStatus();
}

absl::Status ProposalRegistry::UpdateStatus(const std::string& proposal_id,
                                            ProposalStatus status) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  it->second.status = status;
  it->second.reviewed_at = absl::Now();
  RETURN_IF_ERROR(WriteMetadataLocked(it->second));
  return absl::OkStatus();
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::GetProposal(const std::string& proposal_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }
  return it->second;
}

std::vector<ProposalRegistry::ProposalMetadata> ProposalRegistry::ListProposals(
    std::optional<ProposalStatus> filter_status) const {
  std::unique_lock<std::mutex> lock(mutex_);

  // Load proposals from disk if we haven't already
  if (proposals_.empty()) {
    // Cast away const for loading - this is a lazy initialization pattern
    auto* self = const_cast<ProposalRegistry*>(this);
    auto status = self->LoadProposalsFromDiskLocked();
    if (!status.ok()) {
      // Log error but continue - return empty list if loading fails
      std::cerr << "Warning: Failed to load proposals from disk: "
                << status.message() << "\n";
    }
  }

  std::vector<ProposalMetadata> result;

  for (const auto& [id, metadata] : proposals_) {
    if (!filter_status.has_value() || metadata.status == *filter_status) {
      result.push_back(metadata);
    }
  }

  // Sort by creation time (newest first)
  std::sort(result.begin(), result.end(),
            [](const ProposalMetadata& a, const ProposalMetadata& b) {
              return a.created_at > b.created_at;
            });

  return result;
}

absl::StatusOr<ProposalRegistry::ProposalMetadata>
ProposalRegistry::GetLatestPendingProposal() const {
  std::lock_guard<std::mutex> lock(mutex_);

  const ProposalMetadata* latest = nullptr;
  for (const auto& [id, metadata] : proposals_) {
    if (metadata.status == ProposalStatus::kPending) {
      if (!latest || metadata.created_at > latest->created_at) {
        latest = &metadata;
      }
    }
  }

  if (!latest) {
    return absl::NotFoundError("No pending proposals found");
  }

  return *latest;
}

absl::Status ProposalRegistry::WriteMetadataLocked(
    const ProposalMetadata& metadata) const {
  std::filesystem::path proposal_dir = ProposalDirectory(metadata.id);
  std::error_code ec;
  if (!std::filesystem::exists(proposal_dir, ec) || ec) {
    return absl::NotFoundError(
        absl::StrCat("Proposal directory missing for ", metadata.id));
  }

  auto relative_to_proposal = [&](const std::filesystem::path& path) {
    if (path.empty()) {
      return std::string();
    }
    std::error_code relative_error;
    auto relative_path =
        std::filesystem::relative(path, proposal_dir, relative_error);
    if (!relative_error) {
      return relative_path.generic_string();
    }
    return path.generic_string();
  };

  nlohmann::json metadata_json;
  metadata_json["id"] = metadata.id;
  metadata_json["sandbox_id"] = metadata.sandbox_id;
  if (!metadata.sandbox_directory.empty()) {
    metadata_json["sandbox_directory"] =
        metadata.sandbox_directory.generic_string();
  }
  if (!metadata.sandbox_rom_path.empty()) {
    metadata_json["sandbox_rom_path"] =
        metadata.sandbox_rom_path.generic_string();
  }
  metadata_json["description"] = metadata.description;
  metadata_json["prompt"] = metadata.prompt;
  metadata_json["status"] = StatusToString(metadata.status);
  metadata_json["created_at_millis"] = TimeToMillis(metadata.created_at);
  metadata_json["reviewed_at_millis"] =
      metadata.reviewed_at.has_value() ? TimeToMillis(*metadata.reviewed_at)
                                       : int64_t{0};
  metadata_json["diff_path"] = relative_to_proposal(metadata.diff_path);
  metadata_json["log_path"] = relative_to_proposal(metadata.log_path);
  metadata_json["bytes_changed"] = metadata.bytes_changed;
  metadata_json["commands_executed"] = metadata.commands_executed;

  nlohmann::json screenshots_json = nlohmann::json::array();
  for (const auto& screenshot : metadata.screenshots) {
    screenshots_json.push_back(relative_to_proposal(screenshot));
  }
  metadata_json["screenshots"] = std::move(screenshots_json);

  std::ofstream metadata_file(proposal_dir / "metadata.json", std::ios::out);
  if (!metadata_file.is_open()) {
    return absl::InternalError(absl::StrCat(
        "Failed to write metadata file for proposal ", metadata.id));
  }

  metadata_file << metadata_json.dump(2);
  metadata_file.close();
  if (!metadata_file) {
    return absl::InternalError(absl::StrCat(
        "Failed to flush metadata file for proposal ", metadata.id));
  }

  return absl::OkStatus();
}

absl::Status ProposalRegistry::RemoveProposal(const std::string& proposal_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = proposals_.find(proposal_id);
  if (it == proposals_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Proposal not found: ", proposal_id));
  }

  std::filesystem::path proposal_dir = ProposalDirectory(proposal_id);
  std::error_code ec;
  std::filesystem::remove_all(proposal_dir, ec);
  if (ec) {
    return absl::InternalError(
        absl::StrCat("Failed to remove proposal directory: ", ec.message()));
  }

  proposals_.erase(it);
  return absl::OkStatus();
}

absl::StatusOr<int> ProposalRegistry::CleanupOlderThan(absl::Duration max_age) {
  std::lock_guard<std::mutex> lock(mutex_);
  absl::Time cutoff = absl::Now() - max_age;
  int removed_count = 0;

  std::vector<std::string> to_remove;
  for (const auto& [id, metadata] : proposals_) {
    if (metadata.created_at < cutoff) {
      to_remove.push_back(id);
    }
  }

  for (const auto& id : to_remove) {
    std::filesystem::path proposal_dir = ProposalDirectory(id);
    std::error_code ec;
    std::filesystem::remove_all(proposal_dir, ec);
    // Continue even if removal fails
    proposals_.erase(id);
    ++removed_count;
  }

  return removed_count;
}

}  // namespace cli
}  // namespace yaze
