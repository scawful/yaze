#include "app/editor/dungeon/ui/minecart/minecart_collision_tool.h"

#include <vector>

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/gui/canvas/canvas_types.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

void SetNode(zelda3::CustomCollisionMap& map, int x, int y, int size,
             uint8_t value) {
  for (int dy = 0; dy < size; ++dy) {
    for (int dx = 0; dx < size; ++dx)
      map.SetOverride((y + dy) * 64 + x + dx, value);
  }
}

TEST(MinecartCollisionMoveTest, MovesEachBehaviorWithoutLeavingSourceOverride) {
  for (const uint8_t value : {0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
                              0xD0, 0xD1, 0xD2, 0xD3}) {
    SCOPED_TRACE(static_cast<int>(value));
    zelda3::CustomCollisionMap before;
    SetNode(before, 10, 12, 2, value);
    SetNode(before, 22, 18, 2, 0xB1);
    before.SetOverride(0, 0);  // Unrelated explicit floor remains authored.
    const MinecartCollisionStamp stamp{22, 18, 2, value,
                                       MinecartCollisionSource{10, 12}};
    const auto after = PreviewMinecartCollisionStamp(before, stamp);
    ASSERT_TRUE(after.ok()) << after.status();
    for (int dy = 0; dy < 2; ++dy) {
      for (int dx = 0; dx < 2; ++dx) {
        EXPECT_FALSE(after->HasOverride((12 + dy) * 64 + 10 + dx));
        EXPECT_EQ(after->tiles[(12 + dy) * 64 + 10 + dx], 0);
        EXPECT_TRUE(before.HasOverride((12 + dy) * 64 + 10 + dx));
        EXPECT_EQ(after->tiles[(18 + dy) * 64 + 22 + dx], value);
      }
    }
    EXPECT_TRUE(after->HasOverride(0));
    EXPECT_EQ(after->tiles[0], 0);
  }
}

TEST(MinecartCollisionMoveTest, OverlappingMoveClearsOnlySourceRemainder) {
  zelda3::CustomCollisionMap before;
  SetNode(before, 10, 12, 2, 0xBB);
  const auto after = PreviewMinecartCollisionStamp(
      before, {11, 12, 2, 0xBB, MinecartCollisionSource{10, 12}});
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_FALSE(after->HasOverride(12 * 64 + 10));
  EXPECT_FALSE(after->HasOverride(13 * 64 + 10));
  for (int y = 12; y < 14; ++y) {
    for (int x = 11; x < 13; ++x) {
      EXPECT_TRUE(after->HasOverride(y * 64 + x));
      EXPECT_EQ(after->tiles[y * 64 + x], 0xBB);
    }
  }
}

TEST(MinecartCollisionMoveTest, RejectsMissingMismatchedAndOutOfBoundsSource) {
  zelda3::CustomCollisionMap before;
  SetNode(before, 10, 12, 2, 0xD0);
  auto stamp =
      MinecartCollisionStamp{20, 20, 2, 0xD0, MinecartCollisionSource{10, 12}};
  before.ClearOverride(13 * 64 + 11);
  EXPECT_FALSE(PreviewMinecartCollisionStamp(before, stamp).ok());
  before.SetOverride(13 * 64 + 11, 0xD1);
  EXPECT_FALSE(PreviewMinecartCollisionStamp(before, stamp).ok());
  for (const auto source :
       {MinecartCollisionSource{-1, 12}, MinecartCollisionSource{63, 12},
        MinecartCollisionSource{10, 64},
        MinecartCollisionSource{0x7FFFFFFF, 0}}) {
    stamp.source = source;
    EXPECT_FALSE(PreviewMinecartCollisionStamp(before, stamp).ok());
  }
  for (const uint8_t value : {0x00, 0xB0, 0xB1, 0xB6}) {
    SetNode(before, 10, 12, 2, value);
    stamp.value = value;
    stamp.source = MinecartCollisionSource{10, 12};
    EXPECT_FALSE(PreviewMinecartCollisionStamp(before, stamp).ok());
  }
}

TEST(MinecartCollisionMoveTest, DestinationRetainsExistingCollisionProtection) {
  zelda3::CustomCollisionMap before;
  SetNode(before, 10, 12, 2, 0xBB);
  const MinecartCollisionStamp stamp{20, 20, 2, 0xBB,
                                     MinecartCollisionSource{10, 12}};
  for (const uint8_t occupied : {0x00, 0x08, 0xB7, 0xD0}) {
    SetNode(before, 20, 20, 2, occupied);
    const auto unchanged = before;
    EXPECT_FALSE(PreviewMinecartCollisionStamp(before, stamp).ok());
    EXPECT_EQ(before, unchanged);
  }
  EXPECT_FALSE(PreviewMinecartCollisionStamp(
                   before, {63, 63, 2, 0xBB, MinecartCollisionSource{10, 12}})
                   .ok());
}

class MinecartCollisionToolTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1800, 1400);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    rt_.canvas_p0 = ImVec2(80, 60);
    rt_.scrolling = ImVec2(-24, 40);
    rt_.scale = 1.5f;
    rt_.canvas_sz = ImVec2(512, 512);
    Frame();
  }

  void TearDown() override { ImGui::DestroyContext(context_); }

  void Frame(bool draw = true) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1600, 1200));
    ImGui::Begin("Collision tool test", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    rt_.draw_list = ImGui::GetWindowDrawList();
    if (draw) {
      tool_.Draw(rt_, collision_, room_id_, value_, size_,
                 [this](int room, const zelda3::CustomCollisionMap& expected,
                        const MinecartCollisionStamp& stamp) {
                   rooms_.push_back(room);
                   snapshots_.push_back(expected);
                   stamps_.push_back(stamp);
                   if (!(collision_ == expected)) {
                     return absl::FailedPreconditionError(
                         "Collision changed; preview the stamp again");
                   }
                   const auto after =
                       PreviewMinecartCollisionStamp(expected, stamp);
                   if (!after.ok())
                     return after.status();
                   collision_ = *after;
                   return absl::OkStatus();
                 });
    }
    ImGui::End();
    ImGui::Render();
  }

  void MoveCell(int x, int y) {
    const DungeonCanvasTransform transform(rt_.canvas_p0, rt_.scrolling,
                                           rt_.scale);
    const auto point =
        transform.RoomPixelsToScreen(ImVec2(x * 8 + 4, y * 8 + 4));
    ImGui::GetIO().AddMousePosEvent(point.x, point.y);
    rt_.hovered = true;
    Frame();
  }

  void Button(bool down, ImGuiMouseButton button = ImGuiMouseButton_Left) {
    ImGui::GetIO().AddMouseButtonEvent(button, down);
    Frame();
  }

  ImGuiContext* context_ = nullptr;
  MinecartCollisionTool tool_;
  gui::CanvasRuntime rt_;
  zelda3::CustomCollisionMap collision_;
  int room_id_ = 0x98;
  uint8_t value_ = 0xD1;
  int size_ = 2;
  std::vector<int> rooms_;
  std::vector<zelda3::CustomCollisionMap> snapshots_;
  std::vector<MinecartCollisionStamp> stamps_;
};

TEST_F(MinecartCollisionToolTest, DragUsesTransformAndCommitsOnceOnRelease) {
  SetNode(collision_, 10, 12, 2, value_);
  const auto before = collision_;
  MoveCell(11, 13);  // Hit anywhere in the 16 pixel source footprint.
  Button(true);
  ASSERT_TRUE(tool_.dragging());
  EXPECT_TRUE(stamps_.empty());
  MoveCell(23, 19);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_EQ(collision_, before);
  Button(false);
  ASSERT_EQ(stamps_.size(), 1u);
  ASSERT_TRUE(stamps_[0].source.has_value());
  EXPECT_EQ(stamps_[0].source->x, 10);
  EXPECT_EQ(stamps_[0].source->y, 12);
  EXPECT_EQ(stamps_[0].x, 22);
  EXPECT_EQ(stamps_[0].y, 18);
  EXPECT_EQ(rooms_[0], 0x98);
  EXPECT_EQ(snapshots_[0], before);
  EXPECT_FALSE(collision_.HasOverride(12 * 64 + 10));
  EXPECT_EQ(collision_.tiles[18 * 64 + 22], value_);
  EXPECT_FALSE(tool_.dragging());
  Frame();
  EXPECT_EQ(stamps_.size(), 1u);
}

TEST_F(MinecartCollisionToolTest, OneCellTJunctionUsesEightPixelGrid) {
  size_ = 1;
  value_ = 0xBE;
  SetNode(collision_, 11, 13, size_, value_);
  MoveCell(11, 13);
  Button(true);
  MoveCell(23, 19);
  Button(false);
  ASSERT_EQ(stamps_.size(), 1u);
  ASSERT_TRUE(stamps_[0].source.has_value());
  EXPECT_EQ(stamps_[0].source->x, 11);
  EXPECT_EQ(stamps_[0].source->y, 13);
  EXPECT_EQ(stamps_[0].x, 23);
  EXPECT_EQ(stamps_[0].y, 19);
}

