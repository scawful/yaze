#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/features.h"
#include "core/project.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mocks/mock_rom.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "test_utils/dungeon_editor_v2_regular_entrance_test_peer.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_stream_ordering.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/water_fill_zone.h"

namespace yaze::editor {

namespace {

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) prev =
      core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = prev; }
};

void ConfigureMinimalDungeonSave() {
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSaveEntrances = false;
  d.kSavePalettes = false;
}

constexpr int kChestDataPc = 0x110000;
constexpr int kRoom0ObjectPc = 0x100000;
constexpr int kRoom1ObjectPc = 0x100100;
constexpr int kRoom2ObjectPc = 0x100200;
constexpr int kPotRoom0Pc = 0x008000;
constexpr int kPotRoom1Pc = 0x008020;
constexpr int kPotRoom2Pc = 0x008040;
constexpr int kRoom0SpritePc = 0x049000;
constexpr int kRoom1SpritePc = 0x049020;
constexpr int kRoom2SpritePc = 0x049040;
constexpr int kSpritePointerTablePc = 0x048000;
constexpr int kSpriteAllocationPc = 0x04A000;
constexpr int kObjectAllocationPc = 0x140000;
constexpr int kHeaderTablePc = 0x0F6000;
constexpr int kRoom0HeaderPc = 0x114000;
constexpr int kRoom1HeaderPc = 0x114020;
constexpr int kRoom2HeaderPc = 0x114040;
constexpr int kBlocksRegion1Pc = 0x115000;
constexpr int kBlocksRegion2Pc = 0x115080;
constexpr int kBlocksRegion3Pc = 0x115100;
constexpr int kBlocksRegion4Pc = 0x115180;

void WriteLongPointer(Rom& rom, int addr, uint32_t snes_addr) {
  rom.mutable_data()[addr + 0] = snes_addr & 0xFF;
  rom.mutable_data()[addr + 1] = (snes_addr >> 8) & 0xFF;
  rom.mutable_data()[addr + 2] = (snes_addr >> 16) & 0xFF;
}

void SetupBlockTable(Rom& rom) {
  const std::array<std::pair<int, int>, 4> pointer_regions = {
      std::pair{zelda3::kBlocksPointer1, kBlocksRegion1Pc},
      std::pair{zelda3::kBlocksPointer2, kBlocksRegion2Pc},
      std::pair{zelda3::kBlocksPointer3, kBlocksRegion3Pc},
      std::pair{zelda3::kBlocksPointer4, kBlocksRegion4Pc},
  };
  for (const auto& [operand_pc, region_pc] : pointer_regions) {
    rom.mutable_data()[operand_pc - 1] = 0xBF;  // LDA.l operand,X
    WriteLongPointer(rom, operand_pc, PcToSnes(region_pc));
    rom.mutable_data()[operand_pc + 3] = 0x9D;  // STA.w addr,X
  }
}

void SetupChestTable(Rom& rom) {
  WriteLongPointer(rom, zelda3::kChestsDataPointer1, PcToSnes(kChestDataPc));
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x00;
  rom.mutable_data()[zelda3::kChestsLengthPointer + 1] = 0x00;
  std::fill_n(rom.mutable_data() + kChestDataPc,
              zelda3::kChestTableCapacityBytes, 0x00);
}

void WriteEmptyObjectStream(Rom& rom, int room_data_pc) {
  rom.mutable_data()[room_data_pc + 0] = 0x00;
  rom.mutable_data()[room_data_pc + 1] = 0x00;
  const std::vector<uint8_t> empty = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  ASSERT_TRUE(rom.WriteVector(room_data_pc + 2, empty).ok());
}

void SetupRoomObjectPointers(Rom& rom) {
  WriteLongPointer(rom, zelda3::kRoomObjectPointer, PcToSnes(0x0F8000));

  WriteLongPointer(rom, 0x0F8000 + 0, PcToSnes(kRoom0ObjectPc));
  WriteLongPointer(rom, 0x0F8000 + 3, PcToSnes(kRoom1ObjectPc));
  WriteLongPointer(rom, 0x0F8000 + 6, PcToSnes(kRoom2ObjectPc));

  WriteEmptyObjectStream(rom, kRoom0ObjectPc);
  WriteEmptyObjectStream(rom, kRoom1ObjectPc);
  WriteEmptyObjectStream(rom, kRoom2ObjectPc);
}

void SetupSharedRoomObjectPointers(Rom& rom) {
  WriteLongPointer(rom, zelda3::kRoomObjectPointer, PcToSnes(0x0F8000));
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    WriteLongPointer(rom, 0x0F8000 + room_id * 3, PcToSnes(kRoom0ObjectPc));
  }
  WriteEmptyObjectStream(rom, kRoom0ObjectPc);
}

void SetupSharedSpritePointers(Rom& rom) {
  const uint16_t table_pointer =
      static_cast<uint16_t>(PcToSnes(kSpritePointerTablePc));
  rom.mutable_data()[zelda3::kRoomsSpritePointer] = table_pointer & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 1] =
      (table_pointer >> 8) & 0xFF;

  const uint16_t stream_pointer =
      static_cast<uint16_t>(PcToSnes(kRoom0SpritePc));
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const int slot = kSpritePointerTablePc + room_id * 2;
    rom.mutable_data()[slot] = stream_pointer & 0xFF;
    rom.mutable_data()[slot + 1] = (stream_pointer >> 8) & 0xFF;
  }
  rom.mutable_data()[kRoom0SpritePc] = 0x00;
  rom.mutable_data()[kRoom0SpritePc + 1] = 0xFF;
}

