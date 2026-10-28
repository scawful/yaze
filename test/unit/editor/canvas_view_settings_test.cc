#include "app/editor/system/session/canvas_view_settings.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/system/session/user_settings.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze::editor {
namespace {

std::filesystem::path TempSettingsPath() {
  auto dir = ::yaze::test::UniqueTempPath("yaze_canvas_view_settings");
  std::filesystem::create_directories(dir);
  return dir / "settings.json";
}

void WriteFile(const std::filesystem::path& path, const std::string& body) {
  std::ofstream out(path);
  out << body;
}

TEST(CanvasViewSettingsTest, DefaultsMatchPreviousCanvasBehavior) {
  const CanvasViewSettings defaults;
  EXPECT_TRUE(defaults.overworld_show_grid);
  EXPECT_FALSE(defaults.dungeon_show_grid);
  EXPECT_EQ(defaults.dungeon_grid_size, 8);

  UserSettings settings;
  EXPECT_EQ(settings.prefs().canvas_view, defaults);
}

TEST(CanvasViewSettingsTest, OnlyListedDungeonGridSizesAreSupported) {
  for (int size : kDungeonGridSizes) {
    EXPECT_TRUE(IsSupportedDungeonGridSize(size)) << size;
  }
  for (int size : {-8, 0, 1, 4, 12, 64, 1 << 20}) {
    EXPECT_FALSE(IsSupportedDungeonGridSize(size)) << size;
  }
}

TEST(CanvasViewSettingsTest, NormalizeReplacesOnlyAnUnsupportedSize) {
  CanvasViewSettings settings;
  settings.overworld_show_grid = false;
  settings.dungeon_show_grid = true;
  settings.dungeon_grid_size = 12;

  const CanvasViewSettings normalized = NormalizeCanvasViewSettings(settings);
  EXPECT_FALSE(normalized.overworld_show_grid);
  EXPECT_TRUE(normalized.dungeon_show_grid);
  EXPECT_EQ(normalized.dungeon_grid_size, 8);

  settings.dungeon_grid_size = 32;
  EXPECT_EQ(NormalizeCanvasViewSettings(settings), settings);
}

TEST(CanvasViewSettingsTest, DiffReportsEveryFieldWhenNothingWasApplied) {
  CanvasViewSettings current;
  current.dungeon_grid_size = 16;
  const auto changes = DiffCanvasViewSettings(std::nullopt, current);
  ASSERT_TRUE(changes.overworld_show_grid.has_value());
  ASSERT_TRUE(changes.dungeon_show_grid.has_value());
  ASSERT_TRUE(changes.dungeon_grid_size.has_value());
  EXPECT_TRUE(*changes.overworld_show_grid);
  EXPECT_FALSE(*changes.dungeon_show_grid);
  EXPECT_EQ(*changes.dungeon_grid_size, 16);
}

TEST(CanvasViewSettingsTest, DiffReportsOnlyChangedFields) {
  const CanvasViewSettings applied;
  EXPECT_FALSE(DiffCanvasViewSettings(applied, applied).overworld_show_grid);
  EXPECT_FALSE(DiffCanvasViewSettings(applied, applied).dungeon_show_grid);
  EXPECT_FALSE(DiffCanvasViewSettings(applied, applied).dungeon_grid_size);

  CanvasViewSettings current = applied;
  current.dungeon_grid_size = 32;
  const auto changes = DiffCanvasViewSettings(applied, current);
  EXPECT_FALSE(changes.overworld_show_grid.has_value());
  EXPECT_FALSE(changes.dungeon_show_grid.has_value());
  ASSERT_TRUE(changes.dungeon_grid_size.has_value());
  EXPECT_EQ(*changes.dungeon_grid_size, 32);
}

TEST(CanvasViewSettingsTest, DiffComparesNormalizedCurrentSettings) {
  const CanvasViewSettings applied;
  CanvasViewSettings current = applied;
  current.dungeon_grid_size = 7;
  EXPECT_FALSE(DiffCanvasViewSettings(applied, current).dungeon_grid_size);
  EXPECT_EQ(*DiffCanvasViewSettings(std::nullopt, current).dungeon_grid_size,
            8);
}

TEST(CanvasViewSettingsTest, JsonRoundTripPreservesEveryField) {
  CanvasViewSettings settings;
  settings.overworld_show_grid = false;
  settings.dungeon_show_grid = true;
  settings.dungeon_grid_size = 16;

  const nlohmann::json json = CanvasViewSettingsToJson(settings);
  EXPECT_EQ(CanvasViewSettingsFromJson(json, CanvasViewSettings{}), settings);
}

TEST(CanvasViewSettingsTest, JsonKeepsFallbackForMissingOrMistypedKeys) {
  CanvasViewSettings fallback;
  fallback.overworld_show_grid = false;
  fallback.dungeon_show_grid = true;
  fallback.dungeon_grid_size = 32;

  EXPECT_EQ(CanvasViewSettingsFromJson(nlohmann::json::object(), fallback),
            fallback);
  EXPECT_EQ(CanvasViewSettingsFromJson(nlohmann::json::array({1, 2}), fallback),
            fallback);

  const nlohmann::json mistyped = {
      {"overworld_show_grid", "yes"},
      {"dungeon_show_grid", 0},
      {"dungeon_grid_size", "16"},
  };
  EXPECT_EQ(CanvasViewSettingsFromJson(mistyped, fallback), fallback);

  const nlohmann::json fractional = {{"dungeon_grid_size", 16.0}};
  EXPECT_EQ(CanvasViewSettingsFromJson(fractional, fallback).dungeon_grid_size,
            32);
}

TEST(CanvasViewSettingsTest, JsonReplacesUnsupportedSizeWithDefault) {
  CanvasViewSettings fallback;
  fallback.dungeon_grid_size = 32;

  for (std::int64_t size : {std::int64_t{64}, std::int64_t{-8}, std::int64_t{0},
                            (std::int64_t{1} << 32) + 16}) {
    const nlohmann::json json = {{"dungeon_grid_size", size}};
    EXPECT_EQ(CanvasViewSettingsFromJson(json, fallback).dungeon_grid_size, 8)
        << size;
  }
}

TEST(CanvasViewSettingsTest, UserSettingsJsonRoundTrip) {
  const auto path = TempSettingsPath();
  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    auto& view = settings.prefs().canvas_view;
    view.overworld_show_grid = false;
    view.dungeon_show_grid = true;
    view.dungeon_grid_size = 32;
    const auto status = settings.Save();
    ASSERT_TRUE(status.ok()) << status;
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  const auto status = restored.Load();
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(restored.prefs().canvas_view.overworld_show_grid);
  EXPECT_TRUE(restored.prefs().canvas_view.dungeon_show_grid);
  EXPECT_EQ(restored.prefs().canvas_view.dungeon_grid_size, 32);

  std::filesystem::remove_all(path.parent_path());
}

TEST(CanvasViewSettingsTest, OlderSettingsFileWithoutCanvasKeysLoadsDefaults) {
  const auto path = TempSettingsPath();
  WriteFile(path, R"({"version": 1, "editor": {"default_editor": 2}})");

  UserSettings settings;
  settings.SetSettingsFilePathForTesting(path.string());
  const auto status = settings.Load();
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(settings.prefs().canvas_view, CanvasViewSettings{});
  EXPECT_EQ(settings.prefs().default_editor, 2);

  std::filesystem::remove_all(path.parent_path());
}

TEST(CanvasViewSettingsTest, InvalidCanvasValuesOnDiskLoadAsDefaults) {
  const auto path = TempSettingsPath();
  WriteFile(path, R"({"version": 1, "canvas": {
      "overworld_show_grid": "off",
      "dungeon_show_grid": true,
      "dungeon_grid_size": 12}})");

  UserSettings settings;
  settings.SetSettingsFilePathForTesting(path.string());
  const auto status = settings.Load();
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(settings.prefs().canvas_view.overworld_show_grid);
  EXPECT_TRUE(settings.prefs().canvas_view.dungeon_show_grid);
  EXPECT_EQ(settings.prefs().canvas_view.dungeon_grid_size, 8);

  std::filesystem::remove_all(path.parent_path());
}

