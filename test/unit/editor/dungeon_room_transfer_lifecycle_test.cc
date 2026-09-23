#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

class DungeonRoomTransferTestPeer {
 public:
  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    editor.current_room_id_ = room_id;
    auto* viewer = editor.GetViewerForRoom(room_id);
    viewer->RefreshRomBackedState(editor.rom_, nullptr, &editor.rooms_,
                                  room_id);
    return viewer;
  }
  static void ReloadWaterFillZones(DungeonEditorV2& editor) {
    editor.ReloadWaterFillZones();
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
constexpr int kBlocksPc = 0x115000;

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

// Global block slots are assigned when saving a newly copied block. They are
// provenance, not a physical property expected to match the source room.
DungeonRoomDocument WithoutBlockSlots(DungeonRoomDocument document) {
  for (auto& object : document.contents.objects) {
    object.set_block_load_order(zelda3::RoomObject::kBlockLoadOrderNew);
  }
  return document;
}

class DungeonRoomTransferLifecycleTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    previous_flags_ = core::FeatureFlags::get().dungeon;
    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kUseWorkbench = GetParam();
    flags.kSaveObjects = flags.kSaveRoomHeaders = flags.kSaveSprites = true;
    flags.kSaveChests = flags.kSavePotItems = true;
    flags.kSaveTorches = flags.kSaveBlocks = true;
    flags.kSavePits = flags.kSaveCollision = flags.kSaveWaterFillZones = false;
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
      const int stream = std::min(id, 3);
      WritePointer(rom_, kObjectTablePc + id * 3, kObjectPc + stream * 0x100);
      ASSERT_TRUE(rom_.WriteWord(kSpriteTablePc + id * 2,
                                 PcToSnes(kSpritePc + stream * 0x100) & 0xFFFF)
                      .ok());
      ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomItemsPointers + id * 2,
                                 PcToSnes(kPotPc + stream * 0x40) & 0xFFFF)
                      .ok());
      ASSERT_TRUE(rom_.WriteWord(kHeaderTablePc + id * 2,
                                 PcToSnes(kHeaderPc + stream * 0x20) & 0xFFFF)
                      .ok());
    }
    for (int id = 0; id < 4; ++id) {
      ASSERT_TRUE(rom_.WriteVector(kObjectPc + id * 0x100,
                                   {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF,
                                    0xFF, 0xFF})
                      .ok());
      ASSERT_TRUE(rom_.WriteVector(kSpritePc + id * 0x100,
                                   {static_cast<uint8_t>(0x40 + id), 0xFF})
                      .ok());
      ASSERT_TRUE(rom_.WriteWord(kPotPc + id * 0x40, 0xFFFF).ok());
    }
    const std::array<int, 4> block_operands{
        zelda3::kBlocksPointer1, zelda3::kBlocksPointer2,
        zelda3::kBlocksPointer3, zelda3::kBlocksPointer4};
    for (size_t i = 0; i < block_operands.size(); ++i) {
      const int operand = block_operands[i];
      rom_.mutable_data()[operand - 1] = 0xBF;
      WritePointer(rom_, operand, kBlocksPc + i * 0x80);
      rom_.mutable_data()[operand + 3] = 0x9D;
    }
    // These unowned header bits and the destination's sprite sort byte must
    // survive replacement and persistence, rather than following the source.
    rom_.mutable_data()[kHeaderPc + 0x20] = 0x02;
    rom_.mutable_data()[kHeaderPc + 0x20 + 8] = 0xFC;
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    for (int id = 0; id < 3; ++id) {
      auto& room = editor_->rooms()[id];
      room = zelda3::LoadRoomFromRom(&rom_, id);
      room.LoadSprites();
      ASSERT_TRUE(room.AreBlocksLoaded());
      ASSERT_TRUE(room.AreTorchesLoaded());
      ASSERT_TRUE(room.ArePotItemsLoaded());
      ASSERT_TRUE(room.AreSpritesLoaded());
      zelda3::RoomObject chest(0xF99, 12 + id, 12,
                               zelda3::CanonicalRoomObjectSize(0xF99, 0), 1);
      chest.set_options(zelda3::ObjectOption::Chest);
      room.SetTileObjects({zelda3::RoomObject(1 + id, 8 + id, 8, 0, 0), chest});
      zelda3::RoomObject torch(0x150, 20 + id * 2, 20, 0, 1);
      torch.set_options(zelda3::ObjectOption::Torch);
      torch.lit_ = true;
      torch.set_torch_reserved_bit(1);
      room.AddTileObject(torch);
      zelda3::RoomObject block(0xE00, 30 + id, 30, 0, 0);
      block.set_options(zelda3::ObjectOption::Block);
      block.set_block_behavior_layer(1);
      room.AddTileObject(block);
      room.GetDoors().push_back(zelda3::Room::Door::FromRomBytes(
          static_cast<uint8_t>(0x63 + id * 0x10), 0));
      room.GetSprites().emplace_back(9 + id, 6 + id, 8, 3, 1);
      room.GetSprites().back().set_key_drop(2);
      room.MarkSpritesDirty();
      room.GetPotItems().push_back({static_cast<uint16_t>(0x0A20 + id * 4),
                                    static_cast<uint8_t>(6 + id)});
      room.MarkPotItemsDirty();
      room.set_floor1(3 + id);
      room.SetMessageId(0x20 + id);
      room.SetHolewarp(0x40 + id);
      room.SetStaircaseRoom(1, 0x50 + id);
      room.SetStaircasePlane(1, id);
    }
    for (int id = 0; id < 3; ++id) {
      const auto status = editor_->SaveRoom(id);
      ASSERT_TRUE(status.ok()) << status;
    }
    for (int id = 0; id < 3; ++id) {
      auto& room = editor_->rooms()[id];
      room = zelda3::LoadRoomFromRom(&rom_, id);
      room.LoadSprites();
      room.ClearSaveDirtyState();
    }
    viewer_ = DungeonRoomTransferTestPeer::Viewer(*editor_, 1);
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
  absl::StatusOr<DungeonRoomTransferPlan> Preview(
      DungeonRoomTransferOptions options = {}) {
    return editor_->PreviewRoomTransfer(1, 0, {}, options);
  }
  void ExpectDocument(const zelda3::Room& room,
                      const DungeonRoomDocument& expected,
                      bool ignore_block_slots = false) {
    const auto actual = CaptureDungeonRoomDocument(room);
    EXPECT_TRUE(ignore_block_slots
                    ? SameDungeonRoomDocument(WithoutBlockSlots(actual),
                                              WithoutBlockSlots(expected))
                    : SameDungeonRoomDocument(actual, expected));
  }
  void ExpectRejectedWithoutChanges(const std::function<absl::Status()>& edit) {
    const auto source = CaptureDungeonRoomDocument(Source());
    const auto target = CaptureDungeonRoomDocument(Target());
    const auto source_dirty = DirtyFields(Source());
    const auto target_dirty = DirtyFields(Target());
    const auto bytes = rom_.vector();
    const auto undo = UndoDepth();
    const auto redo = RedoDepth();
    const auto selection =
        viewer_->object_interaction().GetSelectedObjectIndices();
    EXPECT_FALSE(edit().ok());
    ExpectDocument(Source(), source);
    ExpectDocument(Target(), target);
    EXPECT_EQ(DirtyFields(Source()), source_dirty);
    EXPECT_EQ(DirtyFields(Target()), target_dirty);
    EXPECT_EQ(rom_.vector(), bytes);
    EXPECT_EQ(UndoDepth(), undo);
    EXPECT_EQ(RedoDepth(), redo);
    EXPECT_EQ(viewer_->object_interaction().GetSelectedObjectIndices(),
              selection);
  }

  Rom rom_;
  std::unique_ptr<DungeonEditorV2> editor_;
  DungeonCanvasViewer* viewer_ = nullptr;
  decltype(core::FeatureFlags::get().dungeon) previous_flags_{};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_P(DungeonRoomTransferLifecycleTest,
       ExportAndDetachedPreviewPreserveLiveRoomsRomDirtyStateAndHistory) {
  const auto source = CaptureDungeonRoomDocument(Source());
  const auto target = CaptureDungeonRoomDocument(Target());
  const auto bytes = rom_.vector();
  const auto exported = editor_->ExportRoomDocument(0);
  ASSERT_TRUE(exported.ok()) << exported.status();
  const auto parsed = ParseDungeonRoomDocument(*exported);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_TRUE(SameDungeonRoomDocument(*parsed, WithoutBlockSlots(source)));
  const auto clone = Preview();
  ASSERT_TRUE(clone.ok()) << clone.status();
  const auto imported = editor_->PreviewRoomTransfer(1, -1, *exported, {});
  ASSERT_TRUE(imported.ok()) << imported.status();
  EXPECT_TRUE(SameDungeonRoomDocument(clone->after, imported->after));
  EXPECT_TRUE(clone->changed());
  ExpectDocument(Source(), source);
  ExpectDocument(Target(), target);
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_FALSE(Target().HasUnsavedChanges());
  EXPECT_EQ(rom_.vector(), bytes);
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       OracleSizedLegacyCountsExportAndPreviewUnrelatedDomains) {
  for (const auto [object_count, chest_count] :
       {std::pair{463u, 0u}, std::pair{425u, 0u}, std::pair{0u, 21u},
        std::pair{0u, 23u}}) {
    SCOPED_TRACE(object_count);
    SCOPED_TRACE(chest_count);
    Source().SetTileObjects(std::vector<zelda3::RoomObject>(
        object_count, zelda3::RoomObject(0x21, 8, 8, 0, 0)));
    Source().GetChests().assign(chest_count, {0x24, false});
    Source().ClearSaveDirtyState();
    const auto source = CaptureDungeonRoomDocument(Source());
    const auto target = CaptureDungeonRoomDocument(Target());
    const auto bytes = rom_.vector();
    const auto exported = editor_->ExportRoomDocument(0);
    ASSERT_TRUE(exported.ok()) << exported.status();
    const auto parsed = ParseDungeonRoomDocument(*exported);
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    EXPECT_EQ(parsed->contents.objects.size(), object_count);
    EXPECT_EQ(parsed->contents.chests.size(), chest_count);
    EXPECT_TRUE(SameDungeonRoomDocument(source, *parsed));

    const auto clone = Preview({kTransferSprites});
    ASSERT_TRUE(clone.ok()) << clone.status();
    const auto imported =
        editor_->PreviewRoomTransfer(1, -1, *exported, {kTransferSprites});
    ASSERT_TRUE(imported.ok()) << imported.status();
    EXPECT_TRUE(SameDungeonRoomDocument(clone->after, imported->after));
    ExpectRejectedWithoutChanges(
        [&] { return Preview({kTransferObjects}).status(); });
    ExpectDocument(Source(), source);
    ExpectDocument(Target(), target);
    EXPECT_FALSE(Source().HasUnsavedChanges());
    EXPECT_FALSE(Target().HasUnsavedChanges());
    EXPECT_EQ(rom_.vector(), bytes);
    EXPECT_EQ(UndoDepth(), 0u);
  }
}

