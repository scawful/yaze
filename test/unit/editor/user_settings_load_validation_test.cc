#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "app/editor/shell/dialogs/rom_load_options_dialog.h"
#include "app/editor/system/session/user_settings.h"
#include "core/features.h"
#include "rom/rom.h"
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

// A project's workspace values are live while it is open, but must never be
// written over the user's own settings. Opening a raw ROM used to turn a
// disabled autosave back on and save that globally.
TEST_F(UserSettingsLoadValidationTest, WorkspaceOverlayStaysOutOfSavedFile) {
  settings_.prefs().font_global_scale = 1.25f;
  settings_.prefs().autosave_enabled = false;
  settings_.prefs().autosave_interval = 120.0f;
  settings_.prefs().backup_before_save = false;
  ASSERT_TRUE(settings_.Save().ok());

  UserSettings::WorkspaceOverrides project;
  project.font_global_scale = 1.5f;
  project.autosave_enabled = true;
  project.autosave_interval = 600.0f;
  project.backup_before_save = true;
  settings_.ApplyWorkspaceOverrides(project);
  EXPECT_TRUE(settings_.HasWorkspaceOverrides());
  EXPECT_TRUE(settings_.prefs().autosave_enabled);
  EXPECT_FLOAT_EQ(settings_.prefs().autosave_interval, 600.0f);
  ASSERT_TRUE(settings_.Save().ok());

  // Save() must not drop the overlay from the live preferences.
  EXPECT_TRUE(settings_.prefs().autosave_enabled);

  UserSettings reloaded;
  reloaded.SetSettingsFilePathForTesting(settings_.settings_file_path());
  ASSERT_TRUE(reloaded.Load().ok());
  EXPECT_FLOAT_EQ(reloaded.prefs().font_global_scale, 1.25f);
  EXPECT_FALSE(reloaded.prefs().autosave_enabled);
  EXPECT_FLOAT_EQ(reloaded.prefs().autosave_interval, 120.0f);
  EXPECT_FALSE(reloaded.prefs().backup_before_save);
}

TEST_F(UserSettingsLoadValidationTest,
       ClearWorkspaceOverridesRestoresFirstBaseline) {
  settings_.prefs().autosave_enabled = false;
  settings_.prefs().autosave_interval = 90.0f;

  UserSettings::WorkspaceOverrides first;
  first.autosave_interval = 200.0f;
  settings_.ApplyWorkspaceOverrides(first);
  // Switching between two project sessions applies a second overlay without
  // clearing; the baseline must stay the user's values, not project A's.
  UserSettings::WorkspaceOverrides second;
  second.autosave_interval = 400.0f;
  settings_.ApplyWorkspaceOverrides(second);
  EXPECT_FLOAT_EQ(settings_.prefs().autosave_interval, 400.0f);

  settings_.ClearWorkspaceOverrides();
  EXPECT_FALSE(settings_.HasWorkspaceOverrides());
  EXPECT_FALSE(settings_.prefs().autosave_enabled);
  EXPECT_FLOAT_EQ(settings_.prefs().autosave_interval, 90.0f);
}

TEST_F(UserSettingsLoadValidationTest, WorkspaceOverlayZeroFontScaleIsRaised) {
  UserSettings::WorkspaceOverrides project;
  project.font_global_scale = 0.0f;
  settings_.ApplyWorkspaceOverrides(project);

  EXPECT_FLOAT_EQ(settings_.prefs().font_global_scale, 0.5f);
}

// Remembered ROM load options are stored as opaque strings in settings.json
// and the legacy INI, so the encoding is a file format.
RomLoadOptionsDialog::LoadOptions CustomOverworldOptions() {
  RomLoadOptionsDialog::LoadOptions options;
  options.enable_custom_overworld = true;
  options.save_dungeon_maps = true;
  options.save_all_palettes = true;
  options.save_gfx_groups = true;
  options.selected_preset = "ZSCustomOverworld v3 (Recommended)";
  return options;
}

// The ten flag keys without "custom", in encoded order.
constexpr char kFlagsAfterCustom[] =
    ";tail=0;ow_maps=1;ow_entrances=1;ow_exits=1;ow_items=1;ow_sprites=1;"
    "dungeon_maps=0;palettes=0;gfx_groups=0";

TEST(RomLoadChoiceCodecTest, EncodesEveryFlagInTableOrder) {
  EXPECT_EQ(
      RomLoadOptionsDialog::EncodeRememberedChoice(CustomOverworldOptions(), 3),
      "v=1;zso=3;custom=1;tail=0;ow_maps=1;ow_entrances=1;ow_exits=1;"
      "ow_items=1;ow_sprites=1;dungeon_maps=1;palettes=1;gfx_groups=1;"
      "preset=ZSCustomOverworld v3 (Recommended)");
}

