#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_entrance_camera.h"
#include "app/editor/dungeon/dungeon_render_context.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

class DungeonEditorV2EntranceCameraTestPeer {
 public:
  static void SetRegularEntrance(DungeonEditorV2& editor, int slot_index,
                                 const DungeonEntranceCameraState& state) {
    auto& entrance = editor.entrances_[slot_index];
    entrance.room_ = static_cast<int16_t>(state.room_id);
    entrance.x_position_ = state.player_x;
    entrance.y_position_ = state.player_y;
    ApplyDungeonEntranceCameraState(state, &entrance);
  }

  static DungeonEntranceCameraState RegularEntrance(
      const DungeonEditorV2& editor, int slot_index) {
    return CaptureDungeonEntranceCamera(editor.entrances_[slot_index]);
  }

  static void SetSpawnPoint(DungeonEditorV2& editor, int slot_index,
                            zelda3::DungeonSpawnPoint spawn) {
    editor.spawn_points_[slot_index] = std::move(spawn);
  }

  static DungeonEntranceCameraState SpawnPoint(const DungeonEditorV2& editor,
                                               int slot_index) {
    return CaptureDungeonSpawnCamera(editor.spawn_points_[slot_index]);
  }

  static bool LegacyEntranceDirty(const DungeonEditorV2& editor,
                                  int slot_index) {
    return editor.entrances_[slot_index].dirty();
  }

  static void SetRenderContextCensus(DungeonEditorV2& editor,
                                     zelda3::RoomCensus census) {
    editor.dungeon_render_context_census_ = std::move(census);
    editor.dungeon_render_context_census_attempted_ = true;
  }

  static void SetSelectedEntranceGraphics(DungeonEditorV2& editor,
                                          int slot_index, int room_id,
                                          uint8_t main_gfx) {
    editor.current_entrance_id_ = slot_index;
    editor.entrances_[slot_index].room_ = static_cast<int16_t>(room_id);
    editor.entrances_[slot_index].blockset_ = main_gfx;
  }

  static void ConfigureViewer(DungeonEditorV2& editor,
                              DungeonCanvasViewer& viewer, int room_id) {
    editor.ConfigureViewerRenderContext(&viewer, room_id);
  }

  static void MaterializeCurrentRoom(DungeonEditorV2& editor, int room_id,
                                     uint8_t room_blockset) {
    editor.current_room_id_ = room_id;
    editor.rooms_[room_id].SetBlockset(room_blockset);
  }

  static absl::Status Repair(DungeonEditorV2& editor, int slot_index) {
    return editor.RepairEntranceCamera(slot_index);
  }
};

namespace {

zelda3::RoomCensus MakeCensus(
    const std::vector<std::vector<int>>& owner_rooms) {
  zelda3::RoomCensus census;
  census.rooms.resize(zelda3::kRoomCensusRoomCount);
  for (int room_id = 0; room_id < zelda3::kRoomCensusRoomCount; ++room_id) {
    census.rooms[room_id].room_id = room_id;
  }
  for (size_t owner_index = 0; owner_index < owner_rooms.size();
       ++owner_index) {
    census.owners.push_back({
        .id = "D" + std::to_string(owner_index + 1),
        .name = "Dungeon " + std::to_string(owner_index + 1),
    });
    for (const int room_id : owner_rooms[owner_index]) {
      census.rooms[room_id].owner_index = static_cast<int>(owner_index);
    }
  }
  return census;
}

DungeonEntranceCameraState StateFromDerived(int room_id, uint16_t player_x,
                                            uint16_t player_y) {
  const auto derived = DeriveDungeonEntranceCamera(room_id, player_x, player_y);
  return {
      .room_id = room_id,
      .player_x = player_x,
      .player_y = player_y,
      .camera_x = derived.camera_x,
      .camera_y = derived.camera_y,
      .trigger_x = derived.trigger_x,
      .trigger_y = derived.trigger_y,
      .boundaries = derived.boundaries,
      .quadrant = derived.quadrant,
  };
}

TEST(DungeonRenderContextTest, SelectedEntranceOwnsSiblingRooms) {
  const auto census = MakeCensus({{0x10, 0x11}, {0x20}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 7, .room_id = 0x10, .main_gfx = 0x22},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x20, .main_gfx = 0x33},
  };

