#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_store.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"

namespace yaze::editor {
namespace {

class DungeonWorkbenchToolbarTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();

    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);

    ImGui::NewFrame();
    ImGui::Begin("ToolbarHost");
  }

  void TearDown() override {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

struct ToolbarGeometry {
  float consumed_height = 0.0f;
  float content_height = 0.0f;
  float cursor_max_right = 0.0f;
  float content_right = 0.0f;
};

ToolbarGeometry DrawToolbarForHistoryGeometry(
    const char* child_id, const std::deque<int>& recent_rooms) {
  const bool child_open = ImGui::BeginChild(
      child_id, ImVec2(760.0f, 80.0f), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  EXPECT_TRUE(child_open);

  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImVec2 start = ImGui::GetCursorScreenPos();

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = false;
  int compare_room_id = 0x011;
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.get_recent_rooms = [&recent_rooms]() -> const std::deque<int>& {
    return recent_rooms;
  };
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  const ToolbarGeometry geometry{
      .consumed_height = ImGui::GetCursorScreenPos().y - start.y,
      .content_height = window->DC.CursorMaxPos.y - start.y,
      .cursor_max_right = window->DC.CursorMaxPos.x,
      .content_right =
          ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x,
  };
  ImGui::EndChild();
  return geometry;
}

ToolbarGeometry DrawWorstCaseToolbarGeometry(const char* child_id, float width,
                                             DungeonCanvasViewer* viewer,
                                             bool open_overflow = false,
                                             bool compare_active = false,
                                             bool open_view = false) {
  const bool child_open = ImGui::BeginChild(
      child_id, ImVec2(width, 96.0f), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  EXPECT_TRUE(child_open);

  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = compare_active;
  int compare_room_id = 0x011;
  const std::deque<int> recent_rooms = {0x011, 0x00F, 0x002};
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.primary_viewer = viewer;
  params.on_room_selected = [](int) {
  };
  params.get_recent_rooms = [&recent_rooms]() -> const std::deque<int>& {
    return recent_rooms;
  };
  params.on_open_room_panel = [](int) {
  };
  params.forget_recent_room = [](int) {
  };
  params.set_workflow_mode = [](bool) {
  };
  params.on_save_room = [](int) {
  };
  params.on_request_dungeon_map = []() {
  };
  params.draw_tools = []() {
    ImGui::Button("Tools");
  };
  params.draw_save_content = []() {
    ImGui::TextUnformatted("Room content");
  };
  params.draw_entrance_context = [](float width) {
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("##TestEntrance", "Auto: Goron Mines"))
      ImGui::EndCombo();
  };
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  if (open_view)
    ImGui::OpenPopup("##WorkbenchViewOptions");
  if (open_overflow) {
    ImGui::OpenPopup("##WorkbenchToolbarOverflow");
  }
  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  const ToolbarGeometry geometry{
      .consumed_height = ImGui::GetCursorScreenPos().y - start.y,
      .content_height = window->DC.CursorMaxPos.y - start.y,
      .cursor_max_right = window->DC.CursorMaxPos.x,
      .content_right =
          ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x,
  };
  ImGui::EndChild();
  return geometry;
}

TEST(DungeonWorkbenchToolbarLogicTest, InlineRoomNavNeverHides) {
  EXPECT_TRUE(DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(960.0f));
  EXPECT_TRUE(DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(760.0f));
  EXPECT_TRUE(DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(720.0f));
  EXPECT_TRUE(DungeonWorkbenchToolbar::ShouldShowInlineRoomNav(320.0f));
}

class DungeonWorkbenchSizeDescriptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_custom_ = core::FeatureFlags::get().kEnableCustomObjects;
    previous_manager_ = zelda3::CustomObjectManager::Get().SnapshotState();
    core::FeatureFlags::get().kEnableCustomObjects = false;
  }

  void TearDown() override {
    core::FeatureFlags::get().kEnableCustomObjects = previous_custom_;
    zelda3::CustomObjectManager::Get().RestoreState(previous_manager_);
  }

  bool previous_custom_ = false;
  zelda3::CustomObjectManager::State previous_manager_;
};