TEST_F(MinecartCollisionToolTest, EscapeRightAndMiddleCancelWithoutMutation) {
  SetNode(collision_, 10, 12, size_, value_);
  const auto before = collision_;
  for (int cancel = 0; cancel < 3; ++cancel) {
    SCOPED_TRACE(cancel);
    MoveCell(10, 12);
    Button(true);
    MoveCell(22, 18);
    ASSERT_TRUE(tool_.dragging());
    if (cancel == 0) {
      ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
      Frame();
      ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
      Frame();
    } else {
      const auto button =
          cancel == 1 ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle;
      Button(true, button);
      Button(false, button);
    }
    EXPECT_FALSE(tool_.dragging());
    Button(false);
    EXPECT_TRUE(stamps_.empty());
    EXPECT_EQ(collision_, before);
  }
}

TEST_F(MinecartCollisionToolTest, PaletteClickDoesNotMoveAndCanvasClickStamps) {
  SetNode(collision_, 10, 12, size_, value_);
  ImGui::GetIO().AddMousePosEvent(1500, 1100);
  rt_.hovered = false;
  Button(true);
  Button(false);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_FALSE(tool_.dragging());
  MoveCell(10, 12);
  Button(true);
  Button(false);  // Clicking an existing node without dragging is a no-op.
  EXPECT_TRUE(stamps_.empty());
  MoveCell(22, 18);
  Button(true);
  ASSERT_EQ(stamps_.size(), 1u);
  EXPECT_FALSE(stamps_[0].source.has_value());
  Button(false);
  EXPECT_EQ(stamps_.size(), 1u);
}

TEST_F(MinecartCollisionToolTest, InvalidDestinationAndCanvasExitDoNotCommit) {
  SetNode(collision_, 10, 12, size_, value_);
  SetNode(collision_, 22, 18, size_, 0);  // Authored floor is protected.
  const auto before = collision_;
  MoveCell(10, 12);
  Button(true);
  MoveCell(22, 18);
  Button(false);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_EQ(collision_, before);
  MoveCell(10, 12);
  Button(true);
  MoveCell(24, 20);
  rt_.hovered = false;
  Button(false);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_EQ(collision_, before);
  EXPECT_FALSE(tool_.dragging());
}

TEST_F(MinecartCollisionToolTest, StaleGesturePassesOriginalSnapshotToCas) {
  SetNode(collision_, 10, 12, size_, value_);
  const auto original = collision_;
  MoveCell(10, 12);
  Button(true);
  MoveCell(22, 18);
  collision_.SetOverride(0, 0x08);  // Another editor changes the working map.
  const auto changed = collision_;
  Frame();
  Button(false);
  ASSERT_EQ(stamps_.size(), 1u);
  EXPECT_EQ(snapshots_[0], original);
  EXPECT_EQ(collision_, changed);
  EXPECT_EQ(tool_.message(), "Collision changed; preview the stamp again");
}

TEST_F(MinecartCollisionToolTest, RoomModeAndSkippedFrameCancelGesture) {
  SetNode(collision_, 10, 12, size_, value_);
  const auto before = collision_;
  for (int cancel = 0; cancel < 3; ++cancel) {
    SCOPED_TRACE(cancel);
    MoveCell(10, 12);
    Button(true);
    MoveCell(22, 18);
    if (cancel == 0) {
      ++room_id_;
      Frame();
    } else if (cancel == 1) {
      value_ = 0xD2;
      Frame();
      value_ = 0xD1;
    } else {
      Frame(false);
    }
    Button(false);
    EXPECT_FALSE(tool_.dragging());
    EXPECT_TRUE(stamps_.empty());
    EXPECT_EQ(collision_, before);
  }
}

TEST_F(MinecartCollisionToolTest, ExplicitCancelEndsGestureOutsideDraw) {
  SetNode(collision_, 10, 12, size_, value_);
  const auto before = collision_;
  MoveCell(10, 12);
  Button(true);
  MoveCell(22, 18);
  ASSERT_TRUE(tool_.dragging());
  tool_.Cancel();  // The viewer uses this when it disables collision editing.
  EXPECT_FALSE(tool_.dragging());
  EXPECT_TRUE(tool_.message().empty());
  Button(false);
  EXPECT_TRUE(stamps_.empty());
  EXPECT_EQ(collision_, before);
}

}  // namespace
}  // namespace yaze::editor
