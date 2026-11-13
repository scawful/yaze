#include "app/gui/core/layout_helpers.h"

#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::gui {
namespace {

TEST(ColumnLabelTest, AppendsStableIdAfterTripleHash) {
  EXPECT_EQ(ColumnLabel("Rooms", "rooms"), "Rooms###rooms");
  EXPECT_EQ(ColumnLabel("", "status"), "###status");
}

// TableSetupColumn() keys a column by ImHashStr(label), and that hash starts
// over at "###". A translated header with the same stable ID therefore keeps
// the ID that saved width, order and visibility are matched by.
TEST(ColumnLabelTest, ColumnIdIgnoresVisibleText) {
  const std::string english = ColumnLabel("Rooms", "rooms");
  const std::string french = ColumnLabel("Salles", "rooms");
  const std::string other = ColumnLabel("Rooms", "status");
  EXPECT_EQ(ImHashStr(english.c_str()), ImHashStr(french.c_str()));
  EXPECT_NE(ImHashStr(english.c_str()), ImHashStr(other.c_str()));
}

struct StackSizes {
  int colors = 0;
  int vars = 0;
};

StackSizes ReadStacks() {
  const ImGuiContext& g = *ImGui::GetCurrentContext();
  return {g.ColorStack.Size, g.StyleVarStack.Size};
}

// Draws a 3-column table with a header row and returns how many vertices it
// added to the window draw list. ImDrawListSplitter splits only the index
// buffers, so vertices from every table channel land in this count.
int DrawHeaderTable(const char* str_id, const char* first, const char* second,
                    const char* third) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const int before = draw_list->VtxBuffer.Size;
  if (ImGui::BeginTable(str_id, 3)) {
    ImGui::TableSetupColumn(first);
    ImGui::TableSetupColumn(second);
    ImGui::TableSetupColumn(third);
    ImGui::TableHeadersRow();
    ImGui::EndTable();
  }
  return draw_list->VtxBuffer.Size - before;
}

// Runs real ImGui frames so BeginTable() takes the same paths it takes in the
// editor, including the ones where it returns false.
class TableHelpersFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    // An unbalanced push or pop should fail the EXPECTs in the test, not
    // abort the test binary from the stack check inside End().
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  // `collapsed` keeps the title bar, because ImGui only collapses windows
  // that have one. A collapsed window sets SkipItems, which makes
  // BeginTable() return false before it opens anything.
  void RunFrame(const std::function<void()>& draw, bool collapsed = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(600.0f, 300.0f), ImGuiCond_Always);
    ImGui::SetNextWindowCollapsed(collapsed, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize;
    if (!collapsed) {
      flags |= ImGuiWindowFlags_NoDecoration;
    }
    ImGui::Begin("TableHost", nullptr, flags);
    draw();
    ImGui::End();
    ImGui::EndFrame();
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(TableHelpersFrameTest, HeaderShowsOnlyVisibleText) {
  const std::string rooms = ColumnLabel("Rooms", "rooms");
  const std::string name = ColumnLabel("Name", "name");
  const std::string status = ColumnLabel("Status", "status");
  int plain = 0;
  int labeled = 0;
  int suffixed = 0;
  std::string column_name;
  ImGuiID column_id = 0;
  auto draw = [&]() {
    plain = DrawHeaderTable("plain", "Rooms", "Name", "Status");
    labeled =
        DrawHeaderTable("labeled", rooms.c_str(), name.c_str(), status.c_str());
    // Control: this is what the header would cost if the ID were drawn.
    suffixed =
        DrawHeaderTable("suffixed", "Roomsrooms", "Namename", "Statusstatus");
    if (ImGui::BeginTable("names", 1)) {
      ImGui::TableSetupColumn(rooms.c_str());
      column_name = ImGui::TableGetColumnName(0);
      column_id = ImGui::GetCurrentTable()->Columns[0].ID;
      ImGui::EndTable();
    }
  };

  // Tables size their columns on the first frame, so measure the second.
  RunFrame(draw);
  RunFrame(draw);

  EXPECT_GT(plain, 0);
  EXPECT_EQ(labeled, plain);
  EXPECT_GT(suffixed, plain);
  // TableGetColumnName() returns the whole label, ID included. Code that
  // compares column names must account for the "###" suffix.
  EXPECT_EQ(column_name, rooms);
  EXPECT_EQ(column_id, ImHashStr("###rooms"));
}

TEST_F(TableHelpersFrameTest, ThemedTableBalancedAfterBeginAndEnd) {
  bool opened = false;
  StackSizes before;
  StackSizes inside;
  StackSizes after;
  float expected_padding_y = 0.0f;
  float padding_y = 0.0f;
  RunFrame([&]() {
    expected_padding_y = LayoutHelpers::GetTableRowHeight() * 0.25f;
    before = ReadStacks();
    opened = LayoutHelpers::BeginTableWithTheming("themed", 2);
    if (opened) {
      inside = ReadStacks();
      padding_y = ImGui::GetStyle().CellPadding.y;
      LayoutHelpers::EndTableWithTheming();
    }
    after = ReadStacks();
  });

  ASSERT_TRUE(opened);
  EXPECT_EQ(inside.colors, before.colors + 5);
  EXPECT_EQ(inside.vars, before.vars + 1);
  EXPECT_FLOAT_EQ(padding_y, expected_padding_y);
  EXPECT_EQ(after.colors, before.colors);
  EXPECT_EQ(after.vars, before.vars);
}

TEST_F(TableHelpersFrameTest, ThemedTableBalancedWithNoHostExtendY) {
  bool opened = false;
  StackSizes before;
  StackSizes inside;
  StackSizes after;
  float style_padding_y = 0.0f;
  float padding_y = -1.0f;
  RunFrame([&]() {
    style_padding_y = ImGui::GetStyle().CellPadding.y;
    before = ReadStacks();
    opened = LayoutHelpers::BeginTableWithTheming(
        "themed", 2, ImGuiTableFlags_NoHostExtendY, ImVec2(0.0f, 100.0f));
    if (opened) {
      inside = ReadStacks();
      padding_y = ImGui::GetStyle().CellPadding.y;
      LayoutHelpers::EndTableWithTheming();
    }
    after = ReadStacks();
  });

  ASSERT_TRUE(opened);
  // The caller opted out of the row height, but the var is still pushed so
  // EndTableWithTheming() pops the same count.
  EXPECT_EQ(inside.colors, before.colors + 5);
  EXPECT_EQ(inside.vars, before.vars + 1);
  EXPECT_FLOAT_EQ(padding_y, style_padding_y);
  EXPECT_EQ(after.colors, before.colors);
  EXPECT_EQ(after.vars, before.vars);
}

TEST_F(TableHelpersFrameTest, NestedThemedTablesStayBalanced) {
  bool outer_opened = false;
  bool inner_opened = false;
  StackSizes before;
  StackSizes inner;
  StackSizes after;
  RunFrame([&]() {
    before = ReadStacks();
    outer_opened = LayoutHelpers::BeginTableWithTheming("outer", 1);
    if (outer_opened) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      inner_opened = LayoutHelpers::BeginTableWithTheming(
          "inner", 2, ImGuiTableFlags_NoHostExtendY, ImVec2(0.0f, 80.0f));
      if (inner_opened) {
        inner = ReadStacks();
        LayoutHelpers::EndTableWithTheming();
      }
      LayoutHelpers::EndTableWithTheming();
    }
    after = ReadStacks();
  });

  ASSERT_TRUE(outer_opened);
  ASSERT_TRUE(inner_opened);
  EXPECT_EQ(inner.colors, before.colors + 10);
  EXPECT_EQ(inner.vars, before.vars + 2);
  EXPECT_EQ(after.colors, before.colors);
  EXPECT_EQ(after.vars, before.vars);
}

