#include "app/editor/overworld/overworld_editor.h"

#include <memory>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "rom/rom.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {
namespace {

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionLeavesValidSelectionUntouched) {
  int current_world = 1;
  int current_map = 0x40;

  EXPECT_FALSE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 1);
  EXPECT_EQ(current_map, 0x40);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsNegativeMapToWorldStart) {
  int current_world = 1;
  int current_map = -7;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 1);
  EXPECT_EQ(current_map, 0x40);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsOversizedMapIntoValidWorld) {
  int current_world = 4;
  int current_map = 0x103;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 2);
  EXPECT_EQ(current_map, 0x80);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionClampsSpecialTailToLoadedSpecialMap) {
  int current_world = 2;
  int current_map = 0xBF;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 2);
  EXPECT_EQ(current_map, 0x80);
}

TEST(OverworldEditorStateTest, NormalizeMapSelectionRealignsWorldToValidMap) {
  int current_world = 0;
  int current_map = 0x81;

  EXPECT_TRUE(
      OverworldEditor::NormalizeMapSelection(current_world, current_map));
  EXPECT_EQ(current_world, 2);
  EXPECT_EQ(current_map, 0x81);
}

TEST(OverworldEditorStateTest,
     NormalizeMapSelectionKeepsProjectTerrainInSpecialWorld) {
  for (int map_id : {0xA0, 0xBF}) {
    int current_world = 0;
    int current_map = map_id;
    EXPECT_TRUE(OverworldEditor::NormalizeMapSelection(current_world,
                                                       current_map, 192));
    EXPECT_EQ(current_world, 2);
    EXPECT_EQ(current_map, map_id);
    EXPECT_FALSE(OverworldEditor::NormalizeMapSelection(current_world,
                                                        current_map, 192));
  }
}

TEST(OverworldEditorStateTest, ProjectPathAloneDoesNotExposeTerrainMaps) {
  project::YazeProject project;
  project.lyra_terrain_file = "missing.lyra-terrain.json";
  EditorDependencies deps;
  deps.project = &project;
  OverworldEditor editor(nullptr, deps);
  EXPECT_EQ(editor.DisplayedMapCount(), 160);
  EXPECT_FALSE(editor.IsProjectTerrainMap(0xA0));
  EXPECT_FALSE(editor.IsProjectTerrainMap(0xBF));
}

TEST(OverworldEditorStateTest, NullRomSaveReturnsFailedPrecondition) {
  OverworldEditor editor(nullptr);

  const auto status = editor.Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
}

TEST(OverworldEditorStateTest, UnloadedContextSnapshotIsStillInformative) {
  OverworldEditor editor(nullptr);
  const EditorContextSnapshot snapshot = editor.BuildContextSnapshot();
  EXPECT_EQ(snapshot.category, "Overworld");
  EXPECT_EQ(snapshot.title, "Overworld");
  EXPECT_NE(snapshot.subtitle.find("Load a ROM"), std::string::npos);
  EXPECT_TRUE(snapshot.actions.empty());
}

auto EntranceRecordState(const zelda3::OverworldEntrance& value) {
  return std::make_tuple(value.map_id_, value.map_pos_, value.entrance_id_,
                         value.x_, value.y_, value.game_x_, value.game_y_,
                         value.entity_id_, value.entity_type_, value.is_hole_,
                         value.deleted);
}

auto ReturnRecordState(const zelda3::OverworldExit& value) {
  return std::make_tuple(
      value.room_id_, value.map_id_, value.map_pos_, value.x_player_,
      value.y_player_, value.x_scroll_, value.y_scroll_, value.x_camera_,
      value.y_camera_, value.scroll_mod_x_, value.scroll_mod_y_,
      value.door_type_1_, value.door_type_2_, value.x_, value.y_, value.game_x_,
      value.game_y_, value.entity_id_, value.entity_type_, value.is_automatic_,
      value.is_hole_, value.deleted_);
}

