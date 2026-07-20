#ifndef YAZE_TEST_ORACLE_ROM_FIXTURE_H
#define YAZE_TEST_ORACLE_ROM_FIXTURE_H

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace yaze::test {

inline std::string AutoDiscoverOracleRom(
    const std::filesystem::path& start_dir = std::filesystem::current_path()) {
  static const std::vector<std::string> kOracleNames = {
      "oos168x.sfc", "oos168.sfc", "oracle_of_secrets.sfc"};
  static const std::vector<std::filesystem::path> kSearchPaths = {
      ".",
      "roms",
      "Roms",
      "../roms",
      "../Roms",
      "../../roms",
      "../../Roms",
      "../oracle-of-secrets/Roms",
      "../../oracle-of-secrets/Roms",
      "../../../oracle-of-secrets/Roms",
      "../../../../oracle-of-secrets/Roms",
  };

  // Filename order is global: a patched fixture in any supported directory
  // must beat an editable base ROM in a nearer directory.
  for (const auto& name : kOracleNames) {
    for (const auto& relative_dir : kSearchPaths) {
      const auto path = (start_dir / relative_dir / name).lexically_normal();
      if (std::filesystem::exists(path)) {
        return path.string();
      }
    }
  }
  return "";
}

inline std::string FindOracleRomFixture() {
  for (const char* env_var : {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_EXPANDED",
                              "YAZE_TEST_ROM_EXPANDED_PATH"}) {
    if (const char* path = std::getenv(env_var)) {
      if (std::filesystem::exists(path)) {
        return path;
      }
    }
  }
  return AutoDiscoverOracleRom();
}

}  // namespace yaze::test

#endif  // YAZE_TEST_ORACLE_ROM_FIXTURE_H
