#ifndef YAZE_TEST_SETTINGS_ISOLATION_H
#define YAZE_TEST_SETTINGS_ISOLATION_H

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

namespace yaze::test {

// The test runner (test/yaze_test.cc) points YAZE_USER_DOCUMENTS_DIR and
// YAZE_APP_DATA_DIR at a per-process temp directory before any test runs, so
// UserSettings resolves settings.json there instead of the developer's real
// ~/Documents/Yaze/settings.json. These helpers name the real file so the
// runner and tests can prove they never resolve to it.

// Every spelling of the developer's real settings.json: the passwd home (which
// a test cannot change by overriding HOME) and the HOME/USERPROFILE value the
// process started with.
inline std::vector<std::filesystem::path> RealUserSettingsPaths() {
  std::vector<std::filesystem::path> homes;
#ifdef _WIN32
  if (const char* profile = std::getenv("USERPROFILE"); profile && *profile) {
    homes.emplace_back(profile);
  }
#else
  if (const passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) {
    homes.emplace_back(pw->pw_dir);
  }
#endif
  if (const char* home = std::getenv("HOME"); home && *home) {
    homes.emplace_back(home);
  }
  std::vector<std::filesystem::path> paths;
  for (const auto& home : homes) {
    paths.push_back(home / "Documents" / "Yaze" / "settings.json");
    paths.push_back(home / "Yaze" /
                    "settings.json");  // no-~/Documents fallback
  }
  return paths;
}

inline std::filesystem::path NormalizeForCompare(
    const std::filesystem::path& path) {
  std::error_code ec;
  auto normalized = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    normalized = std::filesystem::absolute(path, ec).lexically_normal();
  }
  return normalized;
}

inline bool IsRealUserSettingsPath(const std::filesystem::path& path) {
  const auto candidate = NormalizeForCompare(path);
  for (const auto& real : RealUserSettingsPaths()) {
    if (candidate == NormalizeForCompare(real)) {
      return true;
    }
  }
  return false;
}

// True when `path` is inside `root` after normalization.
inline bool IsPathUnder(const std::filesystem::path& path,
                        const std::filesystem::path& root) {
  const auto rel =
      NormalizeForCompare(path).lexically_relative(NormalizeForCompare(root));
  return !rel.empty() && *rel.begin() != "..";
}

}  // namespace yaze::test

#endif  // YAZE_TEST_SETTINGS_ISOLATION_H
