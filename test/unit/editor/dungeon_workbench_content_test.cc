#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/workspace/dungeon_pit_damage_view_model.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_layout.h"
#include "core/features.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/pit_damage_table.h"

namespace yaze::editor {
namespace {

constexpr float kMinCanvasWidth = 420.0f;
constexpr float kMinSidebarWidth = 320.0f;
constexpr float kSplitterWidth = 8.0f;
constexpr float kCompactLeftWidth = 230.4f;
constexpr float kCompactRightWidth = 272.0f;

std::filesystem::path MakeTempProjectRoot() {
  return std::filesystem::temp_directory_path() /
         ("yaze_dungeon_project_labels_" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
}

DungeonWorkbenchContent MakeWorkbenchForToolStateTests(
    int& current_room_id, const std::deque<int>& recent_rooms) {
  return DungeonWorkbenchContent(
      nullptr, &current_room_id, [](int) {}, [](int, RoomSelectionIntent) {},
      [](int) {}, []() {}, []() -> DungeonCanvasViewer* { return nullptr; },
      [](int) -> DungeonCanvasViewer* { return nullptr; },
      [&recent_rooms]() -> const std::deque<int>& { return recent_rooms; },
      [](int) {}, [](bool) {});
}

class FakeWorkbenchToolContent final : public WindowContent {
 public:
  explicit FakeWorkbenchToolContent(std::string id) : id_(std::move(id)) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Fake Tool"; }
  std::string GetIcon() const override { return ""; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  void Draw(bool*) override {}

 private:
  std::string id_;
};

zelda3::PitDamageTable MakePitDamageTable(
    std::initializer_list<uint16_t> room_ids) {
  zelda3::PitDamageTable table;
  table.SetRoomIds(std::vector<uint16_t>(room_ids));
  table.ClearDirty();
  return table;
}

class DungeonWorkbenchPitDamageUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    io.AddMousePosEvent(-1000.0f, -1000.0f);
    io.AddMouseButtonEvent(0, false);
  }

  void TearDown() override {
    if (context_ != nullptr) {
      ImGui::DestroyContext(context_);
      context_ = nullptr;
    }
  }

  void DrawPitDamageFrame(DungeonWorkbenchContent& content, int room_id) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 320.0f), ImGuiCond_Always);
    ImGui::Begin(
        "PitDamageHost", nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);
    content.DrawPitDamageControlsForTesting(room_id);
    ImGui::End();
    ImGui::Render();
  }

  void ClickRect(DungeonWorkbenchContent& content, int room_id,
                 const DungeonWorkbenchTestRect& rect) {
    const float center_x = (rect.min_x + rect.max_x) * 0.5f;
    const float center_y = (rect.min_y + rect.max_y) * 0.5f;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(center_x, center_y);
    io.AddMouseButtonEvent(0, true);
    DrawPitDamageFrame(content, room_id);

    io.AddMousePosEvent(center_x, center_y);
    io.AddMouseButtonEvent(0, false);
    DrawPitDamageFrame(content, room_id);
  }

 private:
  ImGuiContext* context_ = nullptr;
};

