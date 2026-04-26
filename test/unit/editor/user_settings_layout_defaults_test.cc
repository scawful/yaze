#include "app/editor/system/session/user_settings.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include "gtest/gtest.h"

namespace yaze::editor {

namespace {

// Returns a unique temp settings file path for round-trip tests. Caller is
// responsible for removing the file when done (or before re-using the slot).
std::string MakeTempSettingsPath(const char* tag) {
  auto dir = std::filesystem::temp_directory_path();
  auto path = dir / (std::string("yaze_user_settings_test_") + tag + "_" +
                     std::to_string(::getpid()) + ".json");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return path.string();
}

}  // namespace

TEST(UserSettingsLayoutDefaultsTest, AppliesRevisionAndResetsPanelLayoutState) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 0;
  prefs.sidebar_visible = false;
  prefs.sidebar_panel_expanded = false;
  prefs.sidebar_panel_width = 412.0f;
  prefs.panel_browser_category_width = 198.0f;
  prefs.sidebar_active_category = "Graphics";
  prefs.panel_visibility_state["Graphics"]["graphics.sheet_browser_v2"] = false;
  prefs.pinned_panels["graphics.sheet_browser_v2"] = true;
  prefs.right_panel_widths["agent_chat"] = 640.0f;
  prefs.saved_layouts["legacy"]["graphics.sheet_browser_v2"] = true;

  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_EQ(prefs.panel_layout_defaults_revision, kTargetRevision);
  EXPECT_TRUE(prefs.sidebar_visible);
  EXPECT_TRUE(prefs.sidebar_panel_expanded);
  EXPECT_FLOAT_EQ(prefs.sidebar_panel_width, 0.0f);
  EXPECT_FLOAT_EQ(prefs.panel_browser_category_width, 260.0f);
  EXPECT_TRUE(prefs.sidebar_active_category.empty());

  EXPECT_TRUE(prefs.panel_visibility_state.empty());
  EXPECT_TRUE(prefs.saved_layouts.empty());

  // Revision-4 full-reset clears `right_panel_widths`; revision-21 seeds a
  // hint for the dungeon workbench inspector pane so first-run users see a
  // reasonably-sized inspector before they drag.
  EXPECT_EQ(prefs.right_panel_widths.size(), 1U);
  EXPECT_FLOAT_EQ(prefs.right_panel_widths.at("dungeon.workbench"), 320.0f);

  // Revision-4 full-reset clears pinned_panels; revision-7 seeds the two
  // former-Persistent panels; revision-17 adds the Layout Designer so
  // "Show: Layout Designer" is drawable without requiring a category switch.
  EXPECT_EQ(prefs.pinned_panels.size(), 3U);
  EXPECT_TRUE(prefs.pinned_panels.at("agent.oracle_ram"));
  EXPECT_TRUE(prefs.pinned_panels.at("workflow.output"));
  EXPECT_TRUE(prefs.pinned_panels.at("layout.designer"));
}

TEST(UserSettingsLayoutDefaultsTest, IgnoresOlderOrEqualRevisionRequests) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  prefs.panel_layout_defaults_revision = kTargetRevision;
  prefs.sidebar_visible = false;

  EXPECT_FALSE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));
  EXPECT_FALSE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision - 1));

  EXPECT_EQ(prefs.panel_layout_defaults_revision, kTargetRevision);
  EXPECT_FALSE(prefs.sidebar_visible);
}