TEST(RomLoadChoiceCodecTest, RoundTripsOptionsAndStripsSeparatorsFromPreset) {
  auto options = CustomOverworldOptions();
  options.enable_tail_expansion = true;
  options.save_overworld_sprites = false;
  options.selected_preset = "Mine;\tv2\r\n";

  const auto decoded = RomLoadOptionsDialog::DecodeRememberedChoice(
      RomLoadOptionsDialog::EncodeRememberedChoice(options, 0xFF));
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->zso_version_byte, 0xFF);
  EXPECT_TRUE(decoded->options.enable_custom_overworld);
  EXPECT_TRUE(decoded->options.enable_tail_expansion);
  EXPECT_TRUE(decoded->options.save_overworld_maps);
  EXPECT_FALSE(decoded->options.save_overworld_sprites);
  EXPECT_TRUE(decoded->options.save_dungeon_maps);
  EXPECT_TRUE(decoded->options.save_all_palettes);
  EXPECT_TRUE(decoded->options.save_gfx_groups);
  EXPECT_EQ(decoded->options.selected_preset, "Minev2");
}

TEST(RomLoadChoiceCodecTest, DecodeRejectsWhatItDoesNotUnderstand) {
  const std::string flags = kFlagsAfterCustom;
  ASSERT_TRUE(
      RomLoadOptionsDialog::DecodeRememberedChoice("v=1;zso=3;custom=1" + flags)
          .has_value());

  const std::vector<std::string> rejected = {
      "",
      "zso=3;custom=1" + flags,
      "v=2;zso=3;custom=1" + flags,
      "v=1;zso=256;custom=1" + flags,
      "v=1;zso=-1;custom=1" + flags,
      "v=1;zso=abc;custom=1" + flags,
      "v=1;zso=3;custom=2" + flags,
      "v=1;zso=3;custom=true" + flags,
      "v=1;zso=3" + flags,
  };
  for (const std::string& encoded : rejected) {
    EXPECT_FALSE(
        RomLoadOptionsDialog::DecodeRememberedChoice(encoded).has_value())
        << encoded;
  }
}

TEST(RomLoadChoiceCodecTest, DecodeIgnoresUnknownKeysAndBareParts) {
  // A newer build may add a flag; this build applies the flags it knows.
  const auto decoded = RomLoadOptionsDialog::DecodeRememberedChoice(
      std::string("v=1;zso=0;custom=0") + kFlagsAfterCustom +
      ";future_flag=1;bare;;");
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->zso_version_byte, 0);
  EXPECT_FALSE(decoded->options.enable_custom_overworld);
  EXPECT_TRUE(decoded->options.selected_preset.empty());
}

TEST(RomLoadChoiceCodecTest, RomChoiceKeyIsAbsoluteAndLexicallyNormal) {
  namespace fs = std::filesystem;
  const fs::path base = fs::temp_directory_path();
  const std::string key = (base / "roms" / "a.sfc").lexically_normal().string();

  EXPECT_EQ(RomLoadOptionsDialog::RomChoiceKey(
                (base / "roms" / "x" / ".." / "a.sfc").string()),
            key);
  EXPECT_EQ(RomLoadOptionsDialog::RomChoiceKey(key), key);
  EXPECT_EQ(
      RomLoadOptionsDialog::RomChoiceKey((fs::path("roms") / "a.sfc").string()),
      (fs::current_path() / "roms" / "a.sfc").lexically_normal().string());
  EXPECT_EQ(RomLoadOptionsDialog::RomChoiceKey(""), "");
}

TEST(RomLoadChoiceCodecTest, FeatureFlagsRoundTripThroughOptions) {
  core::FeatureFlags::Flags flags;
  flags.overworld.kLoadCustomOverworld = true;
  flags.overworld.kEnableSpecialWorldExpansion = true;
  flags.overworld.kSaveOverworldExits = false;
  flags.kSaveDungeonMaps = true;
  flags.kSaveGfxGroups = true;

  core::FeatureFlags::Flags applied;
  RomLoadOptionsDialog::ApplyToFeatureFlags(
      RomLoadOptionsDialog::OptionsFromFeatureFlags(flags), applied);
  EXPECT_TRUE(applied.overworld.kLoadCustomOverworld);
  EXPECT_TRUE(applied.overworld.kEnableSpecialWorldExpansion);
  EXPECT_TRUE(applied.overworld.kSaveOverworldMaps);
  EXPECT_FALSE(applied.overworld.kSaveOverworldExits);
  EXPECT_TRUE(applied.kSaveDungeonMaps);
  EXPECT_FALSE(applied.kSaveAllPalettes);
  EXPECT_TRUE(applied.kSaveGfxGroups);
}

