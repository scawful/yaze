#include "app/editor/overworld/entity/overworld_entity_renderer.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/canvas/canvas_draw.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "util/hex.h"

namespace yaze::editor {
namespace {

class OverworldProjectConnectionRendererTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(900, 700);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    authoring_.maps[0].doors.push_back({101, 2, 3, 0x25});
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  std::vector<ImDrawVert> Frame(
      const std::function<void(const gui::CanvasRuntime&)>& draw,
      ImVec2 mouse = ImVec2(108, 108), bool hovered = true,
      float scale = 1.5f) {
    ImGui::GetIO().AddMousePosEvent(mouse.x, mouse.y);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(850, 650), ImGuiCond_Always);
    ImGui::Begin("ProjectConnectionRendererHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    gui::CanvasRuntime rt;
    rt.draw_list = ImGui::GetWindowDrawList();
    rt.canvas_p0 = ImVec2(100, 100);
    rt.canvas_sz = ImVec2(700, 450);
    rt.scale = scale;
    rt.scrolling = ImVec2(-32 * scale, -2096 * scale);
    rt.hovered = hovered;
    // All frames use the same untextured outline geometry for comparison.
    rt.draw_list->Flags &=
        ~(ImDrawFlags_AALines | ImDrawFlags_UseTexForRoundCorners);
    const int first = rt.draw_list->VtxBuffer.Size;
    draw(rt);
    std::vector<ImDrawVert> vertices(rt.draw_list->VtxBuffer.begin() + first,
                                     rt.draw_list->VtxBuffer.end());
    ImGui::End();
    ImGui::Render();
    return vertices;
  }

  static void ExpectSameVertices(const std::vector<ImDrawVert>& actual,
                                 const std::vector<ImDrawVert>& expected) {
    ASSERT_FALSE(expected.empty());
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      SCOPED_TRACE(i);
      EXPECT_FLOAT_EQ(actual[i].pos.x, expected[i].pos.x);
      EXPECT_FLOAT_EQ(actual[i].pos.y, expected[i].pos.y);
      EXPECT_FLOAT_EQ(actual[i].uv.x, expected[i].uv.x);
      EXPECT_FLOAT_EQ(actual[i].uv.y, expected[i].uv.y);
      EXPECT_EQ(actual[i].col, expected[i].col);
    }
  }

  void NativeEntranceAt(int x = 32, int y = 2096) {
    zelda3::OverworldEntrance entrance;
    entrance.map_id_ = 0xA0;
    entrance.x_ = x;
    entrance.y_ = y;
    entrance.entrance_id_ = 0x25;
    overworld_.entrances().push_back(entrance);
  }

  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  zelda3::Overworld overworld_{nullptr};
  core::LyraAuthoringState authoring_;
  OverworldEntityRenderer renderer_{&overworld_, nullptr, nullptr};
};

TEST_F(OverworldProjectConnectionRendererTest,
       EntranceUsesActualNativeMarkerGeometryThemeAndHexLabel) {
  NativeEntranceAt();
  const auto native =
      Frame([&](const auto& rt) { renderer_.DrawEntrances(rt, 2); });
  const auto project = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  });
  ExpectSameVertices(project, native);
  ASSERT_TRUE(renderer_.hovered_project_connection().has_value());
  EXPECT_EQ(*renderer_.hovered_project_connection(),
            OverworldConnectionReference(ProjectEntranceReference{101}));
  EXPECT_EQ(renderer_.hovered_entity(), nullptr);
}

TEST_F(OverworldProjectConnectionRendererTest,
       ReturnUsesActualNativeMarkerGeometryThemeAndRowLabel) {
  authoring_.maps[0].doors.clear();
  authoring_.maps[0].exits.push_back({202, 0x105, 2, 3, 0});
  zelda3::OverworldExit exit;
  exit.map_id_ = 0xA0;
  exit.x_ = 32;
  exit.y_ = 2096;
  exit.room_id_ = 0x105;
  overworld_.mutable_exits()->push_back(exit);
  const auto native =
      Frame([&](const auto& rt) { renderer_.DrawExits(rt, 2); });
  const auto project = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  });
  ExpectSameVertices(project, native);
  EXPECT_EQ(renderer_.hovered_project_connection(),
            OverworldConnectionReference(ProjectReturnReference{202}));
}

TEST_F(OverworldProjectConnectionRendererTest,
       UnassignedEntranceAndReturnUseQuestionMarkWithOrdinaryStyle) {
  const auto expected = [&](zelda3::GameEntity::EntityType type) {
    return Frame([&](const auto& rt) {
      gui::DrawRect(rt, 32, 2096, 16, 16,
                    EntityMarkerColor(AgentUI::GetTheme(), type));
      gui::DrawText(rt, "?", 32, 2096);
    });
  };
  authoring_.maps[0].doors[0].entrance_id.reset();
  const auto entrance = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  });
  ExpectSameVertices(entrance, expected(zelda3::GameEntity::kEntrance));
  authoring_.maps[0].doors.clear();
  authoring_.maps[0].exits.push_back({202, 0x105, 2, 3, std::nullopt});
  const auto exit = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  });
  ExpectSameVertices(exit, expected(zelda3::GameEntity::kExit));
}