TEST_P(DungeonRoomTransferLifecycleTest,
       ExactLegacyImportPreservesBlockSlotsDirtyStateAndHistory) {
  auto objects = Target().GetTileObjects();
  objects.resize(425, zelda3::RoomObject(0x21, 8, 8, 0, 0));
  Target().SetTileObjects(objects);
  Target().GetChests().assign(23, {0x24, false});
  Target().ClearSaveDirtyState();
  const auto target = CaptureDungeonRoomDocument(Target());
  ASSERT_GE(target.contents.objects[3].block_load_order(), 0);
  const auto bytes = rom_.vector();
  const auto exported = editor_->ExportRoomDocument(1);
  ASSERT_TRUE(exported.ok()) << exported.status();
  const auto plan =
      editor_->PreviewRoomTransfer(1, -1, *exported, {kTransferAll, true});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_FALSE(plan->changed());
  EXPECT_TRUE(SameDungeonRoomDocument(target, plan->after));
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  ExpectDocument(Target(), target);
  EXPECT_FALSE(Target().HasUnsavedChanges());
  EXPECT_EQ(rom_.vector(), bytes);
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       CoreCloneIsOneUndoActionPreservingSourceAndTargetSelection) {
  const auto source = CaptureDungeonRoomDocument(Source());
  const auto target = CaptureDungeonRoomDocument(Target());
  viewer_->object_interaction().SetSelectedObjects({0, 1});
  const auto plan = Preview();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  ExpectDocument(Target(), plan->after);
  EXPECT_EQ(UndoDepth(), 1u);
  ExpectDocument(Source(), source);
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_EQ(Target().holewarp(), target.metadata.holewarp);
  EXPECT_EQ(Target().staircase_room(1), target.metadata.staircase_rooms[1]);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectDocument(Target(), target);
  EXPECT_EQ(viewer_->object_interaction().GetSelectedObjectIndices(),
            (std::vector<size_t>{0, 1}));
  ASSERT_TRUE(editor_->Redo().ok());
  ExpectDocument(Target(), plan->after);
  ExpectDocument(Source(), source);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       PartialTransferPreservesUnknownExcludedDataAndProjectOverlays) {
  Target().GetDoors()[0] = zelda3::Room::Door::FromRomBytes(0xFC, 0xFE);
  Target().GetChests()[0].id = 0xE7;
  Target().custom_collision().has_data = true;
  Target().custom_collision().tiles[65] = 0xFC;
  Target().SetWaterFillTile(2, 3, true);
  Target().set_water_fill_sram_bit_mask(0x20);
  Target().ClearSaveDirtyState();
  Target().ClearWaterFillDirty();
  const auto before = CaptureDungeonRoomDocument(Target());
  const auto plan = Preview({kTransferSprites});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  auto expected = before;
  expected.contents.sprites =
      CaptureDungeonRoomDocument(Source()).contents.sprites;
  ExpectDocument(Target(), expected);
  EXPECT_TRUE(Target().sprites_dirty());
  EXPECT_FALSE(Target().object_stream_dirty());
  EXPECT_FALSE(Target().chests_dirty());
  EXPECT_FALSE(Target().water_fill_dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectDocument(Target(), before);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       NoopImportPreservesExistingRedoHistory) {
  const auto changed = Preview({kTransferDoors});
  ASSERT_TRUE(changed.ok()) << changed.status();
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*changed).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  const auto exported = editor_->ExportRoomDocument(1);
  ASSERT_TRUE(exported.ok()) << exported.status();
  const auto noop =
      editor_->PreviewRoomTransfer(1, -1, *exported, {kTransferDoors});
  ASSERT_TRUE(noop.ok()) << noop.status();
  ASSERT_FALSE(noop->changed());
  const auto dirty = DirtyFields(Target());
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*noop).ok());
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_EQ(RedoDepth(), 1u);
  EXPECT_EQ(DirtyFields(Target()), dirty);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(Target().GetDoors()[0].EncodeBytes(),
            Source().GetDoors()[0].EncodeBytes());
}

