#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

#include "app/editor/menu/window_sidebar.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class WindowSidebarTestPeer {
 public:
  static void SetSearch(WindowSidebar& sidebar, const char* search) {
    std::snprintf(sidebar.sidebar_search_, sizeof(sidebar.sidebar_search_),
                  "%s", search);
  }
};

namespace {

TEST(WindowSidebarTest, MatchesSearchByNameIdAndShortcut) {
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "item", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "overworld.item", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_TRUE(WindowSidebar::MatchesWindowSearch(
      "ctrl+i", "Item List", "overworld.item_list", "Ctrl+I"));
  EXPECT_FALSE(WindowSidebar::MatchesWindowSearch(
      "graphics", "Item List", "overworld.item_list", "Ctrl+I"));
}

TEST(WindowSidebarTest, DetectsDungeonWindowModeTargets) {
  EXPECT_TRUE(
      WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_selector"));
  EXPECT_TRUE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_matrix"));
  EXPECT_TRUE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.room_298"));
  EXPECT_FALSE(WindowSidebar::IsDungeonWindowModeTarget("dungeon.workbench"));
  EXPECT_FALSE(
      WindowSidebar::IsDungeonWindowModeTarget("overworld.map_properties"));
}

TEST(WindowSidebarTest, SidebarSectionForMapsGroupsAndFallbacks) {
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(""), "Editors");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Windows"), "Editors");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Core"), "Core");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Advanced"), "Advanced");
  EXPECT_EQ(WindowSidebar::SidebarSectionFor("Planning"), "Planning");
}

TEST(WindowSidebarTest, SidebarSectionForInfersRoomsFromCardId) {
  WindowDescriptor room;
  room.card_id = "dungeon.room_42";
  room.workflow_group = "";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(room), "Rooms");

  WindowDescriptor selector;
  selector.card_id = "dungeon.room_selector";
  selector.workflow_group = "";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(selector), "Editors");

  WindowDescriptor core;
  core.card_id = "dungeon.workbench";
  core.workflow_group = "Core";
  EXPECT_EQ(WindowSidebar::SidebarSectionFor(core), "Core");
}

TEST(WindowSidebarTest, OmitsWindowModeTargetsInWorkbench) {
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_selector", true));
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_matrix", true));
  EXPECT_TRUE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_298", true));
  EXPECT_FALSE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.workbench", true));
  EXPECT_FALSE(WindowSidebar::ShouldOmitWindowInSidebar(
      "dungeon.object_selector", true));
  EXPECT_FALSE(
      WindowSidebar::ShouldOmitWindowInSidebar("dungeon.room_selector", false));
}

TEST(WindowSidebarTest, RoomToolsAndNonNumericSuffixesAreNotRoomWindows) {
  for (const char* id :
       {"dungeon.room_graphics", "dungeon.room_tags", "dungeon.room_",
        "dungeon.room_12a", "dungeon.room_12_extra"}) {
    SCOPED_TRACE(id);
    EXPECT_FALSE(WindowSidebar::IsDungeonWindowModeTarget(id));
    EXPECT_FALSE(WindowSidebar::ShouldOmitWindowInSidebar(id, true));
    WindowDescriptor window{};
    window.card_id = id;
    EXPECT_EQ(WindowSidebar::SidebarSectionFor(window), "Editors");
  }
  for (const char* id :
       {"dungeon.room_0", "dungeon.room_42", "dungeon.room_295"}) {
    EXPECT_TRUE(WindowSidebar::IsDungeonWindowModeTarget(id));
    EXPECT_TRUE(WindowSidebar::ShouldOmitWindowInSidebar(id, true));
    WindowDescriptor window{};
    window.card_id = id;
    EXPECT_EQ(WindowSidebar::SidebarSectionFor(window), "Rooms");
  }
}

class WindowSidebarFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    manager_.RegisterSession(0);
    manager_.SetActiveSession(0);
  }

  void TearDown() override { ImGui::DestroyContext(context_); }

  void RegisterWindow(const char* id, const char* name) {
    manager_.RegisterPanel({.card_id = id,
                            .display_name = name,
                            .icon = "T",
                            .category = "Dungeon",
                            .workflow_group = "Editors",
                            .visibility_flag = &visible_,
                            .priority = 0});
  }

  std::string DrawFrame(WindowSidebar& sidebar) {
    ImGui::NewFrame();
    // Log actual submitted text. CollapsingHeader deliberately does not auto-
    // expand for logging, so missing rows remain missing in this observation.
    ImGui::LogToBuffer();
    sidebar.Draw(0, "Dungeon", []() { return true; });
    const std::string text = context_->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::EndFrame();
    return text;
  }

  ImGuiWindow* FindWindowList() {
    for (ImGuiWindow* window : context_->Windows) {
      if (window->ParentWindow &&
          std::strcmp(window->ParentWindow->Name, "##SidePanel") == 0 &&
          std::strstr(window->Name, "##WindowContent") != nullptr) {
        return window;
      }
    }
    return nullptr;
  }

  ImGuiContext* context_ = nullptr;
  WorkspaceWindowManager manager_;
  bool visible_ = false;
};

TEST_F(WindowSidebarFrameTest,
       FilterRevealsCollapsedSectionWithoutChangingUnfilteredState) {
  RegisterWindow("dungeon.object_selector", "Unique Object Picker");
  WindowSidebar sidebar(manager_, []() { return true; });
  DrawFrame(sidebar);
  auto* list = FindWindowList();
  ASSERT_NE(list, nullptr);
  const ImGuiID section_id = list->GetID("Editors##sidebar_section_Editors");

  // Confirm observation of the real row, then seed the persisted state written
  // when the user collapses this section.
  list->StateStorage.SetInt(section_id, 1);
  EXPECT_NE(DrawFrame(sidebar).find("Unique Object Picker"), std::string::npos);
  list->StateStorage.SetInt(section_id, 0);
  EXPECT_EQ(DrawFrame(sidebar).find("Unique Object Picker"), std::string::npos);

  WindowSidebarTestPeer::SetSearch(sidebar, "Unique");
  EXPECT_NE(DrawFrame(sidebar).find("Unique Object Picker"), std::string::npos);
  EXPECT_EQ(list->StateStorage.GetInt(section_id), 0);

  WindowSidebarTestPeer::SetSearch(sidebar, "");
  EXPECT_EQ(DrawFrame(sidebar).find("Unique Object Picker"), std::string::npos);
  EXPECT_EQ(list->StateStorage.GetInt(section_id), 0);
}

TEST_F(WindowSidebarFrameTest, WorkbenchSearchIncludesStandaloneRoomTools) {
  RegisterWindow("dungeon.room_graphics", "Room Graphics Tool");
  RegisterWindow("dungeon.room_tags", "Room Tags Tool");
  WindowSidebar sidebar(manager_, []() { return true; });
  WindowSidebarTestPeer::SetSearch(sidebar, "Room");
  DrawFrame(sidebar);
  const std::string text = DrawFrame(sidebar);
  EXPECT_NE(text.find("Room Graphics Tool"), std::string::npos);
  EXPECT_NE(text.find("Room Tags Tool"), std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
