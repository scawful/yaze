#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <memory>
#include <tuple>
#include <vector>

#include "app/editor/dungeon/dungeon_selection_edit.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_dimensions.h"

namespace yaze::editor {

class DungeonSelectionEditsTestPeer {
 public:
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

// Compare authored data independently of renderer caches and preview pointers.
void ExpectRoomData(const zelda3::Room& room,
                    const DungeonSelectionEditState& expected) {
  ASSERT_EQ(room.GetTileObjects().size(), expected.objects.size());
  for (size_t i = 0; i < expected.objects.size(); ++i) {
    const auto& actual = room.GetTileObjects()[i];
    const auto& object = expected.objects[i];
    EXPECT_EQ(actual.id_, object.id_);
    EXPECT_EQ(actual.x_, object.x_);
    EXPECT_EQ(actual.y_, object.y_);
    EXPECT_EQ(actual.size_, object.size_);
    EXPECT_EQ(actual.layer_, object.layer_);
  }
  ASSERT_EQ(room.GetChests().size(), expected.chests.size());
  for (size_t i = 0; i < expected.chests.size(); ++i) {
    EXPECT_EQ(room.GetChests()[i].id, expected.chests[i].id);
    EXPECT_EQ(room.GetChests()[i].size, expected.chests[i].size);
  }
  ASSERT_EQ(room.GetDoors().size(), expected.doors.size());
  for (size_t i = 0; i < expected.doors.size(); ++i) {
    EXPECT_EQ(room.GetDoors()[i].EncodeBytes(),
              expected.doors[i].EncodeBytes());
    EXPECT_EQ(room.GetDoors()[i].byte1, expected.doors[i].byte1);
    EXPECT_EQ(room.GetDoors()[i].byte2, expected.doors[i].byte2);
  }
  ASSERT_EQ(room.GetSprites().size(), expected.sprites.size());
  for (size_t i = 0; i < expected.sprites.size(); ++i) {
    const auto& actual = room.GetSprites()[i];
    const auto& sprite = expected.sprites[i];
    EXPECT_EQ(actual.id(), sprite.id);
    EXPECT_EQ(actual.x(), sprite.x);
    EXPECT_EQ(actual.y(), sprite.y);
    EXPECT_EQ(actual.subtype(), sprite.subtype);
    EXPECT_EQ(actual.layer(), sprite.layer);
    EXPECT_EQ(actual.key_drop(), sprite.key_drop);
    EXPECT_EQ(actual.deleted(), sprite.deleted);
  }
  ASSERT_EQ(room.GetPotItems().size(), expected.items.size());
  for (size_t i = 0; i < expected.items.size(); ++i) {
    EXPECT_EQ(room.GetPotItems()[i].position, expected.items[i].position);
    EXPECT_EQ(room.GetPotItems()[i].item, expected.items[i].item);
  }
}

class DungeonSelectionEditsLifecycleTest
    : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    previous_flags_ = core::FeatureFlags::get().dungeon;
    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kUseWorkbench = GetParam();
    flags.kSaveObjects = flags.kSaveSprites = flags.kSaveChests = true;
    flags.kSavePotItems = true;
    flags.kSaveRoomHeaders = false;
    flags.kSaveTorches = flags.kSavePits = flags.kSaveBlocks = false;
    flags.kSaveCollision = flags.kSaveWaterFillZones = false;
    flags.kSaveEntrances = flags.kSavePalettes = false;
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    ASSERT_TRUE(
        rom_.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
            .ok());
    WritePointer(rom_, zelda3::kChestsDataPointer1, kChestPc);
    ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer, 3).ok());
    ASSERT_TRUE(rom_.WriteVector(kChestPc, {0, 0, 0xF1}).ok());
    WritePointer(rom_, zelda3::kRoomHeaderPointer, kHeaderTablePc);
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        (PcToSnes(kHeaderPc) >> 16) & 0xFF;
    WritePointer(rom_, zelda3::kRoomObjectPointer, 0x0F8000);
    ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomsSpritePointer,
                               PcToSnes(kSpriteTablePc) & 0xFFFF)
                    .ok());
    for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
      const int offset = id == 0 ? 0 : 0x100;
      WritePointer(rom_, 0x0F8000 + id * 3, kObjectPc + offset);
      ASSERT_TRUE(rom_.WriteWord(kSpriteTablePc + id * 2,
                                 PcToSnes(kSpritePc + offset) & 0xFFFF)
                      .ok());
      ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomItemsPointers + id * 2,
                                 PcToSnes(kPotPc + (id ? 0x40 : 0)) & 0xFFFF)
                      .ok());
    }
    for (int offset : {0, 0x100}) {
      ASSERT_TRUE(
          rom_.WriteVector(kObjectPc + offset, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
                                                0xF0, 0xFF, 0xFF, 0xFF})
              .ok());
      ASSERT_TRUE(rom_.WriteVector(kSpritePc + offset, {0, 0xFF}).ok());
    }
    ASSERT_TRUE(rom_.WriteWord(kPotPc, 0xFFFF).ok());
    ASSERT_TRUE(rom_.WriteWord(kPotPc + 0x40, 0xFFFF).ok());
    for (int id = 0; id < 2; ++id) {
      ASSERT_TRUE(rom_.WriteWord(kHeaderTablePc + id * 2,
                                 PcToSnes(kHeaderPc + id * 0x20) & 0xFFFF)
                      .ok());
    }
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    for (int id = 0; id < 2; ++id) {
      auto& room = editor_->rooms()[id];
      room = zelda3::LoadRoomHeaderFromRom(&rom_, id);
      room.SetLoaded(true);
      room.SetTileObjects({});
      room.LoadChests();
      room.LoadSprites();
      room.LoadPotItems();
      room.ClearSaveDirtyState();
    }
    room_ = &editor_->rooms()[0];
    zelda3::RoomObject chest(0xF99, 12, 12,
                             zelda3::CanonicalRoomObjectSize(0xF99, 0), 1);
    chest.set_options(zelda3::ObjectOption::Chest);
    room_->SetTileObjects({zelda3::RoomObject(0x01, 8, 8, 0, 0), chest});
    room_->GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0, 0));
    room_->GetSprites().emplace_back(9, 6, 8, 3, 1);
    room_->GetSprites().back().set_key_drop(2);
    room_->GetPotItems().push_back({0x0A20, 6});
    room_->ClearSaveDirtyState();
    viewer_ = DungeonSelectionEditsTestPeer::Viewer(*editor_, 0);
  }

  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get().dungeon = previous_flags_;
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  DungeonObjectInteraction& interaction() {
    return viewer_->object_interaction();
  }
  InteractionCoordinator& coordinator() {
    return interaction().entity_coordinator();
  }
  void SelectMixed(bool include_door = false) {
    interaction().SetSelectedObjects({0, 1});
    std::vector<SelectedEntity> entities{{EntityType::Sprite, 0},
                                         {EntityType::Item, 0}};
    if (include_door)
      entities.push_back({EntityType::Door, 0});
    coordinator().SetSelectedEntities(std::move(entities));
  }
  size_t UndoDepth() const { return editor_->undo_manager().UndoStackSize(); }
  size_t RedoDepth() const { return editor_->undo_manager().RedoStackSize(); }

  void ExpectRejectedWithoutChanges(const std::function<absl::Status()>& edit) {
    const auto data = CaptureDungeonSelectionEditState(*room_);
    const auto metadata = room_->CaptureMetadataSnapshot();
    const auto dirty = DirtyFields(*room_);
    const auto objects = interaction().GetSelectedObjectIndices();
    const auto entities = coordinator().SelectedEntitiesForEdit();
    const auto bytes = rom_.vector();
    const auto undo = UndoDepth();
    const auto redo = RedoDepth();
    EXPECT_FALSE(edit().ok());
    ExpectRoomData(*room_, data);
    EXPECT_EQ(room_->CaptureMetadataSnapshot(), metadata);
    EXPECT_EQ(DirtyFields(*room_), dirty);
    EXPECT_EQ(interaction().GetSelectedObjectIndices(), objects);
    EXPECT_EQ(coordinator().SelectedEntitiesForEdit(), entities);
    EXPECT_EQ(UndoDepth(), undo);
    EXPECT_EQ(RedoDepth(), redo);
    EXPECT_EQ(rom_.vector(), bytes);
  }

  Rom rom_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  DungeonCanvasViewer* viewer_ = nullptr;
  decltype(core::FeatureFlags::get().dungeon) previous_flags_{};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_P(DungeonSelectionEditsLifecycleTest,
       DeleteEveryDomainAndChestRecordIsOneUndoableCommand) {
  SelectMixed(true);
  const auto before = CaptureDungeonSelectionEditState(*room_);
  const auto metadata = room_->CaptureMetadataSnapshot();
  const auto objects = interaction().GetSelectedObjectIndices();
  const auto entities = coordinator().SelectedEntitiesForEdit();
  ASSERT_TRUE(interaction().HandleDeleteSelected().ok());
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_TRUE(room_->GetChests().empty());
  EXPECT_TRUE(room_->GetDoors().empty());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_TRUE(room_->GetPotItems().empty());
  EXPECT_TRUE(interaction().GetSelectedObjectIndices().empty());
  EXPECT_FALSE(coordinator().HasEntitySelection());
  EXPECT_TRUE(room_->object_stream_dirty());
  EXPECT_TRUE(room_->chests_dirty());
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_TRUE(room_->pot_items_dirty());
  EXPECT_FALSE(room_->header_dirty());
  EXPECT_EQ(room_->CaptureMetadataSnapshot(), metadata);
  room_->ClearSaveDirtyState();
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
  EXPECT_EQ(interaction().GetSelectedObjectIndices(), objects);
  EXPECT_EQ(coordinator().SelectedEntitiesForEdit(), entities);
  EXPECT_TRUE(room_->object_stream_dirty());
  EXPECT_TRUE(room_->chests_dirty());
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_TRUE(room_->pot_items_dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_TRUE(room_->GetDoors().empty());
  EXPECT_TRUE(room_->GetPotItems().empty());
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       BracketCycleUsesActiveRoomSelectionWithoutMutatingOrReplacingCallbacks) {
  int selection_changes = 0;
  interaction().SetSelectionChangeCallback([&] { ++selection_changes; });
  const auto original = CaptureDungeonSelectionEditState(*room_);
  const auto original_dirty = DirtyFields(*room_);
  const size_t original_undo_depth = UndoDepth();

  interaction().SetSelectedObjects({});
  const int before = selection_changes;
  ASSERT_TRUE(editor_->CycleObjectSelection(1));  // Right bracket.
  EXPECT_EQ(interaction().GetSelectedObjectIndices(), (std::vector<size_t>{1}));
  EXPECT_GT(selection_changes, before);
  ASSERT_TRUE(editor_->CycleObjectSelection(1));
  EXPECT_EQ(interaction().GetSelectedObjectIndices(), (std::vector<size_t>{0}));
  ASSERT_TRUE(editor_->CycleObjectSelection(-1));  // Left bracket wraps.
  EXPECT_EQ(interaction().GetSelectedObjectIndices(), (std::vector<size_t>{1}));
  EXPECT_FALSE(editor_->CycleObjectSelection(0));
  EXPECT_EQ(UndoDepth(), original_undo_depth);
  EXPECT_EQ(DirtyFields(*room_), original_dirty);
  ExpectRoomData(*room_, original);

  auto& other_room = editor_->rooms()[1];
  other_room.SetTileObjects({zelda3::RoomObject(0x02, 4, 5, 0, 0)});
  other_room.ClearSaveDirtyState();
  auto* other_viewer = DungeonSelectionEditsTestPeer::Viewer(*editor_, 1);
  ASSERT_NE(other_viewer, nullptr);
  ASSERT_TRUE(editor_->CycleObjectSelection(1));
  EXPECT_EQ(other_viewer->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{0}));
  // Workbench reuses one viewer; standalone keeps the original room viewer.
  EXPECT_EQ(interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{GetParam() ? 0u : 1u}));
  EXPECT_EQ(UndoDepth(), original_undo_depth);
  EXPECT_FALSE(other_room.HasUnsavedChanges());
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       DuplicateMixedSelectionPreservesSpacingRewardAndSpriteMetadata) {
  SelectMixed();
  const auto before = CaptureDungeonSelectionEditState(*room_);
  ASSERT_TRUE(interaction().HandleDuplicateSelected().ok());
  ASSERT_EQ(room_->GetTileObjects().size(), 4u);
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  ASSERT_EQ(room_->GetPotItems().size(), 2u);
  ASSERT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetTileObjects()[2].x_, 10);
  EXPECT_EQ(room_->GetTileObjects()[2].y_, 10);
  EXPECT_EQ(room_->GetTileObjects()[3].x_, 14);
  EXPECT_EQ(room_->GetTileObjects()[3].y_, 14);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
  EXPECT_FALSE(room_->GetChests()[1].size);
  EXPECT_EQ(room_->GetSprites()[1].x(), 7);
  EXPECT_EQ(room_->GetSprites()[1].y(), 9);
  EXPECT_EQ(room_->GetSprites()[1].subtype(), 3);
  EXPECT_EQ(room_->GetSprites()[1].layer(), 1);
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelX(), 144);
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelY(), 176);
  EXPECT_EQ(interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{2, 3}));
  EXPECT_EQ(coordinator().SelectedEntitiesForEdit(),
            (std::vector<SelectedEntity>{{EntityType::Sprite, 1},
                                         {EntityType::Item, 1}}));
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 4u);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       CopyPasteMovesWholeGroupByOneSharedPixelDisplacement) {
  SelectMixed();
  ASSERT_TRUE(interaction().HandleCopySelected().ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->HasUnsavedChanges());
  ASSERT_TRUE(interaction().HandlePasteAt(96, 112).ok());
  ASSERT_EQ(room_->GetTileObjects().size(), 4u);
  EXPECT_EQ(room_->GetTileObjects()[2].x_, 12);
  EXPECT_EQ(room_->GetTileObjects()[2].y_, 14);
  EXPECT_EQ(room_->GetTileObjects()[3].x_, 16);
  EXPECT_EQ(room_->GetTileObjects()[3].y_, 18);
  EXPECT_EQ(room_->GetSprites()[1].x(), 8);
  EXPECT_EQ(room_->GetSprites()[1].y(), 11);
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelX(), 160);
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelY(), 208);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
  EXPECT_EQ(UndoDepth(), 1u);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       EditorCopyPastesAcrossRoomViewersAndUndoesInDestination) {
  // Room 1 needs independent in-place storage before adding an object. Room 2
  // supplies its physical boundary; this test exercises the editor clipboard,
  // not the allocator's shared-stream refusal.
  for (int id = 2; id < zelda3::kNumberOfRooms; ++id)
    WritePointer(rom_, 0x0F8000 + id * 3, kObjectPc + 0x200);
  ASSERT_TRUE(rom_.WriteVector(kObjectPc + 0x200, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
                                                   0xF0, 0xFF, 0xFF, 0xFF})
                  .ok());
  interaction().SetSelectedObjects({0});
  ASSERT_TRUE(editor_->Copy().ok());
  ASSERT_EQ(UndoDepth(), 0u);
  auto* destination_viewer = DungeonSelectionEditsTestPeer::Viewer(*editor_, 1);
  ASSERT_NE(destination_viewer, nullptr);
  auto& destination = editor_->rooms()[1];
  const auto before = CaptureDungeonSelectionEditState(destination);
  const auto bytes = rom_.vector();
  ImGui::GetIO().MousePos = ImVec2(-1000, -1000);
  const auto paste_status = editor_->Paste();
  ASSERT_TRUE(paste_status.ok()) << paste_status;
  ASSERT_EQ(destination.GetTileObjects().size(), 1u);
  EXPECT_EQ(destination.GetTileObjects()[0].id_,
            room_->GetTileObjects()[0].id_);
  EXPECT_EQ(destination.GetTileObjects()[0].x_, 9);
  EXPECT_EQ(destination.GetTileObjects()[0].y_, 9);
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(rom_.vector(), bytes);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(destination, before);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_EQ(destination.GetTileObjects().size(), 1u);
  EXPECT_EQ(destination.GetTileObjects()[0].x_, 9);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SelectionClipboardDoesNotCrossDungeonEditorSessions) {
  interaction().SetSelectedObjects({0});
  ASSERT_TRUE(editor_->Copy().ok());
  ASSERT_TRUE(interaction().HasClipboardData());

  DungeonEditorV2 other_editor(&rom_);
  auto& other_room = other_editor.rooms()[1];
  other_room = zelda3::LoadRoomHeaderFromRom(&rom_, 1);
  other_room.SetLoaded(true);
  other_room.SetTileObjects({});
  other_room.ClearSaveDirtyState();
  auto* other_viewer = DungeonSelectionEditsTestPeer::Viewer(other_editor, 1);
  ASSERT_NE(other_viewer, nullptr);
  EXPECT_FALSE(other_viewer->object_interaction().HasClipboardData());
  const auto bytes = rom_.vector();
  EXPECT_TRUE(other_editor.Paste().ok());
  EXPECT_TRUE(other_room.GetTileObjects().empty());
  EXPECT_EQ(other_editor.undo_manager().UndoStackSize(), 0u);
  EXPECT_EQ(rom_.vector(), bytes);
  EXPECT_TRUE(interaction().HasClipboardData());
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       RoomSwitchKeepsClipboardButRomReplacementClearsIt) {
  interaction().SetSelectedObjects({0});
  ASSERT_TRUE(editor_->Copy().ok());
  auto* destination = DungeonSelectionEditsTestPeer::Viewer(*editor_, 1);
  ASSERT_NE(destination, nullptr);
  EXPECT_TRUE(destination->object_interaction().HasClipboardData());

  Rom replacement;
  ASSERT_TRUE(replacement.LoadFromData(rom_.vector()).ok());
  editor_->SetRom(&replacement);
  auto* replacement_viewer = DungeonSelectionEditsTestPeer::Viewer(*editor_, 0);
  ASSERT_NE(replacement_viewer, nullptr);
  EXPECT_FALSE(replacement_viewer->object_interaction().HasClipboardData());
  editor_->SetRom(&rom_);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       CutAndPasteIncludeDoorsAtExactLegalAnchors) {
  SelectMixed(true);
  const auto before = CaptureDungeonSelectionEditState(*room_);
  ASSERT_TRUE(editor_->Cut().ok());
  EXPECT_TRUE(room_->GetDoors().empty());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_EQ(UndoDepth(), 1u);
  // North door 0 is anchored at (112, 32); the tile object is the leftmost
  // member at x=64, so the copied group's origin is (64, 32).
  ASSERT_TRUE(interaction().HandlePasteAt(64, 32).ok());
  ExpectRoomData(*room_, before);
  EXPECT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(room_->GetDoors().empty());
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       IllegalDoorTranslationRejectsEveryOtherSelectedDomain) {
  SelectMixed(true);
  ASSERT_TRUE(interaction().HandleCopySelected().ok());
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandlePasteAt(80, 48); });
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kMove;
  request.objects = interaction().GetSelectedObjectIndices();
  request.entities = coordinator().SelectedEntitiesForEdit();
  request.delta_x_pixels = request.delta_y_pixels = 16;
  ExpectRejectedWithoutChanges(
      [&] { return coordinator().CommitSelectionEdit(request); });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       PasteOutsideCanvasKeepsDoorAndMixedGroupAtOriginalAnchors) {
  SelectMixed(true);
  const auto before = CaptureDungeonSelectionEditState(*room_);
  ASSERT_TRUE(interaction().HandleCopySelected().ok());
  ImGui::GetIO().MousePos = ImVec2(-1000, -1000);
  ASSERT_TRUE(interaction().HandlePasteObjects().ok());
  ASSERT_EQ(room_->GetTileObjects().size(), 4u);
  ASSERT_EQ(room_->GetDoors().size(), 2u);
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  ASSERT_EQ(room_->GetPotItems().size(), 2u);
  ASSERT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetTileObjects()[2].x_, before.objects[0].x_);
  EXPECT_EQ(room_->GetTileObjects()[2].y_, before.objects[0].y_);
  EXPECT_EQ(room_->GetTileObjects()[3].x_, before.objects[1].x_);
  EXPECT_EQ(room_->GetTileObjects()[3].y_, before.objects[1].y_);
  EXPECT_EQ(room_->GetDoors()[1].EncodeBytes(), before.doors[0].EncodeBytes());
  EXPECT_EQ(room_->GetSprites()[1].x(), before.sprites[0].x);
  EXPECT_EQ(room_->GetSprites()[1].y(), before.sprites[0].y);
  EXPECT_EQ(room_->GetPotItems()[1].position, before.items[0].position);
  EXPECT_EQ(room_->GetChests()[1].id, 0xF1);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SpriteTerminatorTranslationRejectsWholeDuplicateAndPaste) {
  room_->GetSprites()[0] = zelda3::Sprite(9, 4, 30, 24, 1);
  SelectMixed();
  ASSERT_TRUE(interaction().HandleCopySelected().ok());
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandlePasteAt(80, 80); });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SpriteCapacityRejectsOtherDomainsBeforePublishing) {
  while (room_->GetSprites().size() < zelda3::kMaxTotalSprites) {
    room_->GetSprites().emplace_back(9, 4, 4, 0, 0);
  }
  SelectMixed();
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       GlobalChestCapacityRejectsOtherDomainsBeforePublishing) {
  for (int i = 1; i < zelda3::kChestTableCapacityRecords; ++i) {
    ASSERT_TRUE(rom_.WriteVector(kChestPc + i * 3, {0xFF, 0x7F, 0xF0}).ok());
  }
  ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer,
                             zelda3::kChestTableCapacityBytes)
                  .ok());
  SelectMixed();
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SpriteAdmissionIncludesPendingObjectHeaderRelocation) {
  // Room 1 shares its object stream with unopened rooms. A pending layout
  // change needs COW even though this gesture only changes room 0's sprites.
  editor_->rooms()[1].SetLayoutId(1);
  auto& handler = coordinator().sprite_handler();
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(handler.UpdateSprite(0, 9, 7, 8, 3, 1, 2));
    const auto status = handler.mutation_status();
    EXPECT_NE(std::string(status.message()).find("shared"), std::string::npos);
    return status;
  });
  EXPECT_TRUE(editor_->rooms()[1].object_stream_header_dirty());
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       InPlaceSpriteSaveIgnoresProtectedUnusedAllocatorAndSortByte) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version":3,
    "protected_regions":{"total_hooks":2,"regions":[
      {"start":"0x09DB00","end":"0x09DB01","size":1,"hook_count":1,"module":"UnusedArena"},
      {"start":"0x09D900","end":"0x09D901","size":1,"hook_count":1,"module":"SortByte"}]},
    "dungeon_stream_regions":{"sprites":{
      "pointer_table":"0x09D000", "pointer_count":296,
      "pointer_encoding":"bank16", "pointer_bank":"0x09",
      "strategy":"copy_on_write",
      "data_regions":[{"start":"0x09D900","end":"0x09DC00"}],
      "allocation_regions":[{"start":"0x09DB00","end":"0x09DB01"}]
    }}})json")
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  const auto bytes = rom_.vector();
  auto& handler = coordinator().sprite_handler();
  ASSERT_TRUE(handler.UpdateSprite(0, 9, 7, 8, 3, 1, 2))
      << handler.mutation_status();
  ASSERT_TRUE(editor_->Save().ok());
  EXPECT_EQ(rom_.vector()[kSpritePc], bytes[kSpritePc]);
  EXPECT_EQ(rom_.vector()[kSpritePc + 0x200], bytes[kSpritePc + 0x200]);
  EXPECT_EQ(*rom_.ReadWord(kSpriteTablePc), PcToSnes(kSpritePc) & 0xFFFF);
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SharedStreamRejectsDirectDoorAndObjectGesturesBeforeHistory) {
  WritePointer(rom_, 0x0F8000 + 3, kObjectPc);
  auto& doors = coordinator().door_handler();
  auto& objects = coordinator().tile_handler();
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(doors.MutateDoorType(0, zelda3::DoorType::NormalDoorLower));
    return doors.mutation_status();
  });
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(objects.MoveObjects(0, {0}, 1, 0));
    return objects.mutation_status();
  });
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(objects.ResizeObjects(0, {0}, 1, true, false));
    return objects.mutation_status();
  });
  // Give the room its private physical storage back. The same gestures can
  // now be applied and undone without changing the ROM during admission.
  WritePointer(rom_, 0x0F8000 + 3, kObjectPc + 0x100);
  const auto bytes = rom_.vector();
  ASSERT_TRUE(objects.MoveObjects(0, {0}, 1, 0)) << objects.mutation_status();
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 9);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 8);
  ASSERT_TRUE(doors.MutateDoorType(0, zelda3::DoorType::NormalDoorLower))
      << doors.mutation_status();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetDoors()[0].type, zelda3::DoorType::NormalDoor);
  EXPECT_EQ(rom_.vector(), bytes);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       DoorPlacementRejectsObjectStreamGrowthBeforeHistory) {
  const int boundary = kObjectPc + 2 + room_->EncodeObjects().size();
  WritePointer(rom_, 0x0F8000 + 3, boundary);
  ASSERT_TRUE(rom_.WriteVector(boundary, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                          0xFF, 0xFF, 0xFF})
                  .ok());
  auto& handler = coordinator().door_handler();
  handler.SetDoorType(zelda3::DoorType::NormalDoor);
  handler.BeginPlacement();
  ExpectRejectedWithoutChanges([&] {
    handler.HandleClick(240, 16);
    EXPECT_EQ(handler.placement_block_reason(),
              DoorInteractionHandler::PlacementBlockReason::kStorageValidation);
    EXPECT_EQ(handler.mutation_status().code(),
              absl::StatusCode::kResourceExhausted);
    return handler.mutation_status();
  });
  handler.CancelPlacement();
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       ObjectStorageRejectsWholeMixedDuplicateBeforeHistory) {
  const int boundary = kObjectPc + 2 + room_->EncodeObjects().size();
  WritePointer(rom_, 0x0F8000 + 3, boundary);
  ASSERT_TRUE(rom_.WriteVector(boundary, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                          0xFF, 0xFF, 0xFF})
                  .ok());
  SelectMixed();
  ExpectRejectedWithoutChanges([&] {
    const auto status = interaction().HandleDuplicateSelected();
    EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
    EXPECT_NE(std::string(status.message()).find("object data too large"),
              std::string::npos);
    return status;
  });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SpriteStorageRejectsPlacementAndKeyGrowthBeforeHistory) {
  auto& handler = coordinator().sprite_handler();
  room_->GetSprites()[0].set_key_drop(0);
  const int boundary = kSpritePc + 1 + room_->EncodeSprites().size();
  ASSERT_TRUE(
      rom_.WriteWord(kSpriteTablePc + 2, PcToSnes(boundary) & 0xFFFF).ok());
  ASSERT_TRUE(rom_.WriteVector(boundary, {0, 0xFF}).ok());
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(handler.UpdateSprite(0, 9, 6, 8, 3, 1, 1));
    EXPECT_EQ(handler.mutation_status().code(),
              absl::StatusCode::kResourceExhausted);
    return handler.mutation_status();
  });
  handler.SetSpriteId(9);
  handler.BeginPlacement();
  ExpectRejectedWithoutChanges([&] {
    handler.HandleClick(160, 160);
    EXPECT_EQ(
        handler.placement_block_reason(),
        SpriteInteractionHandler::PlacementBlockReason::kStorageValidation);
    return handler.mutation_status();
  });
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SpriteStorageCountsPendingRoomsAndUndoReleasesReservation) {
  // All 296 rooms share an empty source stream. The declared COW arena has room
  // for exactly one five-byte sprite list. Only saved pointers consume ROM.
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
    ASSERT_TRUE(
        rom_.WriteWord(kSpriteTablePc + id * 2, PcToSnes(kSpritePc) & 0xFFFF)
            .ok());
  }
  for (int id : {0, 1}) {
    editor_->rooms()[id].GetSprites().clear();
    editor_->rooms()[id].ClearSaveDirtyState();
  }
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version":3, "dungeon_stream_regions":{"sprites":{
      "pointer_table":"0x09D000", "pointer_count":296,
      "pointer_encoding":"bank16", "pointer_bank":"0x09",
      "strategy":"copy_on_write",
      "data_regions":[{"start":"0x09D900","end":"0x09DB05"}],
      "allocation_regions":[{"start":"0x09DB00","end":"0x09DB05"}]
    }}})json")
                  .ok());
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  auto place = [&](int id) {
    room_ = &editor_->rooms()[id];
    viewer_ = DungeonSelectionEditsTestPeer::Viewer(*editor_, id);
    auto& handler = coordinator().sprite_handler();
    handler.SetSpriteId(9);
    handler.BeginPlacement();
    handler.HandleClick(160, 160);
    handler.CancelPlacement();
    return handler.mutation_status();
  };
  const auto bytes = rom_.vector();
  ASSERT_TRUE(place(0).ok());
  room_ = &editor_->rooms()[1];
  viewer_ = DungeonSelectionEditsTestPeer::Viewer(*editor_, 1);
  ExpectRejectedWithoutChanges([&] {
    const auto status = place(1);
    EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
    return status;
  });
  EXPECT_EQ(editor_->rooms()[0].GetSprites().size(), 1u);
  EXPECT_EQ(rom_.vector(), bytes);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(place(1).ok());
  EXPECT_EQ(editor_->rooms()[0].GetSprites().size(), 0u);
  EXPECT_EQ(editor_->rooms()[1].GetSprites().size(), 1u);
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       ProtectedSpritePayloadRejectsMixedEditAndPreservesRedo) {
  SelectMixed();
  ASSERT_TRUE(interaction().HandleDuplicateSelected().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_EQ(RedoDepth(), 1u);
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version":3,"protected_regions":{"total_hooks":1,
    "regions":[{"start":"0x09D901","end":"0x09D902","size":1,
    "hook_count":1,"module":"SpritePayloadGuard"}]}})json")
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
  EXPECT_EQ(RedoDepth(), 1u);
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       TorchCapacityRejectsWholeMixedPasteBeforeHistory) {
  // One torch in room 0 plus 139 in unopened room 2 exactly fills 0x120 bytes.
  std::vector<uint8_t> torches{0, 0, 0x10, 0x04, 0xFF, 0xFF, 2, 0};
  for (int i = 0; i < 139; ++i) {
    torches.insert(torches.end(), {0x10, 0x04});
  }
  torches.insert(torches.end(), {0xFF, 0xFF});
  ASSERT_EQ(torches.size(), 0x120u);
  ASSERT_TRUE(rom_.WriteVector(zelda3::kTorchData, torches).ok());
  ASSERT_TRUE(
      rom_.WriteWord(zelda3::kTorchesLengthPointer, torches.size()).ok());
  room_->LoadTorches();
  room_->ClearSaveDirtyState();
  interaction().SetSelectedObjects({room_->GetTileObjects().size() - 1});
  coordinator().SetSelectedEntities({{EntityType::Sprite, 0}});
  ASSERT_TRUE(interaction().HandleCopySelected().ok());
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandlePasteAt(80, 80); });
  EXPECT_EQ(editor_->rooms().GetIfMaterialized(2), nullptr);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       TorchPlacementCountsAnotherRoomsPendingEdit) {
  // ROM has 282 bytes: adding the first torch to room 1 would fit exactly.
  std::vector<uint8_t> torches{0, 0, 0x10, 0x04, 0xFF, 0xFF, 2, 0};
  for (int i = 0; i < 136; ++i) {
    torches.insert(torches.end(), {0x10, 0x04});
  }
  torches.insert(torches.end(), {0xFF, 0xFF});
  ASSERT_TRUE(rom_.WriteVector(zelda3::kTorchData, torches).ok());
  ASSERT_TRUE(
      rom_.WriteWord(zelda3::kTorchesLengthPointer, torches.size()).ok());
  room_->LoadTorches();
  interaction().SetSelectedObjects({room_->GetTileObjects().size() - 1});
  ASSERT_TRUE(interaction().HandleDuplicateSelected().ok());
  const auto pending_room = CaptureDungeonSelectionEditState(*room_);
  room_ = &editor_->rooms()[1];
  room_->LoadTorches();
  room_->ClearSaveDirtyState();
  viewer_ = DungeonSelectionEditsTestPeer::Viewer(*editor_, 1);
  zelda3::RoomObject torch(0x150, 8, 8, 0, 0);
  torch.set_options(zelda3::ObjectOption::Torch);
  auto& handler = coordinator().tile_handler();
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(handler.PlaceObjectAt(1, torch, 8, 8));
    return handler.mutation_status();
  });
  ExpectRoomData(editor_->rooms()[0], pending_room);
  EXPECT_EQ(editor_->rooms().GetIfMaterialized(2), nullptr);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       NativeBlockCapacityRejectsDuplicateAndFinalBlockDeletion) {
  const int pointers[] = {zelda3::kBlocksPointer1, zelda3::kBlocksPointer2,
                          zelda3::kBlocksPointer3, zelda3::kBlocksPointer4};
  for (int page = 0; page < 4; ++page) {
    rom_.mutable_data()[pointers[page] - 1] = 0xBF;
    rom_.mutable_data()[pointers[page] + 3] = 0x9D;
    WritePointer(rom_, pointers[page], 0x271DE + page * 0x80);
  }
  for (int i = 0; i < 99; ++i) {
    ASSERT_TRUE(rom_.WriteVector(0x271DE + i * 4,
                                 {static_cast<uint8_t>(i ? 2 : 0), 0, 0x10, 4})
                    .ok());
  }
  ASSERT_TRUE(rom_.WriteWord(zelda3::kBlocksLength, 99 * 4).ok());
  room_->LoadBlocks();
  ASSERT_TRUE(room_->AreBlocksLoaded());
  ASSERT_EQ(room_->GetTileObjects().size(), 3u);
  room_->ClearSaveDirtyState();
  interaction().SetSelectedObjects({room_->GetTileObjects().size() - 1});
  coordinator().SetSelectedEntities({{EntityType::Sprite, 0}});
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDuplicateSelected(); });
  EXPECT_EQ(editor_->rooms().GetIfMaterialized(2), nullptr);

  EXPECT_NE(std::string(interaction().selection_edit_status().message())
                .find("overlaps torch data"),
            std::string::npos);

  // With only the room's original block left, deletion would make the runtime
  // scan unsafe. The accompanying sprite must also remain in the room.
  ASSERT_TRUE(rom_.WriteWord(zelda3::kBlocksLength, 4).ok());
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDeleteSelected(); });
  EXPECT_NE(std::string(interaction().selection_edit_status().message())
                .find("cannot be empty"),
            std::string::npos);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       ProtectedTorchLengthRejectsPlacementBeforeUndo) {
  ASSERT_TRUE(rom_.WriteWord(zelda3::kTorchesLengthPointer, 0).ok());
  room_->LoadTorches();
  project::YazeProject project;
  ASSERT_TRUE(
      project.hack_manifest
          .LoadFromString(absl::StrFormat(
              R"json({"manifest_version":3,"protected_regions":{"total_hooks":1,
      "regions":[{"start":"0x%06X","end":"0x%06X","size":2,
      "hook_count":1,"module":"SpecialTableGuard"}]}})json",
              PcToSnes(zelda3::kTorchesLengthPointer),
              PcToSnes(zelda3::kTorchesLengthPointer + 2)))
          .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  zelda3::RoomObject torch(0x150, 8, 8, 0, 0);
  torch.set_options(zelda3::ObjectOption::Torch);
  auto& handler = coordinator().tile_handler();
  ExpectRejectedWithoutChanges([&] {
    EXPECT_FALSE(handler.PlaceObjectAt(0, torch, 8, 8));
    return handler.mutation_status();
  });
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       ProtectedChestLengthRejectsWholeMixedDelete) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version": 3,
    "protected_regions": {"total_hooks": 1, "regions": [{
      "start": "0x01EBF6", "end": "0x01EBF8", "size": 2,
      "hook_count": 1, "module": "MixedSelectionGuard"
    }]}
  })json")
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  SelectMixed(true);
  ExpectRejectedWithoutChanges(
      [&] { return interaction().HandleDeleteSelected(); });
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       InvalidCopyPreservesClipboardAndInvalidCutPreservesAllData) {
  SelectMixed();
  ASSERT_TRUE(editor_->Copy().ok());
  interaction().SetSelectedObjects({0, 999});
  coordinator().SetSelectedEntities({{EntityType::Sprite, 0}});
  ExpectRejectedWithoutChanges([&] { return editor_->Copy(); });
  ExpectRejectedWithoutChanges([&] { return editor_->Cut(); });
  SelectMixed();
  ASSERT_TRUE(interaction().HandlePasteAt(80, 80).ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 4u);
  EXPECT_EQ(room_->GetSprites().size(), 2u);
  EXPECT_EQ(room_->GetPotItems().size(), 2u);
  EXPECT_EQ(room_->GetChests().size(), 2u);
  EXPECT_EQ(UndoDepth(), 1u);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       NoopAndStaleRequestsPreserveRedoHistory) {
  SelectMixed();
  ASSERT_TRUE(interaction().HandleDuplicateSelected().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_EQ(RedoDepth(), 1u);
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kMove;
  request.objects = {0, 1};
  request.entities = {{EntityType::Sprite, 0}, {EntityType::Item, 0}};
  const auto dirty = DirtyFields(*room_);
  ASSERT_TRUE(coordinator().CommitSelectionEdit(request).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(RedoDepth(), 1u);
  EXPECT_EQ(DirtyFields(*room_), dirty);
  request.entities.push_back({EntityType::Item, 9});
  ExpectRejectedWithoutChanges(
      [&] { return coordinator().CommitSelectionEdit(request); });
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetSprites().size(), 2u);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       MixedDragRecordsOneActionAndRejectsOutOfBoundsIncrement) {
  SelectMixed();
  const auto before = CaptureDungeonSelectionEditState(*room_);
  coordinator().BeginSelectionDrag(ImVec2(64, 64));
  coordinator().HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  coordinator().HandleDrag(ImVec2(96, 96), ImVec2(16, 16));
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 12);
  EXPECT_EQ(room_->GetSprites()[0].x(), 8);
  const auto moved = CaptureDungeonSelectionEditState(*room_);
  coordinator().HandleDrag(ImVec2(640, 640), ImVec2(544, 544));
  EXPECT_FALSE(coordinator().selection_edit_status().ok());
  ExpectRoomData(*room_, moved);
  coordinator().HandleRelease();
  EXPECT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
  ASSERT_TRUE(editor_->Redo().ok());
  ExpectRoomData(*room_, moved);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       RoomSwitchFinishesMixedDragAndUndoPreservesDestinationSelection) {
  SelectMixed();
  const auto before = CaptureDungeonSelectionEditState(*room_);
  coordinator().BeginSelectionDrag(ImVec2(64, 64));
  coordinator().HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  EXPECT_EQ(UndoDepth(), 0u);
  auto& other = editor_->rooms()[1];
  other.GetSprites().emplace_back(10, 20, 20, 0, 0);
  viewer_->RefreshRomBackedState(&rom_, nullptr, &editor_->rooms(), 1);
  coordinator().SetSelectedEntities({{EntityType::Sprite, 0}});
  ASSERT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
  EXPECT_EQ(viewer_->current_room_id(), 1);
  EXPECT_EQ(other.GetSprites()[0].x(), 20);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  EXPECT_FALSE(other.HasUnsavedChanges());
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       DragOutAndBackPreservesDirtyStateAndRedoHistory) {
  SelectMixed();
  ASSERT_TRUE(interaction().HandleDuplicateSelected().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  room_->ClearSaveDirtyState();
  // Unrelated pre-existing dirty work must also survive no-op finalization.
  room_->SetTag1(zelda3::TagKey::Clear_Room_to_Open);
  const auto dirty = DirtyFields(*room_);
  const auto before = CaptureDungeonSelectionEditState(*room_);
  coordinator().BeginSelectionDrag(ImVec2(64, 64));
  coordinator().HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  coordinator().HandleDrag(ImVec2(64, 64), ImVec2(-16, -16));
  coordinator().HandleRelease();
  ExpectRoomData(*room_, before);
  EXPECT_EQ(DirtyFields(*room_), dirty);
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(RedoDepth(), 1u);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetTileObjects().size(), 4u);
  EXPECT_EQ(room_->GetSprites().size(), 2u);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       MixedDragFinishesEarlierTileGestureAsSeparateUndoAction) {
  interaction().SetSelectedObjects({0});
  interaction().mode_manager().SetMode(InteractionMode::DraggingObjects);
  auto& tile_handler = coordinator().tile_handler();
  tile_handler.InitDrag(ImVec2(64, 64));
  tile_handler.HandleDrag(ImVec2(72, 64), ImVec2(8, 0));
  ASSERT_EQ(room_->GetTileObjects()[0].x_, 9);
  ASSERT_EQ(UndoDepth(), 0u);
  SelectMixed();
  const auto after_tile_drag = CaptureDungeonSelectionEditState(*room_);
  coordinator().BeginSelectionDrag(ImVec2(72, 64));
  coordinator().HandleDrag(ImVec2(88, 80), ImVec2(16, 16));
  coordinator().HandleRelease();
  ASSERT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, after_tile_drag);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x_, 8);
  EXPECT_EQ(room_->GetTileObjects()[0].y_, 8);
  EXPECT_EQ(room_->GetSprites()[0].x(), 6);
  EXPECT_EQ(room_->GetPotItems()[0].GetPixelX(), 128);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       SaveEndsDragBeforeReturnMovementAndUndoResave) {
  SelectMixed();
  const auto before = CaptureDungeonSelectionEditState(*room_);
  coordinator().BeginSelectionDrag(ImVec2(64, 64));
  coordinator().HandleDrag(ImVec2(80, 80), ImVec2(16, 16));
  const auto moved = CaptureDungeonSelectionEditState(*room_);
  ASSERT_EQ(UndoDepth(), 0u);
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_FALSE(room_->HasUnsavedChanges());

  // Returning to the pre-save position is a new edit, not a net-zero gesture.
  coordinator().BeginSelectionDrag(ImVec2(80, 80));
  coordinator().HandleDrag(ImVec2(64, 64), ImVec2(-16, -16));
  coordinator().HandleRelease();
  ExpectRoomData(*room_, before);
  ASSERT_EQ(UndoDepth(), 2u);
  EXPECT_TRUE(room_->object_stream_dirty());
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_TRUE(room_->pot_items_dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, moved);
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto saved = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  saved.LoadObjects();
  saved.LoadSprites();
  saved.LoadPotItems();
  ExpectRoomData(saved, moved);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectRoomData(*room_, before);
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto restored = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  restored.LoadObjects();
  restored.LoadSprites();
  restored.LoadPotItems();
  ExpectRoomData(restored, before);
}