TEST_P(DungeonRoomTransferLifecycleTest,
       StaleSourceAndTargetRejectWithoutChangingEitherRoom) {
  const auto plan = Preview();
  ASSERT_TRUE(plan.ok()) << plan.status();
  Source().GetTileObjects()[0].x_ += 1;
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyRoomTransfer(*plan); });
  Source().GetTileObjects()[0].x_ -= 1;
  Target().GetSprites()[0].set_layer(0);
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyRoomTransfer(*plan); });
}

TEST_P(DungeonRoomTransferLifecycleTest,
       ForeignRomAndForeignUndoTargetRejectBeforePublication) {
  const auto plan = Preview();
  ASSERT_TRUE(plan.ok()) << plan.status();
  Rom foreign;
  ASSERT_TRUE(foreign.LoadFromData(rom_.vector()).ok());
  auto foreign_plan = *plan;
  foreign_plan.rom = &foreign;
  ExpectRejectedWithoutChanges(
      [&] { return editor_->ApplyRoomTransfer(foreign_plan); });
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  Target().SetRom(&foreign);
  ExpectRejectedWithoutChanges([&] { return editor_->Undo(); });
  Target().SetRom(&rom_);
  ASSERT_TRUE(editor_->Undo().ok());
}

TEST_P(DungeonRoomTransferLifecycleTest, ChangedDomainsMustHaveSavingEnabled) {
  auto& flags = core::FeatureFlags::get().dungeon;
  for (bool* enabled :
       {&flags.kSaveSprites, &flags.kSaveTorches, &flags.kSaveBlocks}) {
    *enabled = false;
    ExpectRejectedWithoutChanges([&] { return Preview().status(); });
    *enabled = true;
  }
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       ScratchSaveRejectsActualStreamGrowthBeyondPhysicalCapacity) {
  for (int i = 0; i < 100; ++i) {
    Source().AddTileObject(zelda3::RoomObject(1, i % 32, i / 32, 0, 0));
  }
  const auto plan = Preview({kTransferObjects});
  EXPECT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  ExpectRejectedWithoutChanges(
      [&] { return Preview({kTransferObjects}).status(); });
}