TEST(UserSettingsLayoutDefaultsTest,
     PreservesMotionPreferencesAcrossLayoutMigration) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 0;
  prefs.reduced_motion = true;
  prefs.switch_motion_profile = 2;

  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_TRUE(prefs.reduced_motion);
  EXPECT_EQ(prefs.switch_motion_profile, 2);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionFiveClosesLegacyOverworldTile16DefaultWithoutFullReset) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 4;
  prefs.sidebar_visible = false;
  prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.canvas"] = true;
  prefs.saved_layouts["custom"]["overworld.tile16_editor"] = true;

  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_EQ(prefs.panel_layout_defaults_revision, kTargetRevision);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Overworld"]["overworld.canvas"]);
  EXPECT_FALSE(prefs.sidebar_visible);
  EXPECT_TRUE(prefs.saved_layouts["custom"]["overworld.tile16_editor"]);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionSixAppliesMinimalOverworldWorkbenchWithoutFullReset) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 5;
  prefs.sidebar_visible = false;
  prefs.panel_visibility_state["Overworld"]["overworld.canvas"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.tile16_selector"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.properties"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.tile8_selector"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.area_graphics"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.item_list"] = true;
  prefs.saved_layouts["custom"]["overworld.item_list"] = true;

  constexpr int kTargetRevision =
      UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_EQ(prefs.panel_layout_defaults_revision, kTargetRevision);
  EXPECT_TRUE(prefs.panel_visibility_state["Overworld"]["overworld.canvas"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile16_selector"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Overworld"]["overworld.properties"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile8_selector"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.area_graphics"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.item_list"]);
  EXPECT_FALSE(prefs.sidebar_visible);
  EXPECT_TRUE(prefs.saved_layouts["custom"]["overworld.item_list"]);
}

// Revision 7: WindowLifecycle::Persistent was collapsed into CrossEditor.
// Auto-pin the former-Persistent panels so the old always-visible behavior
// carries through the upgrade.
TEST(UserSettingsLayoutDefaultsTest,
     RevisionSevenAutoPinsFormerPersistentPanels) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 6;
  prefs.pinned_panels.clear();

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_TRUE(prefs.pinned_panels.at("agent.oracle_ram"));
  EXPECT_TRUE(prefs.pinned_panels.at("workflow.output"));
}

// A pre-existing pinned=false entry for a former-Persistent panel was a silent
// no-op under the old regime (the draw loop ignored pin state for Persistent),
// so the revision-7 migration must OVERWRITE it — treating the stale value as
// a user choice would make the upgrade visibly regress that panel's
// always-visible behavior.
TEST(UserSettingsLayoutDefaultsTest,
     RevisionSevenOverwritesStaleUnpinFromPersistentEra) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 6;
  prefs.pinned_panels["agent.oracle_ram"] = false;  // pre-migration no-op
  prefs.pinned_panels["workflow.output"] = false;   // pre-migration no-op

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_TRUE(prefs.pinned_panels.at("agent.oracle_ram"));
  EXPECT_TRUE(prefs.pinned_panels.at("workflow.output"));
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionThirteenAppliesDungeonWorkbenchToolDefaultsWithoutFullReset) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 7;
  prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_selector"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_matrix"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_editor"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.door_editor"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_graphics"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.palette_editor"] = false;
  prefs.saved_layouts["custom"]["dungeon.room_selector"] = true;

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"]);
  // Rev-13 closes `dungeon.room_selector`; Rev-21 (Layout C) re-opens it as
  // part of the ZScream-style left-stack so a first-run user lands on the
  // browser/entrances surface without hunting through a menu.
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.room_selector"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.room_matrix"]);
  EXPECT_EQ(
      prefs.panel_visibility_state["Dungeon"].count("dungeon.object_editor"),
      0U);
  EXPECT_FALSE(prefs.panel_visibility_state["Dungeon"]["dungeon.door_editor"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.room_graphics"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.palette_editor"]);
  EXPECT_TRUE(prefs.saved_layouts["custom"]["dungeon.room_selector"]);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionFourteenRestoresClassicThemeAndClosesNoisyDungeonTools) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 13;
  prefs.last_theme_name = "YAZE Tre";
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_tile_editor"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.settings"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.dungeon_map"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_matrix"] = true;
  prefs.saved_layouts["custom"]["dungeon.settings"] = true;

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_EQ(prefs.last_theme_name, "Classic YAZE");
  EXPECT_FALSE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_tile_editor"]);
  EXPECT_EQ(prefs.panel_visibility_state["Dungeon"].count("dungeon.settings"),
            0U);
  EXPECT_EQ(
      prefs.panel_visibility_state["Dungeon"].count("dungeon.dungeon_map"), 0U);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.room_matrix"]);
  EXPECT_EQ(prefs.saved_layouts["custom"].count("dungeon.settings"), 0U);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionFifteenPrefersObjectSelectorOverRoomGraphics) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 14;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_graphics"] = true;

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.room_graphics"]);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionEighteenPrunesTransientDungeonRoomPanels) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 17;
  prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_98"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_165"] = true;
  prefs.pinned_panels["dungeon.room_98"] = true;
  prefs.pinned_panels["layout.designer"] = true;

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_EQ(prefs.panel_visibility_state["Dungeon"].count("dungeon.room_98"),
            0U);
  EXPECT_EQ(prefs.panel_visibility_state["Dungeon"].count("dungeon.room_165"),
            0U);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"]);
  EXPECT_EQ(prefs.pinned_panels.count("dungeon.room_98"), 0U);
  EXPECT_TRUE(prefs.pinned_panels["layout.designer"]);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionNineteenEmbedsDungeonUtilityPanelsInWorkbench) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 18;
  prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_editor"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.settings"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.dungeon_map"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"] = true;
  prefs.pinned_panels["dungeon.object_editor"] = true;
  prefs.pinned_panels["dungeon.settings"] = true;
  prefs.pinned_panels["dungeon.room_matrix"] = true;
  prefs.saved_layouts["custom"]["dungeon.object_editor"] = true;
  prefs.saved_layouts["custom"]["dungeon.dungeon_map"] = true;
  prefs.saved_layouts["custom"]["dungeon.room_matrix"] = true;
  prefs.named_layouts["custom"] =
      R"({"schema_version":2,"name":"custom","root":{"id":1,"type":"leaf","active_tab_index":2,"panels":[{"panel_id":"dungeon.object_editor"},{"panel_id":"dungeon.room_matrix"},{"panel_id":"dungeon.dungeon_map"}]}})";

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"]);
  EXPECT_EQ(
      prefs.panel_visibility_state["Dungeon"].count("dungeon.object_editor"),
      0U);
  EXPECT_EQ(prefs.panel_visibility_state["Dungeon"].count("dungeon.settings"),
            0U);
  EXPECT_EQ(
      prefs.panel_visibility_state["Dungeon"].count("dungeon.dungeon_map"), 0U);
  EXPECT_EQ(prefs.pinned_panels.count("dungeon.object_editor"), 0U);
  EXPECT_EQ(prefs.pinned_panels.count("dungeon.settings"), 0U);
  EXPECT_TRUE(prefs.pinned_panels["dungeon.room_matrix"]);
  EXPECT_EQ(prefs.saved_layouts["custom"].count("dungeon.object_editor"), 0U);
  EXPECT_EQ(prefs.saved_layouts["custom"].count("dungeon.dungeon_map"), 0U);
  EXPECT_TRUE(prefs.saved_layouts["custom"]["dungeon.room_matrix"]);
  EXPECT_EQ(prefs.named_layouts["custom"].find("dungeon.object_editor"),
            std::string::npos);
  EXPECT_EQ(prefs.named_layouts["custom"].find("dungeon.dungeon_map"),
            std::string::npos);
  EXPECT_NE(prefs.named_layouts["custom"].find("dungeon.room_matrix"),
            std::string::npos);
  EXPECT_NE(prefs.named_layouts["custom"].find("\"active_tab_index\":0"),
            std::string::npos);
}