// The overworld reads both load options when it loads. The dialog compares
// its choices with the flags in effect when it opened, so it can say when a
// choice waits for a reopen.
TEST(RomLoadOptionsDialogTest, OverworldLoadOptionsChangedComparesOpenFlags) {
  const core::FeatureFlags::Flags saved = core::FeatureFlags::get();
  absl::Cleanup restore = [&saved] {
    core::FeatureFlags::get() = saved;
  };
  auto& overworld = core::FeatureFlags::get().overworld;
  RomLoadOptionsDialog dialog;

  // With no ROM, Open() treats the file as vanilla and picks the v3 preset,
  // which turns the custom overworld fields on.
  overworld.kLoadCustomOverworld = false;
  overworld.kEnableSpecialWorldExpansion = false;
  dialog.Open(nullptr, "vanilla.sfc", false);
  EXPECT_TRUE(dialog.GetOptions().enable_custom_overworld);
  EXPECT_TRUE(dialog.OverworldLoadOptionsChanged());

  overworld.kLoadCustomOverworld = true;
  dialog.Open(nullptr, "vanilla.sfc", false);
  EXPECT_FALSE(dialog.OverworldLoadOptionsChanged());

  // No preset sets the tail, so a tail that loaded on differs from it.
  overworld.kEnableSpecialWorldExpansion = true;
  dialog.Open(nullptr, "vanilla.sfc", false);
  EXPECT_FALSE(dialog.GetOptions().enable_tail_expansion);
  EXPECT_TRUE(dialog.OverworldLoadOptionsChanged());

  // Review shows the flags the session uses now, so nothing differs yet.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(512 * 1024, 0x00)).ok());
  core::FeatureFlags::Flags active;
  active.overworld.kLoadCustomOverworld = false;
  active.overworld.kEnableSpecialWorldExpansion = true;
  dialog.OpenForReview(&rom, active, "", /*remembered=*/false);
  EXPECT_FALSE(dialog.GetOptions().enable_custom_overworld);
  EXPECT_TRUE(dialog.GetOptions().enable_tail_expansion);
  EXPECT_FALSE(dialog.OverworldLoadOptionsChanged());
}

TEST_F(UserSettingsLoadValidationTest, RomLoadChoicesSurviveSaveAndReload) {
  auto& choices = settings_.prefs().rom_load_choices;
  choices["/roms/oracle.sfc"] = "v=1;zso=3;custom=1";
  choices["/roms/with=equals.sfc"] = "v=1;zso=255";
  ASSERT_TRUE(settings_.Save().ok());

  UserSettings reloaded;
  reloaded.SetSettingsFilePathForTesting(settings_.settings_file_path());
  ASSERT_TRUE(reloaded.Load().ok());
  EXPECT_EQ(reloaded.prefs().rom_load_choices, choices);
}

// The INI keeps one choice per line as "<choice>\t<path>". A path may hold a
// tab or '='; an entry that cannot fit on one line is left out.
TEST_F(UserSettingsLoadValidationTest, RomLoadChoicesSurviveLegacyIniAlone) {
  auto& choices = settings_.prefs().rom_load_choices;
  choices["/roms/oracle.sfc"] = "v=1;zso=3";
  choices["/roms/tab\tname=x.sfc"] = "v=1;zso=0";
  choices["/roms/new\nline.sfc"] = "v=1;zso=0";
  choices["/roms/tab-in-choice.sfc"] = "v=1\t;zso=0";
  ASSERT_TRUE(settings_.Save().ok());
  std::error_code ec;
  std::filesystem::remove(settings_.settings_file_path(), ec);

  UserSettings reloaded;
  reloaded.SetSettingsFilePathForTesting(settings_.settings_file_path());
  ASSERT_TRUE(reloaded.Load().ok());
  const std::unordered_map<std::string, std::string> expected = {
      {"/roms/oracle.sfc", "v=1;zso=3"},
      {"/roms/tab\tname=x.sfc", "v=1;zso=0"},
  };
  EXPECT_EQ(reloaded.prefs().rom_load_choices, expected);
}

TEST_F(UserSettingsLoadValidationTest, LegacyIniRomLoadChoiceSplitsAtFirstTab) {
  WriteLegacyIni(
      "rom_load_choice=v=1;zso=3\t/roms/a=b.sfc\n"
      "rom_load_choice=no-tab-is-skipped\n"
      "rom_load_choice=v=1;zso=3\t\n");

  ASSERT_TRUE(settings_.Load().ok());

  const auto& choices = settings_.prefs().rom_load_choices;
  ASSERT_EQ(choices.size(), 1u);
  EXPECT_EQ(choices.at("/roms/a=b.sfc"), "v=1;zso=3");
}

}  // namespace
}  // namespace yaze::editor