TEST_P(DungeonRoomTransferLifecycleTest,
       VanillaBlockGrowthCannotOverwriteTorchesDuringPreview) {
  constexpr int native_blocks = 0x271DE;
  const std::array<int, 4> operands{
      zelda3::kBlocksPointer1, zelda3::kBlocksPointer2, zelda3::kBlocksPointer3,
      zelda3::kBlocksPointer4};
  for (size_t page = 0; page < operands.size(); ++page)
    WritePointer(rom_, operands[page], native_blocks + page * 0x80);
  ASSERT_TRUE(rom_.WriteWord(zelda3::kBlocksLength, 99 * 4).ok());
  for (int slot = 0; slot < 99; ++slot) {
    const uint16_t room = slot < 5 ? 0 : slot < 9 ? 1 : 2;
    const auto encoded = zelda3::EncodePushableBlockEntry(
        {room, static_cast<uint8_t>(slot % 32), 10, 0, 0});
    ASSERT_TRUE(
        rom_.WriteVector(native_blocks + slot * 4,
                         {encoded.b1, encoded.b2, encoded.b3, encoded.b4})
            .ok());
  }
  for (int id = 0; id < 3; ++id) {
    editor_->rooms()[id] = zelda3::LoadRoomFromRom(&rom_, id);
    editor_->rooms()[id].LoadSprites();
  }
  const auto rejected = Preview({kTransferObjects});
  ASSERT_FALSE(rejected.ok());
  EXPECT_NE(std::string(rejected.status().message()).find("torch"),
            std::string::npos)
      << rejected.status();
  ExpectRejectedWithoutChanges(
      [&] { return Preview({kTransferObjects}).status(); });
}