TEST(UserSettingsLayoutDefaultsTest,
     RevisionTwentyOneSeedsLayoutCDungeonDefaultsAndInspectorSide) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 20;
  // Pre-existing dungeon visibility entries (rev 19 baseline). The Layout C
  // migration must open the three left-stack selectors plus the room browser
  // and matrix without clobbering the workbench.
  prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"] = false;
  prefs.panel_visibility_state["Dungeon"]["dungeon.room_selector"] = false;
  // Pre-existing inspector_side: empty (user hasn't chosen). The migration
  // should fill in the "right" default. (Reset because UserSettings's default
  // ctor leaves it as "right" — this is the only knob we need to set.)
  prefs.dungeon_inspector_side.clear();

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);

  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Dungeon"]["dungeon.object_selector"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.sprite_editor"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.item_editor"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.room_selector"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.room_matrix"]);

  EXPECT_EQ(prefs.dungeon_inspector_side, "right");
  EXPECT_EQ(settings.GetDungeonInspectorSide(), "right");

  ASSERT_TRUE(prefs.right_panel_widths.contains("dungeon.workbench"));
  EXPECT_FLOAT_EQ(prefs.right_panel_widths["dungeon.workbench"], 320.0f);
}

// Layout C migration must NOT overwrite a user's prior inspector-side choice
// (e.g., somebody who flipped it to "left" via a future picker before
// upgrading should keep that value).
TEST(UserSettingsLayoutDefaultsTest,
     RevisionTwentyOnePreservesUserInspectorSideChoice) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 20;
  prefs.dungeon_inspector_side = "left";

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));
  EXPECT_EQ(prefs.dungeon_inspector_side, "left");
  EXPECT_EQ(settings.GetDungeonInspectorSide(), "left");
}