TEST_F(DungeonWorkbenchSizeDescriptionTest,
       WallLengthUsesRenderedExtentForBothOrientations) {
  const auto horizontal =
      workbench::DescribeObjectSize(zelda3::RoomObject(0x00, 12, 9, 3));
  const auto vertical =
      workbench::DescribeObjectSize(zelda3::RoomObject(0x60, 12, 9, 3));
  EXPECT_EQ(horizontal.kind, workbench::ObjectSizeControlKind::kLength);
  EXPECT_EQ(vertical.kind, workbench::ObjectSizeControlKind::kLength);
  EXPECT_TRUE(horizontal.length_horizontal);
  EXPECT_FALSE(vertical.length_horizontal);
  EXPECT_EQ(horizontal.footprint, "6 x 2 tiles (48 x 16 px)");
  EXPECT_EQ(vertical.footprint, "2 x 6 tiles (16 x 48 px)");
  EXPECT_EQ(horizontal.wheel_hint, "Wheel: length.");

  // Encoded zero means the long wall, not an empty object or minimum length.
  const auto long_wall =
      workbench::DescribeObjectSize(zelda3::RoomObject(0x00, 12, 9, 0));
  EXPECT_EQ(long_wall.width_tiles, 64);
  EXPECT_EQ(long_wall.height_tiles, 2);
}

TEST_F(DungeonWorkbenchSizeDescriptionTest,
       PackedAreaExplainsBothAxesWithoutChangingObject) {
  const zelda3::RoomObject object(0xD1, 12, 9, 0x0B, 1);
  const auto description = workbench::DescribeObjectSize(object);
  EXPECT_EQ(description.kind, workbench::ObjectSizeControlKind::kArea);
  EXPECT_EQ(description.footprint, "12 x 16 tiles (96 x 128 px)");
  EXPECT_EQ(description.wheel_hint,
            "Wheel: grow / shrink. Shift + wheel: width.");
  EXPECT_EQ(object.size_, 0x0B);
  EXPECT_EQ(object.x_, 12);
  EXPECT_EQ(object.y_, 9);
  EXPECT_EQ(object.GetLayerValue(), 1);
}

TEST_F(DungeonWorkbenchSizeDescriptionTest,
       FixedAndCustomObjectsDoNotAdvertiseWheelResizing) {
  const auto fixed =
      workbench::DescribeObjectSize(zelda3::RoomObject(0x100, 12, 9, 0));
  EXPECT_EQ(fixed.kind, workbench::ObjectSizeControlKind::kFixed);
  EXPECT_TRUE(fixed.wheel_hint.empty());

  core::FeatureFlags::get().kEnableCustomObjects = true;
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0xC1, {"closed_a.bin", "closed_b.bin"}}});
  const auto custom =
      workbench::DescribeObjectSize(zelda3::RoomObject(0xC1, 12, 9, 1));
  EXPECT_EQ(custom.kind, workbench::ObjectSizeControlKind::kVariant);
  EXPECT_TRUE(custom.wheel_hint.empty());
}

TEST_F(DungeonWorkbenchToolbarTest, WallControlsExposeLengthAndFootprint) {
  const zelda3::RoomObject object(0x00, 12, 9, 3);
  uint8_t requested = object.size_;
  ImGui::LogToBuffer();
  ASSERT_TRUE(ImGui::BeginTable("ObjectProperties", 2));
  EXPECT_FALSE(workbench::DrawObjectSizeControls(object, &requested));
  ImGui::EndTable();
  const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
  ImGui::LogFinish();
  EXPECT_NE(text.find("Length"), std::string::npos);
  EXPECT_NE(text.find("6 tiles (48 px)"), std::string::npos);
  EXPECT_NE(text.find("Footprint"), std::string::npos);
  EXPECT_NE(text.find("6 x 2 tiles"), std::string::npos);
  EXPECT_EQ(requested, object.size_);
}

TEST_F(DungeonWorkbenchToolbarTest,
       DrawBalancesStyleStacksBeforeToolbarTeardown) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  const int style_before = context->StyleVarStack.Size;
  const int color_before = context->ColorStack.Size;

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x001;
  int previous_room_id = 0x000;
  bool split_view_enabled = false;
  int compare_room_id = 0x002;
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  EXPECT_EQ(context->StyleVarStack.Size, style_before);
  EXPECT_EQ(context->ColorStack.Size, color_before);
}

TEST_F(DungeonWorkbenchToolbarTest,
       DrawSplitViewWithNullSearchBufferKeepsImGuiStacksBalanced) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  const int style_before = context->StyleVarStack.Size;
  const int color_before = context->ColorStack.Size;
  const int window_stack_before = context->CurrentWindowStack.Size;

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x020;
  int previous_room_id = 0x01F;
  bool split_view_enabled = true;
  int compare_room_id = 0x021;

  std::deque<int> mru_rooms = {0x021, 0x01F, 0x010};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.get_recent_rooms = [&]() -> const std::deque<int>& {
    return mru_rooms;
  };
  params.compare_search_buf = nullptr;
  params.compare_search_buf_size = 0;

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  EXPECT_EQ(context->StyleVarStack.Size, style_before);
  EXPECT_EQ(context->ColorStack.Size, color_before);
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);
}