TEST(DungeonWorkbenchContentLayoutTest,
     PrefersCompactingAndHidingLeftPaneBeforeRightPane) {
  const auto full = ResolveDungeonWorkbenchResponsiveLayout(
      1076.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(full.show_left);
  EXPECT_TRUE(full.show_right);
  EXPECT_FALSE(full.compact_left);
  EXPECT_FALSE(full.compact_right);

  const auto compact_left = ResolveDungeonWorkbenchResponsiveLayout(
      1075.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(compact_left.show_left);
  EXPECT_TRUE(compact_left.show_right);
  EXPECT_TRUE(compact_left.compact_left);
  EXPECT_FALSE(compact_left.compact_right);

  const auto both_compact = ResolveDungeonWorkbenchResponsiveLayout(
      979.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(both_compact.show_left);
  EXPECT_TRUE(both_compact.show_right);
  EXPECT_TRUE(both_compact.compact_left);
  EXPECT_TRUE(both_compact.compact_right);

  const auto hide_left = ResolveDungeonWorkbenchResponsiveLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_left.show_left);
  EXPECT_TRUE(hide_left.show_right);
  EXPECT_FALSE(hide_left.compact_left);
  EXPECT_TRUE(hide_left.compact_right);

  const auto hide_both = ResolveDungeonWorkbenchResponsiveLayout(
      699.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_both.show_left);
  EXPECT_FALSE(hide_both.show_right);
}

TEST(DungeonWorkbenchApplyScopeTest, SelectAllAndNoneIncludeEntrances) {
  auto& flags = core::FeatureFlags::get().dungeon;
  const auto previous = flags;
  int current_room_id = 0;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  flags.kSaveWaterFillZones = true;
  content.SetAllSaveFlagsForTesting(false);
  EXPECT_FALSE(flags.kSaveObjects);
  EXPECT_FALSE(flags.kSaveSprites);
  EXPECT_FALSE(flags.kSaveRoomHeaders);
  EXPECT_FALSE(flags.kSaveChests);
  EXPECT_FALSE(flags.kSavePotItems);
  EXPECT_FALSE(flags.kSaveEntrances);
  EXPECT_FALSE(flags.kSavePalettes);
  EXPECT_FALSE(flags.kSaveCollision);
  EXPECT_FALSE(flags.kSaveBlocks);
  EXPECT_FALSE(flags.kSaveTorches);
  EXPECT_FALSE(flags.kSavePits);
  EXPECT_TRUE(flags.kSaveWaterFillZones);

  flags.kSaveWaterFillZones = false;
  content.SetAllSaveFlagsForTesting(true);
  EXPECT_TRUE(flags.kSaveEntrances);
  EXPECT_TRUE(flags.kSaveObjects);
  EXPECT_FALSE(flags.kSaveWaterFillZones);

  flags = previous;
}

TEST(DungeonWorkbenchContentLayoutTest,
     CompactInspectorDetailRequestResetsOutsideCompactLayout) {
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(false, false));
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(true, false));
  EXPECT_TRUE(ResolveCompactInspectorDetailRequest(true, true));
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(false, true));
}

TEST(DungeonWorkbenchContentObjectSizeTest,
     SizeControlsEnableOnlyWhenSelectionContainsEditableObject) {
  const std::vector<zelda3::RoomObject> objects = {
      zelda3::RoomObject(0x100, 5, 5, 0, 0),
      zelda3::RoomObject(0xF99, 10, 10, 0x06, 0),
      zelda3::RoomObject(0x001, 15, 15, 3, 0),
  };
  const std::vector<size_t> fixed_only = {0, 1};
  const std::vector<size_t> mixed = {0, 1, 2};
  const std::vector<size_t> invalid = {objects.size()};

  EXPECT_FALSE(workbench::HasEditableRoomObjectSize(objects, fixed_only));
  EXPECT_TRUE(workbench::HasEditableRoomObjectSize(objects, mixed));
  EXPECT_FALSE(workbench::HasEditableRoomObjectSize(objects, invalid));
}