TEST(CanvasViewSettingsTest, LegacyIniValuesLoadAndNormalize) {
  const auto path = TempSettingsPath();
  UserSettings settings;
  settings.SetSettingsFilePathForTesting(path.string());
  WriteFile(settings.legacy_settings_file_path(),
            "canvas_overworld_show_grid=0\n"
            "canvas_dungeon_show_grid=1\n"
            "canvas_dungeon_grid_size=5\n");

  const auto status = settings.Load();
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(settings.prefs().canvas_view.overworld_show_grid);
  EXPECT_TRUE(settings.prefs().canvas_view.dungeon_show_grid);
  EXPECT_EQ(settings.prefs().canvas_view.dungeon_grid_size, 8);

  std::filesystem::remove_all(path.parent_path());
}

class OverworldCanvasViewSettingsTest : public ::testing::Test {
 protected:
  void SetUp() override { context_ = ImGui::CreateContext(); }
  void TearDown() override { ImGui::DestroyContext(context_); }

  ImGuiContext* context_ = nullptr;
};

TEST_F(OverworldCanvasViewSettingsTest,
       AppliesChangedDefaultAndKeepsSessionToggle) {
  Rom rom;
  UserSettings settings;
  settings.prefs().canvas_view.overworld_show_grid = false;
  EditorDependencies deps;
  deps.user_settings = &settings;
  OverworldEditor editor(&rom, deps);
  ASSERT_TRUE(editor.grid_visible());

  editor.ApplyCanvasViewSettings();
  EXPECT_FALSE(editor.grid_visible());

  // An editor toggle is session-only; unrelated settings edits keep it.
  editor.ToggleGrid();
  settings.prefs().canvas_view.dungeon_grid_size = 16;
  editor.ApplyCanvasViewSettings();
  EXPECT_TRUE(editor.grid_visible());

  settings.prefs().canvas_view.overworld_show_grid = true;
  editor.ApplyCanvasViewSettings();
  EXPECT_TRUE(editor.grid_visible());
  settings.prefs().canvas_view.overworld_show_grid = false;
  editor.ApplyCanvasViewSettings();
  EXPECT_FALSE(editor.grid_visible());
}

}  // namespace
}  // namespace yaze::editor