  const auto context = ResolveDungeonRenderContext(
      0x11, /*selected_entrance_slot=*/7, /*room_header_blockset=*/0x04,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kSelectedEntrance);
  EXPECT_EQ(context.entrance_slot, 7);
  EXPECT_EQ(context.entrance_blockset, 0x22);
  EXPECT_EQ(context.owner_id, "D1");
}

TEST(DungeonRenderContextTest, DirectTargetWorksWithoutCensusOwnership) {
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x44, .main_gfx = 0x12},
  };

  const auto context = ResolveDungeonRenderContext(
      0x44, /*selected_entrance_slot=*/9, /*room_header_blockset=*/0x08,
      nullptr, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kSelectedEntrance);
  EXPECT_EQ(context.effective_blockset(), 0x12);
}

TEST(DungeonRenderContextTest, MissingSelectionDoesNotInferWithoutOwner) {
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x44, .main_gfx = 0x12},
  };

  const auto context = ResolveDungeonRenderContext(
      0x44, /*selected_entrance_slot=*/0, /*room_header_blockset=*/0x08,
      nullptr, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kRoomHeaderFallback);
  EXPECT_FALSE(context.uses_entrance());
  EXPECT_EQ(context.effective_blockset(), 0x08);
}

TEST(DungeonRenderContextTest, InfersOwnerWhenAllEntrancesAgree) {
  const auto census = MakeCensus({{0x30, 0x31, 0x32}, {0x40}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 12, .room_id = 0x30, .main_gfx = 0x18},
      DungeonRenderEntranceCandidate{
          .slot = 9, .room_id = 0x31, .main_gfx = 0x18},
      DungeonRenderEntranceCandidate{
          .slot = 20, .room_id = 0x40, .main_gfx = 0x2A},
  };

  const auto context = ResolveDungeonRenderContext(
      0x32, /*selected_entrance_slot=*/20, /*room_header_blockset=*/0x06,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kInferredOwnerEntrance);
  EXPECT_EQ(context.entrance_slot, 9);
  EXPECT_EQ(context.entrance_blockset, 0x18);
  EXPECT_EQ(context.candidate_slots, (std::vector<int>{9, 12}));
}

TEST(DungeonRenderContextTest, ConflictingOwnerEntrancesFallBack) {
  const auto census = MakeCensus({{0x50, 0x51, 0x52}});
  const std::array candidates = {
      DungeonRenderEntranceCandidate{
          .slot = 7, .room_id = 0x50, .main_gfx = 0x10},
      DungeonRenderEntranceCandidate{
          .slot = 8, .room_id = 0x51, .main_gfx = 0x11},
  };

  const auto context = ResolveDungeonRenderContext(
      0x52, /*selected_entrance_slot=*/99, /*room_header_blockset=*/0x07,
      &census, candidates);

  EXPECT_EQ(context.source, DungeonRenderContextSource::kAmbiguous);
  EXPECT_FALSE(context.uses_entrance());
  EXPECT_EQ(context.effective_blockset(), 0x07);
  EXPECT_EQ(context.candidate_slots, (std::vector<int>{7, 8}));
}

TEST(DungeonRenderContextTest, ViewerUsesResolvedOwnerEntranceSlot) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  auto census = MakeCensus({{0x10, 0x11}});
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(
      editor, std::move(census));
  constexpr int kEntranceSlot = zelda3::kNumDungeonSpawnPoints;
  DungeonEditorV2EntranceCameraTestPeer::SetSelectedEntranceGraphics(
      editor, kEntranceSlot, /*room_id=*/0x10, /*main_gfx=*/0x2A);
  DungeonCanvasViewer viewer(&rom);

  DungeonEditorV2EntranceCameraTestPeer::ConfigureViewer(editor, viewer, 0x11);

  EXPECT_EQ(viewer.current_entrance_id(), kEntranceSlot);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0x2A);
}

