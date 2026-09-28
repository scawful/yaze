#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "app/editor/system/session/user_settings.h"
#include "settings_isolation.h"
#include "util/platform_paths.h"

namespace yaze::editor {
namespace {

// The test runner must keep every UserSettings instance away from the
// developer's real ~/Documents/Yaze/settings.json. These cases fail if the
// YAZE_USER_DOCUMENTS_DIR / YAZE_APP_DATA_DIR redirection in test/yaze_test.cc
// or PlatformPaths stops working.
TEST(UserSettingsTestIsolationTest, DefaultSettingsPathIsUnderIsolatedTempDir) {
  const char* docs_override = std::getenv("YAZE_USER_DOCUMENTS_DIR");
  ASSERT_NE(docs_override, nullptr);
  ASSERT_NE(*docs_override, '\0');

  const std::filesystem::path docs_dir(docs_override);
  EXPECT_TRUE(
      test::IsPathUnder(docs_dir, std::filesystem::temp_directory_path()))
      << docs_dir;

  const UserSettings settings;
  const std::filesystem::path settings_path = settings.settings_file_path();
  EXPECT_FALSE(test::IsRealUserSettingsPath(settings_path)) << settings_path;
  EXPECT_TRUE(test::IsPathUnder(settings_path, docs_dir)) << settings_path;
  EXPECT_EQ(settings_path.filename(), "settings.json");

  const std::filesystem::path legacy_path =
      settings.legacy_settings_file_path();
  const char* app_data_override = std::getenv("YAZE_APP_DATA_DIR");
  ASSERT_NE(app_data_override, nullptr);
  EXPECT_TRUE(test::IsPathUnder(legacy_path, app_data_override)) << legacy_path;
}

TEST(UserSettingsTestIsolationTest, DocumentsDirectoryHonorsOverride) {
  const auto docs_dir = util::PlatformPaths::GetUserDocumentsDirectory();
  ASSERT_TRUE(docs_dir.ok()) << docs_dir.status();
  EXPECT_EQ(test::NormalizeForCompare(*docs_dir),
            test::NormalizeForCompare(std::getenv("YAZE_USER_DOCUMENTS_DIR")));
}

TEST(UserSettingsTestIsolationTest, GuardRecognizesRealSettingsPath) {
  const auto real_paths = test::RealUserSettingsPaths();
  ASSERT_FALSE(real_paths.empty());
  EXPECT_TRUE(test::IsRealUserSettingsPath(real_paths.front()));
}

}  // namespace
}  // namespace yaze::editor
