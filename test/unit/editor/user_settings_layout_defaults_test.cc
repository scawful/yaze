#include "app/editor/system/user_settings.h"

#include "gtest/gtest.h"

namespace yaze::editor {

TEST(UserSettingsLayoutDefaultsTest,
     AppliesRevisionAndResetsPanelLayoutState) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  prefs.panel_layout_defaults_revision = 0;
  prefs.sidebar_visible = false;
  prefs.sidebar_panel_expanded = false;
  prefs.sidebar_panel_width = 412.0f;
  prefs.panel_browser_category_width = 198.0f;
  prefs.sidebar_active_category = "Graphics";
  prefs.panel_visibility_state["Graphics"]["graphics.sheet_browser_v2"] =
      false;
  prefs.pinned_panels["graphics.sheet_browser_v2"] = true;
  prefs.right_panel_widths["agent_chat"] = 640.0f;
  prefs.saved_layouts["legacy"]["graphics.sheet_browser_v2"] = true;

  constexpr int kTargetRevision = UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_EQ(prefs.panel_layout_defaults_revision, kTargetRevision);
  EXPECT_TRUE(prefs.sidebar_visible);
  EXPECT_TRUE(prefs.sidebar_panel_expanded);
  EXPECT_FLOAT_EQ(prefs.sidebar_panel_width, 0.0f);
  EXPECT_FLOAT_EQ(prefs.panel_browser_category_width, 260.0f);
  EXPECT_TRUE(prefs.sidebar_active_category.empty());

  EXPECT_TRUE(prefs.panel_visibility_state.empty());
  EXPECT_TRUE(prefs.pinned_panels.empty());
  EXPECT_TRUE(prefs.right_panel_widths.empty());
  EXPECT_TRUE(prefs.saved_layouts.empty());
}

TEST(UserSettingsLayoutDefaultsTest,
     IgnoresOlderOrEqualRevisionRequests) {
  UserSettings settings;
  auto& prefs = settings.prefs();

  constexpr int kTargetRevision = UserSettings::kLatestPanelLayoutDefaultsRevision;
  prefs.panel_layout_defaults_revision = kTargetRevision;
  prefs.sidebar_visible = false;

  EXPECT_FALSE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));
  EXPECT_FALSE(
      settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision - 1));

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

  constexpr int kTargetRevision = UserSettings::kLatestPanelLayoutDefaultsRevision;
  EXPECT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(kTargetRevision));

  EXPECT_TRUE(prefs.reduced_motion);
  EXPECT_EQ(prefs.switch_motion_profile, 2);
}

}  // namespace yaze::editor
