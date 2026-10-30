#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/ui/minecart/minecart_node_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/canvas/canvas_types.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {

class MinecartPaletteWorkflowTestPeer {
 public:
  static bool Draw(DungeonCanvasViewer& viewer, const gui::CanvasRuntime& rt,
                   zelda3::Room& room, int room_id) {
    viewer.room_canvas_shortcut_focus_ = rt.hovered;
    return viewer.DrawMinecartCanvas(rt, room, room_id);
  }
};

namespace {

class MinecartPaletteWorkflowTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1500, 1000);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ASSERT_TRUE(panel_.SetProject(&project_).ok());
    panel_.SetActiveRoom(kRoom);
    viewer_.SetMinecartTrackPanel(&panel_);
    other_viewer_.SetMinecartTrackPanel(&panel_);
    panel_.SetNodePlacementCallback([this](int room, int x, int y,
                                           const MinecartNodeBrush& brush) {
      nodes_.push_back({room, x, y, brush});
      auto& target = room == kRoom ? room_ : other_room_;
      const auto candidate = PreviewMinecartNodePlacement(target, x, y, brush);
      if (!candidate.ok())
        return candidate.status();
      target.GetSprites() = candidate->sprites;
      target.custom_collision() = candidate->collision;
      return absl::OkStatus();
    });
    panel_.SetCollisionStampCallback(
        [this](int room, const zelda3::CustomCollisionMap& expected,
               const MinecartCollisionStamp& stamp) {
          stamp_rooms_.push_back(room);
          stamps_.push_back(stamp);
          auto& target = room == kRoom ? room_ : other_room_;
          if (!(target.custom_collision() == expected))
            return absl::FailedPreconditionError("Stale collision preview");
          const auto after = PreviewMinecartCollisionStamp(expected, stamp);
          if (!after.ok())
            return after.status();
          target.custom_collision() = *after;
          return absl::OkStatus();
        });
    Frame();
    Frame();
  }

  void TearDown() override {
    viewer_.SetMinecartTrackPanel(nullptr);
    other_viewer_.SetMinecartTrackPanel(nullptr);
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
  }

  void Frame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    if (draw_palette_) {
      ImGui::SetNextWindowPos(ImVec2(1000, 40), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(350, 650), ImGuiCond_Always);
      ImGui::Begin("MinecartPalette", nullptr,
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize);
      {
        gui::AutoWidgetScope scope("MinecartPaletteTest");
        if (draw_full_tools_) {
          panel_.DrawRailTools();
        } else {
          panel_.DrawNodeTools();
        }
      }
      ImGui::End();
    }
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(850, 850), ImGuiCond_Always);
    ImGui::Begin("MinecartPaletteCanvas", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);
    ImGui::SetCursorScreenPos(ImVec2(80, 90));
    // Match the production canvas's InvisibleButton and default hover policy.
    // Palette dragging retains another ActiveId; a forced hovered=true would
    // hide a broken custom drop target integration.
    ImGui::InvisibleButton("RoomCanvas", ImVec2(800 * 1.25f, 750 * 1.25f));
    gui::CanvasRuntime rt;
    rt.draw_list = ImGui::GetWindowDrawList();
    rt.canvas_p0 = ImVec2(80, 90);
    rt.canvas_sz = ImVec2(800, 750);
    rt.scrolling = ImVec2(-24, -40);
    rt.scale = 1.25f;
    rt.hovered = ImGui::IsItemHovered();
    canvas_hovered_ = rt.hovered;
    auto& active_room = active_room_id_ == kRoom ? room_ : other_room_;
    owns_input_ = MinecartPaletteWorkflowTestPeer::Draw(
        viewer_, rt, active_room, active_room_id_);
    ImGui::End();
    if (draw_other_canvas_) {
      ImGui::SetNextWindowPos(ImVec2(1000, 720), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(350, 240), ImGuiCond_Always);
      ImGui::Begin("OtherEditableRoom", nullptr,
                   ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoFocusOnAppearing);
      ImGui::InvisibleButton("OtherCanvas", ImVec2(300, 180));
      rt.draw_list = ImGui::GetWindowDrawList();
      rt.canvas_p0 = ImGui::GetItemRectMin();
      rt.canvas_sz = ImVec2(300, 180);
      rt.scrolling = ImVec2(0, 0);
      rt.scale = 1;
      rt.hovered = ImGui::IsItemHovered();
      MinecartPaletteWorkflowTestPeer::Draw(other_viewer_, rt, other_room_,
                                            kRoom + 1);
      ImGui::End();
    }
    ImGui::Render();
  }

  std::optional<ImVec2> PalettePoint(const char* label) const {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.starts_with("MinecartPaletteTest/") &&
          widget.label == normalized && widget.visible && widget.enabled &&
          widget.bounds.valid) {
        return ImVec2((widget.bounds.min_x + widget.bounds.max_x) * 0.5f,
                      (widget.bounds.min_y + widget.bounds.max_y) * 0.5f);
      }
    }
    return std::nullopt;
  }

  void Move(ImVec2 point) {
    ImGui::GetIO().AddMousePosEvent(point.x, point.y);
    Frame();
  }
  void MoveCell(int x, int y) {
    // Independent numeric oracle: (80,90)+(-24,-40), then 1.25x room pixels.
    Move(ImVec2(56 + (x * 8 + 4) * 1.25f, 50 + (y * 8 + 4) * 1.25f));
  }
  void Button(bool down) {
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down);
    Frame();
  }
  void ClickPalette(const char* label) {
    const auto point = PalettePoint(label);
    ASSERT_TRUE(point.has_value()) << label;
    Move(*point);
    Button(true);
    Button(false);
  }
  void BeginPaletteDrag(const char* label) {
    const auto point = PalettePoint(label);
    ASSERT_TRUE(point.has_value()) << label;
    Move(*point);
    Button(true);
    Move(ImVec2(point->x + 20, point->y));
    ASSERT_NE(ImGui::GetDragDropPayload(), nullptr);
    ASSERT_TRUE(
        ImGui::GetDragDropPayload()->IsDataType(kMinecartNodeDragPayload));
  }
  void Escape() {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    Frame();
  }
  void SeedSwitchActor(bool rotating) {
    room_.GetSprites() = {
        zelda3::Sprite(rotating ? 0xB0 : 0xAF, 5, 6, rotating ? 23 : 17, 0)};
    room_.GetSprites()[0].set_key_drop(2);
    room_.custom_collision().SetOverride(100, 0x08);
    if (rotating)
      room_.custom_collision().SetOverride(778, 0xD3);
  }
  void BeginActorDrag(const char* palette_label) {
    ASSERT_NO_FATAL_FAILURE(ClickPalette(palette_label));
    MoveCell(11, 13);  // Source sprite anchor is tile (10,12).
    Button(true);
    ASSERT_TRUE(nodes_.empty());
    MoveCell(15, 13);
    ASSERT_NE(ImGui::GetDragDropPayload(), nullptr);
    ASSERT_TRUE(
        ImGui::GetDragDropPayload()->IsDataType(kMinecartNodeDragPayload));
    ASSERT_TRUE(nodes_.empty());
  }

  struct NodeCall {
    int room;
    int x;
    int y;
    MinecartNodeBrush brush;
  };
  static constexpr int kRoom = 0x98;
  int active_room_id_ = kRoom;
  ImGuiContext* context_ = nullptr;
  project::YazeProject project_;
  project::YazeProject rebound_project_;
  MinecartTrackEditorPanel panel_;
  DungeonCanvasViewer viewer_;
  DungeonCanvasViewer other_viewer_;
  zelda3::Room room_{kRoom, nullptr};
  zelda3::Room other_room_{kRoom + 1, nullptr};
  std::vector<NodeCall> nodes_;
  std::vector<int> stamp_rooms_;
  std::vector<MinecartCollisionStamp> stamps_;
  bool draw_palette_ = true;
  bool draw_full_tools_ = false;
  bool draw_other_canvas_ = false;
  bool owns_input_ = false;
  bool canvas_hovered_ = false;
};

