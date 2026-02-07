#include "app/editor/system/panel_manager.h"

#include <gtest/gtest.h>

#include <string>

namespace yaze::editor {
namespace {

class MockEditorPanelWithHooks final : public EditorPanel {
 public:
  explicit MockEditorPanelWithHooks(std::string id, std::string category)
      : id_(std::move(id)), category_(std::move(category)) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Mock Panel"; }
  std::string GetIcon() const override { return "ICON_MOCK"; }
  std::string GetEditorCategory() const override { return category_; }

  void Draw(bool* /*p_open*/) override {}

  void OnOpen() override { open_count++; }
  void OnClose() override { close_count++; }

  int open_count = 0;
  int close_count = 0;

 private:
  std::string id_;
  std::string category_;
};

TEST(PanelManagerPolicyTest, ShowHideCallsOnOpenOnCloseForEditorPanelInstances) {
  PanelManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterEditorPanel(
      std::make_unique<MockEditorPanelWithHooks>("test.hooks", "Test"));

  auto* base = pm.GetEditorPanel("test.hooks");
  ASSERT_NE(base, nullptr);
  auto* panel = dynamic_cast<MockEditorPanelWithHooks*>(base);
  ASSERT_NE(panel, nullptr);

  EXPECT_EQ(panel->open_count, 0);
  EXPECT_EQ(panel->close_count, 0);

  EXPECT_TRUE(pm.ShowPanel(0, "test.hooks"));
  EXPECT_EQ(panel->open_count, 1);
  EXPECT_EQ(panel->close_count, 0);

  EXPECT_TRUE(pm.HidePanel(0, "test.hooks"));
  EXPECT_EQ(panel->open_count, 1);
  EXPECT_EQ(panel->close_count, 1);
}

TEST(PanelManagerPolicyTest, OnEditorSwitchDoesNotMutateVisibilityFlags) {
  PanelManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterPanel(
      {.card_id = "dungeon.manual_ephemeral",
       .display_name = "Manual",
       .window_title = " Manual",
       .icon = "ICON_MOCK",
       .category = "Dungeon",
       .panel_category = PanelCategory::EditorBound});

  pm.RegisterPanel(
      {.card_id = "dungeon.manual_pinned",
       .display_name = "Pinned",
       .window_title = " Pinned",
       .icon = "ICON_MOCK",
       .category = "Dungeon",
       .panel_category = PanelCategory::EditorBound});

  pm.RegisterPanel(
      {.card_id = "dungeon.manual_persistent",
       .display_name = "Persistent",
       .window_title = " Persistent",
       .icon = "ICON_MOCK",
       .category = "Dungeon",
       .panel_category = PanelCategory::Persistent});

  EXPECT_TRUE(pm.ShowPanel(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.ShowPanel(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.ShowPanel(0, "dungeon.manual_persistent"));
  pm.SetPanelPinned(0, "dungeon.manual_pinned", true);

  pm.OnEditorSwitch("Dungeon", "Graphics");

  EXPECT_EQ(pm.GetActiveCategory(), "Graphics");

  // Switching editors should not be treated as "user closed this panel".
  // Visibility persistence and default rules are owned by EditorManager.
  EXPECT_TRUE(pm.IsPanelVisible(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.IsPanelVisible(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.IsPanelVisible(0, "dungeon.manual_persistent"));
}

}  // namespace
}  // namespace yaze::editor
