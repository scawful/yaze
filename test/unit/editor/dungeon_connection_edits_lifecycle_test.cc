#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

#include "app/editor/dungeon/dungeon_connection_edit.h"
#include "app/editor/dungeon/dungeon_selection_edit.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

class DungeonConnectionEditsTestPeer {
 public:
  static void ReloadWaterFillZones(DungeonEditorV2& editor) {
    editor.ReloadWaterFillZones();
  }
  static DungeonCanvasViewer* ExistingViewer(DungeonEditorV2& editor,
                                             int room_id) {
    return editor.GetViewerForRoom(room_id);
  }
  static int CurrentRoom(const DungeonEditorV2& editor) {
    return editor.current_room_id_;
  }
  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    editor.current_room_id_ = room_id;
    auto* viewer = editor.GetViewerForRoom(room_id);
    viewer->RefreshRomBackedState(editor.rom_, nullptr, &editor.rooms_,
                                  room_id);
    return viewer;
  }
};

namespace {
constexpr int kChestPc = 0x110000;
constexpr int kHeaderTablePc = 0x0F6000;
constexpr int kHeaderPc = 0x114000;
constexpr int kObjectTablePc = 0x0F8000;
constexpr int kObjectPc = 0x100000;
constexpr int kSpriteTablePc = 0x04D000;
constexpr int kSpritePc = 0x04D900;
constexpr int kPotPc = 0x008000;

void WritePointer(Rom& rom, int address, int destination) {
  const auto snes = PcToSnes(destination);
  for (int byte = 0; byte < 3; ++byte) {
    rom.mutable_data()[address + byte] = (snes >> (8 * byte)) & 0xFF;
  }
}

auto DirtyFields(const zelda3::Room& room) {
  const auto dirty = room.CaptureSaveDirtySnapshot();
  return std::tuple{
      dirty.header,    dirty.object_stream, dirty.object_stream_header,
      dirty.sprites,   dirty.chests,        dirty.pot_items,
      dirty.torches,   dirty.blocks,        dirty.custom_collision,
      dirty.water_fill};
}

zelda3::Room::Door Door(int slot, zelda3::DoorDirection direction,
                        zelda3::DoorType type = zelda3::DoorType::NormalDoor) {
  return zelda3::Room::Door::FromRomBytes(
      static_cast<uint8_t>((slot << 4) | static_cast<int>(direction)),
      static_cast<uint8_t>(type));
}

class DungeonConnectionEditsLifecycleTest
    : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    previous_flags_ = core::FeatureFlags::get().dungeon;
    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kUseWorkbench = GetParam();
    flags.kSaveObjects = true;
    flags.kSaveSprites = flags.kSaveChests = flags.kSavePotItems = false;
    flags.kSaveRoomHeaders = false;
    flags.kSaveTorches = flags.kSavePits = flags.kSaveBlocks = false;
    flags.kSaveCollision = flags.kSaveWaterFillZones = false;
    flags.kSaveEntrances = flags.kSavePalettes = false;
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    ASSERT_TRUE(
        rom_.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
            .ok());
    WritePointer(rom_, zelda3::kChestsDataPointer1, kChestPc);
    ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer, 9).ok());
    ASSERT_TRUE(
        rom_.WriteVector(kChestPc, {0, 0, 0xF1, 1, 0, 0x24, 2, 0, 9}).ok());
    WritePointer(rom_, zelda3::kRoomHeaderPointer, kHeaderTablePc);
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        (PcToSnes(kHeaderPc) >> 16) & 0xFF;
    WritePointer(rom_, zelda3::kRoomObjectPointer, kObjectTablePc);
    ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomsSpritePointer,
                               PcToSnes(kSpriteTablePc) & 0xFFFF)
                    .ok());
    for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
      const int offset = std::min(id, 3) * 0x100;
      WritePointer(rom_, kObjectTablePc + id * 3, kObjectPc + offset);
      ASSERT_TRUE(
          rom_.WriteWord(kSpriteTablePc + id * 2, PcToSnes(kSpritePc) & 0xFFFF)
              .ok());
      ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomItemsPointers + id * 2,
                                 PcToSnes(kPotPc) & 0xFFFF)
                      .ok());
      ASSERT_TRUE(
          rom_.WriteWord(kHeaderTablePc + id * 2,
                         PcToSnes(kHeaderPc + std::min(id, 3) * 0x20) & 0xFFFF)
              .ok());
    }
    for (int id = 0; id < 4; ++id) {
      ASSERT_TRUE(rom_.WriteVector(kObjectPc + id * 0x100,
                                   {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF,
                                    0xFF, 0xFF})
                      .ok());
    }
    ASSERT_TRUE(rom_.WriteVector(kSpritePc, {0, 0xFF}).ok());
    ASSERT_TRUE(rom_.WriteWord(kPotPc, 0xFFFF).ok());
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    for (int id = 0; id < 3; ++id) {
      auto& room = editor_->rooms()[id];
      room = zelda3::LoadRoomHeaderFromRom(&rom_, id);
      room.SetLoaded(true);
      zelda3::RoomObject chest(0xF99, 12, 12,
                               zelda3::CanonicalRoomObjectSize(0xF99, 0), 1);
      chest.set_options(zelda3::ObjectOption::Chest);
      room.SetTileObjects({zelda3::RoomObject(1, 8, 8, 0, 0), chest});
      room.LoadChests();
      room.GetSprites().emplace_back(9, 6, 8, 3, 1);
      room.GetSprites().back().set_key_drop(2);
      room.GetPotItems().push_back({0x0A20, 6});
      room.ClearSaveDirtyState();
    }
    Source().GetDoors().push_back(Door(6, zelda3::DoorDirection::East));
    // An unrelated door preserves its type and original byte provenance.
    Target().GetDoors().push_back(
        Door(1, zelda3::DoorDirection::North, zelda3::DoorType::SmallKeyDoor));
    viewer_ = DungeonConnectionEditsTestPeer::Viewer(*editor_, 0);
  }

  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get().dungeon = previous_flags_;
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  zelda3::Room& Source() { return editor_->rooms()[0]; }
  zelda3::Room& Target() { return editor_->rooms()[1]; }
  size_t UndoDepth() const { return editor_->undo_manager().UndoStackSize(); }
  size_t RedoDepth() const { return editor_->undo_manager().RedoStackSize(); }
  DungeonConnectionRequest Request(
      DungeonConnectionLayer layer = DungeonConnectionLayer::kLower) {
    return {0, 0, layer};
  }

  void ExpectDoor(const zelda3::Room::Door& door, int slot,
                  zelda3::DoorDirection direction,
                  zelda3::DoorType type) const {
    EXPECT_EQ(door.position, slot);
    EXPECT_EQ(door.direction, direction);
    EXPECT_EQ(door.type, type);
  }

  void ExpectRejectedWithoutChanges(const std::function<absl::Status()>& edit) {
    const auto before_source = CaptureDungeonSelectionEditState(Source());
    const auto before_target = CaptureDungeonSelectionEditState(Target());
    const auto source_metadata = Source().CaptureMetadataSnapshot();
    const auto target_metadata = Target().CaptureMetadataSnapshot();
    const auto source_dirty = DirtyFields(Source());
    const auto target_dirty = DirtyFields(Target());
    const auto rom_bytes = rom_.vector();
    const auto undo = UndoDepth();
    const auto redo = RedoDepth();
    EXPECT_FALSE(edit().ok());
    EXPECT_EQ(ChangedDungeonSelectionDomains(
                  before_source, CaptureDungeonSelectionEditState(Source())),
              0);
    EXPECT_EQ(ChangedDungeonSelectionDomains(
                  before_target, CaptureDungeonSelectionEditState(Target())),
              0);
    EXPECT_EQ(Source().CaptureMetadataSnapshot(), source_metadata);
    EXPECT_EQ(Target().CaptureMetadataSnapshot(), target_metadata);
    EXPECT_EQ(DirtyFields(Source()), source_dirty);
    EXPECT_EQ(DirtyFields(Target()), target_dirty);
    EXPECT_EQ(rom_.vector(), rom_bytes);
    EXPECT_EQ(UndoDepth(), undo);
    EXPECT_EQ(RedoDepth(), redo);
  }

  Rom rom_;
  std::unique_ptr<DungeonEditorV2> editor_;
  DungeonCanvasViewer* viewer_ = nullptr;
  decltype(core::FeatureFlags::get().dungeon) previous_flags_{};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_P(DungeonConnectionEditsLifecycleTest, PreviewDoesNotChangeDataOrHistory) {
  const auto source = CaptureDungeonSelectionEditState(Source());
  const auto target = CaptureDungeonSelectionEditState(Target());
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->target_room_id, 1);
  EXPECT_TRUE(plan->creates_return);
  EXPECT_TRUE(plan->changed());
  EXPECT_EQ(plan->target_door_index, 1u);
  ExpectDoor(plan->source_after[0], 6, zelda3::DoorDirection::East,
             zelda3::DoorType::NormalDoorLower);
  ExpectDoor(plan->target_after[1], 0, zelda3::DoorDirection::West,
             zelda3::DoorType::NormalDoorLower);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                source, CaptureDungeonSelectionEditState(Source())),
            0);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                target, CaptureDungeonSelectionEditState(Target())),
            0);
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_FALSE(Target().HasUnsavedChanges());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(rom_.vector(), bytes);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       BothEndpointsChangeTogetherAndUndoPreservesOtherDomains) {
  const auto source = CaptureDungeonSelectionEditState(Source());
  const auto target = CaptureDungeonSelectionEditState(Target());
  const auto metadata0 = Source().CaptureMetadataSnapshot();
  const auto metadata1 = Target().CaptureMetadataSnapshot();
  const auto third = CaptureDungeonSelectionEditState(editor_->rooms()[2]);
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_TRUE(Source().object_stream_dirty());
  EXPECT_TRUE(Target().object_stream_dirty());
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                source, CaptureDungeonSelectionEditState(Source())),
            kSelectionDoors);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                target, CaptureDungeonSelectionEditState(Target())),
            kSelectionDoors);
  for (int id : {0, 1}) {
    const auto dirty = editor_->rooms()[id].CaptureSaveDirtySnapshot();
    EXPECT_FALSE(dirty.header || dirty.object_stream_header || dirty.chests ||
                 dirty.sprites || dirty.pot_items || dirty.torches ||
                 dirty.blocks || dirty.custom_collision || dirty.water_fill);
  }
  ASSERT_EQ(Target().GetDoors().size(), 2u);
  ExpectDoor(Target().GetDoors()[1], 0, zelda3::DoorDirection::West,
             zelda3::DoorType::NormalDoorLower);
  EXPECT_EQ(rom_.vector(), bytes);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                source, CaptureDungeonSelectionEditState(Source())),
            0);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                target, CaptureDungeonSelectionEditState(Target())),
            0);
  EXPECT_EQ(Source().CaptureMetadataSnapshot(), metadata0);
  EXPECT_EQ(Target().CaptureMetadataSnapshot(), metadata1);
  EXPECT_EQ(ChangedDungeonSelectionDomains(
                third, CaptureDungeonSelectionEditState(editor_->rooms()[2])),
            0);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Source().GetDoors()[0].type, zelda3::DoorType::NormalDoorLower);
  EXPECT_EQ(Target().GetDoors().size(), 2u);
  EXPECT_FALSE(editor_->rooms()[2].HasUnsavedChanges());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       ReturnOnlyCreationDirtiesOnlyTarget) {
  const auto plan =
      editor_->PreviewDoorConnection(Request(DungeonConnectionLayer::kUpper));
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_TRUE(Target().object_stream_dirty());
  EXPECT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_EQ(Target().GetDoors().size(), 1u);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       ExistingPairIsNoopAndPreservesRedoHistory) {
  Target().GetDoors().push_back(Door(0, zelda3::DoorDirection::West));
  const auto lower = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(lower.ok()) << lower.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*lower).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  Source().ClearSaveDirtyState();
  Target().ClearSaveDirtyState();
  const auto upper =
      editor_->PreviewDoorConnection(Request(DungeonConnectionLayer::kUpper));
  ASSERT_TRUE(upper.ok()) << upper.status();
  EXPECT_FALSE(upper->changed());
  ASSERT_TRUE(editor_->ApplyDoorConnection(*upper).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(RedoDepth(), 1u);
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_FALSE(Target().HasUnsavedChanges());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Source().GetDoors()[0].type, zelda3::DoorType::NormalDoorLower);
  EXPECT_EQ(Target().GetDoors()[1].type, zelda3::DoorType::NormalDoorLower);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       StaleSourceAndTargetPreviewsRejectWithoutPartialPublication) {
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  Source().GetDoors()[0].position = 7;
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyDoorConnection(*plan); });
  Source().GetDoors()[0] = Door(6, zelda3::DoorDirection::East);
  Target().GetDoors()[0].type = zelda3::DoorType::BigKeyDoor;
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyDoorConnection(*plan); });
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       OccupiedReturnAndTargetCapacityRejectBeforeChangingSource) {
  Target().GetDoors().push_back(
      Door(0, zelda3::DoorDirection::West, zelda3::DoorType::BigKeyDoor));
  ExpectRejectedWithoutChanges(
      [&] { return editor_->PreviewDoorConnection(Request()).status(); });
  Target().GetDoors().clear();
  for (int i = 0; i < 16; ++i) {
    Target().GetDoors().push_back(Door(i % 6, zelda3::DoorDirection::North));
  }
  ExpectRejectedWithoutChanges(
      [&] { return editor_->PreviewDoorConnection(Request()).status(); });
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       ForeignRoomIdentityRejectsApplyAndUndoBeforeEitherEndpointChanges) {
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  Rom foreign_rom;
  ASSERT_TRUE(foreign_rom.LoadFromData(rom_.vector()).ok());
  Target().SetRom(&foreign_rom);
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyDoorConnection(*plan); });
  Target().SetRom(&rom_);
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  Target().SetRom(&foreign_rom);
  ExpectRejectedWithoutChanges([&] { return editor_->Undo(); });
  Target().SetRom(&rom_);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Source().GetDoors()[0].type, zelda3::DoorType::NormalDoor);
  EXPECT_EQ(Target().GetDoors().size(), 1u);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       PreviewLoadsUnopenedDestinationWithoutCreatingAnEdit) {
  // Persist target data, then leave only its header materialized, as startup
  // does before the room has ever been opened on a canvas.
  Target().MarkObjectStreamDirty();
  ASSERT_TRUE(Target().SaveObjects().ok());
  const auto expected = Target().EncodeObjects();
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  EXPECT_FALSE(Target().AreObjectsLoaded());
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(Target().IsLoaded());
  EXPECT_TRUE(Target().AreObjectsLoaded());
  EXPECT_EQ(Target().EncodeObjects(), expected);
  EXPECT_FALSE(Target().HasUnsavedChanges());
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(rom_.vector(), bytes);
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  EXPECT_EQ(Target().GetDoors().size(), 2u);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       PartiallyLoadedDirtyDestinationIsNotOverwrittenByPreview) {
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  Target().set_floor1(3);
  ASSERT_FALSE(Target().AreObjectsLoaded());
  ASSERT_TRUE(Target().HasUnsavedChanges());
  ExpectRejectedWithoutChanges(
      [&] { return editor_->PreviewDoorConnection(Request()).status(); });
  EXPECT_EQ(Target().floor1(), 3);
  EXPECT_FALSE(Target().AreObjectsLoaded());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       PreviewPreservesCleanWaterFillOverlayOnUnopenedDestination) {
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  ASSERT_TRUE(zelda3::WriteWaterFillTable(&rom_, {{1, 0x02, {131, 263}}}).ok());
  DungeonConnectionEditsTestPeer::ReloadWaterFillZones(*editor_);
  ASSERT_FALSE(Target().AreObjectsLoaded());
  ASSERT_FALSE(Target().water_fill_dirty());
  ASSERT_EQ(Target().WaterFillTileCount(), 2);
  const auto tiles = Target().water_fill_zone().tiles;
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(Target().AreObjectsLoaded());
  EXPECT_TRUE(Target().has_water_fill_zone());
  EXPECT_EQ(Target().water_fill_zone().tiles, tiles);
  EXPECT_EQ(Target().WaterFillTileCount(), 2);
  EXPECT_EQ(Target().water_fill_sram_bit_mask(), 0x02);
  EXPECT_FALSE(Target().HasUnsavedChanges());
  EXPECT_EQ(UndoDepth(), 0);
  EXPECT_EQ(rom_.vector(), bytes);

  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Target().water_fill_zone().tiles, tiles);
  EXPECT_FALSE(Target().water_fill_dirty());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       SavingAnotherZoneAfterPreviewRetainsUnopenedDestinationsZone) {
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  ASSERT_TRUE(zelda3::WriteWaterFillTable(&rom_, {{1, 0x02, {131, 263}}}).ok());
  DungeonConnectionEditsTestPeer::ReloadWaterFillZones(*editor_);
  ASSERT_TRUE(editor_->PreviewDoorConnection(Request()).ok());
  auto& other = editor_->rooms()[2];
  other.SetWaterFillTile(9, 7, true);
  other.set_water_fill_sram_bit_mask(0x04);
  core::FeatureFlags::get().dungeon.kSaveWaterFillZones = true;
  ASSERT_TRUE(editor_->SaveRoom(2).ok());
  const auto zones = zelda3::LoadWaterFillTable(&rom_);
  ASSERT_TRUE(zones.ok()) << zones.status();
  ASSERT_EQ(zones->size(), 2);
  const auto target_zone =
      std::find_if(zones->begin(), zones->end(),
                   [](const auto& zone) { return zone.room_id == 1; });
  ASSERT_NE(target_zone, zones->end());
  EXPECT_EQ(target_zone->sram_bit_mask, 0x02);
  EXPECT_EQ(target_zone->fill_offsets, (std::vector<uint16_t>{131, 263}));
  EXPECT_FALSE(Target().water_fill_dirty());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       DirtyPartialWaterFillOverlayRejectsPreviewWithoutReplacement) {
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  Target().SetWaterFillTile(3, 2, true);
  Target().set_water_fill_sram_bit_mask(0x02);
  ExpectRejectedWithoutChanges(
      [&] { return editor_->PreviewDoorConnection(Request()).status(); });
  EXPECT_TRUE(Target().GetWaterFillTile(3, 2));
  EXPECT_EQ(Target().WaterFillTileCount(), 1);
  EXPECT_TRUE(Target().water_fill_dirty());
  EXPECT_FALSE(Target().AreObjectsLoaded());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       MalformedUnopenedDestinationRejectsWithoutAnEdit) {
  ASSERT_TRUE(
      rom_.WriteVector(kObjectPc + 0x100, std::vector<uint8_t>(0x100, 0)).ok());
  Target() = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  ExpectRejectedWithoutChanges(
      [&] { return editor_->PreviewDoorConnection(Request()).status(); });
  EXPECT_FALSE(Target().AreObjectsLoaded());
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       PreviewFromAnotherRomCannotBeAppliedToIdenticalDoorLists) {
  Rom foreign_rom;
  ASSERT_TRUE(foreign_rom.LoadFromData(rom_.vector()).ok());
  DungeonEditorV2 foreign_editor(&foreign_rom);
  for (int id : {0, 1}) {
    auto& room = foreign_editor.rooms()[id];
    room = zelda3::LoadRoomHeaderFromRom(&foreign_rom, id);
    room.SetLoaded(true);
    room.SetTileObjects({});
    room.GetDoors() = editor_->rooms()[id].GetDoors();
    room.ClearSaveDirtyState();
  }
  const auto plan = foreign_editor.PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyDoorConnection(*plan); });
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       AcceptedConnectionFinishesPendingMixedDragAsSeparateHistory) {
  auto& interaction = viewer_->object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  interaction.SetSelectedObjects({0});
  coordinator.SetSelectedEntities({{EntityType::Sprite, 0}});
  coordinator.BeginSelectionDrag(ImVec2(64, 64));
  coordinator.HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  ASSERT_EQ(UndoDepth(), 0u);
  ASSERT_EQ(Source().GetTileObjects()[0].x_, 10);
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  EXPECT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Source().GetTileObjects()[0].x_, 10);
  EXPECT_EQ(Source().GetSprites()[0].x(), 7);
  EXPECT_EQ(Target().GetDoors().size(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Source().GetTileObjects()[0].x_, 8);
  EXPECT_EQ(Source().GetSprites()[0].x(), 6);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       RejectedPreviewDoesNotFinishOrDiscardPendingDrag) {
  auto& interaction = viewer_->object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  interaction.SetSelectedObjects({0});
  coordinator.SetSelectedEntities({{EntityType::Sprite, 0}});
  coordinator.BeginSelectionDrag(ImVec2(64, 64));
  coordinator.HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  Target().GetDoors().push_back(Door(2, zelda3::DoorDirection::North));
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyDoorConnection(*plan); });
  coordinator.HandleDrag(ImVec2(96, 96), ImVec2(16, 16));
  EXPECT_EQ(Source().GetTileObjects()[0].x_, 12);
  EXPECT_EQ(UndoDepth(), 0u);
  coordinator.HandleRelease();
  EXPECT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Source().GetTileObjects()[0].x_, 8);
  EXPECT_EQ(Target().GetDoors().size(), 2u);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       OffscreenUndoDoesNotNavigateOrReplaceCurrentSelection) {
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  viewer_->RefreshRomBackedState(&rom_, nullptr, &editor_->rooms(), 2);
  auto& coordinator = viewer_->object_interaction().entity_coordinator();
  coordinator.SetSelectedEntities({{EntityType::Sprite, 0}});
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(viewer_->current_room_id(), 2);
  EXPECT_EQ(coordinator.GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  EXPECT_FALSE(editor_->rooms()[2].HasUnsavedChanges());
  EXPECT_EQ(Source().GetDoors()[0].type, zelda3::DoorType::NormalDoor);
  EXPECT_EQ(Target().GetDoors().size(), 1u);
}

TEST_P(DungeonConnectionEditsLifecycleTest,
       SaveReloadUndoResavePreservesOtherRoomAndAllOtherRomBytes) {
  for (int id : {0, 1}) {
    editor_->rooms()[id].MarkObjectStreamDirty();
    ASSERT_TRUE(editor_->SaveRoom(id).ok());
  }
  const auto source_before = Source().EncodeObjects();
  const auto target_before = Target().EncodeObjects();
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewDoorConnection(Request());
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyDoorConnection(*plan).ok());
  for (int id : {0, 1}) {
    ASSERT_TRUE(editor_->SaveRoom(id).ok());
    auto reloaded = zelda3::LoadRoomHeaderFromRom(&rom_, id);
    reloaded.LoadObjects();
    EXPECT_EQ(reloaded.EncodeObjects(), editor_->rooms()[id].EncodeObjects());
  }
  for (size_t address = 0; address < bytes.size(); ++address) {
    if ((address >= kObjectPc && address < kObjectPc + 0x200) ||
        (address >= zelda3::kDoorPointers &&
         address < zelda3::kDoorPointers + 6)) {
      continue;
    }
    ASSERT_EQ(rom_.vector()[address], bytes[address])
        << "Unexpected write at " << address;
  }
  ASSERT_TRUE(editor_->Undo().ok());
  for (int id : {0, 1}) {
    ASSERT_TRUE(editor_->SaveRoom(id).ok());
    auto reloaded = zelda3::LoadRoomHeaderFromRom(&rom_, id);
    reloaded.LoadObjects();
    EXPECT_EQ(reloaded.EncodeObjects(),
              id == 0 ? source_before : target_before);
  }
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  ASSERT_TRUE(editor_->SaveRoom(1).ok());
  auto reloaded = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  reloaded.LoadObjects();
  ASSERT_EQ(reloaded.GetDoors().size(), 2u);
  ExpectDoor(reloaded.GetDoors()[1], 0, zelda3::DoorDirection::West,
             zelda3::DoorType::NormalDoorLower);
}