TEST_F(MinecartPaletteWorkflowTest,
       PaletteClickArmsSwitchAndCanvasClickUsesScrolledScaledSpriteGrid) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->node.kind, MinecartNodeKind::kSwitch);
  EXPECT_TRUE(nodes_.empty());
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].room, kRoom);
  EXPECT_EQ(nodes_[0].x, 10);
  EXPECT_EQ(nodes_[0].y, 12);
  ASSERT_EQ(room_.GetSprites().size(), 1);
  EXPECT_EQ(room_.GetSprites()[0].id(), 0xAF);
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  EXPECT_EQ(room_.GetSprites()[0].y(), 6);
  EXPECT_FALSE(room_.custom_collision().HasOverrides());
}

TEST_F(MinecartPaletteWorkflowTest,
       ProductionPaletteDragCallsPlacementOnlyOnDeliveryAtCorrectCoordinates) {
  ASSERT_NO_FATAL_FAILURE(BeginPaletteDrag("Switch"));
  EXPECT_TRUE(nodes_.empty());
  MoveCell(11, 13);
  Frame();  // A hover/preview frame must never invoke the mutation callback.
  EXPECT_TRUE(nodes_.empty());
  EXPECT_TRUE(room_.GetSprites().empty());
  EXPECT_FALSE(canvas_hovered_);  // Source ActiveId blocks ordinary item hover.
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].room, kRoom);
  EXPECT_EQ(nodes_[0].x, 10);
  EXPECT_EQ(nodes_[0].y, 12);
  EXPECT_EQ(nodes_[0].brush.kind, MinecartNodeKind::kSwitch);
  Frame();
  EXPECT_EQ(nodes_.size(), 1);
}