class OverworldRecordEditLifecycleTest : public ::testing::Test {
 protected:
  static constexpr size_t kEntranceRow = 5;
  static constexpr size_t kReturnRow = 7;

  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    rom_[zelda3::OverworldCustomASMHasBeenApplied] = 0x03;
    rom_[zelda3::kOverworldEntranceExpandedFlagPos] = 0;
    editor_ = std::make_unique<OverworldEditor>(&rom_);
    auto& maps = Maps();
    maps.reserve(192);
    // Synthetic loaded map models establish edit geometry only. This fixture
    // does not admit a runtime writer or load expanded terrain into the ROM.
    for (int i = 0; i < 192; ++i) {
      maps.emplace_back(i, &rom_);
      maps.back().SetAsSmallMap(i);
    }
    auto& entrances = Entrances();
    entrances.reserve(256);
    for (int i = 0; i < 256; ++i) {
      entrances.emplace_back((i % 32) * 16, (i / 32) * 16,
                             static_cast<uint8_t>(i), 0, 0, false);
      entrances.back().entity_id_ = i;
      entrances.back().UpdateMapProperties(0);
    }
    auto& returns = Returns();
    returns.reserve(zelda3::kNumOverworldExits);
    for (int i = 0; i < zelda3::kNumOverworldExits; ++i) {
      returns.emplace_back(i + 1, 0, 0, 0, 0, 48, 32, 0, 0, 1, 2, 0x1234,
                           0x5678);
      returns.back().entity_id_ = i;
      returns.back().UpdateMapProperties(0, &editor_->overworld());
    }
    rom_.set_dirty(false);
  }

  std::vector<zelda3::OverworldMap>& Maps() {
    return const_cast<std::vector<zelda3::OverworldMap>&>(
        editor_->overworld().overworld_maps());
  }
  std::vector<zelda3::OverworldEntrance>& Entrances() {
    return *editor_->overworld().mutable_entrances();
  }
  std::vector<zelda3::OverworldExit>& Returns() {
    return *editor_->overworld().mutable_exits();
  }
  zelda3::OverworldEntrance MovedEntrance() {
    auto value = Entrances()[kEntranceRow];
    value.map_id_ = 0xB0;
    value.x_ = 21 * 16;
    value.y_ = 6 * 512 + 9 * 16;
    value.entrance_id_ = 0x0B;
    return value;
  }
  zelda3::OverworldExit MovedReturn() {
    auto value = Returns()[kReturnRow];
    value.room_id_ = 0x83;
    value.map_id_ = 0xB0;
    value.x_ = 22 * 16;
    value.y_ = 6 * 512 + 11 * 16;
    return value;
  }

  Rom rom_;
  std::unique_ptr<OverworldEditor> editor_;
};