TEST_P(DungeonSelectionEditsLifecycleTest,
       MixedDeleteSaveReloadUndoResaveRestoresEveryPersistentDomain) {
  ASSERT_TRUE(room_->SaveObjects().ok());
  ASSERT_TRUE(room_->SaveSprites().ok());
  room_->MarkPotItemsDirty();
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  room_->ClearSaveDirtyState();
  const auto before = CaptureDungeonSelectionEditState(*room_);
  const auto original_rom = rom_.vector();
  SelectMixed(true);
  ASSERT_TRUE(interaction().HandleDeleteSelected().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto deleted = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  deleted.LoadObjects();
  deleted.LoadSprites();
  deleted.LoadPotItems();
  EXPECT_TRUE(deleted.GetTileObjects().empty());
  EXPECT_TRUE(deleted.GetChests().empty());
  EXPECT_TRUE(deleted.GetDoors().empty());
  EXPECT_TRUE(deleted.GetSprites().empty());
  EXPECT_TRUE(deleted.GetPotItems().empty());
  for (size_t i = 0; i < original_rom.size(); ++i) {
    if ((i >= kObjectPc && i < kObjectPc + 0x100) ||
        (i >= zelda3::kDoorPointers && i < zelda3::kDoorPointers + 3) ||
        (i >= kSpritePc && i < kSpritePc + 0x100) ||
        (i >= kPotPc && i < kPotPc + 0x40) ||
        (i >= kChestPc && i < kChestPc + zelda3::kChestTableCapacityBytes) ||
        (i >= zelda3::kChestsLengthPointer &&
         i < zelda3::kChestsLengthPointer + 2))
      continue;
    ASSERT_EQ(rom_.vector()[i], original_rom[i]) << "Unexpected write at " << i;
  }
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->SaveRoom(0).ok());
  auto restored = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  restored.LoadObjects();
  restored.LoadSprites();
  restored.LoadPotItems();
  ExpectRoomData(restored, before);
  ASSERT_TRUE(zelda3::ValidateChestObjectMapping(restored.GetTileObjects(),
                                                 restored.GetChests())
                  .ok());
}

INSTANTIATE_TEST_SUITE_P(ViewerModes, DungeonSelectionEditsLifecycleTest,
                         ::testing::Bool(), [](const auto& param) {
                           return param.param ? "Workbench" : "Standalone";
                         });
}  // namespace
}  // namespace yaze::editor