TEST_F(OverworldProjectConnectionRendererTest,
       HoverRequiresCanvasOwnershipAndResetsWhenMouseLeaves) {
  const auto draw = [&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  };
  Frame(draw);
  ASSERT_TRUE(renderer_.hovered_project_connection());
  Frame(draw, ImVec2(108, 108), false);
  EXPECT_FALSE(renderer_.hovered_project_connection());
  Frame(draw, ImVec2(200, 200));
  EXPECT_FALSE(renderer_.hovered_project_connection());
  Frame(draw);
  ASSERT_TRUE(renderer_.hovered_project_connection());
  renderer_.ResetHoveredEntity();
  EXPECT_FALSE(renderer_.hovered_project_connection());
  EXPECT_EQ(renderer_.hovered_entity(), nullptr);
}

TEST_F(OverworldProjectConnectionRendererTest,
       ProjectHitClearsNativeHoverAndEntrancesWinOverlapLikeNativeOrder) {
  NativeEntranceAt();
  authoring_.maps[0].exits.push_back({101, 0x105, 2, 3, 0});
  Frame([&](const auto& rt) {
    renderer_.DrawEntrances(rt, 2);
    ASSERT_NE(renderer_.hovered_entity(), nullptr);
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  });
  EXPECT_EQ(renderer_.hovered_entity(), nullptr);
  EXPECT_EQ(renderer_.hovered_project_connection(),
            OverworldConnectionReference(ProjectEntranceReference{101}));
  Frame([&](const auto& rt) { renderer_.DrawExits(rt, 2); });
  EXPECT_FALSE(renderer_.hovered_project_connection());
}

TEST_F(OverworldProjectConnectionRendererTest,
       LayerFlagsAndWorldFilterSuppressMarkersAndHover) {
  const auto hidden_entrances = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2, false, true);
  });
  EXPECT_TRUE(hidden_entrances.empty());
  EXPECT_FALSE(renderer_.hovered_project_connection());
  authoring_.maps[0].doors.clear();
  authoring_.maps[0].exits.push_back({202, 0x105, 2, 3, 0});
  const auto hidden_exits = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2, true, false);
  });
  EXPECT_TRUE(hidden_exits.empty());
  const auto wrong_world = Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 0);
  });
  EXPECT_TRUE(wrong_world.empty());
  EXPECT_FALSE(renderer_.hovered_project_connection());
}

TEST_F(OverworldProjectConnectionRendererTest,
       DragPreviewChangesOnlyMatchingMarkerLocationWithoutMutation) {
  const auto before = authoring_;
  auto view = ResolveOverworldConnection({{}, {}, {}, &authoring_},
                                         ProjectEntranceReference{101});
  ASSERT_TRUE(view.ok()) << view.status();
  view->world_x += 32;
  // A drag preview does not change the label from the current model.
  view->destination = DungeonEntranceDestination{0xFE};
  NativeEntranceAt(view->world_x, view->world_y);
  const auto native =
      Frame([&](const auto& rt) { renderer_.DrawEntrances(rt, 2); });
  const auto project = Frame(
      [&](const auto& rt) {
        renderer_.DrawProjectConnections(rt, authoring_, 2, true, true, &*view);
      },
      ImVec2(156, 108));
  ExpectSameVertices(project, native);
  EXPECT_EQ(renderer_.hovered_project_connection(),
            OverworldConnectionReference(ProjectEntranceReference{101}));
  EXPECT_EQ(authoring_, before);
  EXPECT_EQ(overworld_.entrances().size(), 1);
  EXPECT_TRUE(overworld_.exits()->empty());
}

TEST_F(OverworldProjectConnectionRendererTest,
       UnrelatedPreviewAndVectorReorderRetainStableHoverIdentity) {
  authoring_.maps[0].doors.push_back({999, 10, 11, 0x27});
  std::reverse(authoring_.maps[0].doors.begin(),
               authoring_.maps[0].doors.end());
  auto other = ResolveOverworldConnection({{}, {}, {}, &authoring_},
                                          ProjectEntranceReference{999});
  ASSERT_TRUE(other.ok()) << other.status();
  Frame([&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2, true, true, &*other);
  });
  EXPECT_EQ(renderer_.hovered_project_connection(),
            OverworldConnectionReference(ProjectEntranceReference{101}));
}

TEST_F(OverworldProjectConnectionRendererTest,
       InvalidRuntimeOrAmbiguousProjectDoesNotRetainHover) {
  const auto draw = [&](const auto& rt) {
    renderer_.DrawProjectConnections(rt, authoring_, 2);
  };
  Frame(draw);
  ASSERT_TRUE(renderer_.hovered_project_connection());
  for (const float scale :
       {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()}) {
    const auto invalid_scale = Frame(draw, ImVec2(108, 108), true, scale);
    EXPECT_TRUE(invalid_scale.empty());
    EXPECT_FALSE(renderer_.hovered_project_connection());
  }
  authoring_.maps[1].doors.push_back({101, 2, 3, 0x25});
  const auto ambiguous = Frame(draw);
  EXPECT_TRUE(ambiguous.empty());
  EXPECT_FALSE(renderer_.hovered_project_connection());
}

}  // namespace
}  // namespace yaze::editor