TEST_P(DungeonRoomTransferLifecycleTest,
       ProtectedDestinationRejectsDetachedSaveWithoutWritingLiveRom) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version": 3,
    "protected_regions": {"total_hooks": 1, "regions": [{
      "start": "0x208100", "end": "0x208200", "size": 256,
      "hook_count": 1, "module": "TransferDestinationGuard"
    }]}
  })json")
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EditorDependencies dependencies;
  dependencies.rom = &rom_;
  dependencies.project = &project;
  editor_->SetDependencies(dependencies);
  ExpectRejectedWithoutChanges([&] { return Preview().status(); });
  dependencies.project = nullptr;
  editor_->SetDependencies(dependencies);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       MetadataReplacementRejectsExactAndPartialHeaderAliases) {
  Source().SetPalette(2);
  for (const int overlap : {0, 7}) {
    SCOPED_TRACE(overlap);
    ASSERT_TRUE(rom_.WriteWord(kHeaderTablePc + 2 * 2,
                               PcToSnes(kHeaderPc + 0x20 + overlap) & 0xFFFF)
                    .ok());
    const auto plan = Preview({kTransferMetadata});
    ASSERT_FALSE(plan.ok());
    EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
    EXPECT_NE(std::string(plan.status().message()).find("header overlaps"),
              std::string::npos);
    EXPECT_TRUE(plan.status().GetPayload(kRoomTransferSharedHeaderPayload));
    ExpectRejectedWithoutChanges(
        [&] { return Preview({kTransferMetadata}).status(); });
    const auto contents_only = Preview({kTransferCore & ~kTransferMetadata});
    ASSERT_TRUE(contents_only.ok()) << contents_only.status();
    EXPECT_EQ(contents_only->after.metadata, contents_only->before.metadata);
  }
}