project::YazeProject MakeProjectWithManifest(const char* json) {
  project::YazeProject project;
  EXPECT_TRUE(project.hack_manifest.LoadFromString(json).ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  return project;
}

void SetupSpritePointers(Rom& rom) {
  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0SpritePc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1SpritePc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2SpritePc));

  rom.mutable_data()[zelda3::kRoomsSpritePointer + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomsSpritePointer + 5] = (room2_ptr >> 8) & 0xFF;

  for (int sprite_pc : {kRoom0SpritePc, kRoom1SpritePc, kRoom2SpritePc}) {
    rom.mutable_data()[sprite_pc + 0] = 0x00;
    rom.mutable_data()[sprite_pc + 1] = 0xFF;
  }
}

void SeedChestEntry(Rom& rom, int room_id, uint8_t chest_id, bool big) {
  const uint16_t word = static_cast<uint16_t>(room_id) | (big ? 0x8000 : 0);
  rom.mutable_data()[zelda3::kChestsLengthPointer] =
      zelda3::kChestTableRecordSize;
  rom.mutable_data()[zelda3::kChestsLengthPointer + 1] = 0x00;
  rom.mutable_data()[kChestDataPc + 0] = word & 0xFF;
  rom.mutable_data()[kChestDataPc + 1] = (word >> 8) & 0xFF;
  rom.mutable_data()[kChestDataPc + 2] = chest_id;
}

void SetupPotItemTable(Rom& rom) {
  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom0Pc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom1Pc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom2Pc));

  rom.mutable_data()[zelda3::kRoomItemsPointers + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[zelda3::kRoomItemsPointers + 5] = (room2_ptr >> 8) & 0xFF;

  rom.mutable_data()[kPotRoom0Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom0Pc + 1] = 0xFF;
  rom.mutable_data()[kPotRoom1Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom1Pc + 1] = 0xFF;
  rom.mutable_data()[kPotRoom2Pc + 0] = 0xFF;
  rom.mutable_data()[kPotRoom2Pc + 1] = 0xFF;
}

void SetupSharedPotItemTable(Rom& rom) {
  constexpr int kSharedStreamPc = 0x00E000;
  const uint16_t pointer = static_cast<uint16_t>(PcToSnes(kSharedStreamPc));
  for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    const int slot = zelda3::kRoomItemsPointers + room_id * 2;
    rom.mutable_data()[slot] = pointer & 0xFF;
    rom.mutable_data()[slot + 1] = (pointer >> 8) & 0xFF;
  }
  rom.mutable_data()[kSharedStreamPc] = 0xFF;
  rom.mutable_data()[kSharedStreamPc + 1] = 0xFF;
}

void SetupHeaderPointers(Rom& rom) {
  WriteLongPointer(rom, zelda3::kRoomHeaderPointer, PcToSnes(kHeaderTablePc));
  rom.mutable_data()[zelda3::kRoomHeaderPointerBank] =
      (PcToSnes(kRoom0HeaderPc) >> 16) & 0xFF;

  const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kRoom0HeaderPc));
  const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kRoom1HeaderPc));
  const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kRoom2HeaderPc));

  rom.mutable_data()[kHeaderTablePc + 0] = room0_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 1] = (room0_ptr >> 8) & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 2] = room1_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 3] = (room1_ptr >> 8) & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 4] = room2_ptr & 0xFF;
  rom.mutable_data()[kHeaderTablePc + 5] = (room2_ptr >> 8) & 0xFF;
}

void SeedHeaderBytes(Rom& rom, int header_pc,
                     std::initializer_list<uint8_t> bytes, uint16_t message_id,
                     int room_id) {
  std::copy(bytes.begin(), bytes.end(), rom.mutable_data() + header_pc);
  rom.mutable_data()[zelda3::kMessagesIdDungeon + (room_id * 2) + 0] =
      message_id & 0xFF;
  rom.mutable_data()[zelda3::kMessagesIdDungeon + (room_id * 2) + 1] =
      (message_id >> 8) & 0xFF;
}

void SeedPotItemBytes(Rom& rom, int pc_addr,
                      std::initializer_list<uint8_t> bytes) {
  std::copy(bytes.begin(), bytes.end(), rom.mutable_data() + pc_addr);
}

}  // namespace