TEST_F(MinecartPaletteWorkflowTest,
       EndStopRoutesOneCellWithoutSpritePlacementAndEscapeDisarms) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("End stop"));
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_TRUE(panel_.placement_brush()->collision_only);
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(stamps_.size(), 1);
  EXPECT_EQ(stamp_rooms_[0], kRoom);
  EXPECT_EQ(stamps_[0].x, 11);
  EXPECT_EQ(stamps_[0].y, 13);
  EXPECT_EQ(stamps_[0].size, 1);
  EXPECT_EQ(stamps_[0].value, 0xB7);
  EXPECT_FALSE(stamps_[0].source);
  EXPECT_EQ(room_.custom_collision().overrides.count(), 1);
  EXPECT_EQ(room_.custom_collision().tiles[843], 0xB7);
  EXPECT_TRUE(nodes_.empty());
  Escape();
  EXPECT_FALSE(panel_.placement_brush());
  MoveCell(15, 15);
  Button(true);
  Button(false);
  EXPECT_EQ(stamps_.size(), 1);
}

TEST_F(MinecartPaletteWorkflowTest,
       ReadOnlyAndDisabledInteractionRefuseBothPlacementRoutes) {
  for (const bool header_read_only : {false, true}) {
    SCOPED_TRACE(header_read_only);
    viewer_.SetObjectInteractionEnabled(true);
    viewer_.SetHeaderReadOnly(false);
    ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
    viewer_.SetObjectInteractionEnabled(header_read_only);
    viewer_.SetHeaderReadOnly(header_read_only);
    MoveCell(10, 12);
    Button(true);
    Button(false);
    EXPECT_TRUE(nodes_.empty());
    EXPECT_FALSE(owns_input_);
    viewer_.SetObjectInteractionEnabled(true);
    viewer_.SetHeaderReadOnly(false);
    ASSERT_NO_FATAL_FAILURE(ClickPalette("End stop"));
    viewer_.SetObjectInteractionEnabled(header_read_only);
    viewer_.SetHeaderReadOnly(header_read_only);
    MoveCell(11, 13);
    Button(true);
    Button(false);
    EXPECT_TRUE(stamps_.empty());
    EXPECT_FALSE(room_.custom_collision().HasOverrides());
  }
}