TEST_F(DungeonWorkbenchToolbarTest,
       HistoryControlKeepsToolbarHeightStableWhenHistoryChanges) {
  const std::deque<int> empty_history;
  const std::deque<int> populated_history = {0x011, 0x00F, 0x002};

  const ToolbarGeometry empty =
      DrawToolbarForHistoryGeometry("##EmptyHistoryToolbar", empty_history);
  const ToolbarGeometry populated = DrawToolbarForHistoryGeometry(
      "##PopulatedHistoryToolbar", populated_history);

  EXPECT_FLOAT_EQ(empty.consumed_height, populated.consumed_height);
  EXPECT_FLOAT_EQ(empty.content_height, populated.content_height);
}

TEST_F(DungeonWorkbenchToolbarTest,
       WorstCaseToolbarFitsNarrowAndScaledContentWidths) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0x010];
  room.SetLoaded(true);
  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const ToolbarGeometry narrow = DrawWorstCaseToolbarGeometry(
      "##NarrowToolbar", 320.0f, &viewer, /*open_overflow=*/true);
  EXPECT_LE(narrow.cursor_max_right, narrow.content_right + 0.5f);

  ImGui::GetStyle().ScaleAllSizes(1.5f);
  const ToolbarGeometry scaled = DrawWorstCaseToolbarGeometry(
      "##ScaledToolbar", 420.0f, &viewer, /*open_overflow=*/true);
  EXPECT_LE(scaled.cursor_max_right, scaled.content_right + 0.5f);
}

TEST_F(DungeonWorkbenchToolbarTest,
       ActiveCompareOverflowRendersRoomPickerWithoutStackLeaks) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0x010];
  room.SetLoaded(true);
  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const int style_before = context->StyleVarStack.Size;
  const int color_before = context->ColorStack.Size;
  const int window_stack_before = context->CurrentWindowStack.Size;
  const int popup_stack_before = context->BeginPopupStack.Size;

  const ToolbarGeometry geometry = DrawWorstCaseToolbarGeometry(
      "##ActiveCompareOverflow", 320.0f, &viewer, /*open_overflow=*/true,
      /*compare_active=*/true);

  EXPECT_LE(geometry.cursor_max_right, geometry.content_right + 0.5f);
  EXPECT_EQ(context->StyleVarStack.Size, style_before);
  EXPECT_EQ(context->ColorStack.Size, color_before);
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);
  EXPECT_EQ(context->BeginPopupStack.Size, popup_stack_before);
}

TEST_F(DungeonWorkbenchToolbarTest,
       DrawRecentRoomsPopupKeepsImGuiStacksBalanced) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  const int style_before = context->StyleVarStack.Size;
  const int color_before = context->ColorStack.Size;
  const int window_stack_before = context->CurrentWindowStack.Size;
  const int popup_stack_before = context->BeginPopupStack.Size;

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = false;
  int compare_room_id = 0x011;
  const std::deque<int> recent_rooms = {0x011, 0x00F, 0x002};
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.get_recent_rooms = [&recent_rooms]() -> const std::deque<int>& {
    return recent_rooms;
  };
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  ImGui::OpenPopup("##WorkbenchRecentRooms");
  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  EXPECT_EQ(context->StyleVarStack.Size, style_before);
  EXPECT_EQ(context->ColorStack.Size, color_before);
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);
  EXPECT_EQ(context->BeginPopupStack.Size, popup_stack_before);
}

TEST_F(DungeonWorkbenchToolbarTest,
       DrawPreservesCallerOwnedStyleAndColorStacks) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 5.0f));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.45f, 1.0f));

  const int style_before = context->StyleVarStack.Size;
  const int color_before = context->ColorStack.Size;
  const int window_stack_before = context->CurrentWindowStack.Size;

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = false;
  int compare_room_id = 0x011;
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));

  EXPECT_EQ(context->StyleVarStack.Size, style_before);
  EXPECT_EQ(context->ColorStack.Size, color_before);
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);

  ImGui::PopStyleColor(1);
  ImGui::PopStyleVar(1);
}

TEST_F(DungeonWorkbenchToolbarTest, DrawDoesNotCreateNestedChildWindowChrome) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  const int window_stack_before = context->CurrentWindowStack.Size;

  DungeonWorkbenchLayoutState layout;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = false;
  int compare_room_id = 0x011;
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);
}

