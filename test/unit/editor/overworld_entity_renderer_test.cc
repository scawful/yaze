#include "app/editor/overworld/entity/overworld_entity_renderer.h"

#include <limits>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/overworld/canvas/overworld_canvas_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/theme_manager.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "test_utils/overworld_model_test_peer.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {
namespace {

TEST(OverworldEntityRendererTest,
     ShouldRenderEntityLabelsRespectsZoomThreshold) {
  EXPECT_FLOAT_EQ(kOverworldEntityLabelMinScale, 0.5f);
  EXPECT_TRUE(ShouldRenderEntityLabels(kOverworldEntityLabelMinScale));
  EXPECT_TRUE(ShouldRenderEntityLabels(0.75f));
  EXPECT_TRUE(ShouldRenderEntityLabels(1.0f));
  EXPECT_TRUE(ShouldRenderEntityLabels(2.0f));

  EXPECT_FALSE(ShouldRenderEntityLabels(0.499f));
  EXPECT_FALSE(ShouldRenderEntityLabels(0.25f));
  EXPECT_FALSE(ShouldRenderEntityLabels(0.2f));
  EXPECT_FALSE(ShouldRenderEntityLabels(0.1f));
  EXPECT_FALSE(ShouldRenderEntityLabels(0.0f));
  EXPECT_FALSE(ShouldRenderEntityLabels(-1.0f));
  EXPECT_FALSE(
      ShouldRenderEntityLabels(std::numeric_limits<float>::quiet_NaN()));
  EXPECT_FALSE(
      ShouldRenderEntityLabels(std::numeric_limits<float>::infinity()));
  EXPECT_FALSE(
      ShouldRenderEntityLabels(-std::numeric_limits<float>::infinity()));
}

TEST(OverworldEntityRendererTest,
     FitZoomHidesEntityTextLabelsWhileKeepingBoxesAndHover) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280, 960);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int tex_w = 0, tex_h = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &tex_w, &tex_h);
  gui::ThemeManager::Get().ApplyClassicYazeTheme();
  AgentUI::RefreshTheme();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::Overworld overworld(&rom);
  zelda3::OverworldModelTestPeer::SetLoaded(overworld, true);

  overworld.mutable_entrances()->emplace_back(64, 64, 0x05, 0x00, 0x0000,
                                              false);
  overworld.mutable_exits()->emplace_back(0x10, 0x40, 0, 0, 0, 128, 128, 0, 0,
                                          0, 0, 0, 0);
  // Keep the item on Light World $00 so it stays inside the viewport clip rect
  // at both fit zoom (0.2x) and the 0.5x label threshold.
  overworld.mutable_all_items()->emplace_back(3, 0x00, 320, 320, false);
  overworld.mutable_sprites(0)->emplace_back(std::vector<uint8_t>{}, 0x80, 0x09,
                                             0, 0, 96, 96);

  gui::Canvas canvas;
  std::vector<gfx::Bitmap> sprite_previews;
  OverworldEntityRenderer renderer(&overworld, &canvas, &sprite_previews);
  const OverworldCanvasLayout unified(OverworldCanvasLayoutMode::kUnified);

  auto render_entities = [&](float scale, ImVec2 mouse_pos) -> int {
    io.AddMousePosEvent(mouse_pos.x, mouse_pos.y);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1280, 960), ImGuiCond_Always);
    ImGui::Begin("EntityRendererTestHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int vtx_before = draw_list->VtxBuffer.Size;
    gui::CanvasRuntime rt;
    rt.draw_list = draw_list;
    rt.canvas_p0 = ImVec2(0, 0);
    rt.scrolling = ImVec2(0, 0);
    rt.scale = scale;
    renderer.DrawAllEntities(rt, unified, /*current_world=*/0,
                             /*game_state=*/0);
    const int vtx_added = draw_list->VtxBuffer.Size - vtx_before;
    ImGui::End();
    ImGui::Render();
    return vtx_added;
  };

  // Below threshold (fit zoom): marker boxes draw and hover works, while labels
  // are suppressed (varying item label length adds zero vertices).
  const ImVec2 dw_block = unified.BlockOrigin(1);
  const ImVec2 dw_exit_hover((dw_block.x + 132.0f) * 0.2f,
                             (dw_block.y + 132.0f) * 0.2f);
  const int vtx_fit_short = render_entities(0.2f, dw_exit_hover);
  EXPECT_GT(vtx_fit_short, 0);
  ASSERT_NE(renderer.hovered_entity(), nullptr);
  EXPECT_EQ(renderer.hovered_entity()->entity_type_,
            zelda3::GameEntity::EntityType::kExit);
  EXPECT_EQ((*overworld.mutable_exits())[0].entity_id_, 0);

  (*overworld.mutable_all_items())[0].id_ = 15;  // "Green Soldier" (longer)
  const int vtx_fit_long = render_entities(0.2f, ImVec2(-10, -10));
  EXPECT_EQ(vtx_fit_long, vtx_fit_short);

  // At and above threshold: entity labels render in addition to marker boxes.
  (*overworld.mutable_all_items())[0].id_ = 3;  // "Bee"
  const int vtx_threshold_short = render_entities(0.5f, ImVec2(-10, -10));
  EXPECT_GT(vtx_threshold_short, vtx_fit_short);

  (*overworld.mutable_all_items())[0].id_ = 15;  // "Green Soldier"
  const int vtx_threshold_long = render_entities(0.5f, ImVec2(-10, -10));
  EXPECT_GT(vtx_threshold_long, vtx_threshold_short);

  ImGui::DestroyContext(context);
}

}  // namespace
}  // namespace yaze::editor
