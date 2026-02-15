#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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

}  // namespace
}  // namespace yaze::editor
