#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <array>
#include <memory>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_dimensions.h"

namespace yaze::editor {

class DungeonRoomEditsTestPeer {
 public:
  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    editor.current_room_id_ = room_id;
    auto* viewer = editor.GetViewerForRoom(room_id);
    viewer->RefreshRomBackedState(editor.rom_, nullptr, &editor.rooms_,
                                  room_id);
    return viewer;
  }
  static void SeedStaircaseIssues(
      DungeonCanvasViewer& viewer, int center_room_id,
      const std::vector<DungeonStaircaseIssue>& issues) {
    viewer.connected_graph_cache_start_room_id_ = center_room_id;
    viewer.connected_graph_cache_.staircase_issues = issues;
  }
  static int ClearStaleStaircases(DungeonCanvasViewer& viewer, int room_id) {
    return viewer.ApplyConnectedStaircaseIssueAutoFixes(room_id);
  }
  static bool ConnectedGraphInvalidated(const DungeonCanvasViewer& viewer) {
    return viewer.connected_graph_cache_start_room_id_ == -1 &&
           viewer.connected_graph_cache_.staircase_issues.empty();
  }
};

namespace {
constexpr int kChestPc = 0x110000;
constexpr int kHeaderTablePc = 0x0F6000;
constexpr int kHeaderPc = 0x114000;

void WritePointer(Rom& rom, int address, int destination) {
  const auto snes = PcToSnes(destination);
  for (int byte = 0; byte < 3; ++byte) {
    rom.mutable_data()[address + byte] = (snes >> (8 * byte)) & 0xFF;
  }
}

class DungeonRoomEditsLifecycleTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    previous_flags_ = core::FeatureFlags::get().dungeon;
    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kUseWorkbench = GetParam();
    flags.kSaveObjects = false;
    flags.kSaveSprites = false;
    flags.kSaveRoomHeaders = true;
    flags.kSaveChests = true;
    flags.kSaveTorches = flags.kSavePits = flags.kSaveBlocks = false;
    flags.kSaveCollision = flags.kSaveWaterFillZones = false;
    flags.kSavePotItems = flags.kSaveEntrances = flags.kSavePalettes = false;
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    ASSERT_TRUE(
        rom_.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
            .ok());
    WritePointer(rom_, zelda3::kChestsDataPointer1, kChestPc);
    ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer, 9).ok());
    // Two room-0 entries, separated physically by an untouched room-1 entry.
    ASSERT_TRUE(
        rom_.WriteVector(kChestPc, {0, 0, 0xF1, 1, 0x80, 0x32, 0, 0, 0x24})
            .ok());
    WritePointer(rom_, zelda3::kRoomHeaderPointer, kHeaderTablePc);
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        (PcToSnes(kHeaderPc) >> 16) & 0xFF;
    for (int id = 0; id < 2; ++id) {
      ASSERT_TRUE(rom_.WriteWord(kHeaderTablePc + id * 2,
                                 PcToSnes(kHeaderPc + id * 0x20) & 0xFFFF)
                      .ok());
    }
    // Reserved bits must survive header edits and undo after saving.
    rom_.mutable_data()[kHeaderPc] = 0x02;
    rom_.mutable_data()[kHeaderPc + 8] = 0xFC;
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    for (int id = 0; id < 2; ++id) {
      auto& room = editor_->rooms()[id];
      room = zelda3::LoadRoomHeaderFromRom(&rom_, id);
      room.SetLoaded(true);
      room.SetTileObjects({});
      room.LoadChests();
      room.ClearSaveDirtyState();
    }
    room_ = &editor_->rooms()[0];
    viewer_ = DungeonRoomEditsTestPeer::Viewer(*editor_, 0);
  }
  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get().dungeon = previous_flags_;
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }
  static zelda3::RoomObject Chest(bool big = false, int layer = 0) {
    const int id = big ? 0xFB1 : 0xF99;
    zelda3::RoomObject object(id, 8, 8, zelda3::CanonicalRoomObjectSize(id, 0),
                              layer);
    object.set_options(zelda3::ObjectOption::Chest);
    return object;
  }
  void SeedChestObjects() {
    room_->SetTileObjects({Chest(), Chest()});
    room_->ClearSaveDirtyState();
  }
  TileObjectHandler& Handler() {
    return viewer_->object_interaction().entity_coordinator().tile_handler();
  }
  void FillGlobalChests(int count) {
    for (int i = 3; i < count; ++i) {
      ASSERT_TRUE(rom_.WriteVector(kChestPc + i * 3, {0xFF, 0x7F, 0xF0}).ok());
    }
    ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer, count * 3).ok());
  }
  size_t UndoDepth() const { return editor_->undo_manager().UndoStackSize(); }
  Rom rom_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  DungeonCanvasViewer* viewer_ = nullptr;
  decltype(core::FeatureFlags::get().dungeon) previous_flags_{};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_P(DungeonRoomEditsLifecycleTest, EveryMetadataFieldUsesSharedUndoHistory) {
  const std::vector<RoomMetadataEdit> edits = {
      {RoomMetadataField::kLayout, 1},
      {RoomMetadataField::kFloor1, 2},
      {RoomMetadataField::kFloor2, 3},
      {RoomMetadataField::kBlockset, 4},
      {RoomMetadataField::kPalette, 5},
      {RoomMetadataField::kSpriteset, 6},
      {RoomMetadataField::kMessage, 0xABC},
      {RoomMetadataField::kBg2, 8},
      {RoomMetadataField::kEffect, 3},
      {RoomMetadataField::kCollision, 2},
      {RoomMetadataField::kTag1, 4},
      {RoomMetadataField::kTag2, 0x3F},
      {RoomMetadataField::kHolewarp, 0xFE},
      {RoomMetadataField::kStaircaseRoom, 0x45, 3},
      {RoomMetadataField::kStaircasePlane, 3, 3}};
  std::vector<zelda3::Room::MetadataSnapshot> states{
      room_->CaptureMetadataSnapshot()};
  for (const auto& edit : edits) {
    ASSERT_TRUE(viewer_->EditRoomMetadata(0, edit).ok());
    states.push_back(room_->CaptureMetadataSnapshot());
  }
  ASSERT_EQ(UndoDepth(), edits.size());
  for (size_t count = edits.size(); count > 0; --count) {
    ASSERT_TRUE(editor_->Undo().ok());
    EXPECT_EQ(room_->CaptureMetadataSnapshot(), states[count - 1]);
  }
  for (size_t count = 1; count < states.size(); ++count) {
    ASSERT_TRUE(editor_->Redo().ok());
    EXPECT_EQ(room_->CaptureMetadataSnapshot(), states[count]);
  }
  EXPECT_FALSE(room_->chests_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_P(DungeonRoomEditsLifecycleTest,
       InvalidAndNoopEditsPreserveHistoryAndDirtyState) {
  const auto before = room_->CaptureMetadataSnapshot();
  ASSERT_TRUE(viewer_->EditRoomMetadata(0, {RoomMetadataField::kTag1, 0}).ok());
  ASSERT_TRUE(viewer_->EditChest(0, 0, 0xF1, false).ok());
  EXPECT_FALSE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kLayout, 8}).ok());
  EXPECT_FALSE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kTag1, -1}).ok());
  EXPECT_FALSE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kStaircaseRoom, 1, 4})
          .ok());
  EXPECT_FALSE(viewer_->EditChest(0, 99, 0, false).ok());
  EXPECT_FALSE(
      viewer_->EditRoomMetadata(-1, {RoomMetadataField::kTag1, 1}).ok());
  EXPECT_FALSE(
      editor_->EditRoomMetadata(8, {RoomMetadataField::kTag1, 1}).ok());
  EXPECT_FALSE(editor_->EditChest(8, 0, 1, false).ok());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), before);
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(editor_->rooms().GetIfMaterialized(8), nullptr);
}