TEST(DungeonWorkbenchContentLayoutTest,
     PaneLayoutUsesSharedCompactWidthsAtResponsiveThresholds) {
  const auto compact_both = ResolveDungeonWorkbenchPaneLayout(
      938.4f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_TRUE(compact_both.responsive.show_left);
  EXPECT_TRUE(compact_both.responsive.show_right);
  EXPECT_TRUE(compact_both.responsive.compact_left);
  EXPECT_TRUE(compact_both.responsive.compact_right);
  EXPECT_NEAR(compact_both.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(compact_both.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(compact_both.center_width, 420.0f, 0.001f);

  const auto hide_left = ResolveDungeonWorkbenchPaneLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_FALSE(hide_left.responsive.show_left);
  EXPECT_TRUE(hide_left.responsive.show_right);
  EXPECT_NEAR(hide_left.left_width, 0.0f, 0.001f);
  EXPECT_NEAR(hide_left.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(hide_left.center_width,
              931.0f - kCompactRightWidth - kSplitterWidth, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     OversizedPaneMemoryStillProtectsMinimumCanvasWidth) {
  const auto layout = ResolveDungeonWorkbenchPaneLayout(
      1000.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 500.0f,
      360.0f, true, true);
  EXPECT_TRUE(layout.responsive.show_left);
  EXPECT_TRUE(layout.responsive.show_right);
  EXPECT_TRUE(layout.responsive.compact_left);
  EXPECT_FALSE(layout.responsive.compact_right);
  EXPECT_NEAR(layout.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(layout.right_width, 333.6f, 0.001f);
  EXPECT_GE(layout.center_width, kMinCanvasWidth);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolDrawerLayoutScalesWithRememberedProportion) {
  const auto closed = ResolveDungeonWorkbenchToolDrawerLayout(
      708.0f, kSplitterWidth, 0.4f, 240.0f, 180.0f, false);
  EXPECT_FALSE(closed.show_drawer);
  EXPECT_FALSE(closed.compact);
  EXPECT_NEAR(closed.canvas_height, 708.0f, 0.001f);
  EXPECT_NEAR(closed.drawer_height, 0.0f, 0.001f);

  const auto open = ResolveDungeonWorkbenchToolDrawerLayout(
      708.0f, kSplitterWidth, 0.4f, 240.0f, 180.0f, true);
  EXPECT_TRUE(open.show_drawer);
  EXPECT_FALSE(open.compact);
  EXPECT_NEAR(open.canvas_height, 420.0f, 0.001f);
  EXPECT_NEAR(open.drawer_height, 280.0f, 0.001f);
  EXPECT_NEAR(open.canvas_height + open.drawer_height + kSplitterWidth, 708.0f,
              0.001f);

  const auto taller = ResolveDungeonWorkbenchToolDrawerLayout(
      908.0f, kSplitterWidth, 0.4f, 240.0f, 180.0f, true);
  EXPECT_FALSE(taller.compact);
  EXPECT_NEAR(taller.canvas_height, 540.0f, 0.001f);
  EXPECT_NEAR(taller.drawer_height, 360.0f, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolDrawerLayoutUsesStableCompactSplitWhenHeightIsConstrained) {
  const auto compact = ResolveDungeonWorkbenchToolDrawerLayout(
      400.0f, kSplitterWidth, 0.55f, 240.0f, 180.0f, true);
  EXPECT_TRUE(compact.show_drawer);
  EXPECT_TRUE(compact.compact);
  EXPECT_NEAR(compact.drawer_height, 156.8f, 0.001f);
  EXPECT_NEAR(compact.canvas_height, 235.2f, 0.001f);
  EXPECT_NEAR(compact.canvas_height + compact.drawer_height + kSplitterWidth,
              400.0f, 0.001f);

  const auto recovered = ResolveDungeonWorkbenchToolDrawerLayout(
      908.0f, kSplitterWidth, 0.55f, 240.0f, 180.0f, true);
  EXPECT_FALSE(recovered.compact);
  EXPECT_NEAR(recovered.drawer_height, 495.0f, 0.001f);
  EXPECT_NEAR(recovered.canvas_height, 405.0f, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolDrawerLayoutClampsProportionToUsableMinimums) {
  const auto drawer_min = ResolveDungeonWorkbenchToolDrawerLayout(
      708.0f, kSplitterWidth, 0.1f, 240.0f, 180.0f, true);
  EXPECT_FALSE(drawer_min.compact);
  EXPECT_NEAR(drawer_min.drawer_height, 180.0f, 0.001f);
  EXPECT_NEAR(drawer_min.canvas_height, 520.0f, 0.001f);

  const auto canvas_min = ResolveDungeonWorkbenchToolDrawerLayout(
      708.0f, kSplitterWidth, 0.9f, 240.0f, 180.0f, true);
  EXPECT_FALSE(canvas_min.compact);
  EXPECT_NEAR(canvas_min.drawer_height, 460.0f, 0.001f);
  EXPECT_NEAR(canvas_min.canvas_height, 240.0f, 0.001f);

  const auto exact_fit = ResolveDungeonWorkbenchToolDrawerLayout(
      428.0f, kSplitterWidth, 0.4f, 240.0f, 180.0f, true);
  EXPECT_FALSE(exact_fit.compact);
  EXPECT_NEAR(exact_fit.drawer_height, 180.0f, 0.001f);
  EXPECT_NEAR(exact_fit.canvas_height, 240.0f, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest, ToolStripUsesOneRowAtItsExactFitWidth) {
  constexpr float kButtonSize = 32.0f;
  constexpr float kSpacing = 4.0f;
  constexpr int kItemCount = 10;
  constexpr float kExactFitWidth =
      kItemCount * kButtonSize + (kItemCount - 1) * kSpacing;

  EXPECT_EQ(ResolveDungeonWorkbenchToolStripColumns(kExactFitWidth, kButtonSize,
                                                    kSpacing, kItemCount),
            kItemCount);
  EXPECT_EQ(ResolveDungeonWorkbenchToolStripColumns(
                kExactFitWidth - 1.0f, kButtonSize, kSpacing, kItemCount),
            kItemCount - 1);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolStripWrapsSafelyAtNarrowAndEmptyWidths) {
  EXPECT_EQ(ResolveDungeonWorkbenchToolStripColumns(
                /*available_width=*/1.0f, /*button_size=*/32.0f,
                /*item_spacing=*/4.0f),
            1);
  EXPECT_EQ(ResolveDungeonWorkbenchToolStripColumns(
                /*available_width=*/400.0f, /*button_size=*/32.0f,
                /*item_spacing=*/4.0f, /*item_count=*/0),
            0);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolStripNeverReturnsMoreColumnsThanItems) {
  EXPECT_EQ(ResolveDungeonWorkbenchToolStripColumns(
                /*available_width=*/1000.0f, /*button_size=*/32.0f,
                /*item_spacing=*/4.0f, /*item_count=*/3),
            3);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ExistingStandaloneToolOwnsThePresentation) {
  EXPECT_EQ(ResolveDungeonWorkbenchToolRequestTarget(false),
            DungeonWorkbenchToolRequestTarget::kBottomDrawer);
  EXPECT_EQ(ResolveDungeonWorkbenchToolRequestTarget(true),
            DungeonWorkbenchToolRequestTarget::kStandaloneWindow);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolRequestsOpenBottomDrawerWithoutReplacingInspector) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");

  content.OpenDoorTool();
  EXPECT_TRUE(content.IsToolDrawerActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "door");

  content.OpenPaletteTool();
  EXPECT_TRUE(content.IsToolDrawerActiveForTesting());
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "palette");
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolStripCyclesAllToolsWithoutLeavingDrawer) {
  // The bottom drawer's compact tool-strip calls Open*Tool() rapidly as users
  // hop between tools. Switching active tools must not replace the independent
  // Room inspector or toggle the drawer off.
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  struct Step {
    void (DungeonWorkbenchContent::*open)();
    const char* expected_id;
  };
  const Step steps[] = {
      {&DungeonWorkbenchContent::OpenObjectSelectorTool, "object_selector"},
      {&DungeonWorkbenchContent::OpenDoorTool, "door"},
      {&DungeonWorkbenchContent::OpenSpriteTool, "sprite"},
      {&DungeonWorkbenchContent::OpenItemTool, "item"},
      {&DungeonWorkbenchContent::OpenPaletteTool, "palette"},
      {&DungeonWorkbenchContent::OpenRoomGraphicsTool, "room_graphics"},
      {&DungeonWorkbenchContent::OpenRoomTagsTool, "room_tags"},
      {&DungeonWorkbenchContent::OpenCustomCollisionTool, "custom_collision"},
      {&DungeonWorkbenchContent::OpenWaterFillTool, "water_fill"},
      {&DungeonWorkbenchContent::OpenMinecartTool, "minecart"},
  };

  for (const auto& step : steps) {
    (content.*step.open)();
    EXPECT_TRUE(content.IsToolDrawerActiveForTesting()) << step.expected_id;
    EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room")
        << step.expected_id;
    EXPECT_STREQ(content.GetActiveToolIdForTesting(), step.expected_id);
  }

  // Re-opening the active tool must keep the drawer open on the same tool;
  // strip taps are deliberately idempotent so users can confirm a selection
  // without toggling the body off.
  content.OpenMinecartTool();
  EXPECT_TRUE(content.IsToolDrawerActiveForTesting());
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "minecart");
}

TEST(DungeonWorkbenchContentLayoutTest,
     PopOutUsesEmbeddedWindowIdentityAndClosesDrawer) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  FakeWorkbenchToolContent object_selector("dungeon.object_selector");
  content.SetEmbeddedEditorPanels(&object_selector, nullptr, nullptr, nullptr,
                                  nullptr, nullptr);

  std::string opened_window_id;
  content.SetStandaloneToolCallbacks([](const std::string&) { return false; },
                                     [&](const std::string& window_id) {
                                       opened_window_id = window_id;
                                       return true;
                                     });

  content.OpenObjectSelectorTool();
  ASSERT_TRUE(content.IsToolDrawerActiveForTesting());
  EXPECT_TRUE(content.PopOutActiveTool());
  EXPECT_EQ(opened_window_id, "dungeon.object_selector");
  EXPECT_FALSE(content.IsToolDrawerActiveForTesting());
}

TEST(DungeonWorkbenchContentLayoutTest,
     OpenToolClosesDrawerAndFocusesExistingStandalone) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  FakeWorkbenchToolContent object_selector("dungeon.object_selector");
  content.SetEmbeddedEditorPanels(&object_selector, nullptr, nullptr, nullptr,
                                  nullptr, nullptr);

  int focus_count = 0;
  bool standalone_open = false;
  std::string focused_window_id;
  content.SetStandaloneToolCallbacks(
      [&](const std::string& window_id) {
        return standalone_open && window_id == "dungeon.object_selector";
      },
      [&](const std::string& window_id) {
        ++focus_count;
        focused_window_id = window_id;
        return true;
      });

  content.OpenObjectSelectorTool();
  ASSERT_TRUE(content.IsToolDrawerActiveForTesting());

  standalone_open = true;
  content.OpenObjectSelectorTool();
  EXPECT_FALSE(content.IsToolDrawerActiveForTesting());
  EXPECT_EQ(focus_count, 1);
  EXPECT_EQ(focused_window_id, "dungeon.object_selector");
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolDrawerSelectionSurvivesRoomChanges) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  content.OpenCustomCollisionTool();
  current_room_id = 0x012;
  content.NotifyRoomChanged(0x011);

  EXPECT_TRUE(content.IsToolDrawerActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "custom_collision");
}

TEST(DungeonWorkbenchPitDamageTest, BuildsStateWithSafeReplacementDefaults) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto state = BuildPitDamageMembershipState(&table, 0x001, 0x020, 0x010);

  EXPECT_TRUE(state.table_available);
  EXPECT_TRUE(state.room_valid);
  EXPECT_EQ(state.room_id, 0x001);
  EXPECT_TRUE(state.deals_damage);
  EXPECT_FALSE(state.dirty);
  ASSERT_TRUE(state.suggested_replacement_room.has_value());
  EXPECT_EQ(*state.suggested_replacement_room, 0x020);
  ASSERT_TRUE(state.suggested_victim_room.has_value());
  EXPECT_EQ(*state.suggested_victim_room, 0x010);
}

TEST(DungeonWorkbenchPitDamageTest,
     RepairsDuplicateReplacementAndNonMemberVictimDefaults) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto state = BuildPitDamageMembershipState(&table, 0x020, 0x010, 0x099);

  EXPECT_TRUE(state.table_available);
  EXPECT_TRUE(state.room_valid);
  EXPECT_FALSE(state.deals_damage);
  ASSERT_TRUE(state.suggested_replacement_room.has_value());
  EXPECT_EQ(*state.suggested_replacement_room, 0x000);
  ASSERT_TRUE(state.suggested_victim_room.has_value());
  EXPECT_EQ(*state.suggested_victim_room, 0x001);
}

TEST(DungeonWorkbenchPitDamageTest,
     AddAndRemoveSwapMembershipWithoutChangingCapacity) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto add_status = AddCurrentRoomToPitDamage(&table, 0x020, 0x010);
  ASSERT_TRUE(add_status.ok()) << add_status;
  EXPECT_TRUE(table.Contains(0x020));
  EXPECT_FALSE(table.Contains(0x010));
  EXPECT_EQ(table.room_ids().size(), 2u);
  EXPECT_TRUE(table.dirty());

  table.ClearDirty();
  auto remove_status = RemoveCurrentRoomFromPitDamage(&table, 0x020, 0x030);
  ASSERT_TRUE(remove_status.ok()) << remove_status;
  EXPECT_TRUE(table.Contains(0x030));
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_EQ(table.room_ids().size(), 2u);
  EXPECT_TRUE(table.dirty());
}

