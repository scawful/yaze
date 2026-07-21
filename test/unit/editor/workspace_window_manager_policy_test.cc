#include "app/editor/system/workspace/workspace_window_manager.h"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <unordered_map>

#include "app/editor/system/workspace/resource_panel.h"

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
    return WindowLifecycle::CrossEditor;
  }

  void Draw(bool* /*p_open*/) override {}

 private:
  std::string id_;
  std::string category_;
};

class MockSessionOwnedPanel final : public WindowContent {
 public:
  MockSessionOwnedPanel(int owner, int* draw_count, int* destroy_count,
                        bool enabled = true)
      : owner_(owner),
        draw_count_(draw_count),
        destroy_count_(destroy_count),
        enabled_(enabled) {}
  ~MockSessionOwnedPanel() override { ++*destroy_count_; }

  std::string GetId() const override { return "palette.dungeon_main"; }
  std::string GetDisplayName() const override { return "Dungeon Palettes"; }
  std::string GetIcon() const override { return "ICON_PALETTE"; }
  std::string GetEditorCategory() const override { return "Palette"; }
  bool IsEnabled() const override { return enabled_; }
  void Draw(bool* /*p_open*/) override { ++*draw_count_; }

  int owner() const { return owner_; }

 private:
  int owner_;
  int* draw_count_;
  int* destroy_count_;
  bool enabled_;
};

class MockSessionResourcePanel final : public ResourceWindowContent {
 public:
  MockSessionResourcePanel(size_t owner, int resource_id,
                           std::array<int, 2>* destroy_counts)
      : owner_(owner),
        resource_id_(resource_id),
        destroy_counts_(destroy_counts) {
    SetSessionId(owner);
  }
  ~MockSessionResourcePanel() override { ++(*destroy_counts_)[owner_]; }

  int GetResourceId() const override { return resource_id_; }
  std::string GetResourceType() const override { return "room"; }
  std::string GetIcon() const override { return "ICON_ROOM"; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  void Draw(bool* /*p_open*/) override {}

 private:
  size_t owner_;
  int resource_id_;
  std::array<int, 2>* destroy_counts_;
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
     VisibilityPersistenceSkipsTransientDungeonRoomPanels) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);

  wm.RegisterPanel({.card_id = "dungeon.workbench",
                    .display_name = "Workbench",
                    .icon = "ICON_WORKBENCH",
                    .category = "Dungeon"});
  wm.RegisterPanel({.card_id = "dungeon.room_98",
                    .display_name = "Room 098",
                    .icon = "ICON_ROOM",
                    .category = "Dungeon"});

  EXPECT_TRUE(wm.OpenWindow(0, "dungeon.workbench"));
  EXPECT_TRUE(wm.OpenWindow(0, "dungeon.room_98"));

  const auto serialized = wm.SerializeVisibilityState(0);
  EXPECT_EQ(serialized.count("dungeon.room_98"), 0U);
  ASSERT_EQ(serialized.count("dungeon.workbench"), 1U);
  EXPECT_TRUE(serialized.at("dungeon.workbench"));

  EXPECT_TRUE(wm.CloseWindow(0, "dungeon.room_98"));
  wm.RestoreVisibilityState(
      0, {{"dungeon.room_98", true}, {"dungeon.workbench", true}});
  EXPECT_FALSE(wm.IsWindowOpen(0, "dungeon.room_98"));
  EXPECT_TRUE(wm.IsWindowOpen(0, "dungeon.workbench"));
}

TEST(WorkspaceWindowManagerPolicyTest,
     DuplicateWindowContentRegistrationReplacesStaleInstance) {
  WorkspaceWindowManager wm;
  wm.RegisterSession(0);
  wm.SetActiveSession(0);

  wm.RegisterWindowContent(std::make_unique<MockEditorPanelWithHooks>(
      "dungeon.workbench", "Dungeon"));
  auto* first = wm.GetWindowContent("dungeon.workbench");
  ASSERT_NE(first, nullptr);

  wm.RegisterWindowContent(std::make_unique<MockEditorPanelWithHooks>(
      "dungeon.workbench", "Dungeon"));
  auto* second = wm.GetWindowContent("dungeon.workbench");
  ASSERT_NE(second, nullptr);
  EXPECT_NE(second, first);

  EXPECT_TRUE(wm.OpenWindow(0, "dungeon.workbench"));
  auto* panel = dynamic_cast<MockEditorPanelWithHooks*>(second);
  ASSERT_NE(panel, nullptr);
  EXPECT_EQ(panel->open_count, 1);
}