TEST_P(DungeonRoomEditsLifecycleTest, ReadOnlyViewerRefusesBothEditDomains) {
  viewer_->SetHeaderReadOnly(true);
  EXPECT_FALSE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kTag1, 1}).ok());
  EXPECT_FALSE(viewer_->EditChest(0, 0, 1, true).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
}

TEST_P(DungeonRoomEditsLifecycleTest, DarkRoomUndoPreservesUnderlyingMode) {
  room_->SetBg2(static_cast<background2>(5));
  room_->SetBg2(background2::DarkRoom);
  const auto dark = room_->CaptureMetadataSnapshot();
  ASSERT_TRUE(viewer_->EditRoomMetadata(0, {RoomMetadataField::kBg2, 1}).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), dark);
  EXPECT_EQ(room_->layer2_mode(), 5);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->layer2_mode(), 1);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ChestUndoPreservesOtherRecordsAndDomains) {
  const auto metadata = room_->CaptureMetadataSnapshot();
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, false).ok());
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_TRUE(room_->chests_dirty());
  EXPECT_FALSE(room_->header_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  EXPECT_FALSE(room_->GetChests()[0].size);
  EXPECT_TRUE(room_->chests_dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetChests()[0].id, 9);
  EXPECT_FALSE(room_->GetChests()[0].size);
  EXPECT_EQ(room_->GetChests()[1].id, 0x24);
  EXPECT_EQ(editor_->rooms()[1].GetChests()[0].id, 0x32);
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), metadata);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       UndoAfterRoomSwitchRestoresOriginalRoomOnly) {
  ASSERT_TRUE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kStaircaseRoom, 7, 2})
          .ok());
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, false).ok());
  auto* other_viewer = DungeonRoomEditsTestPeer::Viewer(*editor_, 1);
  auto& other = editor_->rooms()[1];
  other.GetSprites().emplace_back(1, 2, 3, 0, 0);
  other_viewer->object_interaction().entity_coordinator().SelectEntity(
      EntityType::Sprite, 0);
  const auto metadata = other.CaptureMetadataSnapshot();
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  EXPECT_EQ(room_->staircase_room(2), 0);
  EXPECT_EQ(other.CaptureMetadataSnapshot(), metadata);
  EXPECT_FALSE(other.HasUnsavedChanges());
  EXPECT_EQ(other_viewer->current_room_id(), 1);
  EXPECT_EQ(other_viewer->object_interaction()
                .entity_coordinator()
                .GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(other_viewer->current_room_id(), 1);
  EXPECT_EQ(room_->GetChests()[0].id, 9);
}