TEST(DungeonRenderContextTest, ContextSnapshotReportsOwnerSourceAndCamera) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  DungeonEditorV2EntranceCameraTestPeer::SetRenderContextCensus(
      editor, MakeCensus({{0x10, 0x11}}));
  constexpr int kEntranceSlot = zelda3::kNumDungeonSpawnPoints;
  DungeonEditorV2EntranceCameraTestPeer::SetSelectedEntranceGraphics(
      editor, kEntranceSlot, /*room_id=*/0x10, /*main_gfx=*/0x2A);
  auto camera = StateFromDerived(/*room_id=*/0x10, /*player_x=*/0x0180,
                                 /*player_y=*/0x0290);
  DungeonEditorV2EntranceCameraTestPeer::SetRegularEntrance(
      editor, kEntranceSlot, camera);
  DungeonEditorV2EntranceCameraTestPeer::MaterializeCurrentRoom(
      editor, /*room_id=*/0x11, /*room_blockset=*/0x04);

  const EditorContextSnapshot snapshot = editor.BuildContextSnapshot();
  const auto value_for = [&snapshot](const std::string& id) {
    const auto it = std::find_if(
        snapshot.metadata.begin(), snapshot.metadata.end(),
        [&id](const EditorContextValue& value) { return value.id == id; });
    return it == snapshot.metadata.end() ? std::string{} : it->value;
  };

  EXPECT_EQ(value_for("graphics_entrance"), "0x07");
  EXPECT_EQ(value_for("graphics_source"), "Selected entrance");
  EXPECT_EQ(value_for("dungeon_owner"), "Dungeon 1");
  EXPECT_EQ(value_for("entrance_camera"), "Matches player position");
}

TEST(DungeonEntranceCameraTest, DerivesUpperLeftAndLowerRightStates) {
  const auto upper_left =
      DeriveDungeonEntranceCamera(/*room_id=*/0x00, /*player_x=*/0x0078,
                                  /*player_y=*/0x0078);
  EXPECT_EQ(upper_left.camera_x, 0x0000);
  EXPECT_EQ(upper_left.camera_y, 0x0010);
  EXPECT_EQ(upper_left.trigger_x, 0x007F);
  EXPECT_EQ(upper_left.trigger_y, 0x0087);
  EXPECT_EQ(upper_left.boundaries,
            (std::array<uint8_t, 8>{0, 0, 0, 1, 0, 0, 0, 1}));
  EXPECT_EQ(upper_left.quadrant, 0x00);

  const auto lower_right =
      DeriveDungeonEntranceCamera(/*room_id=*/0x12, /*player_x=*/0x05F8,
                                  /*player_y=*/0x03F8);
  EXPECT_EQ(lower_right.camera_x, 0x0500);
  EXPECT_EQ(lower_right.camera_y, 0x0310);
  EXPECT_EQ(lower_right.trigger_x, 0x017F);
  EXPECT_EQ(lower_right.trigger_y, 0x0187);
  EXPECT_EQ(lower_right.boundaries,
            (std::array<uint8_t, 8>{3, 2, 3, 3, 5, 4, 5, 5}));
  EXPECT_EQ(lower_right.quadrant, 0x12);
}

TEST(DungeonEntranceCameraTest, UsesIndependentHorizontalAndVerticalLimits) {
  const auto horizontal =
      DeriveDungeonEntranceCamera(/*room_id=*/0x11F, /*player_x=*/0x1FF8,
                                  /*player_y=*/0x23F8);
  EXPECT_EQ(horizontal.camera_x, 0x1F00);
  EXPECT_EQ(horizontal.boundaries[7], 0x1F);

  const auto vertical =
      DeriveDungeonEntranceCamera(/*room_id=*/0x127, /*player_x=*/0x0FF8,
                                  /*player_y=*/0x25F8);
  EXPECT_EQ(vertical.camera_y, 0x2510);
  EXPECT_EQ(vertical.boundaries[3], 0x25);
}

