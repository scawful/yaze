#ifndef YAZE_TEST_FRESH_ROM_FIXTURE_OUTPUT_H_
#define YAZE_TEST_FRESH_ROM_FIXTURE_OUTPUT_H_

#include <filesystem>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze::test {

// Run before loading or changing the source ROM. symlink_status also rejects
// dangling symlinks, which exists(path) would otherwise treat as a fresh path.
inline absl::Status ValidateFreshFixtureOutput(
    const std::filesystem::path& target) {
  if (target.empty() || target.filename().empty()) {
    return absl::InvalidArgumentError("Choose a fresh scratch ROM filename");
  }
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(target, ec);
  if (ec && ec != std::errc::no_such_file_or_directory) {
    return absl::UnavailableError(absl::StrCat(
        "Cannot inspect fixture output ", target.string(), ": ", ec.message()));
  }
  if (status.type() != std::filesystem::file_type::not_found) {
    return absl::AlreadyExistsError(absl::StrCat(
        "Fixture output already exists; choose a fresh scratch ROM: ",
        target.string()));
  }
  return absl::OkStatus();
}

inline absl::Status PublishFreshFixtureOutput(
    const std::filesystem::path& staged, const std::filesystem::path& target) {
  // Fails if any directory entry exists, including a symlink or a file created
  // after validation. Never remove the target to make publication succeed.
  std::error_code ec;
  std::filesystem::create_hard_link(staged, target, ec);
  if (ec) {
    return absl::UnavailableError(
        absl::StrCat("Cannot publish fresh fixture ROM ", target.string(), ": ",
                     ec.message()));
  }
  return absl::OkStatus();
}

// Save through the real ROM save path, then publish without replacement. The
// staging directory is on the target filesystem so the hard link is atomic.
// Filesystems without hard-link support fail closed; there is no overwrite
// fallback. This helper is only for opt-in, desktop integration fixtures.
inline absl::Status SaveFreshFixtureRom(Rom& rom,
                                        const std::filesystem::path& target) {
  const auto validation = ValidateFreshFixtureOutput(target);
  if (!validation.ok()) {
    return validation;
  }
  const auto scratch =
      target.parent_path() / UniqueTempPath("yaze_edit_fixture").filename();
  std::error_code ec;
  if (!std::filesystem::create_directory(scratch, ec)) {
    return absl::UnavailableError(
        absl::StrCat("Cannot create fixture staging directory ",
                     scratch.string(), ": ", ec.message()));
  }
  struct ScratchCleanup {
    std::filesystem::path path;
    ~ScratchCleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } cleanup{scratch};

  const auto staged = scratch / "edited.sfc";
  Rom::SaveSettings settings;
  settings.filename = staged.string();
  const auto saved = rom.SaveToFile(settings);
  if (!saved.ok()) {
    return saved;
  }
  return PublishFreshFixtureOutput(staged, target);
}

}  // namespace yaze::test

#endif  // YAZE_TEST_FRESH_ROM_FIXTURE_OUTPUT_H_