TEST(DungeonEditorV2RomSafetyTest, SaveUsesRoomIndicesNotInternalIds) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // No rooms are loaded, so SaveRoomData should be a no-op and never write.
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Inject a room with an out-of-range internal ID. Save must not use
  // room.id() for iteration (otherwise it would try SaveRoomData(999)).
  editor->rooms()[5] = zelda3::Room(999, &rom, nullptr);
  editor->rooms()[5].SetLoaded(false);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomBlocksInvalidRoomBeforeWriting) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());
  EXPECT_CALL(rom, WriteHelper(testing::_)).Times(0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/70, /*y=*/0,
                                        /*size=*/0, /*layer=*/0));

  auto status = editor->SaveRoom(0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomDetachesSharedObjectStreamWithManifestAllocator) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupSharedRoomObjectPointers(rom);
  const std::vector<uint8_t> old_stream(rom.data() + kRoom0ObjectPc,
                                        rom.data() + kRoom0ObjectPc + 16);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "dungeon_stream_regions": {
    "objects": {
      "pointer_table": "0x1F8000",
      "pointer_count": 296,
      "pointer_encoding": "long24",
      "strategy": "copy_on_write",
      "data_regions": [
        {"start": "0x208000", "end": "0x208100"},
        {"start": "0x288000", "end": "0x298000"}
      ],
      "allocation_regions": [
        {"start": "0x288000", "end": "0x298000"}
      ]
    }
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);

  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x10, 10, 10, 0, 0)).ok());

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  const auto write_ranges = editor->CollectWriteRanges();
  EXPECT_NE(
      std::find(write_ranges.begin(), write_ranges.end(),
                std::pair<uint32_t, uint32_t>{kObjectAllocationPc, 0x148000}),
      write_ranges.end());
  EXPECT_NE(std::find(write_ranges.begin(), write_ranges.end(),
                      std::pair<uint32_t, uint32_t>{0x0F8000, 0x0F8003}),
            write_ranges.end());
  EXPECT_NE(std::find(write_ranges.begin(), write_ranges.end(),
                      std::pair<uint32_t, uint32_t>{zelda3::kDoorPointers,
                                                    zelda3::kDoorPointers + 3}),
            write_ranges.end());

  const auto status = editor->SaveRoom(0);

  ASSERT_TRUE(status.ok()) << status.message();
  ASSERT_TRUE(rom.ReadLong(0x0F8000).ok());
  EXPECT_EQ(SnesToPc(*rom.ReadLong(0x0F8000)), kObjectAllocationPc);
  EXPECT_EQ(SnesToPc(*rom.ReadLong(0x0F8003)), kRoom0ObjectPc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom.data() + kRoom0ObjectPc));
  EXPECT_FALSE(room.object_stream_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomDetachesSharedSpriteStreamWithManifestAllocator) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupSharedSpritePointers(rom);
  const std::vector<uint8_t> old_stream(rom.data() + kRoom0SpritePc,
                                        rom.data() + kRoom0SpritePc + 8);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "dungeon_stream_regions": {
    "sprites": {
      "pointer_table": "0x098000",
      "pointer_count": 296,
      "pointer_encoding": "bank16",
      "pointer_bank": "0x09",
      "strategy": "copy_on_write",
      "data_regions": [
        {"start": "0x099000", "end": "0x099100"},
        {"start": "0x09A000", "end": "0x09B000"}
      ],
      "allocation_regions": [
        {"start": "0x09A000", "end": "0x09B000"}
      ]
    }
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);

  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room.MarkSpritesDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveSprites = true;

  const auto write_ranges = editor->CollectWriteRanges();
  EXPECT_NE(
      std::find(write_ranges.begin(), write_ranges.end(),
                std::pair<uint32_t, uint32_t>{kSpriteAllocationPc, 0x04B000}),
      write_ranges.end());
  EXPECT_NE(std::find(write_ranges.begin(), write_ranges.end(),
                      std::pair<uint32_t, uint32_t>{kSpritePointerTablePc,
                                                    kSpritePointerTablePc + 2}),
            write_ranges.end());

  const auto status = editor->SaveRoom(0);

  ASSERT_TRUE(status.ok()) << status.message();
  const uint16_t room0_pointer = *rom.ReadWord(kSpritePointerTablePc + 0 * 2);
  const uint16_t room1_pointer = *rom.ReadWord(kSpritePointerTablePc + 1 * 2);
  EXPECT_EQ(SnesToPc(0x090000u | room0_pointer), kSpriteAllocationPc);
  EXPECT_EQ(SnesToPc(0x090000u | room1_pointer), kRoom0SpritePc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom.data() + kRoom0SpritePc));
  EXPECT_FALSE(room.sprites_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedDoorPointerWriteWithoutCowLayout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupRoomObjectPointers(rom);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x1F83C0",
        "end": "0x1F83C3",
        "size": 3,
        "hook_count": 1,
        "module": "DoorGuard"
      }
    ]
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.AddDoor(zelda3::Room::Door{});

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  const auto before = rom.vector();

  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.object_stream_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedPotItemWriteBeforeMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupPotItemTable(rom);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x018000",
        "end": "0x018005",
        "size": 5,
        "hook_count": 1,
        "module": "PotGuard"
      }
    ]
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetPotItems().push_back(zelda3::PotItem{0x1234, 0x56});
  room.MarkPotItemsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSavePotItems = true;
  const auto before = rom.vector();

  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.pot_items_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomRepacksSharedPotItemsAndPredictsCompleteRanges) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupSharedPotItemTable(rom);
  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "dungeon_stream_regions": {
    "pot_items": {
      "pointer_table": "0x01DB69",
      "pointer_count": 296,
      "pointer_encoding": "bank16",
      "pointer_bank": "0x01",
      "strategy": "repack_all",
      "data_regions": [
        {"start": "0x01E000", "end": "0x01E140"}
      ],
      "allocation_regions": [
        {"start": "0x01E100", "end": "0x01E140"}
      ]
    }
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room.GetPotItems().push_back(zelda3::PotItem{0x1234, 0x56});
  room.MarkPotItemsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSavePotItems = true;

  const auto ranges = editor->CollectWriteRanges();
  EXPECT_NE(std::find(ranges.begin(), ranges.end(),
                      std::pair<uint32_t, uint32_t>{0x00E100, 0x00E140}),
            ranges.end());
  EXPECT_NE(
      std::find(ranges.begin(), ranges.end(),
                std::pair<uint32_t, uint32_t>{
                    zelda3::kRoomItemsPointers,
                    zelda3::kRoomItemsPointers + zelda3::kNumberOfRooms * 2}),
      ranges.end());

  const auto status = editor->SaveRoom(0);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(room.pot_items_dirty());
  const uint16_t room0_pointer = *rom.ReadWord(zelda3::kRoomItemsPointers);
  const uint16_t room1_pointer = *rom.ReadWord(zelda3::kRoomItemsPointers + 2);
  EXPECT_EQ(SnesToPc(0x010000u | room0_pointer), 0x00E100);
  EXPECT_EQ(SnesToPc(0x010000u | room1_pointer), 0x00E105);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomPotRepackFailureRestoresRomAndDirtyState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupSharedPotItemTable(rom);
  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "dungeon_stream_regions": {
    "pot_items": {
      "pointer_table": "0x01DB69",
      "pointer_count": 296,
      "pointer_encoding": "bank16",
      "pointer_bank": "0x01",
      "strategy": "repack_all",
      "data_regions": [
        {"start": "0x01E000", "end": "0x01E140"}
      ],
      "allocation_regions": [
        {"start": "0x01E100", "end": "0x01E106"}
      ]
    }
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room.GetPotItems().push_back(zelda3::PotItem{0x1234, 0x56});
  room.MarkPotItemsDirty();
  const auto before = rom.vector();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSavePotItems = true;

  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.pot_items_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomRollsBackEarlierWritesOnLateFailure) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SetupChestTable(rom);
  // The chest length operand is a byte count and must be divisible by three.
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x01;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;
  flags.kSaveChests = true;

  const auto before = rom.vector();
  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.data()[kRoom0HeaderPc + 1], 0x00);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(room.chests_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveAllRoomsRollsBackEarlierWritesOnLateFailure) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SetupChestTable(rom);
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x01;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;
  flags.kSaveChests = true;

  const auto before = rom.vector();
  editor->SaveAllRooms();

  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(rom.data()[kRoom0HeaderPc + 1], 0x00);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(room.chests_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomRollbackRestoresNormalizedWaterFillMasks) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);
  // The chest length operand is a byte count and must be divisible by three.
  rom.mutable_data()[zelda3::kChestsLengthPointer] = 0x01;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& first_room = editor->rooms()[0];
  first_room.SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  first_room.set_water_fill_sram_bit_mask(0x01);
  first_room.MarkChestsDirty();
  auto& second_room = editor->rooms()[1];
  second_room.SetWaterFillTile(/*x=*/2, /*y=*/2, /*filled=*/true);
  second_room.set_water_fill_sram_bit_mask(0x01);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveWaterFillZones = true;
  flags.kSaveChests = true;

  const auto before = rom.vector();
  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(first_room.water_fill_sram_bit_mask(), 0x01);
  EXPECT_EQ(second_room.water_fill_sram_bit_mask(), 0x01);
  EXPECT_TRUE(first_room.water_fill_dirty());
  EXPECT_TRUE(second_room.water_fill_dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     LateCoordinatorRollbackRestoresEntranceDirtyState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;

  auto& entrance = editor->entrances_[zelda3::kNumDungeonSpawnPoints];
  entrance.room_ = 0x0123;
  entrance.MarkDirty();
  const auto before = rom.vector();

  ScopedRomTransaction rom_transaction(rom);
  ASSERT_TRUE(editor->BeginSaveTransaction().ok());
  ASSERT_TRUE(editor->Save().ok());
  EXPECT_FALSE(entrance.dirty());
  EXPECT_EQ(rom.ReadWord(zelda3::kEntranceRoom).value(), 0x0123);

  // Simulate a later editor/conflict/disk failure in EditorManager.
  editor->RollbackSaveTransaction();
  rom_transaction.Rollback();

  EXPECT_TRUE(entrance.dirty());
  EXPECT_EQ(rom.vector(), before);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesExactDirtyRegularEntranceRanges) {
  constexpr int kEntranceId = 3;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  DungeonEditorV2RegularEntranceTestPeer::LoadRegularEntranceFromRom(
      *editor, kEntranceId);
  auto& entrance = DungeonEditorV2RegularEntranceTestPeer::RegularEntrance(
      *editor, kEntranceId);
  entrance.room_ = 0x0123;
  entrance.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;

  EXPECT_EQ(editor->CollectWriteRanges(),
            zelda3::RegularDungeonEntranceWriteRanges(kEntranceId));
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesExactDirtySpawnPointRanges) {
  constexpr int kSpawnId = 1;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  ASSERT_TRUE(DungeonEditorV2SpawnPointTestPeer::LoadSpawnPointFromRom(*editor,
                                                                       kSpawnId)
                  .ok());
  auto& spawn =
      DungeonEditorV2SpawnPointTestPeer::SpawnPoint(*editor, kSpawnId);
  spawn.room_id = 0x0123;
  spawn.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;

  EXPECT_EQ(editor->CollectWriteRanges(),
            zelda3::DungeonSpawnPointWriteRanges(kSpawnId));
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomPersistsDedicatedSpawnPointAndClearsDirtyState) {
  constexpr int kSpawnId = 1;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  ASSERT_TRUE(DungeonEditorV2SpawnPointTestPeer::LoadSpawnPointFromRom(*editor,
                                                                       kSpawnId)
                  .ok());
  auto& spawn =
      DungeonEditorV2SpawnPointTestPeer::SpawnPoint(*editor, kSpawnId);
  spawn.room_id = 0x0123;
  spawn.overworld_door_tilemap = 0xA5C3;
  spawn.entrance_id = 0x0084;
  spawn.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;

  const absl::Status status = editor->SaveRoom(0);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(spawn.dirty());
  auto reopened = zelda3::DungeonSpawnPoint::Load(rom, kSpawnId);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->room_id, 0x0123);
  EXPECT_EQ(reopened->overworld_door_tilemap, 0xA5C3);
  EXPECT_EQ(reopened->entrance_id, 0x0084);
}