TEST(DungeonEntranceCameraTest, DistinguishesSafeDifferencesFromErrors) {
  auto state = StateFromDerived(/*room_id=*/0x23, /*player_x=*/0x0750,
                                /*player_y=*/0x0520);
  state.trigger_x += 8;

  const auto safe_difference = ValidateDungeonEntranceCamera(state);
  EXPECT_TRUE(safe_difference.geometry_valid());
  EXPECT_FALSE(safe_difference.matches_derived());
  EXPECT_TRUE(safe_difference.can_repair());

  state.camera_x = 0;
  state.boundaries[4] = 0x20;
  const auto invalid = ValidateDungeonEntranceCamera(state);
  EXPECT_FALSE(invalid.geometry_valid());
  EXPECT_FALSE(invalid.errors.empty());
  EXPECT_TRUE(invalid.can_repair());

  state.player_x = 0;
  const auto invalid_source = ValidateDungeonEntranceCamera(state);
  EXPECT_FALSE(invalid_source.repair_source_valid);
  EXPECT_FALSE(invalid_source.can_repair());
}

TEST(DungeonEntranceCameraTest, RepairPreservesSpecialRoomQuadrant) {
  auto state = StateFromDerived(/*room_id=*/0x104, /*player_x=*/0x0950,
                                /*player_y=*/0x2150);
  state.camera_x += 8;
  state.quadrant ^= 0x12;

  const auto validation = ValidateDungeonEntranceCamera(state);
  ASSERT_TRUE(validation.geometry_valid());
  EXPECT_FALSE(validation.quadrant_repair_safe);

  const auto repaired = BuildRepairedDungeonEntranceCamera(state);
  EXPECT_EQ(repaired.camera_x, validation.expected.camera_x);
  EXPECT_EQ(repaired.quadrant, state.quadrant);

  auto quadrant_only = StateFromDerived(/*room_id=*/0x104, /*player_x=*/0x0950,
                                        /*player_y=*/0x2150);
  quadrant_only.quadrant ^= 0x12;
  EXPECT_FALSE(ValidateDungeonEntranceCamera(quadrant_only).can_repair());
}

TEST(DungeonEntranceCameraTest, ApplyRestoresDataAndDirtyState) {
  auto state = StateFromDerived(/*room_id=*/0x40, /*player_x=*/0x0180,
                                /*player_y=*/0x08A0);
  state.dirty = true;

  zelda3::RoomEntrance entrance;
  ApplyDungeonEntranceCameraState(state, &entrance);
  EXPECT_EQ(CaptureDungeonEntranceCamera(entrance).camera_x, state.camera_x);
  EXPECT_TRUE(entrance.dirty());

  state.kind = DungeonEntranceRecordKind::kSpawnPoint;
  state.dirty = false;
  zelda3::DungeonSpawnPoint spawn;
  spawn.MarkDirty();
  ApplyDungeonSpawnCameraState(state, &spawn);
  EXPECT_EQ(CaptureDungeonSpawnCamera(spawn).boundaries, state.boundaries);
  EXPECT_FALSE(spawn.dirty());
}

TEST(DungeonEntranceCameraTest, EditorRepairIsOneUndoableDirtyTransition) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  constexpr int kSlot = zelda3::kNumDungeonSpawnPoints;
  auto before = StateFromDerived(/*room_id=*/0x10, /*player_x=*/0x0180,
                                 /*player_y=*/0x0290);
  before.trigger_x -= 8;
  before.dirty = false;
  DungeonEditorV2EntranceCameraTestPeer::SetRegularEntrance(editor, kSlot,
                                                            before);

  const absl::Status repair_status =
      DungeonEditorV2EntranceCameraTestPeer::Repair(editor, kSlot);
  ASSERT_TRUE(repair_status.ok()) << repair_status;
  auto repaired =
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot);
  EXPECT_EQ(repaired.trigger_x,
            DeriveDungeonEntranceCamera(before.room_id, before.player_x,
                                        before.player_y)
                .trigger_x);
  EXPECT_TRUE(repaired.dirty);

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot),
      before);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(
      DungeonEditorV2EntranceCameraTestPeer::RegularEntrance(editor, kSlot),
      repaired);
}

