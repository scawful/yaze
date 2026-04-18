#include "app/editor/system/workspace_window_manager.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

namespace yaze::editor {
namespace {

class MockEditorPanelWithHooks final : public WindowContent {
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

class MockGlobalEditorPanel final : public WindowContent {
 public:
  MockGlobalEditorPanel(std::string id, std::string category)
      : id_(std::move(id)), category_(std::move(category)) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Global Panel"; }
  std::string GetIcon() const override { return "ICON_GLOBAL"; }
  std::string GetEditorCategory() const override { return category_; }
  WindowScope GetScope() const override { return WindowScope::kGlobal; }
  WindowLifecycle GetWindowLifecycle() const override {
    return WindowLifecycle::Persistent;
  }

  void Draw(bool* /*p_open*/) override {}

 private:
  std::string id_;
  std::string category_;
};

TEST(WorkspaceWindowManagerPolicyTest,
     ShowHideCallsOnOpenOnCloseForEditorPanelInstances) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterWindowContent(
      std::make_unique<MockEditorPanelWithHooks>("test.hooks", "Test"));

  auto* base = pm.GetWindowContent("test.hooks");
  ASSERT_NE(base, nullptr);
  auto* panel = dynamic_cast<MockEditorPanelWithHooks*>(base);
  ASSERT_NE(panel, nullptr);

  EXPECT_EQ(panel->open_count, 0);
  EXPECT_EQ(panel->close_count, 0);

  EXPECT_TRUE(pm.OpenWindow(0, "test.hooks"));
  EXPECT_EQ(panel->open_count, 1);
  EXPECT_EQ(panel->close_count, 0);

  EXPECT_TRUE(pm.CloseWindow(0, "test.hooks"));
  EXPECT_EQ(panel->open_count, 1);
  EXPECT_EQ(panel->close_count, 1);
}

TEST(WorkspaceWindowManagerPolicyTest, AliasResolutionSupportsLegacyPanelIds) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterPanelAlias("legacy.panel", "modern.panel");
  pm.RegisterPanel({.card_id = "modern.panel",
                    .display_name = "Modern",
                    .icon = "ICON_MOCK",
                    .category = "Test"});

  EXPECT_TRUE(pm.OpenWindow(0, "legacy.panel"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "modern.panel"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "legacy.panel"));

  std::unordered_map<std::string, bool> restored{{"legacy.panel", false}};
  pm.RestoreVisibilityState(0, restored);
  EXPECT_FALSE(pm.IsWindowOpen(0, "modern.panel"));

  pm.RestorePinnedState({{"legacy.panel", true}});
  EXPECT_TRUE(pm.IsWindowPinned(0, "modern.panel"));
}

TEST(WorkspaceWindowManagerPolicyTest,
     WindowShimsMirrorPanelVisibilityBehavior) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);

  bool visible = false;
  WindowDescriptor descriptor;
  descriptor.card_id = "test.window";
  descriptor.display_name = "Test Window";
  descriptor.icon = "ICON_MOCK";
  descriptor.category = "Test";
  descriptor.visibility_flag = &visible;
  descriptor.priority = 10;

  wm.RegisterWindow(0, descriptor);

  EXPECT_TRUE(wm.OpenWindow(0, "test.window"));
  EXPECT_TRUE(wm.IsWindowOpen(0, "test.window"));
  EXPECT_TRUE(visible);
  EXPECT_NE(wm.GetWindowDescriptor(0, "test.window"), nullptr);
  EXPECT_FALSE(wm.GetWorkspaceWindowName(0, "test.window").empty());

  EXPECT_TRUE(wm.CloseWindow(0, "test.window"));
  EXPECT_FALSE(wm.IsWindowOpen(0, "test.window"));
  EXPECT_FALSE(visible);
}

TEST(WorkspaceWindowManagerPolicyTest,
     OnEditorSwitchDoesNotMutateVisibilityFlags) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterPanel({.card_id = "dungeon.manual_ephemeral",
                    .display_name = "Manual",
                    .window_title = " Manual",
                    .icon = "ICON_MOCK",
                    .category = "Dungeon",
                    .window_lifecycle = WindowLifecycle::EditorBound});

  pm.RegisterPanel({.card_id = "dungeon.manual_pinned",
                    .display_name = "Pinned",
                    .window_title = " Pinned",
                    .icon = "ICON_MOCK",
                    .category = "Dungeon",
                    .window_lifecycle = WindowLifecycle::EditorBound});

  pm.RegisterPanel({.card_id = "dungeon.manual_persistent",
                    .display_name = "Persistent",
                    .window_title = " Persistent",
                    .icon = "ICON_MOCK",
                    .category = "Dungeon",
                    .window_lifecycle = WindowLifecycle::Persistent});

  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_persistent"));
  pm.SetWindowPinned(0, "dungeon.manual_pinned", true);

  pm.OnEditorSwitch("Dungeon", "Graphics");

  EXPECT_EQ(pm.GetActiveCategory(), "Graphics");

  // Switching editors should not be treated as "user closed this panel".
  // Visibility persistence and default rules are owned by EditorManager.
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_persistent"));
}