TEST(DungeonEditorV2RomSafetyTest,
     LateCoordinatorRollbackRestoresSpawnPointDirtyState) {
  constexpr int kSpawnId = 1;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  ASSERT_TRUE(DungeonEditorV2SpawnPointTestPeer::LoadSpawnPointFromRom(*editor,
                                                                       kSpawnId)
                  .ok());
  auto& spawn =
      DungeonEditorV2SpawnPointTestPeer::SpawnPoint(*editor, kSpawnId);
  spawn.room_id = 0x0123;
  spawn.MarkDirty();
  const auto before = rom.vector();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;

  ScopedRomTransaction rom_transaction(rom);
  ASSERT_TRUE(editor->BeginSaveTransaction().ok());
  ASSERT_TRUE(editor->Save().ok());
  EXPECT_FALSE(spawn.dirty());
  EXPECT_EQ(rom.ReadWord(zelda3::kDungeonSpawnRoom + kSpawnId * 2).value(),
            0x0123);

  editor->RollbackSaveTransaction();
  rom_transaction.Rollback();

  EXPECT_TRUE(spawn.dirty());
  EXPECT_EQ(rom.vector(), before);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedSpawnPointBeforeMutation) {
  constexpr int kSpawnId = 1;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x02DB70",
        "end": "0x02DB72",
        "size": 2,
        "hook_count": 1,
        "module": "SpawnPointGuard"
      }
    ]
  }
}
)json");
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  ASSERT_TRUE(DungeonEditorV2SpawnPointTestPeer::LoadSpawnPointFromRom(*editor,
                                                                       kSpawnId)
                  .ok());
  auto& spawn =
      DungeonEditorV2SpawnPointTestPeer::SpawnPoint(*editor, kSpawnId);
  spawn.room_id = 0x0123;
  spawn.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;
  const auto before = rom.vector();

  const absl::Status status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(spawn.dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveBlocksProtectedRegularEntranceBeforeMutation) {
  constexpr int kEntranceId = 3;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x02C819",
        "end": "0x02C81B",
        "size": 2,
        "hook_count": 1,
        "module": "RegularEntranceGuard"
      }
    ]
  }
}
)json");
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  DungeonEditorV2RegularEntranceTestPeer::LoadRegularEntranceFromRom(
      *editor, kEntranceId);
  auto& entrance = DungeonEditorV2RegularEntranceTestPeer::RegularEntrance(
      *editor, kEntranceId);
  entrance.room_ = 0x0123;
  entrance.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;
  const auto before = rom.vector();

  const absl::Status status = editor->Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(entrance.dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedRegularEntranceBeforeMutation) {
  constexpr int kEntranceId = 3;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x02C819",
        "end": "0x02C81B",
        "size": 2,
        "hook_count": 1,
        "module": "RegularEntranceGuard"
      }
    ]
  }
}
)json");
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  DungeonEditorV2RegularEntranceTestPeer::LoadRegularEntranceFromRom(
      *editor, kEntranceId);
  auto& entrance = DungeonEditorV2RegularEntranceTestPeer::RegularEntrance(
      *editor, kEntranceId);
  entrance.room_ = 0x0123;
  entrance.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveEntrances = true;
  const auto before = rom.vector();

  const absl::Status status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(entrance.dirty());
}

