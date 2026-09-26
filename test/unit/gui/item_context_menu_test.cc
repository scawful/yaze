#include "app/gui/canvas/item_context_menu.h"

#include <gtest/gtest.h>

#include <functional>

#include "app/gui/canvas/canvas_menu.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

// ---------------------------------------------------------------------------
// MenuConfirmState: pure state machine
// ---------------------------------------------------------------------------

TEST(MenuConfirmStateTest, FirstClickArmsSecondClickConfirms) {
  MenuConfirmState state;
  state.NoteRendered(42, 10);
  EXPECT_FALSE(state.IsArmed(42, 10));
  EXPECT_FALSE(state.Click(42, 10));
  EXPECT_TRUE(state.IsArmed(42, 10));

  state.NoteRendered(42, 11);
  EXPECT_TRUE(state.IsArmed(42, 11));
  EXPECT_TRUE(state.Click(42, 11));
  EXPECT_FALSE(state.IsArmed(42, 11));
  EXPECT_EQ(state.armed_id(), 0u);
}

TEST(MenuConfirmStateTest, ClosingTheMenuDisarms) {
  MenuConfirmState state;
  EXPECT_FALSE(state.Click(7, 1));
  // Frames 2..4 do not render the item (menu closed); frame 5 reopens it.
  state.NoteRendered(7, 5);
  EXPECT_FALSE(state.IsArmed(7, 5));
  EXPECT_FALSE(state.Click(7, 5));  // Re-arms rather than confirming.
  EXPECT_TRUE(state.IsArmed(7, 5));
}

TEST(MenuConfirmStateTest, ArmingAnotherItemDisarmsTheFirst) {
  MenuConfirmState state;
  EXPECT_FALSE(state.Click(1, 1));
  EXPECT_FALSE(state.Click(2, 1));
  EXPECT_FALSE(state.IsArmed(1, 1));
  EXPECT_TRUE(state.IsArmed(2, 1));
  // Clicking the first item again only re-arms it.
  EXPECT_FALSE(state.Click(1, 2));
  EXPECT_TRUE(state.Click(1, 3));
}

TEST(MenuConfirmStateTest, ZeroIdIsNeverArmed) {
  MenuConfirmState state;
  EXPECT_FALSE(state.IsArmed(0, 0));
}

TEST(MenuConfirmLabelTest, StripsEllipsisAndAsks) {
  EXPECT_EQ(ConfirmLabel("Delete Song..."), "Confirm Delete Song?");
  EXPECT_EQ(ConfirmLabel("Forget Project\xE2\x80\xA6"),
            "Confirm Forget Project?");
  EXPECT_EQ(ConfirmLabel("Close Session"), "Confirm Close Session?");
}

TEST(MenuItemSpecTest, DestructiveFactorySetsFlags) {
  auto item = MenuItemSpec::Destructive("Delete...", "", nullptr);
  EXPECT_TRUE(item.destructive);
  EXPECT_TRUE(item.requires_confirmation);
  auto quick = MenuItemSpec::Destructive("Close", "", nullptr, false, "Ctrl+W");
  EXPECT_TRUE(quick.destructive);
  EXPECT_FALSE(quick.requires_confirmation);
  EXPECT_EQ(quick.shortcut, "Ctrl+W");
}

// ---------------------------------------------------------------------------
// RenderMenuItem inside a headless ImGui popup
// ---------------------------------------------------------------------------

class MenuRenderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int w = 0;
    int h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    GetMenuConfirmState().Reset();
  }

  void TearDown() override {
    GetMenuConfirmState().Reset();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  // Runs one frame. The popup "menu" is opened on the first frame; `items`
  // render inside it. Returns whether the popup was open after the frame.
  bool Frame(const std::vector<MenuItemSpec>& items) {
    ImGui::NewFrame();
    if (first_frame_) {
      ImGui::OpenPopup("menu");
      first_frame_ = false;
    }
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
    bool open = false;
    item_rects_.clear();
    if (ImGui::BeginPopup("menu")) {
      open = true;
      for (const auto& item : items) {
        RenderMenuItem(item);
        item_rects_.push_back(
            ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
      }
      ImGui::EndPopup();
    }
    ImGui::EndFrame();
    ImGui::Render();
    return open;
  }

  // Moves the mouse onto item `index` and clicks it over a few frames.
  bool ClickItem(const std::vector<MenuItemSpec>& items, size_t index) {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 center = item_rects_.at(index).GetCenter();
    io.AddMousePosEvent(center.x, center.y);
    Frame(items);
    io.AddMouseButtonEvent(0, true);
    Frame(items);
    io.AddMouseButtonEvent(0, false);
    Frame(items);
    // A popup closed by a click stops rendering on the following frame.
    return Frame(items);
  }

  ImGuiContext* context_ = nullptr;
  bool first_frame_ = true;
  std::vector<ImRect> item_rects_;
};

TEST_F(MenuRenderTest, ConfirmItemNeedsTwoClicksAndStaysOpen) {
  int runs = 0;
  std::vector<MenuItemSpec> items;
  items.push_back(
      MenuItemSpec::Destructive("Delete All...", "", [&runs]() { ++runs; }));
  ASSERT_TRUE(Frame(items));
  ASSERT_TRUE(Frame(items));  // Popup auto-fit settles.

  EXPECT_TRUE(ClickItem(items, 0));  // Arming click keeps the menu open.
  EXPECT_EQ(runs, 0);

  EXPECT_FALSE(ClickItem(items, 0));  // Confirming click runs and closes.
  EXPECT_EQ(runs, 1);
}

TEST_F(MenuRenderTest, PlainItemRunsOnFirstClick) {
  int runs = 0;
  std::vector<MenuItemSpec> items;
  items.emplace_back("Open", "", [&runs]() { ++runs; });
  ASSERT_TRUE(Frame(items));
  ASSERT_TRUE(Frame(items));
  EXPECT_FALSE(ClickItem(items, 0));
  EXPECT_EQ(runs, 1);
}

TEST_F(MenuRenderTest, HiddenAndDisabledItemsDoNotRun) {
  int runs = 0;
  std::vector<MenuItemSpec> items;
  items.emplace_back("Enabled", "", [&runs]() { runs += 100; });
  MenuItemSpec disabled("Disabled", "", [&runs]() { ++runs; });
  disabled.enabled_condition = []() {
    return false;
  };
  items.push_back(disabled);
  MenuItemSpec hidden("Hidden", "", [&runs]() { ++runs; });
  hidden.visible_condition = []() {
    return false;
  };
  items.push_back(hidden);

  ASSERT_TRUE(Frame(items));
  ASSERT_TRUE(Frame(items));
  // Hidden item submits nothing, so the last recorded rect repeats the
  // disabled item's rect rather than adding a new row below it.
  ASSERT_EQ(item_rects_.size(), 3u);
  EXPECT_EQ(item_rects_[1].Min.y, item_rects_[2].Min.y);

  EXPECT_TRUE(ClickItem(items, 1));  // Disabled: no run, menu stays open.
  EXPECT_EQ(runs, 0);
}

}  // namespace
}  // namespace gui
}  // namespace yaze