TEST_P(DungeonRoomEditsLifecycleTest, MixedDomainsUndoChronologically) {
  room_->GetSprites().emplace_back(9, 2, 3, 0, 0);
  auto& coordinator = viewer_->object_interaction().entity_coordinator();
  ASSERT_TRUE(coordinator.sprite_handler().UpdateSprite(0, 1, 8, 9, 0, 0, 0));
  ASSERT_TRUE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kMessage, 0x123}).ok());
  ASSERT_TRUE(viewer_->EditChest(0, 1, 0x33, false).ok());
  ASSERT_EQ(UndoDepth(), 3u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests()[1].id, 0x24);
  EXPECT_EQ(room_->message_id(), 0x123);
  EXPECT_EQ(room_->GetSprites()[0].id(), 1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->message_id(), 0);
  EXPECT_EQ(room_->GetSprites()[0].id(), 1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].id(), 9);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       SaveReopenThenUndoPreservesReservedAndOtherRoomBytes) {
  const auto before = rom_.vector();
  ASSERT_TRUE(viewer_->EditRoomMetadata(0, {RoomMetadataField::kTag1, 2}).ok());
  ASSERT_TRUE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kStaircaseRoom, 0x7A, 2})
          .ok());
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, false).ok());
  auto saved = editor_->SaveRoom(0);
  ASSERT_TRUE(saved.ok()) << saved;
  EXPECT_FALSE(room_->HasUnsavedChanges());
  auto reopened = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  reopened.LoadChests();
  EXPECT_EQ(static_cast<int>(reopened.tag1()), 2);
  EXPECT_EQ(reopened.staircase_room(2), 0x7A);
  ASSERT_EQ(reopened.GetChests().size(), 2u);
  EXPECT_EQ(reopened.GetChests()[0].id, 9);
  EXPECT_FALSE(reopened.GetChests()[0].size);
  EXPECT_EQ(rom_.vector()[kHeaderPc] & 0x02, 0x02);
  EXPECT_EQ(rom_.vector()[kHeaderPc + 8] & 0xFC, 0xFC);
  for (size_t i = 0; i < before.size(); ++i) {
    if ((i >= kHeaderPc && i < kHeaderPc + 14) ||
        (i >= kChestPc && i < kChestPc + 3))
      continue;
    ASSERT_EQ(rom_.vector()[i], before[i]) << "Unexpected write at " << i;
  }
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  EXPECT_EQ(rom_.vector(), before);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto redone = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  redone.LoadChests();
  EXPECT_EQ(static_cast<int>(redone.tag1()), 2);
  EXPECT_EQ(redone.GetChests()[0].id, 9);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       BatchAcrossRoomsUsesOneUndoActionAndPreservesOtherDomains) {
  const auto original0 = room_->CaptureMetadataSnapshot();
  auto& other = editor_->rooms()[1];
  const auto original1 = other.CaptureMetadataSnapshot();
  ASSERT_TRUE(viewer_
                  ->EditRoomMetadataBatch(
                      {{0, {RoomMetadataField::kMessage, 0x123}},
                       {1, {RoomMetadataField::kStaircaseRoom, 0x78, 1}},
                       {0, {RoomMetadataField::kFloor2, 4}}})
                  .ok());
  const auto edited0 = room_->CaptureMetadataSnapshot();
  const auto edited1 = other.CaptureMetadataSnapshot();
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->message_id(), 0x123);
  EXPECT_EQ(room_->floor2(), 4);
  EXPECT_EQ(other.staircase_room(1), 0x78);
  EXPECT_FALSE(room_->chests_dirty());
  EXPECT_FALSE(other.chests_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
  EXPECT_FALSE(other.object_stream_header_dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), original0);
  EXPECT_EQ(other.CaptureMetadataSnapshot(), original1);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), edited0);
  EXPECT_EQ(other.CaptureMetadataSnapshot(), edited1);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       InvalidLastBatchRequestCannotPartiallyChangeEarlierRoom) {
  const auto original0 = room_->CaptureMetadataSnapshot();
  const auto original1 = editor_->rooms()[1].CaptureMetadataSnapshot();
  const auto original_rom = rom_.vector();
  const uint64_t original_revision = room_->composite_source_revision();
  for (const RoomMetadataRequest last :
       {RoomMetadataRequest{1, {RoomMetadataField::kStaircaseRoom, 2, 4}},
        RoomMetadataRequest{8, {RoomMetadataField::kTag1, 3}}}) {
    EXPECT_FALSE(
        editor_
            ->EditRoomMetadataBatch({{0, {RoomMetadataField::kTag1, 4}}, last})
            .ok());
    EXPECT_EQ(room_->CaptureMetadataSnapshot(), original0);
    EXPECT_EQ(editor_->rooms()[1].CaptureMetadataSnapshot(), original1);
    EXPECT_EQ(room_->composite_source_revision(), original_revision);
    EXPECT_FALSE(room_->HasUnsavedChanges());
    EXPECT_FALSE(editor_->rooms()[1].HasUnsavedChanges());
    EXPECT_EQ(UndoDepth(), 0u);
    EXPECT_EQ(rom_.vector(), original_rom);
  }
  EXPECT_EQ(editor_->rooms().GetIfMaterialized(8), nullptr);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       NetNoopBatchDoesNotDirtyRoomsOrReplaceRedoHistory) {
  ASSERT_TRUE(viewer_->EditRoomMetadata(0, {RoomMetadataField::kTag1, 3}).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  room_->ClearSaveDirtyState();
  room_->MarkChestsDirty();
  const uint64_t original_revision = room_->composite_source_revision();
  ASSERT_TRUE(viewer_
                  ->EditRoomMetadataBatch({{0, {RoomMetadataField::kTag1, 5}},
                                           {1, {RoomMetadataField::kFloor1, 0}},
                                           {0, {RoomMetadataField::kTag1, 0}}})
                  .ok());
  ASSERT_TRUE(viewer_->EditRoomMetadataBatch({}).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(room_->composite_source_revision(), original_revision);
  EXPECT_FALSE(room_->header_dirty());
  EXPECT_TRUE(room_->chests_dirty());
  EXPECT_FALSE(editor_->rooms()[1].HasUnsavedChanges());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(static_cast<int>(room_->tag1()), 3);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       BatchUndoAndRedoPreflightEveryRoomBeforeChangingAnyRoom) {
  auto& other = editor_->rooms()[1];
  ASSERT_TRUE(viewer_
                  ->EditRoomMetadataBatch({{0, {RoomMetadataField::kTag1, 5}},
                                           {1, {RoomMetadataField::kTag1, 6}}})
                  .ok());
  room_->ClearSaveDirtyState();
  other.ClearSaveDirtyState();
  other.SetLoaded(false);
  EXPECT_FALSE(editor_->Undo().ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(static_cast<int>(room_->tag1()), 5);
  EXPECT_EQ(static_cast<int>(other.tag1()), 6);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_FALSE(other.HasUnsavedChanges());
  other.SetLoaded(true);
  ASSERT_TRUE(editor_->Undo().ok());
  room_->ClearSaveDirtyState();
  other.ClearSaveDirtyState();
  other.SetLoaded(false);
  EXPECT_FALSE(editor_->Redo().ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(room_->tag1(), zelda3::Nothing);
  EXPECT_EQ(other.tag1(), zelda3::Nothing);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_FALSE(other.HasUnsavedChanges());
  other.SetLoaded(true);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(static_cast<int>(room_->tag1()), 5);
  EXPECT_EQ(static_cast<int>(other.tag1()), 6);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ConnectedClearStaleIsOneActionAfterEarlierPropertyEdit) {
  auto& other = editor_->rooms()[1];
  room_->SetStaircaseRoom(0, 0x12);
  room_->SetStaircaseRoom(1, 0x13);
  other.SetStaircaseRoom(2, 0x34);
  room_->ClearSaveDirtyState();
  other.ClearSaveDirtyState();
  ASSERT_TRUE(
      viewer_->EditRoomMetadata(0, {RoomMetadataField::kMessage, 0x123}).ok());
  const auto original0 = room_->CaptureMetadataSnapshot();
  const auto original1 = other.CaptureMetadataSnapshot();
  DungeonRoomEditsTestPeer::SeedStaircaseIssues(
      *viewer_, 0,
      {{0, DungeonStaircaseIssueKind::UnusedHeader, 0, 0x12},
       {0, DungeonStaircaseIssueKind::UnusedHeader, 1, 0x13},
       {1, DungeonStaircaseIssueKind::UnusedHeader, 2, 0x34},
       {1, DungeonStaircaseIssueKind::MissingDestination, 3, 0}});
  EXPECT_EQ(DungeonRoomEditsTestPeer::ClearStaleStaircases(*viewer_, 0), 3);
  EXPECT_TRUE(DungeonRoomEditsTestPeer::ConnectedGraphInvalidated(*viewer_));
  ASSERT_EQ(UndoDepth(), 2u);
  EXPECT_EQ(room_->staircase_room(0), 0);
  EXPECT_EQ(room_->staircase_room(1), 0);
  EXPECT_EQ(other.staircase_room(2), 0);
  EXPECT_EQ(room_->message_id(), 0x123);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), original0);
  EXPECT_EQ(other.CaptureMetadataSnapshot(), original1);
  EXPECT_EQ(room_->message_id(), 0x123);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->message_id(), 0);
  EXPECT_EQ(room_->staircase_room(0), 0x12);
  EXPECT_EQ(room_->staircase_room(1), 0x13);
  EXPECT_EQ(other.staircase_room(2), 0x34);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->staircase_room(0), 0);
  EXPECT_EQ(room_->staircase_room(1), 0);
  EXPECT_EQ(other.staircase_room(2), 0);
  EXPECT_EQ(room_->message_id(), 0x123);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ReadOnlyConnectedRepairCannotMutateOrCreateHistory) {
  room_->SetStaircaseRoom(0, 0x12);
  room_->ClearSaveDirtyState();
  DungeonRoomEditsTestPeer::SeedStaircaseIssues(
      *viewer_, 0, {{0, DungeonStaircaseIssueKind::UnusedHeader, 0, 0x12}});
  viewer_->SetHeaderReadOnly(true);
  EXPECT_EQ(DungeonRoomEditsTestPeer::ClearStaleStaircases(*viewer_, 0), 0);
  EXPECT_EQ(room_->staircase_room(0), 0x12);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ChestPlacementAndDeletionAreCompoundUndo) {
  SeedChestObjects();
  const auto rom_before = rom_.vector();
  ASSERT_TRUE(Handler().PlaceObjectAt(0, Chest(true), 12, 14));
  ASSERT_EQ(UndoDepth(), 1u);
  ASSERT_EQ(room_->GetTileObjects().size(), 3u);
  ASSERT_EQ(room_->GetChests().size(), 3u);
  EXPECT_EQ(room_->GetChests()[2].id, 0x34);
  EXPECT_TRUE(room_->GetChests()[2].size);
  EXPECT_TRUE(room_->object_stream_dirty());
  EXPECT_TRUE(room_->chests_dirty());
  EXPECT_EQ(rom_.vector(), rom_before);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(room_->GetChests().size(), 2u);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(viewer_->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{2}));
  ASSERT_TRUE(viewer_->DeleteChest(0, 0).ok());
  ASSERT_EQ(UndoDepth(), 2u);
  EXPECT_EQ(room_->GetChests()[0].id, 0x24);
  EXPECT_EQ(room_->GetChests()[1].id, 0x34);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 3u);
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(room_->GetChests()[0].id, 0x24);
}