TEST(DungeonWorkbenchPitDamageTest, RejectsInvalidMembershipSwaps) {
  auto table = MakePitDamageTable({0x001, 0x010});

  EXPECT_EQ(AddCurrentRoomToPitDamage(nullptr, 0x020, 0x010).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(AddCurrentRoomToPitDamage(&table, 0x001, 0x010).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(AddCurrentRoomToPitDamage(&table, 0x020, 0x030).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(RemoveCurrentRoomFromPitDamage(&table, 0x001, 0x010).code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonWorkbenchPitDamageUiTest,
       ButtonsClickThroughToPitDamageMembershipSwaps) {
  int current_room_id = 0x020;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  auto table = MakePitDamageTable({0x001, 0x010});
  content.SetPitDamageTableProvider([&table]() { return &table; });

  DrawPitDamageFrame(content, current_room_id);
  const auto add_rect =
      content.GetPitDamageControlRectsForTesting().add_current;
  ASSERT_TRUE(add_rect.visible);
  ASSERT_LT(add_rect.min_x, add_rect.max_x);
  ASSERT_LT(add_rect.min_y, add_rect.max_y);

  ClickRect(content, current_room_id, add_rect);
  EXPECT_TRUE(table.Contains(0x020));
  EXPECT_FALSE(table.Contains(0x001));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_TRUE(table.dirty());

  table.ClearDirty();
  DrawPitDamageFrame(content, current_room_id);
  const auto replace_rect =
      content.GetPitDamageControlRectsForTesting().replace_current;
  ASSERT_TRUE(replace_rect.visible);
  ASSERT_LT(replace_rect.min_x, replace_rect.max_x);
  ASSERT_LT(replace_rect.min_y, replace_rect.max_y);

  ClickRect(content, current_room_id, replace_rect);
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_TRUE(table.Contains(0x000));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_TRUE(table.dirty());
}

TEST(DungeonWorkbenchProjectLabelsTest,
     UsesOracleRegistryDungeonAndRoomNamesWhenProjectIsOpen) {
  const std::filesystem::path root = MakeTempProjectRoot();
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  ASSERT_TRUE(std::filesystem::create_directories(planning));
  {
    std::ofstream out(planning / "dungeons.json");
    out << R"json({
      "dungeons": [
        {
          "id": "D4",
          "name": "Zora Temple",
          "vanilla_name": "Thieves' Town",
          "rooms": [
            {
              "id": "0x25",
              "name": "Water Grate",
              "grid_row": 1,
              "grid_col": 2,
              "type": "connector",
              "palette": 1,
              "blockset": 6,
              "spriteset": 3,
              "tag1": 0,
              "tag2": 0
            }
          ]
        }
      ]
    })json";
  }

  project::YazeProject project;
  project.name = "Oracle of Secrets";
  project.filepath = (root / "Oracle-of-Secrets.yaze").string();
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
                    "manifest_version": 1,
                    "hack_name": "Oracle of Secrets"
                  })json")
                  .ok());
  ASSERT_TRUE(project.hack_manifest.LoadProjectRegistry(root.string()).ok());

  EXPECT_EQ(dungeon_project_labels::GetDungeonNameForRoom(&project, 0x25),
            "D4 Zora Temple");
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");

  zelda3::ResourceLabelProvider::ProjectLabels stale_labels;
  stale_labels["room"]["37"] = "Thieves' Town";
  zelda3::GetResourceLabels().SetProjectLabels(&stale_labels);
  zelda3::GetResourceLabels().SetHackManifest(&project.hack_manifest);
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), "Water Grate");
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);

  std::filesystem::remove_all(root);
}

}  // namespace
}  // namespace yaze::editor