TEST(DungeonEditorV2RomSafetyTest, SaveRejectsDirtySpawnBeforeAnyMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  auto& spawn =
      DungeonEditorV2SpawnRejectionTestPeer::LoadSpawnFromRom(*editor, 0);
  spawn.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;
  flags.kSaveEntrances = true;
  const auto before = rom.vector();

  const absl::Status status = editor->Save();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(spawn.dirty());
}

TEST(DungeonEditorV2RomSafetyTest, SaveRoomRejectsDirtySpawnBeforeAnyMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  auto& spawn =
      DungeonEditorV2SpawnRejectionTestPeer::LoadSpawnFromRom(*editor, 0);
  spawn.MarkDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;
  flags.kSaveEntrances = true;
  const auto before = rom.vector();

  const absl::Status status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(spawn.dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     LateCoordinatorRollbackRestoresBlockSlotIdentity) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupBlockTable(rom);

  const auto deleted_entry = zelda3::EncodePushableBlockEntry(
      {/*room_id=*/0, /*px=*/10, /*py=*/20, /*draw_layer=*/0,
       /*behavior_layer=*/0});
  const auto surviving_entry = zelda3::EncodePushableBlockEntry(
      {/*room_id=*/0, /*px=*/30, /*py=*/21, /*draw_layer=*/1,
       /*behavior_layer=*/1});
  const auto unmaterialized_entry = zelda3::EncodePushableBlockEntry(
      {/*room_id=*/1, /*px=*/25, /*py=*/45, /*draw_layer=*/0,
       /*behavior_layer=*/1});
  const std::array<zelda3::PushableBlockBytes, 3> entries = {
      deleted_entry, surviving_entry, unmaterialized_entry};
  for (size_t slot = 0; slot < entries.size(); ++slot) {
    const int pc = kBlocksRegion1Pc + static_cast<int>(slot * 4);
    rom.mutable_data()[pc + 0] = entries[slot].b1;
    rom.mutable_data()[pc + 1] = entries[slot].b2;
    rom.mutable_data()[pc + 2] = entries[slot].b3;
    rom.mutable_data()[pc + 3] = entries[slot].b4;
  }
  rom.mutable_data()[zelda3::kBlocksLength] = 0x0C;
  rom.mutable_data()[zelda3::kBlocksLength + 1] = 0x00;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.LoadBlocks();
  ASSERT_EQ(room.GetTileObjects().size(), 2u);
  room.RemoveTileObject(0);
  ASSERT_TRUE(room.blocks_dirty());
  ASSERT_EQ(room.GetTileObjects()[0].block_load_order(), 1);
  const auto before = rom.vector();

  ScopedRomTransaction rom_transaction(rom);
  ASSERT_TRUE(editor->BeginSaveTransaction().ok());
  ASSERT_TRUE(zelda3::SaveAllBlocks(&rom, zelda3::kNumberOfRooms,
                                    [&editor](int room_id) {
                                      return editor->rooms().GetIfMaterialized(
                                          room_id);
                                    })
                  .ok());
  EXPECT_FALSE(room.blocks_dirty());
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_NE(rom.vector(), before);

  // Simulate a later editor/conflict/disk failure after block saving already
  // committed its writes and rebased the in-memory slot identity.
  editor->RollbackSaveTransaction();
  rom_transaction.Rollback();

  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(), 1);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveWritesWaterFillZonesWhenCollisionSavingDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  ASSERT_TRUE(editor->rooms()[0].water_fill_dirty());

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_FALSE(editor->rooms()[0].water_fill_dirty());
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  ASSERT_EQ(zones[0].fill_offsets.size(), 1u);
  EXPECT_EQ(zones[0].fill_offsets[0], 65u);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesChestsWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetChests().push_back(chest_data{0x42, false});
  room.GetChests().push_back(chest_data{0x77, true});
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveChests = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer], 0x06);
  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer + 1], 0x00);

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadChests();
  ASSERT_EQ(reloaded_room.GetChests().size(), 2u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x42);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
  EXPECT_EQ(reloaded_room.GetChests()[1].id, 0x77);
  EXPECT_TRUE(reloaded_room.GetChests()[1].size);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesDirtyChestTableButNotPointerOperand) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);
  SeedChestEntry(rom, /*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room = zelda3::Room(0, &rom);
  room.LoadChests();
  ASSERT_EQ(room.GetChests().size(), 1u);
  room.GetChests()[0].id = 0x56;
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveChests = true;

  const auto ranges = editor->CollectWriteRanges();
  EXPECT_NE(std::find(ranges.begin(), ranges.end(),
                      std::pair<uint32_t, uint32_t>{
                          zelda3::kChestsLengthPointer,
                          zelda3::kChestsLengthPointer + 2}),
            ranges.end());
  EXPECT_NE(std::find(ranges.begin(), ranges.end(),
                      std::pair<uint32_t, uint32_t>{
                          kChestDataPc,
                          kChestDataPc + zelda3::kChestTableCapacityBytes}),
            ranges.end());
  EXPECT_FALSE(std::any_of(ranges.begin(), ranges.end(), [](const auto& range) {
    constexpr uint32_t kPointerBegin = zelda3::kChestsDataPointer1;
    constexpr uint32_t kPointerEnd = zelda3::kChestsDataPointer1 + 3;
    return range.first < kPointerEnd && kPointerBegin < range.second;
  })) << "The read-only chest pointer operand must not be predicted as a write";
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedChestTableBeforeMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);
  SeedChestEntry(rom, /*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x228000",
        "end": "0x228003",
        "size": 3,
        "hook_count": 1,
        "module": "ChestGuard"
      }
    ]
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room = zelda3::Room(0, &rom);
  room.LoadChests();
  ASSERT_EQ(room.GetChests().size(), 1u);
  room.GetChests()[0].id = 0x56;
  room.MarkChestsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveChests = true;
  const auto before = rom.vector();

  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.chests_dirty());

  const auto save_all_status = editor->Save();
  EXPECT_EQ(save_all_status.code(), absl::StatusCode::kPermissionDenied)
      << save_all_status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.chests_dirty());
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsChestsWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupChestTable(rom);
  SeedChestEntry(rom, /*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetChests().push_back(chest_data{0x42, false});

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveChests = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer], 0x03);
  EXPECT_EQ(rom.data()[zelda3::kChestsLengthPointer + 1], 0x00);

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadChests();
  ASSERT_EQ(reloaded_room.GetChests().size(), 1u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x55);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesPotItemsWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupPotItemTable(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);

  zelda3::PotItem first;
  first.position = 0x1234;
  first.item = 0x56;
  room.GetPotItems().push_back(first);
  room.MarkPotItemsDirty();

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSavePotItems = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsPotItemsWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupPotItemTable(rom);
  SeedPotItemBytes(rom, kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);

  zelda3::PotItem first;
  first.position = 0x5678;
  first.item = 0x9A;
  room.GetPotItems().push_back(first);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSavePotItems = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveSkipsPotItemsWhenDisabledForSelectedRoom) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupRoomObjectPointers(rom);
  SetupSpritePointers(rom);
  SetupHeaderPointers(rom);
  SetupChestTable(rom);
  SetupPotItemTable(rom);
  SeedPotItemBytes(rom, kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.GetPotItems().push_back(zelda3::PotItem{0x5678, 0x9A});

  editor->add_room(0);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room(0, &rom);
  reloaded_room.LoadPotItems();
  ASSERT_EQ(reloaded_room.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
}

TEST(DungeonEditorV2RomSafetyTest, SaveWritesRoomHeadersWhenEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.SetBlockset(0x1C);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveRoomHeaders = true;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  EXPECT_EQ(reloaded_room.palette(), 0x2A);
  EXPECT_EQ(reloaded_room.blockset(), 0x1C);
  EXPECT_EQ(reloaded_room.message_id(), 0x1357);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesDirtyRoomMessageSlot) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  room.SetLoaded(true);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;

  const auto ranges = editor->CollectWriteRanges();
  EXPECT_NE(
      std::find(ranges.begin(), ranges.end(),
                std::pair<uint32_t, uint32_t>{zelda3::kMessagesIdDungeon,
                                              zelda3::kMessagesIdDungeon + 2}),
      ranges.end());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveRoomBlocksProtectedMessageSlotBeforeMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto project = MakeProjectWithManifest(R"json(
{
  "manifest_version": 3,
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x07F61D",
        "end": "0x07F61F",
        "size": 2,
        "hook_count": 1,
        "module": "DungeonMessageGuard"
      }
    ]
  }
}
)json");

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project;
  editor->SetDependencies(dependencies);
  auto& room = editor->rooms()[0];
  room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  room.SetLoaded(true);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveRoomHeaders = true;
  const auto before = rom.vector();

  const auto status = editor->SaveRoom(0);

  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(rom.vector(), before);
  EXPECT_TRUE(room.header_dirty());
}

TEST(DungeonEditorV2RomSafetyTest, SaveSkipsRoomHeadersWhenDisabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupHeaderPointers(rom);
  SeedHeaderBytes(rom, kRoom0HeaderPc,
                  {0x00, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                   0x0C, 0x0D, 0x0E, 0x0F},
                  /*message_id=*/0x1111, /*room_id=*/0);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.SetPalette(0x2A);
  room.SetBlockset(0x1C);
  room.SetMessageId(0x1357);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveRoomHeaders = false;

  auto status = editor->Save();
  ASSERT_TRUE(status.ok()) << status.message();

  zelda3::Room reloaded_room = zelda3::LoadRoomHeaderFromRom(&rom, 0);
  EXPECT_EQ(reloaded_room.palette(), 0x03);
  EXPECT_EQ(reloaded_room.blockset(), 0x04);
  EXPECT_EQ(reloaded_room.message_id(), 0x1111);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesSkipsCleanLoadedRoomsUntilDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  SetupRoomObjectPointers(rom);
  SetupHeaderPointers(rom);

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0] = zelda3::LoadRoomFromRom(&rom, 0);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = true;
  d.kSaveRoomHeaders = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetPalette(0x2A);
  EXPECT_FALSE(editor->CollectWriteRanges().empty());
}