TEST_P(DungeonRoomEditsLifecycleTest, ChestTypeAndRewardChangeRestoreTogether) {
  SeedChestObjects();
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, true).ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetTileObjects()[0].id_, 0xFB1);
  EXPECT_EQ(room_->GetTileObjects()[0].size_,
            zelda3::CanonicalRoomObjectSize(0xFB1, 0));
  EXPECT_EQ(room_->GetChests()[0].id, 9);
  EXPECT_TRUE(room_->GetChests()[0].size);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].id_, 0xF99);
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  EXPECT_FALSE(room_->GetChests()[0].size);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].id_, 0xFB1);
  EXPECT_TRUE(room_->GetChests()[0].size);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ReorderingAndLayerChangesKeepChestRewards) {
  SeedChestObjects();
  room_->GetTileObjects()[1].x_ = 20;
  Handler().SendToFront(0, {0});
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 20);
  EXPECT_EQ(room_->GetChests()[0].id, 0x24);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(Handler().UpdateObjectsLayer(0, {0}, 1));
  EXPECT_EQ(room_->GetChests()[0].id, 0x24);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       DuplicateAndCrossRoomPasteKeepUnknownReward) {
  SeedChestObjects();
  ASSERT_EQ(Handler().DuplicateObjects(0, {0}, 2, 2), (std::vector<size_t>{2}));
  EXPECT_EQ(room_->GetChests()[2].id, 0xF1);
  ASSERT_TRUE(editor_->Undo().ok());
  Handler().CopyObjectsToClipboard(0, {0});
  auto& other = editor_->rooms()[1];
  other.SetTileObjects({Chest(true)});
  other.ClearSaveDirtyState();
  // Reuse the originating canvas after navigation so the clipboard remains
  // available in either presentation mode, while callbacks target room 1.
  viewer_->RefreshRomBackedState(&rom_, nullptr, &editor_->rooms(), 1);
  const auto pasted = Handler().PasteFromClipboard(1, 4, 4);
  ASSERT_EQ(pasted.size(), 1u) << Handler().mutation_status();
  ASSERT_EQ(other.GetChests().size(), 2u);
  EXPECT_EQ(other.GetChests()[0].id, 0x32);
  EXPECT_EQ(other.GetChests()[1].id, 0xF1);
  EXPECT_FALSE(other.GetChests()[1].size);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       FullGlobalChestTableRejectsBeforeMutation) {
  SeedChestObjects();
  FillGlobalChests(zelda3::kChestTableCapacityRecords);
  const auto before = rom_.vector();
  viewer_->object_interaction().SetSelectedObjects({1});
  EXPECT_FALSE(Handler().PlaceObjectAt(0, Chest(), 10, 10));
  EXPECT_TRUE(absl::IsResourceExhausted(Handler().mutation_status()));
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(room_->GetChests().size(), 2u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(viewer_->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{1}));
  EXPECT_EQ(rom_.vector(), before);
}