TEST_F(MinecartPaletteWorkflowTest,
       ChoosingAnotherPlacementModeCancelsTheNodeBrush) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  MoveCell(10, 12);
  ASSERT_TRUE(owns_input_);
  viewer_.object_interaction().SetSpritePlacementMode(true, 0x01);
  Frame();
  EXPECT_FALSE(panel_.placement_brush());
  EXPECT_FALSE(owns_input_);
  EXPECT_TRUE(viewer_.object_interaction().IsSpritePlacementActive());
  EXPECT_TRUE(nodes_.empty());
}

TEST_F(MinecartPaletteWorkflowTest,
       DrawingAnotherEditableRoomDoesNotDisarmTheActiveRoomBrush) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  ASSERT_TRUE(panel_.placement_brush());
  const auto brush = *panel_.placement_brush();
  draw_other_canvas_ = true;
  Frame();
  Frame();
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->owner, brush.owner);
  EXPECT_EQ(panel_.placement_brush()->revision, brush.revision);
  EXPECT_EQ(panel_.placement_brush()->room_id, kRoom);
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].room, kRoom);
  EXPECT_EQ(nodes_[0].x, 10);
  EXPECT_EQ(nodes_[0].y, 12);
  EXPECT_TRUE(other_room_.GetSprites().empty());
}

TEST_F(MinecartPaletteWorkflowTest,
       RoomNavigationRejectsOldDragAndCancelDisarmsReboundBrush) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  ASSERT_TRUE(panel_.placement_brush());
  const auto old_brush = *panel_.placement_brush();
  ASSERT_NO_FATAL_FAILURE(BeginPaletteDrag("Switch"));
  panel_.SetActiveRoom(kRoom + 1);
  active_room_id_ = kRoom + 1;
  draw_palette_ =
      false;  // Preserve the old payload while its source disappears.
  MoveCell(11, 13);
  Frame();
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  EXPECT_TRUE(stamps_.empty());
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->node.kind, old_brush.node.kind);
  EXPECT_EQ(panel_.placement_brush()->node.id, old_brush.node.id);
  EXPECT_EQ(panel_.placement_brush()->room_id, kRoom + 1);
  EXPECT_NE(panel_.placement_brush()->revision, old_brush.revision);

  // A fresh target-room click uses the rebound brush once, without reselecting.
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].room, kRoom + 1);
  EXPECT_EQ(nodes_[0].x, 10);
  EXPECT_EQ(nodes_[0].y, 12);
  EXPECT_TRUE(room_.GetSprites().empty());
  EXPECT_EQ(other_room_.GetSprites().size(), 1);

  draw_palette_ = true;
  draw_full_tools_ = true;
  Frame();
  Frame();
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Cancel node"));
  EXPECT_FALSE(panel_.placement_brush());
  MoveCell(15, 15);
  Button(true);
  Button(false);
  EXPECT_EQ(nodes_.size(), 1);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_TRUE(room_.GetSprites().empty());
  EXPECT_EQ(other_room_.GetSprites().size(), 1);
  EXPECT_FALSE(other_room_.custom_collision().HasOverrides());
}

TEST_F(MinecartPaletteWorkflowTest,
       ReboundPanelRejectsRetainedDragPayloadBeforeDelivery) {
  ASSERT_NO_FATAL_FAILURE(BeginPaletteDrag("Switch"));
  const uint64_t old_revision = panel_.canvas_revision();
  ASSERT_TRUE(panel_.SetProject(&rebound_project_).ok());
  ASSERT_NE(panel_.canvas_revision(), old_revision);
  panel_.SetActiveRoom(kRoom);
  // The old source window may disappear when the project changes. Keep its
  // still-active ImGui payload and deliver it to the newly bound canvas.
  draw_palette_ = false;
  MoveCell(10, 12);
  Frame();
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  EXPECT_TRUE(stamps_.empty());
  EXPECT_TRUE(room_.GetSprites().empty());
}