TEST(DungeonEditorV2RomSafetyTest, PendingRoomCountTracksRoomDirtyState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  EXPECT_EQ(editor->PendingRoomCount(), 0);
  EXPECT_FALSE(editor->HasPendingRoomChanges());

  editor->rooms()[0].SetPalette(0x2A);
  editor->rooms()[1].MarkPotItemsDirty();

  EXPECT_EQ(editor->PendingRoomCount(), 2);
  EXPECT_TRUE(editor->HasPendingRoomChanges());

  editor->FocusRoom(0);
  *editor->mutable_current_room_id() = 0;
  EXPECT_TRUE(editor->CurrentRoomHasPendingChanges());

  editor->rooms()[0].ClearSaveDirtyState();
  editor->rooms()[0].ClearCustomCollisionDirty();
  editor->rooms()[0].ClearWaterFillDirty();
  editor->rooms()[1].ClearSaveDirtyState();
  editor->rooms()[1].ClearCustomCollisionDirty();
  editor->rooms()[1].ClearWaterFillDirty();

  EXPECT_EQ(editor->PendingRoomCount(), 0);
  EXPECT_FALSE(editor->HasPendingRoomChanges());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveTransactionRollbackRestoresRoomAndPitDirtyState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data(&rom);
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->SetGameData(&game_data);

  auto& room = editor->rooms()[0];
  room.MarkHeaderDirty();
  room.MarkObjectStreamDirty();
  room.MarkSpritesDirty();
  room.MarkChestsDirty();
  room.MarkPotItemsDirty();
  room.MarkTorchesDirty();
  room.MarkBlocksDirty();
  room.MarkCustomCollisionDirty();
  room.MarkWaterFillDirty();
  game_data.pit_damage_table.MarkDirty();

  ASSERT_TRUE(editor->BeginSaveTransaction().ok());
  room.ClearSaveDirtyState();
  room.ClearCustomCollisionDirty();
  room.ClearWaterFillDirty();
  game_data.pit_damage_table.ClearDirty();

  editor->RollbackSaveTransaction();

  EXPECT_TRUE(room.header_dirty());
  EXPECT_TRUE(room.object_stream_dirty());
  EXPECT_TRUE(room.sprites_dirty());
  EXPECT_TRUE(room.chests_dirty());
  EXPECT_TRUE(room.pot_items_dirty());
  EXPECT_TRUE(room.torches_dirty());
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_TRUE(room.custom_collision_dirty());
  EXPECT_TRUE(room.water_fill_dirty());
  EXPECT_TRUE(game_data.pit_damage_table.dirty());
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesDuplicateWaterFillMasksInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(0x01);
  editor->rooms()[1].SetWaterFillTile(/*x=*/2, /*y=*/2, /*filled=*/true);
  editor->rooms()[1].set_water_fill_sram_bit_mask(
      0x01);  // Duplicate on purpose.

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);
  EXPECT_EQ(editor->rooms()[1].water_fill_sram_bit_mask(), 0x02);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 2u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
  EXPECT_EQ(zones[1].room_id, 1);
  EXPECT_EQ(zones[1].sram_bit_mask, 0x02);
}