TEST_F(DungeonWorkbenchToolbarTest,
       DrawConnectedModeInlineControlsStayInToolbarWindow) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  ASSERT_NE(context, nullptr);

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0x010];
  room.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const int window_stack_before = context->CurrentWindowStack.Size;

  DungeonWorkbenchLayoutState layout;
  layout.show_connected_canvas_view = true;
  int current_room_id = 0x010;
  int previous_room_id = 0x00F;
  bool split_view_enabled = false;
  int compare_room_id = 0x011;
  char compare_search[32] = {};

  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &current_room_id;
  params.previous_room_id = &previous_room_id;
  params.split_view_enabled = &split_view_enabled;
  params.compare_room_id = &compare_room_id;
  params.primary_viewer = &viewer;
  params.compare_search_buf = compare_search;
  params.compare_search_buf_size = sizeof(compare_search);

  EXPECT_FALSE(DungeonWorkbenchToolbar::Draw(params));
  EXPECT_EQ(context->CurrentWindowStack.Size, window_stack_before);
}

}  // namespace
}  // namespace yaze::editor

namespace yaze::editor {
namespace {
TEST_F(DungeonWorkbenchToolbarTest, CompareAndLayersUseDirectActions) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0x10].SetLoaded(true);
  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  DungeonWorkbenchLayoutState layout;
  int room = 0x10, previous = 0x11, compare = -1;
  bool split = false;
  char search[32] = {};
  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &room;
  params.previous_room_id = &previous;
  params.compare_room_id = &compare;
  params.split_view_enabled = &split;
  params.primary_viewer = &viewer;
  params.compare_search_buf = search;
  params.compare_search_buf_size = sizeof(search);
  const auto frame = [&] {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1100, 400), ImGuiCond_Always);
    ImGui::Begin("ToolbarHost");
    gui::WidgetIdRegistry::Instance().Clear();
    DungeonWorkbenchToolbar::Draw(params);
  };
  const auto click = [&](const char* name) {
    bool found = false;
    ImVec2 center;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == name && widget.bounds.valid) {
        found = true;
        center = ImVec2((widget.bounds.min_x + widget.bounds.max_x) * 0.5f,
                        (widget.bounds.min_y + widget.bounds.max_y) * 0.5f);
      }
    }
    ASSERT_TRUE(found) << name;
    ImGui::GetIO().AddMousePosEvent(center.x, center.y);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
  };
  frame();
  frame();
  for (const char* label : {"BG1", "BG2", "Sprites"}) {
    bool found = false;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == label) {
        found = true;
        EXPECT_EQ(widget.type, "checkbox") << label;
      }
    }
    EXPECT_TRUE(found) << label;
  }
  const bool bg1 = viewer.IsBG1Visible(room);
  click("BG1");
  EXPECT_EQ(viewer.IsBG1Visible(room), !bg1);
  const bool bg2 = viewer.IsBG2Visible(room);
  click("BG2");
  EXPECT_EQ(viewer.IsBG2Visible(room), !bg2);
  const bool sprites = viewer.AreSpritesVisible();
  click("Sprites");
  EXPECT_EQ(viewer.AreSpritesVisible(), !sprites);
  click("CompareMenuButton");
  EXPECT_TRUE(split);
  EXPECT_EQ(compare, previous);
  EXPECT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  click("CompareMenuButton");
  EXPECT_EQ(ImGui::GetCurrentContext()->OpenPopupStack.Size, 1);
  bool list_found = false;
  for (auto* window : ImGui::GetCurrentContext()->Windows)
    if (window->Active && std::string(window->Name).find("CompareSearchList") !=
                              std::string::npos)
      list_found = true;
  EXPECT_TRUE(list_found);
  gui::WidgetIdRegistry::Instance().Clear();
}