TEST_F(OverworldRecordEditLifecycleTest,
       UnchangedLoadedReturnAndRoomOnlyEditPreserveRetainedCamera) {
  auto& value = Returns()[kReturnRow];
  value.map_id_ = 0x12;
  value.x_ = value.x_player_ = 1032;
  value.y_ = value.y_player_ = 1048;
  value.game_x_ = 0;
  value.game_y_ = 1;
  value.x_scroll_ = 1000;
  value.y_scroll_ = 1010;
  value.x_camera_ = 1222;
  value.y_camera_ = 1111;
  value.map_pos_ = 0x1357;
  value.is_automatic_ = true;  // Loaded records do not persist this preference.
  const auto before = ReturnRecordState(value);
  ASSERT_TRUE(editor_->ApplyExitEdit(&value, value).ok());
  EXPECT_EQ(ReturnRecordState(value), before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_FALSE(editor_->CanUndo());

  auto draft = value;
  draft.room_id_ = 0x83;
  ASSERT_TRUE(editor_->ApplyExitEdit(&value, draft).ok());
  EXPECT_EQ(value.room_id_, 0x83);
  EXPECT_EQ(value.x_scroll_, 1000);
  EXPECT_EQ(value.y_scroll_, 1010);
  EXPECT_EQ(value.x_camera_, 1222);
  EXPECT_EQ(value.y_camera_, 1111);
  EXPECT_EQ(value.map_pos_, 0x1357);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(ReturnRecordState(value), before);
}

TEST_F(OverworldRecordEditLifecycleTest,
       MovedEntranceDerivesSavedPositionAndHasOneUndoStep) {
  auto& value = Entrances()[kEntranceRow];
  const auto before = EntranceRecordState(value);
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&value, MovedEntrance()).ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(value.map_id_, 0xB0);
  EXPECT_EQ(value.entrance_id_, 0x0B);
  EXPECT_EQ(value.game_x_, 21);
  EXPECT_EQ(value.game_y_, 9);
  EXPECT_EQ(value.map_pos_, ((9 * 64) + 21) * 2);
  const auto after = EntranceRecordState(value);

  rom_.set_dirty(false);
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&value, value).ok());
  EXPECT_FALSE(rom_.dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(EntranceRecordState(value), before);
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  rom_.set_dirty(false);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(EntranceRecordState(value), after);
  EXPECT_EQ(editor_->Redo().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(OverworldRecordEditLifecycleTest,
       RectangularParentEditsPreserveWideRightAndTallBottomCoordinates) {
  struct Destination {
    bool wide;
    int x;
    int y;
    int game_x;
    int game_y;
    int entrance_position;
    int scroll_x;
    int scroll_y;
    int camera_x;
    int camera_y;
    int return_vram;
  };
  const Destination destinations[] = {
      {true, 528, 3232, 33, 10, 0x0542, 408, 3152, 535, 3263, 0x02B2},
      {false, 160, 3648, 10, 36, 0x1214, 40, 3568, 167, 3679, 0x0F84}};
  for (const auto& destination : destinations) {
    SCOPED_TRACE(destination.wide ? "wide right" : "tall bottom");
    Maps()[0xB0].SetAsSmallMap(0xB0);
    Maps()[0xB1].SetAsSmallMap(0xB1);
    Maps()[0xB8].SetAsSmallMap(0xB8);
    if (destination.wide) {
      Maps()[0xB0].SetAsWideMap(0xB0, 0);
      Maps()[0xB1].SetAsWideMap(0xB0, 1);
    } else {
      Maps()[0xB0].SetAsTallMap(0xB0, 0);
      Maps()[0xB8].SetAsTallMap(0xB0, 2);
    }
    auto& entrance = Entrances()[kEntranceRow];
    auto& returning = Returns()[kReturnRow];
    const auto entrance_before = EntranceRecordState(entrance);
    const auto return_before = ReturnRecordState(returning);
    auto entrance_draft = MovedEntrance();
    entrance_draft.x_ = destination.x;
    entrance_draft.y_ = destination.y;
    auto return_draft = MovedReturn();
    return_draft.x_ = destination.x;
    return_draft.y_ = destination.y;
    ASSERT_TRUE(editor_->ApplyEntranceEdit(&entrance, entrance_draft).ok());
    ASSERT_TRUE(editor_->ApplyExitEdit(&returning, return_draft).ok());
    EXPECT_TRUE(rom_.dirty());
    EXPECT_EQ(entrance.map_id_, 0xB0);
    EXPECT_EQ(entrance.game_x_, destination.game_x);
    EXPECT_EQ(entrance.game_y_, destination.game_y);
    EXPECT_EQ(entrance.map_pos_, destination.entrance_position);
    EXPECT_EQ(returning.map_id_, 0xB0);
    EXPECT_EQ(returning.x_player_, destination.x);
    EXPECT_EQ(returning.y_player_, destination.y);
    EXPECT_EQ(returning.game_x_, destination.game_x);
    EXPECT_EQ(returning.game_y_, destination.game_y);
    EXPECT_EQ(returning.x_scroll_, destination.scroll_x);
    EXPECT_EQ(returning.y_scroll_, destination.scroll_y);
    EXPECT_EQ(returning.x_camera_, destination.camera_x);
    EXPECT_EQ(returning.y_camera_, destination.camera_y);
    EXPECT_EQ(returning.map_pos_, destination.return_vram);
    ASSERT_TRUE(editor_->Undo().ok());
    ASSERT_TRUE(editor_->Undo().ok());
    EXPECT_EQ(EntranceRecordState(entrance), entrance_before);
    EXPECT_EQ(ReturnRecordState(returning), return_before);
    EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
    rom_.set_dirty(false);
  }
}

TEST_F(OverworldRecordEditLifecycleTest,
       RectangularEditsRejectChildSelectionAndForeignPhysicalPage) {
  auto& entrance = Entrances()[kEntranceRow];
  auto& returning = Returns()[kReturnRow];
  const auto entrance_before = EntranceRecordState(entrance);
  const auto return_before = ReturnRecordState(returning);
  for (bool wide : {true, false}) {
    SCOPED_TRACE(wide ? "wide right" : "tall bottom");
    const int child = wide ? 0xB1 : 0xB8;
    if (wide) {
      Maps()[0xB0].SetAsWideMap(0xB0, 0);
      Maps()[child].SetAsWideMap(0xB0, 1);
    } else {
      Maps()[0xB0].SetAsTallMap(0xB0, 0);
      Maps()[child].SetAsTallMap(0xB0, 2);
    }
    auto entrance_draft = MovedEntrance();
    auto return_draft = MovedReturn();
    entrance_draft.x_ = return_draft.x_ = wide ? 528 : 160;
    entrance_draft.y_ = return_draft.y_ = wide ? 3232 : 3648;
    entrance_draft.map_id_ = return_draft.map_id_ = child;
    EXPECT_FALSE(editor_->ApplyEntranceEdit(&entrance, entrance_draft).ok());
    EXPECT_FALSE(editor_->ApplyExitEdit(&returning, return_draft).ok());
    EXPECT_EQ(EntranceRecordState(entrance), entrance_before);
    EXPECT_EQ(ReturnRecordState(returning), return_before);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);

    // The parent still has rectangular geometry, but the physical page now
    // belongs to another root. Geometry alone cannot grant page ownership.
    Maps()[child].SetAsSmallMap(child);
    entrance_draft.map_id_ = return_draft.map_id_ = 0xB0;
    EXPECT_FALSE(editor_->ApplyEntranceEdit(&entrance, entrance_draft).ok());
    EXPECT_FALSE(editor_->ApplyExitEdit(&returning, return_draft).ok());
    EXPECT_EQ(EntranceRecordState(entrance), entrance_before);
    EXPECT_EQ(ReturnRecordState(returning), return_before);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  }
}

TEST_F(OverworldRecordEditLifecycleTest,
       LegacyHoleRejectsTopRowsAndBoundaryPositionSurvivesReload) {
  rom_[zelda3::kOverworldEntranceExpandedFlagPos] = 0xB8;
  auto& holes = *editor_->overworld().mutable_holes();
  holes.reserve(zelda3::kNumOverworldHoles);
  for (int i = 0; i < zelda3::kNumOverworldHoles; ++i) {
    holes.emplace_back(i * 16, 256, static_cast<uint8_t>(i), 0, 0, true);
    holes.back().entity_id_ = i;
    holes.back().UpdateMapProperties(0);
  }
  auto& hole = holes.front();
  const auto before = EntranceRecordState(hole);
  auto draft = hole;
  draft.x_ = 16;
  draft.y_ = 16;
  EXPECT_FALSE(editor_->ApplyEntranceEdit(&hole, draft).ok());
  EXPECT_EQ(EntranceRecordState(hole), before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);

  draft.y_ = 128;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&hole, draft).ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(hole.game_x_, 1);
  EXPECT_EQ(hole.game_y_, 8);
  EXPECT_EQ(hole.map_pos_, 0x0402);
  const auto after = EntranceRecordState(hole);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(EntranceRecordState(hole), before);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(EntranceRecordState(hole), after);
  ASSERT_TRUE(zelda3::SaveHoles(&rom_, holes).ok());
  const auto stored_position = rom_.ReadWord(zelda3::kOverworldHolePos);
  ASSERT_TRUE(stored_position.ok()) << stored_position.status();
  EXPECT_EQ(*stored_position, 0x0002);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom_.vector()).ok());
  const auto reloaded = zelda3::LoadHoles(&reopened);
  ASSERT_TRUE(reloaded.ok()) << reloaded.status();
  ASSERT_EQ(reloaded->size(), zelda3::kNumOverworldHoles);
  EXPECT_EQ(reloaded->front().map_id_, 0);
  EXPECT_EQ(reloaded->front().x_, 16);
  EXPECT_EQ(reloaded->front().y_, 128);
  EXPECT_EQ(reloaded->front().map_pos_, 0x0402);
  EXPECT_TRUE(reloaded->front().is_hole_);
  EXPECT_EQ(reloaded->at(1).map_pos_, holes[1].map_pos_);
}

