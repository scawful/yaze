#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "app/editor/system/session/user_settings.h"
#include "unique_temp_path.h"

namespace yaze::editor {
namespace {

// Load() reads settings.json, or the legacy INI when no JSON exists. Values a
// user can hand-edit must not abort the load or leave the UI unusable.
class UserSettingsLoadValidationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    settings_.SetSettingsFilePathForTesting(
        test::UniqueTempPath("yaze_user_settings_validation", ".json")
            .string());
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(settings_.settings_file_path(), ec);
    std::filesystem::remove(settings_.legacy_settings_file_path(), ec);
  }

  void WriteLegacyIni(const std::string& contents) {
    std::ofstream out(settings_.legacy_settings_file_path(), std::ios::trunc);
    out << contents;
  }

  void WriteJson(const std::string& contents) {
    std::ofstream out(settings_.settings_file_path(), std::ios::trunc);
    out << contents;
  }

  UserSettings settings_;
};

TEST_F(UserSettingsLoadValidationTest,
       LegacyIniBadFontFamilyIndexKeepsDefaultAndLoadsLaterKeys) {
  WriteLegacyIni(
      "font_family_index=not-a-number\n"
      "target_fps=30\n"
      "undo_history_size=75\n");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_EQ(settings_.prefs().font_family_index, 0);
  EXPECT_EQ(settings_.prefs().target_fps, 30);
  EXPECT_EQ(settings_.prefs().undo_history_size, 75);
}

TEST_F(UserSettingsLoadValidationTest, LegacyIniZeroFontScaleIsRaised) {
  WriteLegacyIni("font_global_scale=0\n");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 0.5f);
}

TEST_F(UserSettingsLoadValidationTest, LegacyIniHugeFontScaleIsLowered) {
  WriteLegacyIni("font_global_scale=50\n");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 2.0f);
}

TEST_F(UserSettingsLoadValidationTest, LegacyIniNanFontScaleFallsBackToOne) {
  WriteLegacyIni("font_global_scale=nan\n");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 1.0f);
}

TEST_F(UserSettingsLoadValidationTest, LegacyIniInRangeFontScaleIsKept) {
  WriteLegacyIni("font_global_scale=1.25\n");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 1.25f);
}

#ifdef YAZE_WITH_JSON
TEST_F(UserSettingsLoadValidationTest, JsonZeroFontScaleIsRaised) {
  WriteJson(R"({"general": {"font_global_scale": 0}})");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 0.5f);
}

TEST_F(UserSettingsLoadValidationTest, JsonInRangeFontScaleIsKept) {
  WriteJson(R"({"general": {"font_global_scale": 1.5}})");

  ASSERT_TRUE(settings_.Load().ok());

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 1.5f);
}
#endif  // YAZE_WITH_JSON

}  // namespace
}  // namespace yaze::editor