TEST(DungeonEntranceCameraTest, SpawnRepairMarksOnlyDedicatedSpawnRecord) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  constexpr int kSlot = 0;
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, kSlot);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;
  const auto derived =
      DeriveDungeonEntranceCamera(/*room_id=*/0x21, /*player_x=*/0x0350,
                                  /*player_y=*/0x0490);
  spawn.room_id = 0x21;
  spawn.x_coordinate = 0x0350;
  spawn.y_coordinate = 0x0490;
  spawn.horizontal_scroll = derived.camera_x;
  spawn.vertical_scroll = derived.camera_y;
  spawn.camera_trigger_x = derived.trigger_x - 8;
  spawn.camera_trigger_y = derived.trigger_y;
  spawn.camera_scroll_boundaries = derived.boundaries;
  spawn.quadrant = derived.quadrant;
  spawn.ClearDirty();
  DungeonEditorV2EntranceCameraTestPeer::SetSpawnPoint(editor, kSlot,
                                                       std::move(spawn));

  const absl::Status repair_status =
      DungeonEditorV2EntranceCameraTestPeer::Repair(editor, kSlot);
  ASSERT_TRUE(repair_status.ok()) << repair_status;
  EXPECT_TRUE(
      DungeonEditorV2EntranceCameraTestPeer::SpawnPoint(editor, kSlot).dirty);
  EXPECT_FALSE(DungeonEditorV2EntranceCameraTestPeer::LegacyEntranceDirty(
      editor, kSlot));

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_FALSE(
      DungeonEditorV2EntranceCameraTestPeer::SpawnPoint(editor, kSlot).dirty);
}

TEST(DungeonEntranceCameraTest, ViewerOverlayRoundTripsSelectedCamera) {
  Rom rom;
  DungeonCanvasViewer viewer(&rom);
  const auto state = StateFromDerived(/*room_id=*/0x42,
                                      /*player_x=*/0x0550,
                                      /*player_y=*/0x0890);

  viewer.SetEntranceCameraOverlay(/*entrance_slot=*/0x12, state);
  ASSERT_TRUE(viewer.entrance_camera_overlay().has_value());
  EXPECT_EQ(*viewer.entrance_camera_overlay(), state);

  viewer.ClearEntranceCameraOverlay();
  EXPECT_FALSE(viewer.entrance_camera_overlay().has_value());
}

TEST(DungeonEntranceCameraTest, RegularRepairFieldsSaveAndReadBack) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::RoomEntrance entrance(&rom, /*entrance_id=*/0,
                                /*is_spawn_point=*/false);
  auto expected = StateFromDerived(/*room_id=*/0x12, /*player_x=*/0x0550,
                                   /*player_y=*/0x0350);
  expected.dirty = true;
  entrance.room_ = expected.room_id;
  entrance.x_position_ = expected.player_x;
  entrance.y_position_ = expected.player_y;
  ApplyDungeonEntranceCameraState(expected, &entrance);

  ASSERT_TRUE(entrance.Save(&rom, /*entrance_id=*/0).ok());
  const zelda3::RoomEntrance reloaded(&rom, /*entrance_id=*/0,
                                      /*is_spawn_point=*/false);
  expected.dirty = false;
  EXPECT_EQ(CaptureDungeonEntranceCamera(reloaded), expected);
}

TEST(DungeonEntranceCameraTest, SpawnRepairFieldsSaveAndReadBack) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom, /*spawn_id=*/0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  auto spawn = *spawn_or;
  auto expected = StateFromDerived(/*room_id=*/0x21, /*player_x=*/0x0350,
                                   /*player_y=*/0x0490);
  expected.kind = DungeonEntranceRecordKind::kSpawnPoint;
  expected.dirty = true;
  spawn.room_id = expected.room_id;
  spawn.x_coordinate = expected.player_x;
  spawn.y_coordinate = expected.player_y;
  ApplyDungeonSpawnCameraState(expected, &spawn);

  ASSERT_TRUE(spawn.Save(&rom, /*spawn_id=*/0).ok());
  auto reloaded_or = zelda3::DungeonSpawnPoint::Load(rom, /*spawn_id=*/0);
  ASSERT_TRUE(reloaded_or.ok()) << reloaded_or.status();
  expected.dirty = false;
  EXPECT_EQ(CaptureDungeonSpawnCamera(*reloaded_or), expected);
}

}  // namespace
}  // namespace yaze::editor