// Default ctor seeds "right". Setter normalizes unrecognized values back to
// "right" so callers can switch on the result without a sentinel.
TEST(UserSettingsLayoutDefaultsTest,
     DungeonInspectorSideDefaultsAndNormalizes) {
  UserSettings settings;
  EXPECT_EQ(settings.prefs().dungeon_inspector_side, "right");
  EXPECT_EQ(settings.GetDungeonInspectorSide(), "right");

  settings.SetDungeonInspectorSide("left");
  EXPECT_EQ(settings.prefs().dungeon_inspector_side, "left");
  EXPECT_EQ(settings.GetDungeonInspectorSide(), "left");

  settings.SetDungeonInspectorSide("garbage");
  EXPECT_EQ(settings.prefs().dungeon_inspector_side, "right");
  EXPECT_EQ(settings.GetDungeonInspectorSide(), "right");

  settings.SetDungeonInspectorSide("right");
  EXPECT_EQ(settings.prefs().dungeon_inspector_side, "right");
}

#ifdef YAZE_WITH_JSON
// Round-trip: writing a settings file with inspector_side="left" and
// re-loading must preserve the choice.
TEST(UserSettingsLayoutDefaultsTest,
     DungeonInspectorSideRoundTripsThroughSaveAndLoad) {
  std::string path = MakeTempSettingsPath("inspector_side");

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path);
    settings.SetDungeonInspectorSide("left");
    ASSERT_TRUE(settings.Save().ok());
  }

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path);
    ASSERT_TRUE(settings.Load().ok());
    EXPECT_EQ(settings.GetDungeonInspectorSide(), "left");
    EXPECT_EQ(settings.prefs().dungeon_inspector_side, "left");
  }

  std::error_code ec;
  std::filesystem::remove(path, ec);
}
#endif  // YAZE_WITH_JSON

TEST(UserSettingsLayoutDefaultsTest,
     RevisionTwentyHidesStaleOverworldTile16EditorVisibilityOnly) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 19;
  prefs.panel_visibility_state["Overworld"]["overworld.canvas"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.tile16_selector"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.properties"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"] = true;
  prefs.panel_visibility_state["Overworld"]["overworld.area_graphics"] = true;
  prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"] = true;
  prefs.saved_layouts["custom"]["overworld.tile16_editor"] = true;
  prefs.named_layouts["custom"] =
      R"({"schema_version":2,"name":"custom","root":{"id":1,"type":"leaf","active_tab_index":0,"panels":[{"panel_id":"overworld.tile16_editor"}]}})";

  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision));

  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_TRUE(prefs.panel_visibility_state["Overworld"]["overworld.canvas"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile16_selector"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Overworld"]["overworld.properties"]);
  EXPECT_FALSE(
      prefs.panel_visibility_state["Overworld"]["overworld.tile16_editor"]);
  EXPECT_TRUE(
      prefs.panel_visibility_state["Overworld"]["overworld.area_graphics"]);
  EXPECT_TRUE(prefs.panel_visibility_state["Dungeon"]["dungeon.workbench"]);
  EXPECT_TRUE(prefs.saved_layouts["custom"]["overworld.tile16_editor"]);
  EXPECT_NE(prefs.named_layouts["custom"].find("overworld.tile16_editor"),
            std::string::npos);
}

}  // namespace yaze::editor