TEST_P(DungeonRoomTransferLifecycleTest,
       GlobalChestCapacityIncludesUnopenedRoomsBeforeClone) {
  for (int index = 3; index < zelda3::kChestTableCapacityRecords; ++index) {
    ASSERT_TRUE(rom_.WriteVector(kChestPc + index * 3, {0x50, 0, 0xF0}).ok());
  }
  ASSERT_TRUE(rom_.WriteWord(zelda3::kChestsLengthPointer,
                             zelda3::kChestTableCapacityBytes)
                  .ok());
  zelda3::RoomObject chest(0xF99, 16, 16,
                           zelda3::CanonicalRoomObjectSize(0xF99, 0), 1);
  chest.set_options(zelda3::ObjectOption::Chest);
  Source().AddTileObject(chest);
  Source().GetChests().push_back({0xFA, false});
  Source().MarkChestsDirty();
  ExpectRejectedWithoutChanges(
      [&] { return Preview({kTransferObjects}).status(); });
}

TEST_P(DungeonRoomTransferLifecycleTest,
       LazyPotExportAcceptsTerminatorSharedWithEmptyRoom) {
  Source() = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  ASSERT_TRUE(
      rom_.WriteVector(kPotPc, {0xCC, 0x13, 0x0A, 0x60, 0x26, 0x0B, 0xFF, 0xFF})
          .ok());
  ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomItemsPointers + 8 * 2,
                             PcToSnes(kPotPc + 6) & 0xFFFF)
                  .ok());
  const auto before = rom_.vector();
  const auto exported = editor_->ExportRoomDocument(0);
  ASSERT_TRUE(exported.ok()) << exported.status();
  const auto document = ParseDungeonRoomDocument(*exported);
  ASSERT_TRUE(document.ok()) << document.status();
  ASSERT_EQ(document->contents.items.size(), 2u);
  EXPECT_EQ(document->contents.items[1].position, 0x2660);
  const auto preview = Preview({kTransferItems});
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       MalformedLazySpriteAndPotStreamsCannotBecomeEmptyCloneSources) {
  Source() = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  ASSERT_TRUE(rom_.WriteWord(kSpriteTablePc, 0).ok());
  ExpectRejectedWithoutChanges([&] { return Preview().status(); });
  EXPECT_FALSE(Source().AreSpritesLoaded());
  ASSERT_TRUE(
      rom_.WriteWord(kSpriteTablePc, PcToSnes(kSpritePc) & 0xFFFF).ok());
  ASSERT_TRUE(rom_.WriteWord(zelda3::kRoomItemsPointers, 0).ok());
  ExpectRejectedWithoutChanges([&] { return Preview().status(); });
  EXPECT_FALSE(Source().ArePotItemsLoaded());
}

