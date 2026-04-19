#include <filesystem>
#include <string>

#include "app/editor/system/session/user_settings.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

// Minimal filesystem helper: builds a unique path inside the OS temp dir so
// tests can round-trip settings without touching the user's real config file.
std::filesystem::path TempSettingsPath(const std::string& slug) {
  auto dir = std::filesystem::temp_directory_path() / "yaze_theme_persist";
  std::filesystem::create_directories(dir);
  return dir / (slug + "_settings.json");
}

TEST(ThemePersistenceTest, DefaultIsEmpty) {
  UserSettings settings;
  EXPECT_TRUE(settings.prefs().last_theme_name.empty());
}

TEST(ThemePersistenceTest, RoundTripPreservesName) {
  auto path = TempSettingsPath("nord_roundtrip");
  std::filesystem::remove(path);

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    settings.prefs().last_theme_name = "Nord";
    auto save_status = settings.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.ToString();
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  auto load_status = restored.Load();
  ASSERT_TRUE(load_status.ok()) << load_status.ToString();
  EXPECT_EQ(restored.prefs().last_theme_name, "Nord");

  std::filesystem::remove(path);
}

TEST(ThemePersistenceTest, RoundTripTolleratesEmptyString) {
  // Empty is the first-run state; it must survive r/t without getting turned
  // into a sentinel or dropped from the JSON.
  auto path = TempSettingsPath("empty_roundtrip");
  std::filesystem::remove(path);

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    // Leave last_theme_name as default-constructed empty string.
    auto save_status = settings.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.ToString();
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  auto load_status = restored.Load();
  ASSERT_TRUE(load_status.ok()) << load_status.ToString();
  EXPECT_TRUE(restored.prefs().last_theme_name.empty());

  std::filesystem::remove(path);
}

TEST(ThemePersistenceTest, UnicodeThemeNameSurvives) {
  // Third-party themes may carry non-ASCII names; make sure the persistence
  // path doesn't mangle them (JSON + UTF-8 everywhere).
  auto path = TempSettingsPath("unicode_roundtrip");
  std::filesystem::remove(path);
  const std::string unicode_name = "\u85cd\u8272\u591c";  // "Blue Night" in CJK

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    settings.prefs().last_theme_name = unicode_name;
    auto save_status = settings.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.ToString();
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  auto load_status = restored.Load();
  ASSERT_TRUE(load_status.ok()) << load_status.ToString();
  EXPECT_EQ(restored.prefs().last_theme_name, unicode_name);

  std::filesystem::remove(path);
}

}  // namespace
}  // namespace yaze::editor