TEST_F(TableHelpersFrameTest, FailedBeginInCollapsedWindowLeavesStacks) {
  bool skip_items = false;
  bool opened = true;
  StackSizes before;
  StackSizes after;
  auto draw = [&]() {
    skip_items = ImGui::GetCurrentWindow()->SkipItems;
    before = ReadStacks();
    opened = LayoutHelpers::BeginTableWithTheming("themed", 2);
    if (opened) {
      LayoutHelpers::EndTableWithTheming();
    }
    after = ReadStacks();
  };

  // The first frames apply the collapse and finish any window auto-fit.
  RunFrame(draw, /*collapsed=*/true);
  RunFrame(draw, /*collapsed=*/true);
  RunFrame(draw, /*collapsed=*/true);

  ASSERT_TRUE(skip_items) << "host window did not reach SkipItems";
  EXPECT_FALSE(opened);
  EXPECT_EQ(after.colors, before.colors);
  EXPECT_EQ(after.vars, before.vars);
}

TEST_F(TableHelpersFrameTest, FailedBeginWhenScrolledOutLeavesStacks) {
  bool opened = true;
  StackSizes before;
  StackSizes after;
  auto draw = [&]() {
    // A scrolling table with a known height is skipped when its outer rect
    // is outside the window's clip rect. BeginTable() returns false there
    // after the theme pushes already happened.
    ImGui::SetCursorPosY(2000.0f);
    before = ReadStacks();
    opened = LayoutHelpers::BeginTableWithTheming(
        "themed", 2, ImGuiTableFlags_ScrollY, ImVec2(0.0f, 100.0f));
    if (opened) {
      LayoutHelpers::EndTableWithTheming();
    }
    after = ReadStacks();
  };

  RunFrame(draw);
  RunFrame(draw);

  EXPECT_FALSE(opened);
  EXPECT_EQ(after.colors, before.colors);
  EXPECT_EQ(after.vars, before.vars);
}

}  // namespace
}  // namespace yaze::gui