TEST_P(DungeonRoomTransferLifecycleTest,
       MalformedLazyTorchTablesCannotBecomeIncompleteCloneSources) {
  struct TorchCase {
    const char* name;
    uint16_t length;
    std::vector<uint8_t> bytes;
  };
  const std::array<TorchCase, 4> cases = {{
      {"odd length", 3, {0, 0, 0}},
      {"length exceeds capacity", 0x122, {}},
      {"missing segment terminator", 4, {0, 0, 0x14, 0x05}},
      {"duplicate source segments",
       12,
       {0, 0, 0x14, 0x05, 0xFF, 0xFF, 0, 0, 0x16, 0x05, 0xFF, 0xFF}},
  }};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.name);
    Source() = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
    ASSERT_FALSE(Source().AreTorchesLoaded());
    ASSERT_TRUE(
        rom_.WriteWord(zelda3::kTorchesLengthPointer, test.length).ok());
    if (!test.bytes.empty()) {
      ASSERT_TRUE(rom_.WriteVector(zelda3::kTorchData, test.bytes).ok());
    }
    const auto plan = Preview();
    ASSERT_FALSE(plan.ok());
    EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
    EXPECT_NE(std::string(plan.status().message()).find("Torch"),
              std::string::npos);
    ExpectRejectedWithoutChanges([&] { return Preview().status(); });
    EXPECT_FALSE(Source().AreTorchesLoaded());
  }
}

TEST_P(DungeonRoomTransferLifecycleTest,
       DirtyUnloadedSpecialTablesAreNotDiscardedDuringHydration) {
  Source() = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  Source().MarkTorchesDirty();
  ExpectRejectedWithoutChanges([&] { return Preview().status(); });
  EXPECT_FALSE(Source().AreTorchesLoaded());
  Source().ClearTorchesDirty();
  Source().MarkBlocksDirty();
  ExpectRejectedWithoutChanges([&] { return Preview().status(); });
  EXPECT_FALSE(Source().AreBlocksLoaded());
}

TEST_P(DungeonRoomTransferLifecycleTest,
       SaveReloadUndoResaveRetainsGlobalsReservedBytesAndOtherRooms) {
  const auto source = CaptureDungeonRoomDocument(Source());
  const auto other = CaptureDungeonRoomDocument(editor_->rooms()[2]);
  const auto target = CaptureDungeonRoomDocument(Target());
  const auto plan = Preview();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  const auto clone_save = editor_->SaveRoom(1);
  ASSERT_TRUE(clone_save.ok()) << clone_save;
  auto reloaded = zelda3::LoadRoomFromRom(&rom_, 1);
  reloaded.LoadSprites();
  ExpectDocument(reloaded, plan->after, true);
  EXPECT_EQ(rom_.vector()[kHeaderPc + 0x20] & 0x02, 0x02);
  EXPECT_EQ(rom_.vector()[kHeaderPc + 0x20 + 8] & 0xFC, 0xFC);
  EXPECT_EQ(rom_.vector()[kSpritePc + 0x100], 0x41);
  for (const auto& [id, expected] :
       {std::pair{0, source}, std::pair{2, other}}) {
    auto untouched = zelda3::LoadRoomFromRom(&rom_, id);
    untouched.LoadSprites();
    ExpectDocument(untouched, expected, true);
  }
  ASSERT_TRUE(editor_->Undo().ok());
  const auto undo_save = editor_->SaveRoom(1);
  ASSERT_TRUE(undo_save.ok()) << undo_save;
  auto restored = zelda3::LoadRoomFromRom(&rom_, 1);
  restored.LoadSprites();
  ExpectDocument(restored, target, true);
  ASSERT_TRUE(editor_->Redo().ok());
  const auto redo_save = editor_->SaveRoom(1);
  ASSERT_TRUE(redo_save.ok()) << redo_save;
  auto redone = zelda3::LoadRoomFromRom(&rom_, 1);
  redone.LoadSprites();
  ExpectDocument(redone, plan->after, true);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       CloneFinishesPendingDragAndUndoRestoresItAsSeparateAction) {
  auto& interaction = viewer_->object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  interaction.SetSelectedObjects({0});
  coordinator.SetSelectedEntities({{EntityType::Sprite, 0}});
  coordinator.BeginSelectionDrag(ImVec2(72, 64));
  coordinator.HandleDrag(ImVec2(88, 80), ImVec2(16, 16));
  ASSERT_EQ(UndoDepth(), 0u);
  ASSERT_EQ(Target().GetTileObjects()[0].x_, 11);
  const auto moved = CaptureDungeonRoomDocument(Target());
  const auto plan = Preview();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  EXPECT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  ExpectDocument(Target(), moved);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Target().GetTileObjects()[0].x_, 9);
}