TEST_F(DungeonWorkbenchToolbarTest,
       RoomAndOverviewChoicesStayVisibleAndLeaveCompareAtLaptopWidths) {
  DungeonWorkbenchLayoutState layout;
  int room = 0x89, compare = 0x98;
  bool split = true;
  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &room;
  params.compare_room_id = &compare;
  params.split_view_enabled = &split;
  params.draw_entrance_context = [](float width) {
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("##ModeEntrance", "Auto: Goron Mines"))
      ImGui::EndCombo();
  };
  params.on_save_room = [](int) {
  };
  params.on_request_dungeon_map = [] {
  };
  params.set_workflow_mode = [](bool) {
  };
  float width = 720.0f;
  const auto frame = [&] {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 180), ImGuiCond_Always);
    ImGui::Begin("ToolbarHost");
    gui::WidgetIdRegistry::Instance().Clear();
    DungeonWorkbenchToolbar::Draw(params);
    EXPECT_LE(
        ImGui::GetCurrentWindow()->DC.CursorMaxPos.x,
        ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x + 0.5f);
  };
  const auto click = [&](const char* id) {
    ImVec2 center;
    bool found = false;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == id && widget.bounds.valid) {
        found = true;
        center = ImVec2((widget.bounds.min_x + widget.bounds.max_x) * 0.5f,
                        (widget.bounds.min_y + widget.bounds.max_y) * 0.5f);
      }
    }
    ASSERT_TRUE(found) << id;
    ImGui::GetIO().AddMousePosEvent(center.x, center.y);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
  };
  for (const float pane_width : {720.0f, 1100.0f}) {
    width = pane_width;
    split = true;
    frame();
    frame();
    click("CanvasModeConnected");
    EXPECT_TRUE(layout.show_connected_canvas_view);
    EXPECT_FALSE(split);
    EXPECT_EQ(room, 0x89);
    // Clicking the selected view is idempotent, not an ambiguous toggle.
    click("CanvasModeConnected");
    EXPECT_TRUE(layout.show_connected_canvas_view);
    click("CanvasModeRoom");
    EXPECT_FALSE(layout.show_connected_canvas_view);
    EXPECT_FALSE(split);
    EXPECT_EQ(room, 0x89);
    EXPECT_EQ(compare, 0x98);
    split = true;
    frame();
    click("CanvasModeRoom");
    EXPECT_FALSE(layout.show_connected_canvas_view);
    EXPECT_FALSE(split);
  }
  gui::WidgetIdRegistry::Instance().Clear();
}

TEST_F(DungeonWorkbenchToolbarTest, ViewOptionsExposeCrumbleFloorControl) {
  gui::WidgetIdRegistry::Instance().Clear();
  DungeonCanvasViewer viewer;
  DrawWorstCaseToolbarGeometry("##CrumbleToolbar", 1100.0f, &viewer, false,
                               false, true);
  bool found = false;
  for (const auto& [path, widget] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    if (widget.label == "Crumble Floor (Oracle)") {
      found = true;
      EXPECT_TRUE(widget.enabled);
    }
  }
  EXPECT_TRUE(found);
  gui::WidgetIdRegistry::Instance().Clear();
}

TEST_F(DungeonWorkbenchToolbarTest,
       HeaderActionsReserveTheirVisibleGlyphWidth) {
  constexpr float height = 22.0f;
  const char* glyph = "Wide icon";
  workbench::DrawHeaderIconAction("WideHeaderAction", glyph, height, "Move");
  const float width = ImGui::GetItemRectMax().x - ImGui::GetItemRectMin().x;
  EXPECT_GE(width, ImGui::CalcTextSize(glyph).x +
                       ImGui::GetStyle().FramePadding.x * 2.0f);
  EXPECT_FLOAT_EQ(width, workbench::CalcIconButtonWidth(glyph, height));
  EXPECT_GT(width, height);
}

TEST_F(DungeonWorkbenchToolbarTest, SaveContentOpensOnlyFromToolbarMenu) {
  DungeonWorkbenchLayoutState layout;
  DungeonCanvasViewer viewer;
  int room = 0x89, compare = -1;
  bool split = false;
  int save_choices_drawn = 0;
  DungeonWorkbenchToolbarParams params;
  params.layout = &layout;
  params.current_room_id = &room;
  params.compare_room_id = &compare;
  params.split_view_enabled = &split;
  params.primary_viewer = &viewer;
  params.draw_tools = []() {
    ImGui::Button("Tools");
  };
  params.draw_save_content = [&]() {
    ++save_choices_drawn;
    ImGui::TextUnformatted("Room objects");
  };
  const auto frame = [&]() {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1100, 400), ImGuiCond_Always);
    ImGui::Begin("ToolbarHost");
    gui::WidgetIdRegistry::Instance().Clear();
    DungeonWorkbenchToolbar::Draw(params);
  };
  frame();
  frame();
  EXPECT_EQ(save_choices_drawn, 0);
  ImVec2 center;
  bool found = false;
  for (const auto& [path, widget] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    if (widget.label == "SaveContentButton" && widget.bounds.valid) {
      found = true;
      center = ImVec2((widget.bounds.min_x + widget.bounds.max_x) * 0.5f,
                      (widget.bounds.min_y + widget.bounds.max_y) * 0.5f);
    }
  }
  ASSERT_TRUE(found);
  ImGui::GetIO().AddMousePosEvent(center.x, center.y);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, true);
  frame();
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  frame();
  EXPECT_GT(save_choices_drawn, 0);
  gui::WidgetIdRegistry::Instance().Clear();
}
}  // namespace
}  // namespace yaze::editor