TEST_F(OverworldRecordEditLifecycleTest,
       ExpandedEntranceHoleAllowsPositionAboveLegacyBoundary) {
  auto& hole = Entrances()[kEntranceRow];
  hole.is_hole_ = true;
  auto draft = hole;
  draft.x_ = 16;
  draft.y_ = 16;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&hole, draft).ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(hole.game_x_, 1);
  EXPECT_EQ(hole.game_y_, 1);
  EXPECT_EQ(hole.map_pos_, 0x0082);
}

TEST_F(OverworldRecordEditLifecycleTest,
       MovedReturnDerivesPlayerCameraAndVramWithExactUndoRedo) {
  auto& value = Returns()[kReturnRow];
  const auto before = ReturnRecordState(value);
  ASSERT_TRUE(editor_->ApplyExitEdit(&value, MovedReturn()).ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(value.map_id_, 0xB0);
  EXPECT_EQ(value.room_id_, 0x83);
  EXPECT_EQ(value.x_player_, 352);
  EXPECT_EQ(value.y_player_, 3248);
  EXPECT_EQ(value.game_x_, 22);
  EXPECT_EQ(value.game_y_, 11);
  EXPECT_EQ(value.x_scroll_, 232);
  EXPECT_EQ(value.y_scroll_, 3168);
  EXPECT_EQ(value.x_camera_, 359);
  EXPECT_EQ(value.y_camera_, 3279);
  EXPECT_EQ(value.map_pos_, 0x031C);
  const auto after = ReturnRecordState(value);

  rom_.set_dirty(false);
  ASSERT_TRUE(editor_->ApplyExitEdit(&value, value).ok());
  EXPECT_FALSE(rom_.dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(ReturnRecordState(value), before);
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(ReturnRecordState(value), after);
  EXPECT_EQ(editor_->Redo().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(OverworldRecordEditLifecycleTest,
       ManualReturnPreservesAuthoredCameraScrollAndOtherSavedFields) {
  auto& value = Returns()[kReturnRow];
  const auto before = ReturnRecordState(value);
  auto draft = MovedReturn();
  draft.is_automatic_ = false;
  draft.x_camera_ = 0x017F;
  draft.y_camera_ = 0x0CFF;
  draft.x_scroll_ = 0x0080;
  draft.y_scroll_ = 0x0C20;
  draft.scroll_mod_x_ = 0x12;
  draft.scroll_mod_y_ = 0x34;
  draft.door_type_1_ = 0x4567;
  draft.door_type_2_ = 0x89AB;
  ASSERT_TRUE(editor_->ApplyExitEdit(&value, draft).ok());
  EXPECT_EQ(value.x_player_, draft.x_);
  EXPECT_EQ(value.y_player_, draft.y_);
  EXPECT_EQ(value.x_camera_, draft.x_camera_);
  EXPECT_EQ(value.y_camera_, draft.y_camera_);
  EXPECT_EQ(value.x_scroll_, draft.x_scroll_);
  EXPECT_EQ(value.y_scroll_, draft.y_scroll_);
  EXPECT_EQ(value.map_pos_, 0x0110);
  const auto after = ReturnRecordState(value);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(ReturnRecordState(value), before);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(ReturnRecordState(value), after);
}

TEST_F(OverworldRecordEditLifecycleTest,
       InvalidEntranceEditsLeaveRecordHistoryAndDirtyStateUnchanged) {
  auto& value = Entrances()[kEntranceRow];
  const auto before = EntranceRecordState(value);
  std::vector<zelda3::OverworldEntrance> drafts;
  auto draft = MovedEntrance();
  draft.x_ = -16;
  drafts.push_back(draft);
  draft = MovedEntrance();
  draft.x_ = 512;
  drafts.push_back(draft);
  draft = MovedEntrance();
  draft.y_ = 4096;
  drafts.push_back(draft);
  draft = MovedEntrance();
  draft.x_ += 1;
  drafts.push_back(draft);
  draft = MovedEntrance();
  draft.map_id_ = 0xC0;
  drafts.push_back(draft);
  draft = MovedEntrance();
  draft.is_hole_ = true;
  drafts.push_back(draft);
  for (size_t i = 0; i < drafts.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(editor_->ApplyEntranceEdit(&value, drafts[i]).ok());
    EXPECT_EQ(EntranceRecordState(value), before);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  }
  auto foreign = value;
  EXPECT_EQ(editor_->ApplyEntranceEdit(&foreign, MovedEntrance()).code(),
            absl::StatusCode::kNotFound);
  Maps().erase(Maps().begin() + 0xB0, Maps().end());
  EXPECT_FALSE(editor_->ApplyEntranceEdit(&value, MovedEntrance()).ok());
  EXPECT_EQ(EntranceRecordState(value), before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(OverworldRecordEditLifecycleTest,
       InvalidReturnEditsLeaveRecordHistoryAndDirtyStateUnchanged) {
  auto& value = Returns()[kReturnRow];
  const auto before = ReturnRecordState(value);
  std::vector<zelda3::OverworldExit> drafts;
  auto draft = MovedReturn();
  draft.x_ = -1;
  drafts.push_back(draft);
  draft = MovedReturn();
  draft.x_ = 512;
  drafts.push_back(draft);
  draft = MovedReturn();
  draft.y_ = 4096;
  drafts.push_back(draft);
  draft = MovedReturn();
  draft.map_id_ = 0xC0;
  drafts.push_back(draft);
  draft = MovedReturn();
  draft.is_hole_ = true;
  drafts.push_back(draft);
  for (size_t i = 0; i < drafts.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(editor_->ApplyExitEdit(&value, drafts[i]).ok());
    EXPECT_EQ(ReturnRecordState(value), before);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  }
  auto foreign = value;
  EXPECT_EQ(editor_->ApplyExitEdit(&foreign, MovedReturn()).code(),
            absl::StatusCode::kNotFound);
  Maps().erase(Maps().begin() + 0xB0, Maps().end());
  EXPECT_FALSE(editor_->ApplyExitEdit(&value, MovedReturn()).ok());
  EXPECT_EQ(ReturnRecordState(value), before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(OverworldRecordEditLifecycleTest,
       HigherEntranceMatchBlocksEditUntilThatRowIsDeleted) {
  auto& value = Entrances()[kEntranceRow];
  const auto before = EntranceRecordState(value);
  auto& higher = Entrances()[kEntranceRow + 1];
  higher = MovedEntrance();
  higher.UpdateMapProperties(higher.map_id_);
  EXPECT_EQ(editor_->ApplyEntranceEdit(&value, MovedEntrance()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(EntranceRecordState(value), before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  higher.deleted = true;
  const auto higher_before = EntranceRecordState(higher);
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&value, MovedEntrance()).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(EntranceRecordState(value), before);
  EXPECT_EQ(EntranceRecordState(higher), higher_before);
}

TEST_F(OverworldRecordEditLifecycleTest,
       HigherReturnRoomMatchBlocksLowerRowButAllowsExplicitWinningRow) {
  auto& lower = Returns()[kReturnRow];
  auto& higher = Returns()[kReturnRow + 1];
  const auto lower_before = ReturnRecordState(lower);
  higher.room_id_ = 0x83;
  const auto higher_before = ReturnRecordState(higher);
  EXPECT_EQ(editor_->ApplyExitEdit(&lower, MovedReturn()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(ReturnRecordState(lower), lower_before);
  EXPECT_EQ(ReturnRecordState(higher), higher_before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  ASSERT_TRUE(editor_->ApplyExitEdit(&higher, MovedReturn()).ok());
  EXPECT_EQ(higher.map_id_, 0xB0);
  EXPECT_EQ(higher.room_id_, 0x83);
  EXPECT_EQ(ReturnRecordState(lower), lower_before);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(ReturnRecordState(higher), higher_before);
}

TEST_F(OverworldRecordEditLifecycleTest,
       DeletedHigherReturnRowStillShadowsNativeRoomDispatch) {
  auto& value = Returns()[kReturnRow];
  const auto before = ReturnRecordState(value);
  auto& higher = Returns()[kReturnRow + 1];
  higher.room_id_ = 0x83;
  higher.deleted_ = true;
  const auto higher_before = ReturnRecordState(higher);
  EXPECT_EQ(editor_->ApplyExitEdit(&value, MovedReturn()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(ReturnRecordState(value), before);
  EXPECT_EQ(ReturnRecordState(higher), higher_before);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(OverworldRecordEditLifecycleTest,
       AuthoredReturnDeletionRetiresDispatchAndSurvivesFreshRomReload) {
  auto& lower = Returns()[kReturnRow];
  auto& higher = Returns()[kReturnRow + 1];
  lower.room_id_ = higher.room_id_ = 0x83;
  const auto lower_before = ReturnRecordState(lower);
  const auto before = ReturnRecordState(higher);
  EXPECT_EQ(editor_->ApplyExitEdit(&lower, MovedReturn()).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);

  auto draft = higher;
  draft.deleted_ = true;
  ASSERT_TRUE(editor_->ApplyExitEdit(&higher, draft).ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_TRUE(higher.deleted_);
  EXPECT_EQ(higher.room_id_, 0xFFFF);
  EXPECT_EQ(higher.x_player_, 0xFFFF);
  EXPECT_EQ(higher.y_player_, 0xFFFF);
  EXPECT_EQ(ReturnRecordState(lower), lower_before);
  const auto after = ReturnRecordState(higher);

  rom_.set_dirty(false);
  ASSERT_TRUE(editor_->ApplyExitEdit(&higher, higher).ok());
  EXPECT_FALSE(rom_.dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(ReturnRecordState(higher), before);
  EXPECT_EQ(editor_->Undo().code(), absl::StatusCode::kFailedPrecondition);
  rom_.set_dirty(false);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_TRUE(rom_.dirty());
  EXPECT_EQ(ReturnRecordState(higher), after);
  EXPECT_EQ(editor_->Redo().code(), absl::StatusCode::kFailedPrecondition);

  ASSERT_TRUE(zelda3::SaveExits(&rom_, Returns()).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom_.vector()).ok());
  const auto reloaded = zelda3::LoadExits(&reopened);
  ASSERT_TRUE(reloaded.ok()) << reloaded.status();
  ASSERT_EQ(reloaded->size(), zelda3::kNumOverworldExits);
  const auto& retired = reloaded->at(kReturnRow + 1);
  EXPECT_TRUE(retired.deleted_);
  EXPECT_EQ(retired.room_id_, 0xFFFF);
  EXPECT_EQ(retired.x_player_, 0xFFFF);
  EXPECT_EQ(retired.y_player_, 0xFFFF);
  EXPECT_EQ(retired.map_id_, higher.map_id_);
  EXPECT_EQ(retired.x_camera_, higher.x_camera_);
  EXPECT_EQ(retired.y_camera_, higher.y_camera_);
  EXPECT_EQ(retired.x_scroll_, higher.x_scroll_);
  EXPECT_EQ(retired.y_scroll_, higher.y_scroll_);

  // Native room dispatch scans from the highest row. The serialized retired
  // row must no longer win for room83, even before another edit is applied.
  int winning_row = -1;
  for (size_t i = 0; i < reloaded->size(); ++i) {
    if (reloaded->at(i).room_id_ == 0x83) {
      winning_row = static_cast<int>(i);
    }
  }
  EXPECT_EQ(winning_row, static_cast<int>(kReturnRow));
  Returns() = *reloaded;
  ASSERT_TRUE(
      editor_->ApplyExitEdit(&Returns()[kReturnRow], MovedReturn()).ok());
  EXPECT_EQ(Returns()[kReturnRow].room_id_, 0x83);
  EXPECT_EQ(Returns()[kReturnRow].map_id_, 0xB0);
  EXPECT_EQ(Returns()[kReturnRow + 1].room_id_, 0xFFFF);
}

TEST_F(OverworldRecordEditLifecycleTest,
       MovedEntranceAndReturnSurviveFreshRomRecordReload) {
  ASSERT_TRUE(
      editor_->ApplyEntranceEdit(&Entrances()[kEntranceRow], MovedEntrance())
          .ok());
  ASSERT_TRUE(
      editor_->ApplyExitEdit(&Returns()[kReturnRow], MovedReturn()).ok());
  const auto entrance = Entrances()[kEntranceRow];
  const auto returning = Returns()[kReturnRow];
  const auto entrance_neighbor = Entrances()[kEntranceRow + 1];
  const auto return_neighbor = Returns()[kReturnRow + 1];
  // Exercise the real record serializers and fresh ROM readers. Normal editor
  // Save, disk persistence and runtime route admission have separate fixtures.
  ASSERT_TRUE(zelda3::SaveEntrances(&rom_, Entrances(), true).ok());
  ASSERT_TRUE(zelda3::SaveExits(&rom_, Returns()).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom_.vector()).ok());
  const auto entrances = zelda3::LoadEntrances(&reopened);
  const auto returns = zelda3::LoadExits(&reopened);
  ASSERT_TRUE(entrances.ok()) << entrances.status();
  ASSERT_TRUE(returns.ok()) << returns.status();
  ASSERT_EQ(entrances->size(), 256);
  ASSERT_EQ(returns->size(), zelda3::kNumOverworldExits);
  const auto& loaded_entrance = entrances->at(kEntranceRow);
  EXPECT_EQ(loaded_entrance.map_id_, entrance.map_id_);
  EXPECT_EQ(loaded_entrance.map_pos_, entrance.map_pos_);
  EXPECT_EQ(loaded_entrance.entrance_id_, entrance.entrance_id_);
  EXPECT_EQ(loaded_entrance.x_, entrance.x_);
  EXPECT_EQ(loaded_entrance.y_, entrance.y_);
  auto loaded_return = returns->at(kReturnRow);
  // These are editor-only fields, not serialized by the native return table.
  loaded_return.entity_id_ = returning.entity_id_;
  EXPECT_EQ(ReturnRecordState(loaded_return), ReturnRecordState(returning));
  EXPECT_EQ(entrances->at(kEntranceRow + 1).map_pos_,
            entrance_neighbor.map_pos_);
  EXPECT_EQ(entrances->at(kEntranceRow + 1).map_id_, entrance_neighbor.map_id_);
  EXPECT_EQ(returns->at(kReturnRow + 1).room_id_, return_neighbor.room_id_);
  EXPECT_EQ(returns->at(kReturnRow + 1).x_player_, return_neighbor.x_player_);
}

}  // namespace
}  // namespace yaze::editor