TEST_P(DungeonRoomEditsLifecycleTest, GlobalCapacityIncludesOtherDirtyRooms) {
  SeedChestObjects();
  FillGlobalChests(zelda3::kChestTableCapacityRecords - 1);
  auto& other = editor_->rooms()[1];
  other.GetChests().push_back({0x33, false});
  other.MarkChestsDirty();
  EXPECT_FALSE(Handler().PlaceObjectAt(0, Chest(), 10, 10));
  EXPECT_TRUE(absl::IsResourceExhausted(Handler().mutation_status()));
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(other.GetChests().size(), 2u);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       MismatchedChestsRejectStructuralEditsOnly) {
  const auto before = rom_.vector();
  EXPECT_FALSE(Handler().PlaceObjectAt(0, Chest(), 10, 10));
  EXPECT_FALSE(viewer_->DeleteChest(0, 0).ok());
  EXPECT_FALSE(viewer_->EditChest(0, 0, 9, true).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(rom_.vector(), before);
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, false).ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ChestUndoOffscreenPreservesCurrentSelection) {
  SeedChestObjects();
  ASSERT_TRUE(viewer_->DeleteChest(0, 0).ok());
  auto* other_viewer = DungeonRoomEditsTestPeer::Viewer(*editor_, 1);
  auto& other = editor_->rooms()[1];
  other.SetTileObjects({Chest(true)});
  other.ClearSaveDirtyState();
  other_viewer->object_interaction().SetSelectedObjects({0});
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(other_viewer->current_room_id(), 1);
  EXPECT_EQ(other_viewer->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{0}));
  EXPECT_FALSE(other.HasUnsavedChanges());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetChests().size(), 1u);
  EXPECT_EQ(other_viewer->current_room_id(), 1);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ChestUndoFailsBeforeChangingUnavailableRoom) {
  SeedChestObjects();
  ASSERT_TRUE(viewer_->DeleteChest(0, 0).ok());
  room_->SetLoaded(false);
  EXPECT_FALSE(editor_->Undo().ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetChests().size(), 1u);
  EXPECT_EQ(room_->GetTileObjects().size(), 1u);
  room_->SetLoaded(true);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetChests().size(), 2u);
}