TEST(DungeonEditorV2RomSafetyTest,
     SaveNormalizesInvalidWaterFillMaskInsteadOfFailing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetWaterFillTile(/*x=*/4, /*y=*/4, /*filled=*/true);
  editor->rooms()[0].set_water_fill_sram_bit_mask(
      0x03);  // Invalid (not single-bit).

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = true;

  auto status = editor->Save();
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(editor->rooms()[0].water_fill_sram_bit_mask(), 0x01);

  auto zones_or = zelda3::LoadWaterFillTable(&rom);
  ASSERT_TRUE(zones_or.ok()) << zones_or.status().message();
  const auto& zones = *zones_or;
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x01);
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesWaterFillRegionWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveWaterFillZones = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetWaterFillTile(/*x=*/1, /*y=*/1, /*filled=*/true);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kWaterFillTableStart));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kWaterFillTableEnd));
}

TEST(DungeonEditorV2RomSafetyTest,
     CollectWriteRangesIncludesCustomCollisionRegionsWhenDirtyAndEnabled) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  DungeonSaveFlagsGuard guard;
  ConfigureMinimalDungeonSave();
  auto& d = core::FeatureFlags::get().dungeon;
  d.kSaveObjects = false;
  d.kSaveCollision = true;

  EXPECT_TRUE(editor->CollectWriteRanges().empty());

  editor->rooms()[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  auto ranges = editor->CollectWriteRanges();
  ASSERT_EQ(ranges.size(), 2u);
  EXPECT_EQ(ranges[0].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers));
  EXPECT_EQ(ranges[0].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionRoomPointers +
                                  (zelda3::kNumberOfRooms * 3)));
  EXPECT_EQ(ranges[1].first,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataPosition));
  EXPECT_EQ(ranges[1].second,
            static_cast<uint32_t>(zelda3::kCustomCollisionDataSoftEnd));
}