TEST_P(DungeonRoomTransferLifecycleTest,
       LazyExportLoadsEveryIncludedDomainWithoutMutatingRomOrHistory) {
  const auto expected = CaptureDungeonRoomDocument(Source());
  Source() = zelda3::LoadRoomHeaderFromRom(&rom_, 0);
  const auto bytes = rom_.vector();
  const auto document = editor_->ExportRoomDocument(0);
  ASSERT_TRUE(document.ok()) << document.status();
  const auto parsed = ParseDungeonRoomDocument(*document);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_TRUE(SameDungeonRoomDocument(*parsed, WithoutBlockSlots(expected)));
  EXPECT_TRUE(Source().AreObjectsLoaded());
  EXPECT_TRUE(Source().AreSpritesLoaded());
  EXPECT_TRUE(Source().ArePotItemsLoaded());
  EXPECT_TRUE(Source().AreChestsLoaded());
  EXPECT_TRUE(Source().AreBlocksLoaded());
  EXPECT_TRUE(Source().AreTorchesLoaded());
  EXPECT_FALSE(Source().HasUnsavedChanges());
  EXPECT_EQ(rom_.vector(), bytes);
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(
    DungeonRoomTransferLifecycleTest,
    WaterCloneIntoLowerRoomAllocatesDistinctBitAndPreservesSourceAndUnopenedZone) {
  ASSERT_TRUE(zelda3::WriteWaterFillTable(
                  &rom_, {{2, 0x01, {131, 263}}, {8, 0x04, {99}}})
                  .ok());
  // Only the source is materialized here. The detached preflight must retain
  // room 8's ROM-backed zone too, rather than normalizing it away.
  auto& source = editor_->rooms()[2];
  source.SetWaterFillTile(3, 2, true);
  source.SetWaterFillTile(7, 4, true);
  source.set_water_fill_sram_bit_mask(0x01);
  source.ClearWaterFillDirty();
  core::FeatureFlags::get().dungeon.kSaveWaterFillZones = true;
  const auto bytes = rom_.vector();
  const auto plan = editor_->PreviewRoomTransfer(1, 2, {}, {kTransferWater});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.water.sram_bit_mask, 0x02);
  EXPECT_EQ(source.water_fill_sram_bit_mask(), 0x01);
  EXPECT_EQ(rom_.vector(), bytes);
  ASSERT_TRUE(editor_->ApplyRoomTransfer(*plan).ok());
  ASSERT_TRUE(editor_->SaveRoom(1).ok());
  const auto zones = zelda3::LoadWaterFillTable(&rom_);
  ASSERT_TRUE(zones.ok()) << zones.status();
  ASSERT_EQ(zones->size(), 3u);
  for (const auto& zone : *zones) {
    if (zone.room_id == 1)
      EXPECT_EQ(zone.sram_bit_mask, 0x02);
    if (zone.room_id == 2)
      EXPECT_EQ(zone.sram_bit_mask, 0x01);
    if (zone.room_id == 8) {
      EXPECT_EQ(zone.sram_bit_mask, 0x04);
      EXPECT_EQ(zone.fill_offsets, (std::vector<uint16_t>{99}));
    }
  }
  EXPECT_EQ(source.water_fill_sram_bit_mask(), 0x01);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_FALSE(Target().has_water_fill_zone());
  EXPECT_EQ(source.water_fill_sram_bit_mask(), 0x01);
}

INSTANTIATE_TEST_SUITE_P(ViewerModes, DungeonRoomTransferLifecycleTest,
                         ::testing::Bool(), [](const auto& param) {
                           return param.param ? "Workbench" : "Standalone";
                         });
}  // namespace
}  // namespace yaze::editor