TEST_P(DungeonRoomEditsLifecycleTest, ChestCreateSaveReloadUndoResave) {
  SeedChestObjects();
  WritePointer(rom_, zelda3::kRoomObjectPointer, 0x0F8000);
  for (int id = 0; id < 3; ++id) {
    const int pc = 0x100000 + id * 0x100;
    WritePointer(rom_, 0x0F8000 + id * 3, pc);
    ASSERT_TRUE(rom_.WriteVector(pc, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF,
                                      0xFF, 0xFF})
                    .ok());
  }
  ASSERT_TRUE(room_->SaveObjects().ok());
  room_->ClearSaveDirtyState();
  core::FeatureFlags::get().dungeon.kSaveObjects = true;
  const auto before = rom_.vector();
  ASSERT_TRUE(Handler().PlaceObjectAt(0, Chest(true, 1), 12, 16));
  ASSERT_TRUE(viewer_->EditChest(0, 2, 0x09, true).ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto reopened = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  reopened.LoadObjects();
  ASSERT_EQ(reopened.GetTileObjects().size(), 3u);
  ASSERT_EQ(reopened.GetChests().size(), 3u);
  EXPECT_EQ(reopened.GetTileObjects()[2].id_, 0xFB1);
  EXPECT_EQ(reopened.GetChests()[2].id, 9);
  EXPECT_TRUE(reopened.GetChests()[2].size);
  EXPECT_TRUE(zelda3::ValidateChestObjectMapping(reopened.GetTileObjects(),
                                                 reopened.GetChests())
                  .ok());
  for (size_t i = 0; i < before.size(); ++i) {
    if ((i >= 0x100000 && i < 0x100100) ||
        (i >= zelda3::kDoorPointers && i < zelda3::kDoorPointers + 3) ||
        (i >= kChestPc && i < kChestPc + zelda3::kChestTableCapacityBytes) ||
        (i >= zelda3::kChestsLengthPointer &&
         i < zelda3::kChestsLengthPointer + 2))
      continue;
    ASSERT_EQ(rom_.vector()[i], before[i]) << "Unexpected write at " << i;
  }
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto undone = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  undone.LoadObjects();
  ASSERT_EQ(undone.GetTileObjects().size(), 2u);
  ASSERT_EQ(undone.GetChests().size(), 2u);
  EXPECT_EQ(undone.GetChests()[0].id, 0xF1);
  EXPECT_EQ(undone.GetChests()[1].id, 0x24);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto redone = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  redone.LoadObjects();
  ASSERT_EQ(redone.GetChests().size(), 3u);
  EXPECT_EQ(redone.GetChests()[2].id, 9);
}