TEST_P(DungeonConnectionEditsLifecycleTest, OpenTargetSelectsExactReturnDoor) {
  const auto preview = viewer_->PreviewDoorConnection(Request());
  ASSERT_TRUE(preview.ok()) << preview.status();
  ASSERT_TRUE(viewer_->ApplyDoorConnection(*preview).ok());
  ASSERT_TRUE(
      viewer_->NavigateToDoorConnectionTarget(1, preview->target_door_index));
  EXPECT_EQ(DungeonConnectionEditsTestPeer::CurrentRoom(*editor_), 1);
  auto* target = DungeonConnectionEditsTestPeer::ExistingViewer(*editor_, 1);
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->object_interaction()
                .entity_coordinator()
                .tile_handler()
                .context()
                ->current_room_id,
            1);
  EXPECT_EQ(target->object_interaction().GetSelectedEntity(),
            (SelectedEntity{EntityType::Door, preview->target_door_index}));
  EXPECT_EQ(UndoDepth(), 1u);
}

INSTANTIATE_TEST_SUITE_P(ViewerModes, DungeonConnectionEditsLifecycleTest,
                         ::testing::Bool(), [](const auto& param) {
                           return param.param ? "Workbench" : "Standalone";
                         });
}  // namespace
}  // namespace yaze::editor
