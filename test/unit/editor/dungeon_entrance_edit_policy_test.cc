#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"

#include <array>
#include <optional>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

TEST(DungeonEntranceEditPolicyTest, SpawnPropertyChangeCannotMarkRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance spawn(&rom, 0, true);

  EXPECT_FALSE(CanEditDungeonEntrance(0, spawn));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     SpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::RoomEntrance default_model;

  EXPECT_FALSE(CanEditDungeonEntrance(0, default_model));
  EXPECT_FALSE(MarkDungeonEntranceDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, RegularPropertyChangeMarksRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance entrance(&rom, 0, false);

  constexpr int kRegularSlot = zelda3::kNumDungeonSpawnPoints;
  EXPECT_TRUE(CanEditDungeonEntrance(kRegularSlot, entrance));
  EXPECT_FALSE(
      MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, false));
  EXPECT_FALSE(entrance.dirty());
  EXPECT_TRUE(MarkDungeonEntranceDirtyIfEditable(kRegularSlot, entrance, true));
  EXPECT_TRUE(entrance.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     LoadedSpawnPropertyChangeMarksMatchingRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  for (int slot = 0; slot < zelda3::kNumDungeonSpawnPoints; ++slot) {
    auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, slot);
    ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
    auto spawn = *spawn_or;

    EXPECT_TRUE(CanEditDungeonSpawnPoint(slot, spawn));
    EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, false));
    EXPECT_FALSE(spawn.dirty());
    EXPECT_TRUE(MarkDungeonSpawnPointDirtyIfEditable(slot, spawn, true));
    EXPECT_TRUE(spawn.dirty());
  }
}

TEST(DungeonEntranceEditPolicyTest,
     DedicatedSpawnSlotCannotMarkDefaultModelDirtyBeforeLoad) {
  zelda3::DungeonSpawnPoint default_model;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, default_model));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, default_model, true));
  EXPECT_FALSE(default_model.dirty());
}

TEST(DungeonEntranceEditPolicyTest, SpawnSlotCannotMarkMismatchedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 1);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;

  EXPECT_FALSE(CanEditDungeonSpawnPoint(0, spawn));
  EXPECT_FALSE(MarkDungeonSpawnPointDirtyIfEditable(0, spawn, true));
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceEditPolicyTest,
     InvalidSpawnSlotsCannotMarkLoadedRecordDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, 0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();

  for (const int invalid_slot : {-1, zelda3::kNumDungeonSpawnPoints}) {
    auto spawn = *spawn_or;
    EXPECT_FALSE(CanEditDungeonSpawnPoint(invalid_slot, spawn));
    EXPECT_FALSE(
        MarkDungeonSpawnPointDirtyIfEditable(invalid_slot, spawn, true));
    EXPECT_FALSE(spawn.dirty());
  }
}

TEST(DungeonEntranceEditPolicyTest,
     SharedEntranceNavigatorRendersNarrowAndSplitLayouts) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int atlas_width = 0;
  int atlas_height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width,
                                           &atlas_height);

  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots> entrances;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints>
      spawn_points;
  int selected = 0;
  DungeonEntrancesPanel panel(&entrances, &spawn_points, &selected, nullptr);
  const auto derived = DeriveDungeonEntranceCamera(
      /*room_id=*/0, /*player_x=*/0x78, /*player_y=*/0x78);
  const DungeonEntranceCameraState camera_state = {
      .kind = DungeonEntranceRecordKind::kSpawnPoint,
      .room_id = 0,
      .player_x = 0x78,
      .player_y = 0x78,
      .camera_x = derived.camera_x,
      .camera_y = derived.camera_y,
      .trigger_x = derived.trigger_x,
      .trigger_y = derived.trigger_y,
      .boundaries = derived.boundaries,
      .quadrant = derived.quadrant,
  };
  panel.SetCameraTools(
      [camera_state](int) { return std::optional(camera_state); },
      [](int) { return absl::OkStatus(); });

  for (float scale : {1.0f, 1.5f}) {
    for (float width : {280.0f, 800.0f}) {
      SCOPED_TRACE(absl::StrFormat("width %.0f scale %.1f", width, scale));
      ImGui::GetIO().FontGlobalScale = scale;
      ImGui::GetIO().DisplaySize = ImVec2(width, 720.0f);
      ImGui::NewFrame();
      ImGui::SetNextWindowSize(ImVec2(width, 680.0f));
      ImGui::SetNextWindowFocus();
      ImGui::Begin("EntranceNavigatorTest");
      EXPECT_NO_FATAL_FAILURE(panel.Draw(nullptr));
      EXPECT_TRUE(panel.OwnsNavigationShortcutFocus());
      ImGui::End();
      EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
      EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
      ImGui::EndFrame();
      ImGui::Render();
    }
  }
  ImGui::DestroyContext(context);
}

}  // namespace
}  // namespace yaze::editor