TEST_F(MinecartPaletteWorkflowTest,
       RedrawnPaletteDoesNotRefreshAnOldDragIntoTheNewProject) {
  ASSERT_NO_FATAL_FAILURE(BeginPaletteDrag("Switch"));
  const uint64_t old_revision = panel_.canvas_revision();
  ASSERT_TRUE(panel_.SetProject(&rebound_project_).ok());
  ASSERT_NE(panel_.canvas_revision(), old_revision);
  panel_.SetActiveRoom(kRoom);
  // Keep drawing the same source widget. ImGuiCond_Once must preserve the
  // original payload token instead of renewing the old gesture's authority.
  MoveCell(10, 12);
  Frame();
  EXPECT_TRUE(nodes_.empty());
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  EXPECT_TRUE(room_.GetSprites().empty());
}

TEST_F(MinecartPaletteWorkflowTest,
       ExistingSwitchAndSwitchTrackClicksWaitForDragWithoutPlacing) {
  for (const bool rotating : {false, true}) {
    SCOPED_TRACE(rotating);
    SeedSwitchActor(rotating);
    const auto original = room_.custom_collision();
    ASSERT_NO_FATAL_FAILURE(ClickPalette(rotating ? "Switch track" : "Switch"));
    MoveCell(11, 13);
    Button(true);
    Button(false);
    EXPECT_TRUE(nodes_.empty());
    ASSERT_EQ(room_.GetSprites().size(), 1);
    EXPECT_EQ(room_.GetSprites()[0].x(), 5);
    EXPECT_EQ(room_.GetSprites()[0].y(), 6);
    EXPECT_EQ(room_.custom_collision(), original);
  }
}

TEST_F(
    MinecartPaletteWorkflowTest,
    ExistingSwitchDragUsesSourceChannelKeyAndCapturedCollisionUntilDelivery) {
  SeedSwitchActor(false);
  const auto original = room_.custom_collision();
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch"));
  MoveCell(21, 23);
  Frame();
  EXPECT_TRUE(nodes_.empty());
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].x, 20);
  EXPECT_EQ(nodes_[0].y, 22);
  EXPECT_EQ(nodes_[0].brush.kind, MinecartNodeKind::kSwitch);
  EXPECT_EQ(nodes_[0].brush.id, 17);  // Palette ID defaults to zero.
  EXPECT_EQ(nodes_[0].brush.source_x, 10);
  EXPECT_EQ(nodes_[0].brush.source_y, 12);
  EXPECT_EQ(nodes_[0].brush.source_key_drop, 2);
  ASSERT_TRUE(nodes_[0].brush.expected_collision);
  EXPECT_EQ(*nodes_[0].brush.expected_collision, original);
  ASSERT_EQ(room_.GetSprites().size(), 1);
  EXPECT_EQ(room_.GetSprites()[0].id(), 0xAF);
  EXPECT_EQ(room_.GetSprites()[0].subtype(), 17);
  EXPECT_EQ(room_.GetSprites()[0].key_drop(), 2);
  EXPECT_EQ(room_.GetSprites()[0].x(), 10);
  EXPECT_EQ(room_.GetSprites()[0].y(), 11);
  EXPECT_EQ(room_.custom_collision(), original);
}

TEST_F(MinecartPaletteWorkflowTest,
       ExistingSwitchTrackDragMovesOneOverrideWithActualChannelAndOrientation) {
  SeedSwitchActor(true);
  const auto original = room_.custom_collision();
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch track"));
  MoveCell(21, 23);
  Frame();
  EXPECT_TRUE(nodes_.empty());
  EXPECT_EQ(room_.custom_collision(), original);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1);
  EXPECT_EQ(nodes_[0].brush.kind, MinecartNodeKind::kSwitchTrack);
  EXPECT_EQ(nodes_[0].brush.id, 23);
  EXPECT_EQ(nodes_[0].brush.collision, 0xD3);  // Palette orientation is D0.
  EXPECT_EQ(nodes_[0].brush.source_x, 10);
  EXPECT_EQ(nodes_[0].brush.source_y, 12);
  ASSERT_TRUE(nodes_[0].brush.expected_collision);
  EXPECT_EQ(*nodes_[0].brush.expected_collision, original);
  ASSERT_EQ(room_.GetSprites().size(), 1);
  EXPECT_EQ(room_.GetSprites()[0].id(), 0xB0);
  EXPECT_EQ(room_.GetSprites()[0].subtype(), 23);
  EXPECT_EQ(room_.GetSprites()[0].key_drop(), 2);
  EXPECT_EQ(room_.GetSprites()[0].x(), 10);
  EXPECT_EQ(room_.GetSprites()[0].y(), 11);
  EXPECT_FALSE(room_.custom_collision().HasOverride(778));
  EXPECT_EQ(room_.custom_collision().tiles[1428], 0xD3);
  EXPECT_EQ(room_.custom_collision().tiles[100], 0x08);
  EXPECT_EQ(room_.custom_collision().overrides.count(), 2);
}