TEST_P(DungeonRoomEditsLifecycleTest, ChestManifestRejectionPreservesAllState) {
  SeedChestObjects();
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version": 3,
    "protected_regions": {"total_hooks": 1, "regions": [{
      "start": "0x228000", "end": "0x2281F8", "size": 504,
      "hook_count": 1, "module": "ChestAuthoringGuard"
    }]}
  })json")
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  viewer_->object_interaction().SetSelectedObjects({1});
  const auto before = rom_.vector();
  EXPECT_FALSE(Handler().PlaceObjectAt(0, Chest(), 12, 12));
  EXPECT_FALSE(viewer_->DeleteChest(0, 0).ok());
  EXPECT_FALSE(viewer_->EditChest(0, 0, 9, true).ok());
  EXPECT_FALSE(viewer_->EditChest(0, 0, 9, false).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  EXPECT_EQ(viewer_->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{1}));
  EXPECT_EQ(rom_.vector(), before);
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonRoomEditsLifecycleTest,
       ChestCommandFinishesDragBeforeCompoundUndo) {
  SeedChestObjects();
  auto& interaction = viewer_->object_interaction();
  interaction.SetSelectedObjects({0});
  interaction.mode_manager().SetMode(InteractionMode::DraggingObjects);
  Handler().InitDrag(ImVec2(64, 64));
  Handler().HandleDrag(ImVec2(80, 64), ImVec2(16, 0));
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 10);
  EXPECT_EQ(UndoDepth(), 0u);
  ASSERT_TRUE(viewer_->EditChest(0, 0, 9, true).ok());
  EXPECT_EQ(UndoDepth(), 2u);
  Handler().HandleDrag(ImVec2(96, 64), ImVec2(16, 0));
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 10);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].id_, 0xF99);
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 10);
  EXPECT_EQ(room_->GetChests()[0].id, 0xF1);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 8);
}