TEST(WorkspaceWindowManagerPolicyTest,
     SessionWindowContentKeepsDistinctOwnersAndClosesBeforeSessionRemoval) {
  WorkspaceWindowManager wm;
  int first_draws = 0;
  int second_draws = 0;
  int reload_draws = 0;
  int first_destroys = 0;
  int second_destroys = 0;
  int reload_destroys = 0;

  wm.RegisterSession(0);
  wm.RegisterSession(1);
  wm.SetActiveSession(0);
  wm.RegisterWindowContent(std::make_unique<MockSessionOwnedPanel>(
      0, &first_draws, &first_destroys));
  auto* first = dynamic_cast<MockSessionOwnedPanel*>(
      wm.GetWindowContent(0, "palette.dungeon_main"));
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->owner(), 0);

  wm.SetActiveSession(1);
  wm.RegisterWindowContent(std::make_unique<MockSessionOwnedPanel>(
      1, &second_draws, &second_destroys));
  auto* second = dynamic_cast<MockSessionOwnedPanel*>(
      wm.GetWindowContent(1, "palette.dungeon_main"));
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->owner(), 1);
  EXPECT_NE(first, second);

  first->Draw(nullptr);
  second->Draw(nullptr);
  EXPECT_EQ(first_draws, 1);
  EXPECT_EQ(second_draws, 1);

  wm.UnregisterSession(1);
  EXPECT_EQ(second_destroys, 1);
  EXPECT_EQ(first_destroys, 0);
  EXPECT_EQ(wm.GetWindowContent(0, "palette.dungeon_main"), first);
  first->Draw(nullptr);
  EXPECT_EQ(first_draws, 2);

  wm.RegisterWindowContent(std::make_unique<MockSessionOwnedPanel>(
      0, &reload_draws, &reload_destroys, false));
  auto* reloaded = dynamic_cast<MockSessionOwnedPanel*>(
      wm.GetWindowContent(0, "palette.dungeon_main"));
  ASSERT_NE(reloaded, nullptr);
  EXPECT_NE(reloaded, first);
  EXPECT_EQ(first_destroys, 1);
  EXPECT_EQ(reload_destroys, 0);
  EXPECT_EQ(wm.GetWindowsInSession(0).size(), 1u);
  const auto* descriptor = wm.GetWindowDescriptor(0, "palette.dungeon_main");
  ASSERT_NE(descriptor, nullptr);
  ASSERT_TRUE(static_cast<bool>(descriptor->enabled_condition));
  EXPECT_FALSE(descriptor->enabled_condition());
}

TEST(WorkspaceWindowManagerPolicyTest,
     ResourceEvictionPrefersExactInactiveUnprefixedInstance) {
  WorkspaceWindowManager wm;
  std::array<int, 2> destroy_counts{};
  constexpr char kSharedResourceId[] = "Dungeon.room_0";

  wm.RegisterSession(0);
  wm.SetActiveSession(0);
  wm.RegisterWindowContent(
      std::make_unique<MockSessionResourcePanel>(0, 0, &destroy_counts));
  auto* inactive_resource = wm.GetWindowContent(0, kSharedResourceId);
  ASSERT_NE(inactive_resource, nullptr);

  wm.RegisterSession(1);
  wm.SetActiveSession(1);
  wm.RegisterWindowContent(
      std::make_unique<MockSessionResourcePanel>(1, 0, &destroy_counts));
  auto* active_resource = wm.GetWindowContent(1, kSharedResourceId);
  ASSERT_NE(active_resource, nullptr);
  ASSERT_NE(active_resource, inactive_resource);

  // Fill the room-panel limit, then add one more. The oldest tracked ID is
  // session 0's exact unprefixed instance, while session 1 owns a prefixed
  // instance with the same base ID.
  for (int resource_id = 1; resource_id <= 6; ++resource_id) {
    wm.RegisterWindowContent(std::make_unique<MockSessionResourcePanel>(
        1, resource_id, &destroy_counts));
  }
  wm.RegisterWindowContent(
      std::make_unique<MockSessionResourcePanel>(1, 7, &destroy_counts));

  EXPECT_EQ(destroy_counts[0], 1);
  EXPECT_EQ(destroy_counts[1], 0);
  EXPECT_EQ(wm.GetWindowContent(0, kSharedResourceId), nullptr);
  EXPECT_EQ(wm.GetWindowContent(1, kSharedResourceId), active_resource);
  EXPECT_NE(wm.GetWindowDescriptor(1, kSharedResourceId), nullptr);
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

  pm.RegisterPanel({.card_id = "dungeon.manual_cross_editor",
                    .display_name = "Cross-Editor",
                    .window_title = " Cross-Editor",
                    .icon = "ICON_MOCK",
                    .category = "Dungeon",
                    .window_lifecycle = WindowLifecycle::CrossEditor});

  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.OpenWindow(0, "dungeon.manual_cross_editor"));
  pm.SetWindowPinned(0, "dungeon.manual_pinned", true);

  pm.OnEditorSwitch("Dungeon", "Graphics");

  EXPECT_EQ(pm.GetActiveCategory(), "Graphics");

  // Switching editors should not be treated as "user closed this panel".
  // Visibility persistence and default rules are owned by EditorManager.
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_ephemeral"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_pinned"));
  EXPECT_TRUE(pm.IsWindowOpen(0, "dungeon.manual_cross_editor"));
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