TEST_F(MinecartPaletteWorkflowTest,
       ChangedCollisionDuringExistingActorDragRejectsDelivery) {
  SeedSwitchActor(true);
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch track"));
  room_.custom_collision().SetOverride(100, 0x20);
  const auto changed = room_.custom_collision();
  MoveCell(21, 23);
  Frame();
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  ASSERT_EQ(room_.GetSprites().size(), 1);
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  EXPECT_EQ(room_.GetSprites()[0].y(), 6);
  EXPECT_EQ(room_.custom_collision(), changed);
}

TEST_F(MinecartPaletteWorkflowTest,
       ProjectRebindDuringExistingActorDragRejectsDelivery) {
  SeedSwitchActor(false);
  const auto original = room_.custom_collision();
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch"));
  ASSERT_TRUE(panel_.SetProject(&rebound_project_).ok());
  panel_.SetActiveRoom(kRoom);
  MoveCell(21, 23);
  Frame();
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  ASSERT_EQ(room_.GetSprites().size(), 1);
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  EXPECT_EQ(room_.GetSprites()[0].y(), 6);
  EXPECT_EQ(room_.custom_collision(), original);
}

TEST_F(MinecartPaletteWorkflowTest,
       ActorDragEscapeRightAndMiddleInvalidateDeliveryExactlyOnce) {
  for (int cancel = 0; cancel < 3; ++cancel) {
    SCOPED_TRACE(cancel);
    SeedSwitchActor(true);
    ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch track"));
    const auto before = room_.custom_collision();
    const auto revision = panel_.canvas_revision();
    const auto expect_canceled = [&] {
      // NewFrame can consume Escape and discard the payload before the panel
      // sees it. A retained payload must instead carry an invalidated token.
      if (const auto* payload = ImGui::GetDragDropPayload()) {
        ASSERT_TRUE(payload->IsDataType(kMinecartNodeDragPayload));
        ASSERT_EQ(payload->DataSize, sizeof(MinecartToolPayload));
        const auto* token =
            static_cast<const MinecartToolPayload*>(payload->Data);
        EXPECT_EQ(token->owner, &panel_);
        EXPECT_NE(token->revision, panel_.canvas_revision());
        EXPECT_FALSE(payload->IsDelivery());
      }
      EXPECT_TRUE(nodes_.empty());
      EXPECT_TRUE(stamps_.empty());
      ASSERT_EQ(room_.GetSprites().size(), 1u);
      const auto& source = room_.GetSprites()[0];
      EXPECT_EQ(source.id(), 0xB0);
      EXPECT_EQ(source.x(), 5);
      EXPECT_EQ(source.y(), 6);
      EXPECT_EQ(source.subtype(), 23);
      EXPECT_EQ(source.key_drop(), 2);
      EXPECT_EQ(source.layer(), 0);
      EXPECT_FALSE(source.deleted());
      EXPECT_EQ(room_.custom_collision(), before);
    };
    MoveCell(21, 23);
    if (cancel == 0) {
      ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
      Frame();
    } else {
      ImGui::GetIO().AddMouseButtonEvent(
          cancel == 1 ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle, true);
      Frame();
    }
    if (cancel != 0)
      EXPECT_EQ(panel_.canvas_revision(), revision + 1);
    ASSERT_NO_FATAL_FAILURE(expect_canceled());
    Frame();
    if (cancel != 0)
      EXPECT_EQ(panel_.canvas_revision(), revision + 1);
    ASSERT_NO_FATAL_FAILURE(expect_canceled());
    if (cancel == 0) {
      ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
    } else {
      ImGui::GetIO().AddMouseButtonEvent(
          cancel == 1 ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle,
          false);
    }
    Frame();
    Button(false);
    ASSERT_NO_FATAL_FAILURE(expect_canceled());
    Frame();
    Frame();
    ASSERT_NO_FATAL_FAILURE(expect_canceled());
  }
}