TEST(WorkspaceWindowManagerPolicyTest, GlobalEditorPanelsTrackAcrossSessions) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterWindowContent(
      std::make_unique<MockGlobalEditorPanel>("workflow.output", "Agent"));

  pm.RegisterSession(1);

  EXPECT_NE(pm.GetWindowDescriptor(0, "workflow.output"), nullptr);
  EXPECT_NE(pm.GetWindowDescriptor(1, "workflow.output"), nullptr);

  EXPECT_TRUE(pm.OpenWindow(1, "workflow.output"));
  EXPECT_TRUE(pm.IsWindowOpen(1, "workflow.output"));

  const auto session0_windows = pm.GetWindowsInSession(0);
  const auto session1_windows = pm.GetWindowsInSession(1);
  EXPECT_NE(std::find(session0_windows.begin(), session0_windows.end(),
                      "workflow.output"),
            session0_windows.end());
  EXPECT_NE(std::find(session1_windows.begin(), session1_windows.end(),
                      "workflow.output"),
            session1_windows.end());
}

TEST(WorkspaceWindowManagerPolicyTest, SessionQueriesReturnCanonicalWindowIds) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.RegisterSession(1);
  pm.SetActiveSession(0);

  bool visible0 = false;
  bool visible1 = false;
  WindowDescriptor descriptor;
  descriptor.card_id = "dungeon.room_selector";
  descriptor.display_name = "Room Selector";
  descriptor.icon = "ICON_ROOM";
  descriptor.category = "Dungeon";
  descriptor.priority = 10;

  descriptor.visibility_flag = &visible0;
  pm.RegisterWindow(0, descriptor);

  descriptor.visibility_flag = &visible1;
  pm.RegisterWindow(1, descriptor);

  pm.SetWindowPinned(1, "dungeon.room_selector", true);

  const auto session1_windows = pm.GetWindowsInSession(1);
  ASSERT_EQ(session1_windows.size(), 1U);
  EXPECT_EQ(session1_windows.front(), "dungeon.room_selector");

  const auto dungeon_windows = pm.GetWindowsInCategory(1, "Dungeon");
  ASSERT_EQ(dungeon_windows.size(), 1U);
  EXPECT_EQ(dungeon_windows.front().card_id, "dungeon.room_selector");

  const auto pinned_windows = pm.GetPinnedWindows(1);
  ASSERT_EQ(pinned_windows.size(), 1U);
  EXPECT_EQ(pinned_windows.front(), "dungeon.room_selector");
}

// Regression: Phase A.2 of the editor window persistence plan. Startup order
// previously dropped any pin entry whose panel had not yet registered, because
// RestorePinnedState only iterated session_card_mapping_. Late-registering
// panels (e.g. the emulator panel the user opens an hour into a session) would
// silently lose their pin across restarts. The fix stashes unmatched entries
// in pending_pinned_base_ids_ and drains them in TrackPanelForSession.
TEST(WorkspaceWindowManagerPolicyTest,
     RestorePinnedStateAppliesToLateRegisteringPanels) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  // Pin an id the manager has never heard of. Previous behavior: silently
  // ignored. New behavior: remembered and applied on registration.
  pm.RestorePinnedState({{"emulator.main", true}});

  // Panel not registered yet — nothing to pin against.
  EXPECT_FALSE(pm.IsWindowPinned(0, "emulator.main"));

  // Simulate the user opening the emulator editor later in the session.
  pm.RegisterPanel({.card_id = "emulator.main",
                    .display_name = "Emulator",
                    .icon = "ICON_EMU",
                    .category = "Emulator"});

  EXPECT_TRUE(pm.IsWindowPinned(0, "emulator.main"))
      << "Pending pin should flush to pinned_panels_ at registration time";

  // User's explicit unpin must win against any stale pending entry that might
  // come back from a second RestorePinnedState call later.
  pm.SetWindowPinned(0, "emulator.main", false);
  EXPECT_FALSE(pm.IsWindowPinned(0, "emulator.main"));
}

// Pins for panels that never register this session must round-trip through
// SerializePinnedState so a subsequent save doesn't forget the user's intent.
TEST(WorkspaceWindowManagerPolicyTest,
     SerializePinnedStateIncludesUnappliedPending) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RestorePinnedState({{"hex.viewer", true}, {"asm.editor", false}});

  auto state = pm.SerializePinnedState();
  EXPECT_EQ(state["hex.viewer"], true);
  EXPECT_EQ(state.count("asm.editor"), 1U);
}

// Dynamic windows (room panels, music trackers, etc.) routinely unregister
// when closed and later come back under the same base id. Their pin intent
// must survive that teardown so a save/reopen cycle or later re-registration
// does not silently forget the user's choice.
TEST(WorkspaceWindowManagerPolicyTest,
     UnregisterPanelPreservesPinStateForReregistration) {
  WorkspaceWindowManager pm;
  pm.RegisterSession(0);
  pm.SetActiveSession(0);

  pm.RegisterPanel({.card_id = "music.song_5",
                    .display_name = "Song 05",
                    .icon = "ICON_MOCK",
                    .category = "Music"});
  pm.SetWindowPinned(0, "music.song_5", true);

  pm.UnregisterPanel(0, "music.song_5");

  auto state = pm.SerializePinnedState();
  ASSERT_EQ(state.count("music.song_5"), 1U);
  EXPECT_TRUE(state["music.song_5"]);

  pm.RegisterPanel({.card_id = "music.song_5",
                    .display_name = "Song 05",
                    .icon = "ICON_MOCK",
                    .category = "Music"});

  EXPECT_TRUE(pm.IsWindowPinned(0, "music.song_5"));
}

}  // namespace
}  // namespace yaze::editor