TEST_P(DungeonRoomEditsLifecycleTest, ChestPlacementFinishesEarlierDrag) {
  SeedChestObjects();
  auto& interaction = viewer_->object_interaction();
  interaction.SetSelectedObjects({0});
  interaction.mode_manager().SetMode(InteractionMode::DraggingObjects);
  Handler().InitDrag(ImVec2(64, 64));
  Handler().HandleDrag(ImVec2(80, 64), ImVec2(16, 0));
  ASSERT_TRUE(Handler().PlaceObjectAt(0, Chest(true), 20, 20));
  EXPECT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 2u);
  EXPECT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 10);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 8);
}

TEST_P(DungeonRoomEditsLifecycleTest, RejectedChestPlacementKeepsPendingDrag) {
  SeedChestObjects();
  FillGlobalChests(zelda3::kChestTableCapacityRecords);
  auto& interaction = viewer_->object_interaction();
  interaction.SetSelectedObjects({0});
  interaction.mode_manager().SetMode(InteractionMode::DraggingObjects);
  Handler().InitDrag(ImVec2(64, 64));
  Handler().HandleDrag(ImVec2(80, 64), ImVec2(16, 0));
  EXPECT_FALSE(Handler().PlaceObjectAt(0, Chest(), 20, 20));
  EXPECT_EQ(UndoDepth(), 0u);
  Handler().HandleDrag(ImVec2(96, 64), ImVec2(16, 0));
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 12);
  interaction.HandleMouseRelease();
  EXPECT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 8);
  EXPECT_EQ(room_->GetChests().size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(ViewerModes, DungeonRoomEditsLifecycleTest,
                         ::testing::Bool());
}  // namespace
}  // namespace yaze::editor