TEST_F(MinecartPaletteWorkflowTest,
       ActorDragModeExitAndReadOnlyTransitionsRejectLaterRelease) {
  for (int exit = 0; exit < 3; ++exit) {
    SCOPED_TRACE(exit);
    viewer_.SetObjectInteractionEnabled(true);
    viewer_.SetHeaderReadOnly(false);
    viewer_.object_interaction().CancelPlacement();
    viewer_.object_interaction().mode_manager().CancelCurrentMode();
    SeedSwitchActor(false);
    ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch"));
    MoveCell(21, 23);
    const auto revision = panel_.canvas_revision();
    if (exit == 0)
      viewer_.SetObjectInteractionEnabled(false);
    else if (exit == 1)
      viewer_.SetHeaderReadOnly(true);
    else
      viewer_.object_interaction().SetSpritePlacementMode(true, 0x01);
    Frame();
    EXPECT_EQ(panel_.canvas_revision(), revision + 1);
    // Re-enabling the view does not re-authorize its old payload.
    viewer_.SetObjectInteractionEnabled(true);
    viewer_.SetHeaderReadOnly(false);
    Button(false);
    EXPECT_TRUE(nodes_.empty());
    ASSERT_EQ(room_.GetSprites().size(), 1u);
    EXPECT_EQ(room_.GetSprites()[0].x(), 5);
    EXPECT_EQ(room_.GetSprites()[0].y(), 6);
  }
}

TEST_F(MinecartPaletteWorkflowTest,
       RefreshCancelsActorPayloadWithoutChangingUnrelatedViewerAuthority) {
  SeedSwitchActor(false);
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch"));
  const auto revision = panel_.canvas_revision();
  other_viewer_.SetHeaderReadOnly(true);
  EXPECT_EQ(panel_.canvas_revision(), revision);
  viewer_.RefreshRomBackedState(nullptr, nullptr, nullptr, kRoom);
  EXPECT_EQ(panel_.canvas_revision(), revision + 1);
  MoveCell(21, 23);
  Frame();
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  EXPECT_EQ(room_.GetSprites()[0].y(), 6);
}

TEST_F(MinecartPaletteWorkflowTest, CleanupWithoutActivePayloadKeepsRevision) {
  const auto revision = panel_.canvas_revision();
  panel_.CancelNodeDrag();
  panel_.CancelNodePlacement();
  viewer_.CancelMinecartNodeDrag();
  EXPECT_EQ(panel_.canvas_revision(), revision);
}

TEST_F(MinecartPaletteWorkflowTest,
       RoomNavigationKeepsToolButRejectsThePreviousActorGesture) {
  SeedSwitchActor(false);
  ASSERT_NO_FATAL_FAILURE(BeginActorDrag("Switch"));
  const auto old_revision = panel_.canvas_revision();
  active_room_id_ = kRoom + 1;
  panel_.SetActiveRoom(active_room_id_);
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_NE(panel_.placement_brush()->revision, old_revision);
  EXPECT_EQ(panel_.placement_brush()->room_id, active_room_id_);
  EXPECT_EQ(panel_.placement_brush()->node.kind, MinecartNodeKind::kSwitch);
  EXPECT_EQ(panel_.placement_brush()->node.source_x, -1);
  EXPECT_EQ(panel_.placement_brush()->node.source_y, -1);
  EXPECT_FALSE(panel_.placement_brush()->node.expected_collision);
  MoveCell(21, 23);
  Button(false);
  EXPECT_TRUE(nodes_.empty());
  EXPECT_TRUE(other_room_.GetSprites().empty());

  Frame();
  MoveCell(25, 27);
  Button(true);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1u);
  EXPECT_EQ(nodes_[0].room, active_room_id_);
  EXPECT_EQ(nodes_[0].x, 24);
  EXPECT_EQ(nodes_[0].y, 26);
  EXPECT_EQ(nodes_[0].brush.source_x, -1);
  EXPECT_FALSE(nodes_[0].brush.expected_collision);
  ASSERT_EQ(room_.GetSprites().size(), 1u);
  EXPECT_EQ(room_.GetSprites()[0].x(), 5);
  EXPECT_EQ(room_.GetSprites()[0].y(), 6);
  ASSERT_EQ(other_room_.GetSprites().size(), 1u);
  EXPECT_EQ(other_room_.GetSprites()[0].id(), 0xAF);
}