TEST(DungeonEditorV2RomSafetyTest, UndoSnapshotLeakDetection) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();

  // Add an initial object to make changes detectable.
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x01, /*x=*/10, /*y=*/10,
                                        /*size=*/0, /*layer=*/0));

  // First BeginUndoSnapshot should work normally.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after BeginUndoSnapshot";

  // Second BeginUndoSnapshot without FinalizeUndoAction should auto-finalize
  // the first snapshot (preventing leak) and start a new one.
  // This tests that double-Begin is handled gracefully.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should still be true after second "
         "BeginUndoSnapshot";

  // FinalizeUndoAction should clear the flag.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after FinalizeUndoAction";

  // BeginUndoSnapshot after FinalizeUndoAction should work normally again.
  editor->BeginUndoSnapshot(0);
  EXPECT_TRUE(editor->has_pending_undo_)
      << "has_pending_undo_ should be true after new BeginUndoSnapshot";

  // Clean up pending undo.
  editor->FinalizeUndoAction(0);
  EXPECT_FALSE(editor->has_pending_undo_)
      << "has_pending_undo_ should be false after final FinalizeUndoAction";
}

TEST(DungeonEditorV2RomSafetyTest,
     ObjectStreamUndoRedoRestoresSelectionIdentity) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonSaveFlagsGuard guard;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  auto& room = editor->rooms()[0];
  room.SetLoaded(true);
  room.ClearTileObjects();
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x11, /*x=*/10, /*y=*/10,
                                        /*size=*/0, /*layer=*/0));
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x22, /*x=*/20, /*y=*/20,
                                        /*size=*/0, /*layer=*/0));
  room.AddTileObject(zelda3::RoomObject(/*id=*/0x33, /*x=*/30, /*y=*/30,
                                        /*size=*/0, /*layer=*/1));

  auto* viewer = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer, nullptr);
  auto& interaction = viewer->object_interaction();
  interaction.SetCurrentRoom(&editor->rooms(), 0);
  interaction.SetSelectedObjects({0});

  editor->BeginUndoSnapshot(0);
  auto mutation =
      zelda3::ReassignObjectStorage(room.GetTileObjects(), {0}, /*layer=*/1);
  ASSERT_TRUE(mutation.ok()) << mutation.status();
  ASSERT_TRUE(mutation->changed);
  ASSERT_THAT(mutation->selected_indices, ::testing::ElementsAre(2));
  interaction.SetSelectedObjects(mutation->selected_indices);
  editor->FinalizeUndoAction(0);

  ASSERT_EQ(room.GetTileObjects().size(), 3u);
  EXPECT_EQ(room.GetTileObjects()[2].id_, 0x11);
  EXPECT_THAT(interaction.GetSelectedObjectIndices(),
              ::testing::ElementsAre(2));

  ASSERT_TRUE(editor->Undo().ok());
  ASSERT_EQ(room.GetTileObjects().size(), 3u);
  EXPECT_EQ(room.GetTileObjects()[0].id_, 0x11);
  EXPECT_THAT(interaction.GetSelectedObjectIndices(),
              ::testing::ElementsAre(0));

  ASSERT_TRUE(editor->Redo().ok());
  ASSERT_EQ(room.GetTileObjects().size(), 3u);
  EXPECT_EQ(room.GetTileObjects()[2].id_, 0x11);
  EXPECT_THAT(interaction.GetSelectedObjectIndices(),
              ::testing::ElementsAre(2));
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUEviction) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 25 viewers (exceeds kMaxCachedViewers = 20)
  for (int i = 0; i < 25; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr) << "Failed to get viewer for room " << i;
  }

  // Verify that viewer count is at most kMaxCachedViewers
  EXPECT_LE(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers))
      << "Viewer cache should not exceed kMaxCachedViewers";

  // Verify LRU ordering: most recent rooms (20-24) should still be cached
  for (int i = 20; i < 25; ++i) {
    EXPECT_TRUE(editor->room_viewers_.Contains(i))
        << "Recent room " << i << " should still be cached";
  }

  // Verify oldest rooms (0-4) were evicted
  int evicted_count = 0;
  for (int i = 0; i < 5; ++i) {
    if (!editor->room_viewers_.Contains(i)) {
      evicted_count++;
    }
  }
  EXPECT_GT(evicted_count, 0) << "Some old rooms should have been evicted";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheNeverEvictsActiveRooms) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Mark room 0 as active
  editor->active_rooms_.push_back(0);

  // Get viewer for room 0
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create 25 more viewers (should trigger eviction)
  for (int i = 1; i < 26; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  // Verify room 0 is still cached (it's active, so should not be evicted)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Active room 0 should never be evicted";

  // Verify the same viewer instance is returned
  EXPECT_EQ(editor->GetViewerForRoom(0), viewer0)
      << "Active room viewer should remain the same instance";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

TEST(DungeonEditorV2RomSafetyTest, ViewerCacheLRUAccessOrderUpdate) {
  test::MockRom rom;
  ASSERT_TRUE(rom.SetTestData(std::vector<uint8_t>(0x8000, 0)).ok());

  // Disable workbench mode to test individual room viewers
  auto prev_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;

  auto editor = std::make_unique<DungeonEditorV2>(&rom);

  // Create 20 viewers (exactly at limit)
  for (int i = 0; i < 20; ++i) {
    auto* viewer = editor->GetViewerForRoom(i);
    ASSERT_NE(viewer, nullptr);
  }

  EXPECT_EQ(editor->room_viewers_.Size(),
            static_cast<size_t>(DungeonEditorV2::kMaxCachedViewers));

  // Access room 0 again (should move it to the back of LRU)
  auto* viewer0 = editor->GetViewerForRoom(0);
  ASSERT_NE(viewer0, nullptr);

  // Create one more viewer (should evict room 1, not room 0)
  auto* viewer20 = editor->GetViewerForRoom(20);
  ASSERT_NE(viewer20, nullptr);

  // Verify room 0 is still cached (was recently accessed)
  EXPECT_TRUE(editor->room_viewers_.Contains(0))
      << "Recently accessed room 0 should not be evicted";

  // Verify room 1 was evicted (oldest in LRU after room 0 was accessed)
  EXPECT_FALSE(editor->room_viewers_.Contains(1))
      << "Room 1 should have been evicted as the oldest";

  // Restore workbench flag
  core::FeatureFlags::get().dungeon.kUseWorkbench = prev_workbench;
}

}  // namespace yaze::editor