TEST_F(MinecartPaletteWorkflowTest,
       EndStopToolContinuesAcrossRoomsWithoutMovingPreviousBehavior) {
  ASSERT_NO_FATAL_FAILURE(ClickPalette("End stop"));
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(stamps_.size(), 1u);
  const auto first_room_collision = room_.custom_collision();
  active_room_id_ = kRoom + 1;
  panel_.SetActiveRoom(active_room_id_);
  MoveCell(21, 23);
  Button(true);
  Button(false);
  ASSERT_EQ(stamps_.size(), 2u);
  EXPECT_EQ(stamp_rooms_[0], kRoom);
  EXPECT_EQ(stamp_rooms_[1], active_room_id_);
  EXPECT_EQ(room_.custom_collision(), first_room_collision);
  EXPECT_EQ(other_room_.custom_collision().tiles[23 * 64 + 21], 0xB7);
  EXPECT_EQ(other_room_.custom_collision().overrides.count(), 1u);
}

TEST_F(MinecartPaletteWorkflowTest,
       AvailableIdButtonsUseProjectValidationAndRefreshTheActiveTool) {
  std::vector<std::pair<MinecartNodeKind, int>> requests;
  panel_.SetAvailableNodeIdCallback(
      [&](MinecartNodeKind kind, int room) -> absl::StatusOr<int> {
        requests.emplace_back(kind, room);
        return kind == MinecartNodeKind::kCartStart ? 27 : 9;
      });
  Frame();
  EXPECT_TRUE(requests.empty());
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Find unused switch channel"));
  ASSERT_EQ(requests.size(), 1u);
  EXPECT_EQ(requests[0], (std::pair{MinecartNodeKind::kSwitch, kRoom}));
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->node.id, 9);
  MoveCell(11, 13);
  Button(true);
  Button(false);
  ASSERT_EQ(nodes_.size(), 1u);
  EXPECT_EQ(nodes_[0].brush.id, 9);

  ASSERT_NO_FATAL_FAILURE(ClickPalette("Find unused cart ID"));
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(requests[1], (std::pair{MinecartNodeKind::kCartStart, kRoom}));
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Cart spawn"));
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->node.id, 27);
  EXPECT_EQ(nodes_.size(), 1u);  // Choosing an identity does not place a cart.
}

TEST_F(MinecartPaletteWorkflowTest,
       FailedAvailableIdLookupKeepsTheSelectedChannelAndRoomData) {
  panel_.SetAvailableNodeIdCallback(
      [](MinecartNodeKind, int) -> absl::StatusOr<int> {
        return absl::FailedPreconditionError("All switch channels are in use");
      });
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Switch"));
  ASSERT_TRUE(panel_.placement_brush());
  const int channel = panel_.placement_brush()->node.id;
  ASSERT_NO_FATAL_FAILURE(ClickPalette("Find unused switch channel"));
  ASSERT_TRUE(panel_.placement_brush());
  EXPECT_EQ(panel_.placement_brush()->node.id, channel);
  EXPECT_TRUE(room_.GetSprites().empty());
  EXPECT_FALSE(room_.custom_collision().HasOverrides());
  EXPECT_TRUE(nodes_.empty());
}

}  // namespace
}  // namespace yaze::editor
