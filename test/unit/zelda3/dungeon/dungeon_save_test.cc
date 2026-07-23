#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_block_codec.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze {
namespace zelda3 {
namespace test {

class DungeonSaveTest : public ::testing::Test {
 protected:
  struct TestChestRecord {
    uint16_t room_id;
    uint8_t item;
    bool big;
  };

  static constexpr int kChestDataPc = 0x110000;
  static constexpr int kPotRoom0Pc = 0x008000;
  static constexpr int kPotRoom1Pc = 0x008020;
  static constexpr int kPotRoom2Pc = 0x008040;
  static constexpr int kPitDataPc = 0x112000;
  static constexpr int kBlocksRegion1Pc = 0x113000;
  static constexpr int kBlocksRegion2Pc = 0x113080;
  static constexpr int kBlocksRegion3Pc = 0x113100;
  static constexpr int kBlocksRegion4Pc = 0x113180;

  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    // Create a minimal ROM for testing (2MB)
    std::vector<uint8_t> dummy_data(0x200000, 0);
    rom_->LoadFromData(dummy_data);

    SetupRoomObjectPointers();
    SetupSpritePointers();

    room_ = std::make_unique<Room>(0, rom_.get());
  }

  void SetupRoomObjectPointers() {
    // 1. Setup kRoomObjectPointer (0x874C) to point to our table at 0xF8000
    // The code reads 3 bytes from kRoomObjectPointer
    // int object_pointer = (rom_data[room_object_pointer + 2] << 16) + ...
    // We want object_pointer to be 0xF8000 (PC address)
    // 0xF8000 is 1F:8000 in LoROM (Bank 1F)
    // So we write 00 80 1F at 0x874C
    int ptr_loc = kRoomObjectPointer;
    rom_->mutable_data()[ptr_loc] = 0x00;
    rom_->mutable_data()[ptr_loc + 1] = 0x80;
    rom_->mutable_data()[ptr_loc + 2] = 0x1F;

    // 2. Setup Room 0 pointer at 0xF8000
    // We want Room 0 data to be at 0x100000 (Bank 20, PC 0x100000)
    // 0x100000 is 20:8000 in LoROM
    // Write 00 80 20 at 0xF8000
    int table_loc = 0xF8000;
    rom_->mutable_data()[table_loc] = 0x00;
    rom_->mutable_data()[table_loc + 1] = 0x80;
    rom_->mutable_data()[table_loc + 2] = 0x20;

    // 3. Setup Room 1 pointer at 0xF8003 (for size calculation)
    // We want Room 0 to have 0x100 bytes of space
    // So Room 1 starts at 0x100100 (20:8100)
    // Write 00 81 20 at 0xF8003
    rom_->mutable_data()[table_loc + 3] = 0x00;
    rom_->mutable_data()[table_loc + 4] = 0x81;
    rom_->mutable_data()[table_loc + 5] = 0x20;

    // 4. Seed Room 0 object data at 0x100000.
    //
    // Room::LoadObjects expects the room pointer table to point directly at the
    // 2-byte room header (floor gfx/layout). The object stream begins at +2.
    const int room_data_loc = 0x100000;
    rom_->mutable_data()[room_data_loc + 0] = 0x00;  // floor gfx nibble data
    rom_->mutable_data()[room_data_loc + 1] = 0x00;  // layout/flags

    // 5. Provide a valid empty object stream so LoadObjects doesn't scan zeros.
    // Format: layer0 terminator, layer1 terminator, door marker, door terminator.
    const int objects_start = room_data_loc + 2;
    std::vector<uint8_t> empty = {
        0xFF, 0xFF,  // end layer0
        0xFF, 0xFF,  // end layer1
        0xF0, 0xFF,  // door marker ($FFF0)
        0xFF, 0xFF,  // door terminator ($FFFF) (also end layer2)
    };
    ASSERT_TRUE(rom_->WriteVector(objects_start, empty).ok());
  }

  void SetupSpritePointers() {
    // 1. Setup kRoomsSpritePointer (0x4C298)
    // Points to table in Bank 09. Let's put table at 0x48000 (09:8000)
    int ptr_loc = kRoomsSpritePointer;
    rom_->mutable_data()[ptr_loc] = 0x00;
    rom_->mutable_data()[ptr_loc + 1] = 0x80;
    // Bank is hardcoded to 0x09 in code, so we only write low 2 bytes.

    // 2. Setup Sprite Pointer Table at 0x48000 (09:8000)
    // Room 0 pointer -> sprite list at 0x49000 (09:9000)
    // Write 00 90 at 0x48000
    int table_loc = 0x48000;
    rom_->mutable_data()[table_loc] = 0x00;
    rom_->mutable_data()[table_loc + 1] = 0x90;

    // Room 1 pointer at 0x48002 (for size calculation)
    // Let's give 0x50 bytes for sprites.
    // Next room at 0x49050 (09:9050)
    // Write 50 90 at 0x48002
    rom_->mutable_data()[table_loc + 2] = 0x50;
    rom_->mutable_data()[table_loc + 3] = 0x90;

    // 3. Setup Sprite Data at 0x49000
    // Sortsprite byte (0 or 1)
    rom_->mutable_data()[0x49000] = 0x00;
    // End of sprites (0xFF)
    rom_->mutable_data()[0x49001] = 0xFF;
  }

  void SetSpriteRoomPointer(int room_id, int pc_addr) {
    const uint16_t pointer = static_cast<uint16_t>(PcToSnes(pc_addr));
    const int table_loc = 0x48000 + room_id * 2;
    rom_->mutable_data()[table_loc] = pointer & 0xFF;
    rom_->mutable_data()[table_loc + 1] = (pointer >> 8) & 0xFF;
  }

  int GetSpriteRoomPointer(int room_id) const {
    const int table_loc = 0x48000 + room_id * 2;
    const uint16_t pointer =
        (static_cast<uint16_t>(rom_->data()[table_loc + 1]) << 8) |
        rom_->data()[table_loc];
    return static_cast<int>(SnesToPc(0x090000 | pointer));
  }

  void WriteLongPointer(int addr, uint32_t snes_addr) {
    rom_->mutable_data()[addr + 0] = snes_addr & 0xFF;
    rom_->mutable_data()[addr + 1] = (snes_addr >> 8) & 0xFF;
    rom_->mutable_data()[addr + 2] = (snes_addr >> 16) & 0xFF;
  }

  void SetObjectRoomPointer(int room_id, int pc_addr) {
    WriteLongPointer(0xF8000 + room_id * 3, PcToSnes(pc_addr));
  }

  int GetObjectRoomPointer(int room_id) const {
    const int pointer_slot = 0xF8000 + room_id * 3;
    const uint32_t pointer =
        (static_cast<uint32_t>(rom_->data()[pointer_slot + 2]) << 16) |
        (static_cast<uint32_t>(rom_->data()[pointer_slot + 1]) << 8) |
        rom_->data()[pointer_slot];
    return static_cast<int>(SnesToPc(pointer));
  }

  int GetDoorPointer(int room_id) const {
    const int pointer_slot = kDoorPointers + room_id * 3;
    const uint32_t pointer =
        (static_cast<uint32_t>(rom_->data()[pointer_slot + 2]) << 16) |
        (static_cast<uint32_t>(rom_->data()[pointer_slot + 1]) << 8) |
        rom_->data()[pointer_slot];
    return static_cast<int>(SnesToPc(pointer));
  }

  void WriteEmptyObjectStream(int pc_addr, uint8_t header0 = 0,
                              uint8_t header1 = 0) {
    const std::vector<uint8_t> bytes = {
        header0, header1, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
    };
    ASSERT_TRUE(rom_->WriteVector(pc_addr, bytes).ok());
  }

  DungeonStreamLayout MakeObjectLayout(DungeonStreamPcRange allocation = {
                                           0x100200, 0x100400}) const {
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kObject;
    layout.pointer_table_pc = 0xF8000;
    layout.pointer_count = 2;
    layout.pointer_encoding = DungeonPointerEncoding::kLong24;
    layout.data_ranges = {{0x100000, 0x100400}};
    layout.allocation_ranges = {allocation};
    return layout;
  }

  DungeonStreamLayout MakeSpriteLayout(DungeonStreamPcRange allocation = {
                                           0x49200, 0x49300}) const {
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kSprite;
    layout.pointer_table_pc = 0x48000;
    layout.pointer_count = 2;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x09;
    layout.data_ranges = {{0x49000, 0x49300}};
    layout.allocation_ranges = {allocation};
    return layout;
  }

  void SetupChestTable() {
    WriteLongPointer(kChestsDataPointer1, PcToSnes(kChestDataPc));
    rom_->mutable_data()[kChestsLengthPointer] = 0x00;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    std::fill_n(rom_->mutable_data() + kChestDataPc, kChestTableCapacityBytes,
                0x00);
  }

  void SeedChestEntry(int room_id, uint8_t chest_id, bool big) {
    const uint16_t word = static_cast<uint16_t>(room_id) | (big ? 0x8000 : 0);
    rom_->mutable_data()[kChestsLengthPointer] = kChestTableRecordSize;
    rom_->mutable_data()[kChestsLengthPointer + 1] = 0x00;
    rom_->mutable_data()[kChestDataPc + 0] = word & 0xFF;
    rom_->mutable_data()[kChestDataPc + 1] = (word >> 8) & 0xFF;
    rom_->mutable_data()[kChestDataPc + 2] = chest_id;
  }

  void SeedChestRecords(std::initializer_list<TestChestRecord> records) {
    ASSERT_LE(records.size(), static_cast<size_t>(kChestTableCapacityRecords));
    const uint16_t byte_length = static_cast<uint16_t>(
        records.size() * static_cast<size_t>(kChestTableRecordSize));
    rom_->mutable_data()[kChestsLengthPointer] = byte_length & 0xFF;
    rom_->mutable_data()[kChestsLengthPointer + 1] = byte_length >> 8;
    int offset = kChestDataPc;
    for (const TestChestRecord& record : records) {
      const uint16_t word =
          record.room_id | (record.big ? static_cast<uint16_t>(0x8000) : 0);
      rom_->mutable_data()[offset++] = word & 0xFF;
      rom_->mutable_data()[offset++] = (word >> 8) & 0xFF;
      rom_->mutable_data()[offset++] = record.item;
    }
  }

  void SetupPotItemTable() {
    const uint16_t room0_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom0Pc));
    const uint16_t room1_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom1Pc));
    const uint16_t room2_ptr = static_cast<uint16_t>(PcToSnes(kPotRoom2Pc));

    rom_->mutable_data()[kRoomItemsPointers + 0] = room0_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 1] = (room0_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 2] = room1_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 3] = (room1_ptr >> 8) & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 4] = room2_ptr & 0xFF;
    rom_->mutable_data()[kRoomItemsPointers + 5] = (room2_ptr >> 8) & 0xFF;

    rom_->mutable_data()[kPotRoom0Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom0Pc + 1] = 0xFF;
    rom_->mutable_data()[kPotRoom1Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom1Pc + 1] = 0xFF;
    rom_->mutable_data()[kPotRoom2Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom2Pc + 1] = 0xFF;
  }

  void SetupSharedPotItemTable() {
    rom_->mutable_data()[kPotRoom0Pc + 0] = 0xFF;
    rom_->mutable_data()[kPotRoom0Pc + 1] = 0xFF;
    for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
      SetPotRoomPointer(room_id, kPotRoom0Pc);
    }
  }

  DungeonStreamLayout MakePotRepackLayout(
      DungeonStreamPcRange allocation) const {
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kPotItem;
    layout.pointer_table_pc = kRoomItemsPointers;
    layout.pointer_count = kNumberOfRooms;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x01;
    layout.data_ranges = {
        {kPotRoom0Pc, kRoomItemsPointers},
        {kRoomItemsPointers + kNumberOfRooms * 2, 0x010000},
    };
    layout.allocation_ranges = {allocation};
    return layout;
  }

  void SetPotRoomPointer(int room_id, int pc_addr) {
    const uint16_t pointer = static_cast<uint16_t>(PcToSnes(pc_addr));
    const int ptr_off = kRoomItemsPointers + room_id * 2;
    rom_->mutable_data()[ptr_off] = pointer & 0xFF;
    rom_->mutable_data()[ptr_off + 1] = (pointer >> 8) & 0xFF;
  }

  int GetPotRoomPointer(int room_id) const {
    const int ptr_off = kRoomItemsPointers + room_id * 2;
    const uint16_t pointer =
        static_cast<uint16_t>(rom_->data()[ptr_off]) |
        (static_cast<uint16_t>(rom_->data()[ptr_off + 1]) << 8);
    return static_cast<int>(SnesToPc(0x010000u | pointer));
  }

  void SeedPotItemBytes(int pc_addr, std::initializer_list<uint8_t> bytes) {
    std::copy(bytes.begin(), bytes.end(), rom_->mutable_data() + pc_addr);
  }

  void SetupPitRegion() {
    // `kPitCount` is the LDX.w immediate (the maximum X offset, NOT a
    // byte count). max_offset = 0x00 → single 2-byte entry at offset 0.
    // See `RoomsWithPitDamage table format pins` in
    // `test/integration/zelda3/dungeon_save_region_test.cc`.
    WriteLongPointer(kPitPointer, PcToSnes(kPitDataPc));
    rom_->mutable_data()[kPitCount] = 0x00;
    rom_->mutable_data()[kPitDataPc + 0] = 0x34;
    rom_->mutable_data()[kPitDataPc + 1] = 0x12;
  }

  void SetupBlockRegions() {
    SeedBlockLoaderOperand(kBlocksPointer1);
    SeedBlockLoaderOperand(kBlocksPointer2);
    SeedBlockLoaderOperand(kBlocksPointer3);
    SeedBlockLoaderOperand(kBlocksPointer4);

    WriteLongPointer(kBlocksPointer1, PcToSnes(kBlocksRegion1Pc));
    WriteLongPointer(kBlocksPointer2, PcToSnes(kBlocksRegion2Pc));
    WriteLongPointer(kBlocksPointer3, PcToSnes(kBlocksRegion3Pc));
    WriteLongPointer(kBlocksPointer4, PcToSnes(kBlocksRegion4Pc));

    rom_->mutable_data()[kBlocksLength] = 0x04;
    rom_->mutable_data()[kBlocksLength + 1] = 0x00;

    rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0xAA;
    rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0xBB;
    rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0xCC;
    rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0xDD;
  }

  void SeedBlockLoaderOperand(int operand_pc) {
    rom_->mutable_data()[operand_pc - 1] = 0xBF;  // LDA.l operand,X
    rom_->mutable_data()[operand_pc + 3] = 0x9D;  // STA.w addr,X
  }

  static RoomObject MakePushableBlock(int px, int py, int draw_layer,
                                      int behavior_layer = 0) {
    RoomObject block(0x0E00, px, py, 0, draw_layer);
    block.set_options(ObjectOption::Block);
    block.set_block_behavior_layer(behavior_layer);
    return block;
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<Room> room_;
};

TEST_F(DungeonSaveTest, SaveObjects_FitsInSpace) {
  // Add a few objects
  RoomObject obj1(0x10, 10, 10, 0, 0);
  room_->AddObject(obj1);

  auto status = room_->SaveObjects();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(DungeonSaveTest,
       SaveObjects_InvalidObjectFailsWithoutRomMutationAndStaysDirty) {
  room_->AddTileObject(RoomObject(/*id=*/0x140, /*x=*/10, /*y=*/10,
                                  /*size=*/0, /*layer=*/0));
  const auto before = rom_->vector();

  const auto status = room_->SaveObjects();

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_StreamMarkerCollisionFailsWithoutRomMutationAndStaysDirty) {
  room_->AddTileObject(RoomObject(/*id=*/0x010, /*x=*/60, /*y=*/63,
                                  /*size=*/3, /*layer=*/0));
  const auto before = rom_->vector();

  const auto status = room_->SaveObjects();

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest, SaveObjects_TooLarge) {
  // Add MANY objects to exceed 256 bytes
  // Each object encodes to 3 bytes.
  // We need > 85 objects.
  for (int i = 0; i < 100; ++i) {
    RoomObject obj(0x10, 10, 10, 0, 0);
    room_->AddObject(obj);
  }

  auto status = room_->SaveObjects();
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonSaveTest, SaveObjects_SharedStreamFailsWithoutMutation) {
  WriteLongPointer(0xF8000 + 3, PcToSnes(0x100000));
  ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  const auto before = rom_->vector();

  const auto status = room_->SaveObjects();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderUsesPerFieldRmwAndPreservesPayload) {
  constexpr int kStreamPc = 0x100000;
  WriteEmptyObjectStream(kStreamPc, 0xA5, 0xE3);
  const std::vector<uint8_t> payload(rom_->data() + kStreamPc + 2,
                                     rom_->data() + kStreamPc + 10);

  room_->set_floor1(0x02);
  EXPECT_TRUE(room_->object_stream_header_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
  ASSERT_TRUE(room_->SaveObjectStreamHeader().ok());
  EXPECT_EQ(rom_->data()[kStreamPc], 0xA2);
  EXPECT_EQ(rom_->data()[kStreamPc + 1], 0xE3);

  room_->set_floor2(0x03);
  ASSERT_TRUE(room_->SaveObjectStreamHeader().ok());
  EXPECT_EQ(rom_->data()[kStreamPc], 0x32);
  EXPECT_EQ(rom_->data()[kStreamPc + 1], 0xE3);

  room_->SetLayoutId(0x04);
  ASSERT_TRUE(room_->SaveObjectStreamHeader().ok());
  EXPECT_EQ(rom_->data()[kStreamPc], 0x32);
  EXPECT_EQ(rom_->data()[kStreamPc + 1], 0xF3);
  EXPECT_TRUE(
      std::equal(payload.begin(), payload.end(), rom_->data() + kStreamPc + 2));
  EXPECT_FALSE(room_->object_stream_header_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderRejectsOutOfRangeValueWithoutMutation) {
  WriteEmptyObjectStream(0x100000, 0xA5, 0xE3);
  room_->SetLayoutId(0x08);
  const auto before = rom_->vector();

  const auto status = room_->SaveObjectStreamHeader();

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_header_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderSharedStreamFailsClosedWithoutManifest) {
  SetObjectRoomPointer(1, 0x100000);
  WriteEmptyObjectStream(0x100000, 0xA5, 0xE3);
  room_->SetLayoutId(0x03);
  const auto before = rom_->vector();

  const auto status = room_->SaveObjectStreamHeader();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_header_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderDetachesSharedStreamWithManifestLayout) {
  constexpr int kOldStreamPc = 0x100000;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(1, kOldStreamPc);
  WriteEmptyObjectStream(kOldStreamPc, 0xA5, 0xE3);
  const std::vector<uint8_t> old_stream(rom_->data() + kOldStreamPc,
                                        rom_->data() + kOldStreamPc + 10);
  room_->set_floor2(0x03);
  const auto layout = MakeObjectLayout();

  const auto status = room_->SaveObjectStreamHeader(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kOldStreamPc);
  EXPECT_EQ(GetDoorPointer(0), kRelocatedStreamPc + 8);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc], 0x35);
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc + 1], 0xE3);
  EXPECT_TRUE(std::equal(old_stream.begin() + 2, old_stream.end(),
                         rom_->data() + kRelocatedStreamPc + 2));
  EXPECT_FALSE(room_->object_stream_header_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderDetachesOverlappingSuffixWithManifestLayout) {
  constexpr int kOwnerStreamPc = 0x100000;
  constexpr int kSuffixStreamPc = kOwnerStreamPc + 2;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(0, kSuffixStreamPc);
  SetObjectRoomPointer(1, kOwnerStreamPc);
  ASSERT_TRUE(rom_->WriteVector(kOwnerStreamPc, {0x00, 0x00, 0xFF, 0xFF, 0xFF,
                                                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
                  .ok());
  const std::vector<uint8_t> owner_stream(rom_->data() + kOwnerStreamPc,
                                          rom_->data() + kOwnerStreamPc + 10);
  room_->SetLayoutId(0x03);
  const auto layout = MakeObjectLayout();

  const auto status = room_->SaveObjectStreamHeader(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kOwnerStreamPc);
  EXPECT_TRUE(std::equal(owner_stream.begin(), owner_stream.end(),
                         rom_->data() + kOwnerStreamPc));
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc], 0xFF);
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc + 1], 0xEF);
  EXPECT_TRUE(std::equal(owner_stream.begin() + 4, owner_stream.end(),
                         rom_->data() + kRelocatedStreamPc + 2));
  EXPECT_FALSE(room_->object_stream_header_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectStreamHeaderAllocatorExhaustionRollsBackRomAndDirtyState) {
  SetObjectRoomPointer(1, 0x100000);
  WriteEmptyObjectStream(0x100000, 0xA5, 0xE3);
  room_->set_floor1(0x02);
  const auto before = rom_->vector();
  const auto layout = MakeObjectLayout({0x100200, 0x100204});

  const auto status = room_->SaveObjectStreamHeader(&layout);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_header_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjectsThenHeaderPreservesCombinedPayloadAndPropertyEdits) {
  constexpr int kOldStreamPc = 0x100000;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(1, kOldStreamPc);
  WriteEmptyObjectStream(kOldStreamPc, 0xA5, 0xE3);
  ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  room_->SetLayoutId(0x03);
  const auto encoded = room_->EncodeObjects();
  const auto layout = MakeObjectLayout();

  ASSERT_TRUE(room_->SaveObjects(&layout).ok());
  ASSERT_TRUE(room_->SaveObjectStreamHeader(&layout).ok());

  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kOldStreamPc);
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc], 0xA5);
  EXPECT_EQ(rom_->data()[kRelocatedStreamPc + 1], 0xEF);
  EXPECT_TRUE(std::equal(encoded.begin(), encoded.end(),
                         rom_->data() + kRelocatedStreamPc + 2));
  EXPECT_FALSE(room_->object_stream_dirty());
  EXPECT_FALSE(room_->object_stream_header_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_SharedStreamDetachesWithDeclaredAllocatorSpace) {
  constexpr int kOldStreamPc = 0x100000;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(1, kOldStreamPc);
  WriteEmptyObjectStream(kOldStreamPc, 0xA5, 0x5A);
  ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  Room::Door door;
  door.position = 3;
  door.direction = DoorDirection::North;
  door.type = DoorType::NormalDoor;
  room_->AddDoor(door);

  const std::vector<uint8_t> old_stream(rom_->data() + kOldStreamPc,
                                        rom_->data() + kOldStreamPc + 16);
  std::vector<uint8_t> expected_replacement = {0xA5, 0x5A};
  const auto encoded = room_->EncodeObjects();
  expected_replacement.insert(expected_replacement.end(), encoded.begin(),
                              encoded.end());
  const int expected_door_pc =
      kRelocatedStreamPc + static_cast<int>(expected_replacement.size()) -
      static_cast<int>(room_->GetDoors().size()) * 2 - 2;
  const auto layout = MakeObjectLayout();

  const auto status = room_->SaveObjects(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kOldStreamPc);
  EXPECT_EQ(GetDoorPointer(0), expected_door_pc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_TRUE(std::equal(expected_replacement.begin(),
                         expected_replacement.end(),
                         rom_->data() + kRelocatedStreamPc));
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_OverlappingSuffixDetachesWithoutMutatingOwnerStream) {
  constexpr int kOwnerStreamPc = 0x100000;
  constexpr int kSuffixStreamPc = kOwnerStreamPc + 2;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(0, kSuffixStreamPc);
  SetObjectRoomPointer(1, kOwnerStreamPc);
  ASSERT_TRUE(rom_->WriteVector(kOwnerStreamPc, {0x00, 0x00, 0xFF, 0xFF, 0xFF,
                                                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})
                  .ok());
  ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());

  const std::vector<uint8_t> owner_stream(rom_->data() + kOwnerStreamPc,
                                          rom_->data() + kOwnerStreamPc + 10);
  const auto layout = MakeObjectLayout();

  const auto status = room_->SaveObjects(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kOwnerStreamPc);
  EXPECT_TRUE(std::equal(owner_stream.begin(), owner_stream.end(),
                         rom_->data() + kOwnerStreamPc));
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_OverflowRelocatesWithDeclaredAllocatorSpace) {
  constexpr int kOldStreamPc = 0x100000;
  constexpr int kNextStreamPc = 0x100010;
  constexpr int kRelocatedStreamPc = 0x100200;
  SetObjectRoomPointer(1, kNextStreamPc);
  WriteEmptyObjectStream(kNextStreamPc, 0x11, 0x22);
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  }

  const std::vector<uint8_t> old_bytes(rom_->data() + kOldStreamPc,
                                       rom_->data() + kNextStreamPc + 10);
  std::vector<uint8_t> expected_replacement = {0x00, 0x00};
  const auto encoded = room_->EncodeObjects();
  expected_replacement.insert(expected_replacement.end(), encoded.begin(),
                              encoded.end());
  const int expected_door_pc =
      kRelocatedStreamPc + static_cast<int>(expected_replacement.size()) - 2;
  const auto layout = MakeObjectLayout();

  const auto status = room_->SaveObjects(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetObjectRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetObjectRoomPointer(1), kNextStreamPc);
  EXPECT_EQ(GetDoorPointer(0), expected_door_pc);
  EXPECT_TRUE(std::equal(old_bytes.begin(), old_bytes.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_TRUE(std::equal(expected_replacement.begin(),
                         expected_replacement.end(),
                         rom_->data() + kRelocatedStreamPc));
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest, SaveObjects_AllocatorExhaustionFailsWithoutMutation) {
  SetObjectRoomPointer(1, 0x100000);
  ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  const auto before = rom_->vector();
  const auto layout = MakeObjectLayout({0x100200, 0x100204});

  const auto status = room_->SaveObjects(&layout);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_UsesNearestPhysicalPointerInsteadOfNextRoomId) {
  WriteLongPointer(0xF8000 + 3, PcToSnes(0x100200));
  WriteLongPointer(0xF8000 + 6, PcToSnes(0x100010));
  std::fill_n(rom_->mutable_data() + 0x100010, 0x20, 0xA5);

  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  }
  EXPECT_EQ(CalculateRoomSize(rom_.get(), 0).room_size, 0x10);
  const auto before = rom_->vector();

  const auto status = room_->SaveObjects();

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest, SaveObjects_SectionOneTailExactFitStopsAtHardEnd) {
  constexpr int kStreamStart = 0x0535A6;
  constexpr int kObjectCount = 128;
  constexpr int kEncodedSize = 8 + (kObjectCount * 3);
  static_assert(kStreamStart + 2 + kEncodedSize ==
                kDungeonObjectDataRegions[0].end);
  WriteLongPointer(0xF8000, PcToSnes(kStreamStart));
  rom_->mutable_data()[kDungeonObjectDataRegions[0].end] = 0xA5;

  for (int i = 0; i < kObjectCount; ++i) {
    ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  }

  const auto status = room_->SaveObjects();

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->data()[kDungeonObjectDataRegions[0].end], 0xA5);
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest,
       SaveObjects_SectionOneTailOneByteOverFailsWithoutMutation) {
  constexpr int kStreamStart = 0x0535A7;
  constexpr int kObjectCount = 128;
  constexpr int kEncodedSize = 8 + (kObjectCount * 3);
  static_assert(kStreamStart + 2 + kEncodedSize ==
                kDungeonObjectDataRegions[0].end + 1);
  WriteLongPointer(0xF8000, PcToSnes(kStreamStart));

  for (int i = 0; i < kObjectCount; ++i) {
    ASSERT_TRUE(room_->AddObject(RoomObject(0x10, 10, 10, 0, 0)).ok());
  }
  const auto before = rom_->vector();

  const auto status = room_->SaveObjects();

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->object_stream_dirty());
}

TEST_F(DungeonSaveTest, SaveSprites_FitsInSpace) {
  // Add a sprite
  zelda3::Sprite spr(0x10, 10, 10, 0, 0);
  room_->GetSprites().push_back(spr);
  room_->MarkSpritesDirty();

  auto status = room_->SaveSprites();
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(DungeonSaveTest, SaveSprites_TooLargeFailsWithoutMutation) {
  // Add MANY sprites to exceed 0x50 (80) bytes
  // Each sprite is 3 bytes.
  // We need > 26 sprites.
  for (int i = 0; i < 30; ++i) {
    zelda3::Sprite spr(0x10, 10, 10, 0, 0);
    room_->GetSprites().push_back(spr);
  }
  room_->MarkSpritesDirty();

  const auto before = rom_->vector();

  auto status = room_->SaveSprites();
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest, SaveSprites_FinalTerminatorFitsExclusiveHardEnd) {
  constexpr int kStreamStart = kSpritesDataEndExclusive - 2;
  SetSpriteRoomPointer(0, kStreamStart);
  rom_->mutable_data()[kStreamStart] = 0x00;
  rom_->mutable_data()[kSpritesDataEndExclusive] = 0xA5;
  room_->MarkSpritesDirty();

  const auto status = room_->SaveSprites();

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->data()[kSpritesEndData], 0xFF);
  EXPECT_EQ(rom_->data()[kSpritesDataEndExclusive], 0xA5);
  EXPECT_FALSE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest,
       SaveSprites_FinalStreamOneByteOverHardEndFailsWithoutMutation) {
  constexpr int kStreamStart = kSpritesDataEndExclusive - 4;
  SetSpriteRoomPointer(0, kStreamStart);
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();
  const auto before = rom_->vector();

  const auto status = room_->SaveSprites();

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest, SaveSprites_PreservesSortspriteHeaderByte) {
  // Seed the room with a known sort-byte value (0x00) followed by terminator.
  rom_->mutable_data()[0x49000] = 0x00;
  rom_->mutable_data()[0x49001] = 0xFF;

  zelda3::Sprite spr(0x10, 10, 10, 0, 0);
  room_->GetSprites().push_back(spr);
  room_->MarkSpritesDirty();

  auto status = room_->SaveSprites();
  ASSERT_TRUE(status.ok()) << status.message();

  // SaveSprites must preserve the first byte (SortSprites mode) and write
  // encoded payload after it.
  EXPECT_EQ(rom_->data()[0x49000], 0x00);
  EXPECT_EQ(rom_->data()[0x49001], 0x0A);  // b1 (Y)
  EXPECT_EQ(rom_->data()[0x49002], 0x0A);  // b2 (X)
  EXPECT_EQ(rom_->data()[0x49003], 0x10);  // b3 (sprite id)
  EXPECT_EQ(rom_->data()[0x49004], 0xFF);  // terminator
}

TEST_F(DungeonSaveTest, SaveSprites_SharedStreamFailsWithoutMutation) {
  SetSpriteRoomPointer(1, 0x49000);
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();
  const auto before = rom_->vector();

  const auto status = room_->SaveSprites();

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest,
       SaveSprites_SharedStreamDetachesWithDeclaredAllocatorSpace) {
  constexpr int kOldStreamPc = 0x49000;
  constexpr int kRelocatedStreamPc = 0x49200;
  SetSpriteRoomPointer(1, kOldStreamPc);
  rom_->mutable_data()[kOldStreamPc] = 0x01;
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();

  const std::vector<uint8_t> old_stream(rom_->data() + kOldStreamPc,
                                        rom_->data() + kOldStreamPc + 16);
  std::vector<uint8_t> expected_replacement = {0x01};
  const auto encoded = room_->EncodeSprites();
  expected_replacement.insert(expected_replacement.end(), encoded.begin(),
                              encoded.end());
  const auto layout = MakeSpriteLayout();

  const auto status = room_->SaveSprites(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetSpriteRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetSpriteRoomPointer(1), kOldStreamPc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_TRUE(std::equal(expected_replacement.begin(),
                         expected_replacement.end(),
                         rom_->data() + kRelocatedStreamPc));
  EXPECT_FALSE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest, SaveSprites_CopyOnWritePreservesSmallAndBigKeyMarkers) {
  constexpr int kOldStreamPc = 0x49000;
  constexpr int kRelocatedStreamPc = 0x49200;
  const std::vector<uint8_t> sprite_stream = {
      0x01,              // sort byte
      0x0A, 0x0A, 0x10,  // sprite with small-key drop
      0xFE, 0x00, 0xE4,  // hidden small-key marker
      0x12, 0x0E, 0x21,  // sprite with big-key drop
      0xFD, 0x00, 0xE4,  // hidden big-key marker
      0xFF,
  };
  ASSERT_TRUE(rom_->WriteVector(kOldStreamPc, sprite_stream).ok());
  SetSpriteRoomPointer(1, kOldStreamPc);  // Force alias-safe COW.
  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  ASSERT_EQ(room_->GetSprites()[0].key_drop(), 1);
  ASSERT_EQ(room_->GetSprites()[1].key_drop(), 2);
  room_->MarkSpritesDirty();
  const auto old_stream =
      std::vector<uint8_t>(rom_->data() + kOldStreamPc,
                           rom_->data() + kOldStreamPc + sprite_stream.size());
  const auto layout = MakeSpriteLayout();

  const auto status = room_->SaveSprites(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetSpriteRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetSpriteRoomPointer(1), kOldStreamPc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_TRUE(std::equal(sprite_stream.begin(), sprite_stream.end(),
                         rom_->data() + kRelocatedStreamPc));

  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 1);
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);
}

TEST_F(DungeonSaveTest,
       SaveSprites_OverlappingSuffixDetachesWithoutMutatingOwnerStream) {
  constexpr int kOwnerStreamPc = 0x49000;
  constexpr int kSuffixStreamPc = kOwnerStreamPc + 2;
  constexpr int kRelocatedStreamPc = 0x49200;
  SetSpriteRoomPointer(0, kSuffixStreamPc);
  SetSpriteRoomPointer(1, kOwnerStreamPc);
  ASSERT_TRUE(
      rom_->WriteVector(kOwnerStreamPc, {0x00, 0x11, 0x22, 0xFF, 0xFF}).ok());
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();

  const std::vector<uint8_t> owner_stream(rom_->data() + kOwnerStreamPc,
                                          rom_->data() + kOwnerStreamPc + 5);
  const auto layout = MakeSpriteLayout();

  const auto status = room_->SaveSprites(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetSpriteRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetSpriteRoomPointer(1), kOwnerStreamPc);
  EXPECT_TRUE(std::equal(owner_stream.begin(), owner_stream.end(),
                         rom_->data() + kOwnerStreamPc));
  EXPECT_FALSE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest, SaveSprites_DeclaredDataBoundaryForcesCopyOnWrite) {
  constexpr int kOldStreamPc = 0x49000;
  constexpr int kRelocatedStreamPc = 0x49200;
  rom_->mutable_data()[kOldStreamPc + 2] = 0xA5;
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();

  auto layout = MakeSpriteLayout();
  layout.pointer_count = 1;
  layout.data_ranges = {{kOldStreamPc, kOldStreamPc + 2},
                        {kRelocatedStreamPc, 0x49300}};
  layout.allocation_ranges = {{kRelocatedStreamPc, 0x49300}};

  const auto status = room_->SaveSprites(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetSpriteRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(rom_->data()[kOldStreamPc], 0x00);
  EXPECT_EQ(rom_->data()[kOldStreamPc + 1], 0xFF);
  EXPECT_EQ(rom_->data()[kOldStreamPc + 2], 0xA5);
  EXPECT_FALSE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest,
       SaveSprites_OverflowRelocatesWithDeclaredAllocatorSpace) {
  constexpr int kOldStreamPc = 0x49000;
  constexpr int kNextStreamPc = 0x49002;
  constexpr int kRelocatedStreamPc = 0x49200;
  SetSpriteRoomPointer(1, kNextStreamPc);
  ASSERT_TRUE(
      rom_->WriteVector(kNextStreamPc, std::vector<uint8_t>{0x01, 0xFF}).ok());
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();

  const std::vector<uint8_t> old_stream(rom_->data() + kOldStreamPc,
                                        rom_->data() + kNextStreamPc + 2);
  std::vector<uint8_t> expected_replacement = {0x00};
  const auto encoded = room_->EncodeSprites();
  expected_replacement.insert(expected_replacement.end(), encoded.begin(),
                              encoded.end());
  const auto layout = MakeSpriteLayout();

  const auto status = room_->SaveSprites(&layout);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(GetSpriteRoomPointer(0), kRelocatedStreamPc);
  EXPECT_EQ(GetSpriteRoomPointer(1), kNextStreamPc);
  EXPECT_TRUE(std::equal(old_stream.begin(), old_stream.end(),
                         rom_->data() + kOldStreamPc));
  EXPECT_TRUE(std::equal(expected_replacement.begin(),
                         expected_replacement.end(),
                         rom_->data() + kRelocatedStreamPc));
  EXPECT_FALSE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest,
       SaveSprites_UsesNearestPhysicalPointerInsteadOfNextRoomId) {
  SetSpriteRoomPointer(1, 0x49100);
  SetSpriteRoomPointer(2, 0x49004);
  room_->GetSprites().emplace_back(0x10, 10, 10, 0, 0);
  room_->MarkSpritesDirty();
  const auto before = rom_->vector();

  const auto status = room_->SaveSprites();

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room_->sprites_dirty());
}

TEST_F(DungeonSaveTest, LoadSprites_DoesNotDuplicateOnReload) {
  // sort byte + one sprite + terminator
  rom_->mutable_data()[0x49000] = 0x00;
  rom_->mutable_data()[0x49001] = 0x0A;  // b1 (Y)
  rom_->mutable_data()[0x49002] = 0x0A;  // b2 (X)
  rom_->mutable_data()[0x49003] = 0x10;  // b3 (id)
  rom_->mutable_data()[0x49004] = 0xFF;

  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 1u);

  // Reloading the same room should refresh, not append duplicate entries.
  room_->LoadSprites();
  EXPECT_EQ(room_->GetSprites().size(), 1u);
}

TEST_F(DungeonSaveTest, EncodeSprites_PreservesKeyDropMarkersOnRoundTrip) {
  // sort byte + small-key sprite/marker + big-key sprite/marker + terminator
  const std::vector<uint8_t> sprite_stream = {
      0x00, 0x0A, 0x0A, 0x10,  // sprite with small-key drop
      0xFE, 0x00, 0xE4,        // hidden small-key marker
      0x12, 0x0E, 0x21,        // sprite with big-key drop
      0xFD, 0x00, 0xE4,        // hidden big-key marker
      0xFF,
  };
  ASSERT_TRUE(rom_->WriteVector(0x49000, sprite_stream).ok());

  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 1);
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);

  const std::vector<uint8_t> expected_payload(sprite_stream.begin() + 1,
                                              sprite_stream.end());
  const auto encoded = room_->EncodeSprites();
  EXPECT_EQ(encoded, expected_payload);

  ASSERT_TRUE(rom_->WriteVector(0x49001, encoded).ok());
  room_->LoadSprites();
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 1);
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);
}

TEST_F(DungeonSaveTest, EncodeObjects_SkipsTorchesAndBlocks) {
  Room room(0, rom_.get());
  room.ClearTileObjects();

  // One normal room object plus two "special table" objects.
  RoomObject normal(0x10, 1, 1, 0, 0);
  RoomObject torch(0x150, 2, 2, 0, 0);
  torch.set_options(ObjectOption::Torch);
  RoomObject block(0x0E00, 3, 3, 0, 0);
  block.set_options(ObjectOption::Block);

  room.AddTileObject(normal);
  room.AddTileObject(torch);
  room.AddTileObject(block);

  auto encoded = room.EncodeObjects();

  // Expect only the normal object to be encoded:
  // - 3 bytes object
  // - layer0 terminator (FF FF)
  // - layer1 terminator (FF FF)
  // - door marker (F0 FF) (in layer2 stream)
  // - door terminator (FF FF) (also layer2 terminator)
  EXPECT_EQ(encoded.size(), 11u);

  const auto nb = normal.EncodeObjectToBytes();
  EXPECT_EQ(encoded[0], nb.b1);
  EXPECT_EQ(encoded[1], nb.b2);
  EXPECT_EQ(encoded[2], nb.b3);

  EXPECT_EQ(encoded[3], 0xFF);
  EXPECT_EQ(encoded[4], 0xFF);
  EXPECT_EQ(encoded[5], 0xFF);
  EXPECT_EQ(encoded[6], 0xFF);

  EXPECT_EQ(encoded[7], 0xF0);
  EXPECT_EQ(encoded[8], 0xFF);
  EXPECT_EQ(encoded[9], 0xFF);
  EXPECT_EQ(encoded[10], 0xFF);
}

TEST_F(DungeonSaveTest, SaveObjects_RoundTripsDoorsInLayer2Stream) {
  // Seed the room's object stream with a valid empty encoding:
  // layer0 terminator, layer1 terminator, door marker, door terminator.
  const int objects_start_pc = 0x100000 + 2;  // 2-byte header at 0x100000
  std::vector<uint8_t> empty = {
      0xFF, 0xFF,  // end layer0
      0xFF, 0xFF,  // end layer1
      0xF0, 0xFF,  // door marker ($FFF0)
      0xFF, 0xFF,  // door terminator ($FFFF) (also end layer2)
  };
  ASSERT_TRUE(rom_->WriteVector(objects_start_pc, empty).ok());

  Room room(0, rom_.get());
  room.LoadObjects();
  EXPECT_EQ(room.GetDoors().size(), 0u);

  // Add one door and save.
  Room::Door door;
  door.position = 3;
  door.direction = DoorDirection::North;
  door.type = DoorType::NormalDoor;
  room.AddDoor(door);

  auto save_status = room.SaveObjects();
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  // Reload and verify the door is still present.
  Room room2(0, rom_.get());
  room2.LoadObjects();
  ASSERT_EQ(room2.GetDoors().size(), 1u);

  const auto [b1_expected, b2_expected] = door.EncodeBytes();
  const auto [b1_actual, b2_actual] = room2.GetDoors()[0].EncodeBytes();
  EXPECT_EQ(b1_actual, b1_expected);
  EXPECT_EQ(b2_actual, b2_expected);
}

TEST_F(DungeonSaveTest, SaveAllTorches_WritesLitBit) {
  std::vector<Room> rooms(kNumberOfRooms);

  RoomObject torch(0x150, 10, 20, 0, 1);
  torch.set_options(ObjectOption::Torch);
  torch.lit_ = true;
  rooms[1].AddTileObject(torch);

  auto status = SaveAllTorches(rom_.get(), rooms);
  EXPECT_TRUE(status.ok()) << status.message();

  const auto& rom_data = rom_->vector();
  const uint16_t torch_len =
      static_cast<uint16_t>(rom_data[kTorchesLengthPointer]) |
      (static_cast<uint16_t>(rom_data[kTorchesLengthPointer + 1]) << 8);
  EXPECT_EQ(torch_len, 6u);

  EXPECT_EQ(rom_data[kTorchData + 0], 0x01);
  EXPECT_EQ(rom_data[kTorchData + 1], 0x00);

  // word = ((x + y*64) << 1) with draw layer in bit 13 and lit in bit 15.
  EXPECT_EQ(rom_data[kTorchData + 2], 0x14);  // low byte
  EXPECT_EQ(rom_data[kTorchData + 3],
            0xAA);  // high byte: draw layer + lit + address bits

  EXPECT_EQ(rom_data[kTorchData + 4], 0xFF);
  EXPECT_EQ(rom_data[kTorchData + 5], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllTorches_RejectsLayerTwoWithoutMutatingRom) {
  std::vector<Room> rooms(kNumberOfRooms);

  RoomObject torch(0x150, 10, 20, 0, 2);
  torch.set_options(ObjectOption::Torch);
  rooms[1].AddTileObject(torch);
  ASSERT_TRUE(rooms[1].torches_dirty());
  const auto before = rom_->vector();

  const auto status = SaveAllTorches(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(status.message()).find("draw-layer selector 2"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[1].torches_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllTorches_RejectsOutOfBoundsTwoByTwoAnchorWithoutMutatingRom) {
  for (const auto [x, y] : {std::pair{63, 20}, std::pair{10, 63}}) {
    SCOPED_TRACE("x=" + std::to_string(x) + " y=" + std::to_string(y));
    std::vector<Room> rooms(kNumberOfRooms);
    RoomObject torch(0x150, x, y, 0, 0);
    torch.set_options(ObjectOption::Torch);
    rooms[1].AddTileObject(torch);
    ASSERT_TRUE(rooms[1].torches_dirty());
    const auto before = rom_->vector();

    const auto status = SaveAllTorches(rom_.get(), rooms);

    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_NE(std::string(status.message()).find("expected x/y in range 0..62"),
              std::string::npos);
    EXPECT_EQ(rom_->vector(), before);
    EXPECT_TRUE(rooms[1].torches_dirty());
  }
}

TEST_F(DungeonSaveTest, SaveAllTorches_AcceptsMaxInBoundsTwoByTwoAnchor) {
  std::vector<Room> rooms(kNumberOfRooms);
  RoomObject torch(0x150, 62, 62, 0, 0);
  torch.set_options(ObjectOption::Torch);
  rooms[1].AddTileObject(torch);

  const auto status = SaveAllTorches(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  const auto& rom_data = rom_->vector();
  EXPECT_EQ(rom_data[kTorchData + 0], 0x01);
  EXPECT_EQ(rom_data[kTorchData + 1], 0x00);
  EXPECT_EQ(rom_data[kTorchData + 2], 0x7C);
  EXPECT_EQ(rom_data[kTorchData + 3], 0x1F);
  EXPECT_EQ(rom_data[kTorchData + 4], 0xFF);
  EXPECT_EQ(rom_data[kTorchData + 5], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllTorches_NoOpWhenUnchanged) {
  // Seed ROM with a torch blob identical to what SaveAllTorches would emit.
  // This should be a no-op (no writes) and keep the ROM clean.
  std::vector<uint8_t> blob = {
      0x01, 0x00,  // room_id = 1
      0x14, 0xAA,  // torch word (x=10,y=20,draw_layer=1,lit=1)
      0xFF, 0xFF,  // terminator
      0xFF, 0xFF,  // standalone authoring padding
      0xFF, 0xFF,  // standalone authoring padding
  };
  ASSERT_TRUE(
      rom_->WriteWord(kTorchesLengthPointer, static_cast<uint16_t>(blob.size()))
          .ok());
  ASSERT_TRUE(rom_->WriteVector(kTorchData, blob).ok());
  rom_->ClearDirty();

  std::vector<Room> rooms(kNumberOfRooms);
  RoomObject torch(0x150, 10, 20, 0, 1);
  torch.set_options(ObjectOption::Torch);
  torch.lit_ = true;
  rooms[1].AddTileObject(torch);

  auto status = SaveAllTorches(rom_.get(), rooms);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rooms[1].torches_dirty());
  EXPECT_FALSE(rom_->dirty());
  EXPECT_TRUE(std::equal(blob.begin(), blob.end(),
                         rom_->vector().begin() + kTorchData));
}

TEST_F(DungeonSaveTest,
       SaveAllTorches_LowerLayerReservedBitRoundTripsWithoutMutation) {
  std::vector<uint8_t> blob = {
      0x01, 0x00,  // room_id = 1
      0x14, 0xEA,  // torch word (x=10,y=20,draw=1,reserved=1,lit=1)
      0xFF, 0xFF,  // terminator
  };
  ASSERT_TRUE(
      rom_->WriteWord(kTorchesLengthPointer, static_cast<uint16_t>(blob.size()))
          .ok());
  ASSERT_TRUE(rom_->WriteVector(kTorchData, blob).ok());
  rom_->ClearDirty();

  Room room(1, rom_.get());
  room.LoadTorches();
  ASSERT_TRUE(room.AreTorchesLoaded());
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  const RoomObject& torch = room.GetTileObjects().front();
  EXPECT_EQ(torch.x(), 10);
  EXPECT_EQ(torch.y(), 20);
  EXPECT_EQ(torch.GetLayerValue(), 1);
  EXPECT_EQ(torch.torch_reserved_bit(), 1);
  EXPECT_TRUE(torch.lit_);

  const auto status = SaveAllTorches(rom_.get(), kNumberOfRooms,
                                     [&room](int room_id) -> const Room* {
                                       return room_id == 1 ? &room : nullptr;
                                     });
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rom_->dirty());
  EXPECT_TRUE(std::equal(blob.begin(), blob.end(),
                         rom_->vector().begin() + kTorchData));
}

TEST_F(DungeonSaveTest, SaveAllTorches_LoadedRoomCanDeleteLastTorch) {
  std::vector<uint8_t> blob = {
      0x01, 0x00,  // room_id = 1
      0x14, 0xAA,  // torch word (x=10,y=20,draw_layer=1,lit=1)
      0xFF, 0xFF,  // terminator
      0x02, 0x00,  // room_id = 2
      0x0A, 0x03,  // torch word (x=5,y=6,draw_layer=0,lit=0)
      0xFF, 0xFF,  // terminator
  };
  ASSERT_TRUE(
      rom_->WriteWord(kTorchesLengthPointer, static_cast<uint16_t>(blob.size()))
          .ok());
  ASSERT_TRUE(rom_->WriteVector(kTorchData, blob).ok());

  Room room(1, rom_.get());
  room.LoadTorches();
  ASSERT_TRUE(room.AreTorchesLoaded());
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  room.RemoveTileObject(0);
  ASSERT_TRUE(room.torches_dirty());

  auto status = SaveAllTorches(rom_.get(), kNumberOfRooms,
                               [&room](int room_id) -> const Room* {
                                 return room_id == 1 ? &room : nullptr;
                               });
  ASSERT_TRUE(status.ok()) << status.message();

  const auto& rom_data = rom_->vector();
  const uint16_t torch_len =
      static_cast<uint16_t>(rom_data[kTorchesLengthPointer]) |
      (static_cast<uint16_t>(rom_data[kTorchesLengthPointer + 1]) << 8);
  EXPECT_EQ(torch_len, 6u);
  EXPECT_EQ(rom_data[kTorchData + 0], 0x02);
  EXPECT_EQ(rom_data[kTorchData + 1], 0x00);
  EXPECT_EQ(rom_data[kTorchData + 2], 0x0A);
  EXPECT_EQ(rom_data[kTorchData + 3], 0x03);
  EXPECT_EQ(rom_data[kTorchData + 4], 0xFF);
  EXPECT_EQ(rom_data[kTorchData + 5], 0xFF);
  EXPECT_FALSE(room.torches_dirty());
}

// Loaded/dirty contract for chest + pot save tests
// -------------------------------------------------
// `SaveAllChests` / `SaveAllPotItems` preserve header-only rooms by
// design: a room is treated as "owned by the editor" only when
// `AreChestsLoaded() == true` (set by `LoadChests`) OR
// `chests_dirty() == true` (set by `MarkChestsDirty`). The same
// pattern applies to pot items. `SetLoaded(true)` on its own only
// flips `is_loaded_`; it does NOT cascade into the per-table loaded
// flags, because the editor materializes those tables individually
// via dedicated `Load*` calls.
//
// The fixture below skips `LoadChests`/`LoadPotItems` (those would
// read vanilla bytes back into the in-memory vectors) and instead
// mutates `GetChests()` / `GetPotItems()` directly to exercise the
// writer. To make those mutations visible to the writer's
// "owned-by-editor" gate, every test that edits a room's chest or
// pot list must call `MarkChestsDirty()` / `MarkPotItemsDirty()`
// afterwards. This mirrors the editor's actual flow: when a UI edit
// changes a chest, the chest editor emits a dirty flip. Tests that
// expect the loaded-but-empty path (e.g.
// `LoadedRoomWithNoChestsClearsExistingRomEntries`) must also mark
// dirty — that is the declaration of "I intend this room's empty
// state to overwrite the vanilla bytes".

TEST_F(DungeonSaveTest, SaveAllChests_WritesSingleSmallChest) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].MarkChestsDirty();

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x03);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x42);
}

TEST_F(DungeonSaveTest, SaveAllChests_WritesBigChestWithHighBit) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x77, true});
  rooms[0].MarkChestsDirty();

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x03);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x80);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x77);
}

TEST_F(DungeonSaveTest, SaveAllChests_PreservesRomEntriesForUntouchedRooms) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/1, /*chest_id=*/0x99, /*big=*/false);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].MarkChestsDirty();
  // rooms[1] intentionally untouched — no SetLoaded, no MarkChestsDirty —
  // so the writer must fall back to ParseRomChests for that room and
  // preserve its `0x99` entry.

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x06);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);

  // The untouched room-1 record retains its original physical slot. The new
  // room-0 record has no slot to replace, so it is appended.
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], 0x01);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x99);

  EXPECT_EQ(rom_->data()[kChestDataPc + 3], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 4], 0x00);
  EXPECT_EQ(rom_->data()[kChestDataPc + 5], 0x42);
}

TEST_F(DungeonSaveTest,
       SaveAllChests_LoadedRoomWithNoChestsClearsExistingRomEntries) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/0, /*chest_id=*/0x55, /*big=*/false);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  // No chests added in memory, but we mark dirty to declare
  // "this room's empty state is intentional and should overwrite
  // the vanilla 0x55 entry".
  rooms[0].MarkChestsDirty();

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0x00);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x00);
}

TEST_F(DungeonSaveTest, SaveAllChests_ReloadedRoomMatchesSerializedState) {
  SetupChestTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].GetChests().push_back(chest_data{0x77, true});
  rooms[0].MarkChestsDirty();

  auto status = SaveAllChests(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  Room reloaded_room(0, rom_.get());
  reloaded_room.LoadChests();

  ASSERT_EQ(reloaded_room.GetChests().size(), 2u);
  EXPECT_EQ(reloaded_room.GetChests()[0].id, 0x42);
  EXPECT_FALSE(reloaded_room.GetChests()[0].size);
  EXPECT_EQ(reloaded_room.GetChests()[1].id, 0x77);
  EXPECT_TRUE(reloaded_room.GetChests()[1].size);
}

TEST_F(DungeonSaveTest, LoadChests_InterpretsLengthAsBytes) {
  SetupChestTable();
  rom_->mutable_data()[kChestsLengthPointer] = 0x06;
  rom_->mutable_data()[kChestDataPc + 0] = 0x00;
  rom_->mutable_data()[kChestDataPc + 1] = 0x00;
  rom_->mutable_data()[kChestDataPc + 2] = 0x42;
  rom_->mutable_data()[kChestDataPc + 3] = 0x01;
  rom_->mutable_data()[kChestDataPc + 4] = 0x00;
  rom_->mutable_data()[kChestDataPc + 5] = 0x77;

  Room room0(0, rom_.get());
  room0.LoadChests();
  ASSERT_EQ(room0.GetChests().size(), 1u);
  EXPECT_EQ(room0.GetChests()[0].id, 0x42);
}

TEST_F(DungeonSaveTest, SaveAllChests_NoDirtyRoomsIsNoOp) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/0, /*chest_id=*/0x42, /*big=*/false);
  std::vector<Room> rooms(kNumberOfRooms);
  const auto before = rom_->vector();
  rom_->ClearDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_NoDirtyRoomsIgnoresUnsupportedDataTarget) {
  SetupChestTable();
  WriteLongPointer(kChestsDataPointer1, 0x7FFFFF);
  std::vector<Room> rooms(kNumberOfRooms);
  const auto before = rom_->vector();
  rom_->ClearDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_DirtyRoomRejectsUnsupportedTargetBeforeMutation) {
  SetupChestTable();
  WriteLongPointer(kChestsDataPointer1, 0x7FFFFF);
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].MarkChestsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllChests(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_DirtyRoomRejectsPointerOperandAliasBeforeMutation) {
  SetupChestTable();
  WriteLongPointer(kChestsDataPointer1, PcToSnes(kChestsDataPointer1));
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].MarkChestsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllChests(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_DirtyRoomRejectsLengthOperandAliasBeforeMutation) {
  SetupChestTable();
  // End the 504-byte target one byte into the length operand so this covers
  // the writable metadata alias without also reaching the pointer operand.
  constexpr int kLengthOnlyAliasPc =
      kChestsLengthPointer - kChestTableCapacityBytes + 1;
  static_assert(kLengthOnlyAliasPc + kChestTableCapacityBytes ==
                kChestsLengthPointer + 1);
  WriteLongPointer(kChestsDataPointer1, PcToSnes(kLengthOnlyAliasPc));
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetChests().push_back(chest_data{0x42, false});
  rooms[0].MarkChestsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllChests(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest, SaveAllChests_UnchangedDirtyRoomAvoidsRomWrite) {
  SetupChestTable();
  SeedChestEntry(/*room_id=*/0, /*chest_id=*/0x42, /*big=*/false);
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0] = Room(0, rom_.get());
  rooms[0].LoadChests();
  rooms[0].MarkChestsDirty();
  rom_->ClearDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_FALSE(rom_->dirty());
  EXPECT_FALSE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_NonMonotonicDirtyNoOpPreservesPhysicalBytes) {
  SetupChestTable();
  SeedChestRecords({
      {2, 0x20, false},
      {1, 0x10, false},
      {2, 0x21, true},
      {0x013D, 0x7A, false},  // Unknown to the 296-room editor model.
      {1, 0x11, true},
  });
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[2] = Room(2, rom_.get());
  rooms[2].LoadChests();
  ASSERT_EQ(rooms[2].GetChests().size(), 2u);
  rooms[2].MarkChestsDirty();
  const auto before = rom_->vector();
  rom_->ClearDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
  EXPECT_FALSE(rooms[2].chests_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_MutatesSelectedOccurrenceWithoutMovingOtherRecords) {
  SetupChestTable();
  SeedChestRecords({
      {2, 0x20, false},
      {1, 0x10, false},
      {2, 0x21, true},
      {0x013D, 0x7A, false},  // Must survive byte-for-byte.
      {1, 0x11, true},
  });
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[2] = Room(2, rom_.get());
  rooms[2].LoadChests();
  ASSERT_EQ(rooms[2].GetChests().size(), 2u);
  rooms[2].GetChests()[0].id = 0x22;
  rooms[2].GetChests()[0].size = true;
  rooms[2].MarkChestsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->data()[kChestDataPc + 0], before[kChestDataPc + 0]);
  EXPECT_EQ(rom_->data()[kChestDataPc + 1], 0x80);
  EXPECT_EQ(rom_->data()[kChestDataPc + 2], 0x22);
  EXPECT_TRUE(std::equal(before.begin() + kChestDataPc + 3,
                         before.begin() + kChestDataPc + 15,
                         rom_->vector().begin() + kChestDataPc + 3));
  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 15);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0);
  EXPECT_FALSE(rooms[2].chests_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllChests_RemovesSurplusAndAppendsGrowthDeterministically) {
  SetupChestTable();
  SeedChestRecords({
      {2, 0x20, false},
      {1, 0x10, false},
      {2, 0x21, true},
      {0x013D, 0x7A, false},
      {3, 0x30, false},
  });
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetChests().push_back(chest_data{0x01, false});
  rooms[0].MarkChestsDirty();
  rooms[2] = Room(2, rom_.get());
  rooms[2].LoadChests();
  rooms[2].GetChests().resize(1);
  rooms[2].GetChests()[0].id = 0x22;
  rooms[2].MarkChestsDirty();
  rooms[3] = Room(3, rom_.get());
  rooms[3].LoadChests();
  rooms[3].GetChests().push_back(chest_data{0x31, true});
  rooms[3].MarkChestsDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  const std::array<uint8_t, 18> expected = {
      0x02, 0x00, 0x22,  // room 2 retained in its first physical slot
      0x01, 0x00, 0x10,  // untouched room 1
      0x3D, 0x01, 0x7A,  // unknown record retained exactly
      0x03, 0x00, 0x30,  // room 3's existing occurrence
      0x00, 0x00, 0x01,  // room 0 growth appended first by room ID
      0x03, 0x80, 0x31,  // then room 3 growth
  };
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(),
                         rom_->vector().begin() + kChestDataPc));
  EXPECT_EQ(rom_->data()[kChestsLengthPointer], expected.size());
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0);
}

TEST_F(DungeonSaveTest, SaveAllChests_AtCapacityWrites01F8ByteLength) {
  SetupChestTable();
  std::vector<Room> rooms(kNumberOfRooms);
  for (int i = 0; i < kChestTableCapacityRecords; ++i) {
    rooms[0].GetChests().push_back(chest_data{static_cast<uint8_t>(i), false});
  }
  rooms[0].MarkChestsDirty();

  const auto status = SaveAllChests(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->data()[kChestsLengthPointer], 0xF8);
  EXPECT_EQ(rom_->data()[kChestsLengthPointer + 1], 0x01);
  EXPECT_FALSE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest, SaveAllChests_OverCapacityFailsWithoutMutation) {
  SetupChestTable();
  std::vector<Room> rooms(kNumberOfRooms);
  for (int i = 0; i < kChestTableCapacityRecords + 1; ++i) {
    rooms[0].GetChests().push_back(chest_data{static_cast<uint8_t>(i), false});
  }
  rooms[0].MarkChestsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllChests(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].chests_dirty());
}

TEST_F(DungeonSaveTest, SaveAllPotItems_LoadedRoomWritesEntriesAndTerminator) {
  SetupPotItemTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  PotItem item;
  item.position = 0x1234;
  item.item = 0x56;
  rooms[0].GetPotItems().push_back(item);
  rooms[0].MarkPotItemsDirty();

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0x34);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0x12);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 2], 0x56);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 3], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 4], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_ReloadedRoomMatchesSerializedState) {
  SetupPotItemTable();

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  PotItem first;
  first.position = 0x1234;
  first.item = 0x56;
  rooms[0].GetPotItems().push_back(first);

  PotItem second;
  second.position = 0x5678;
  second.item = 0x9A;
  rooms[0].GetPotItems().push_back(second);
  rooms[0].MarkPotItemsDirty();

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  Room reloaded_room(0, rom_.get());
  reloaded_room.LoadPotItems();

  ASSERT_EQ(reloaded_room.GetPotItems().size(), 2u);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded_room.GetPotItems()[0].item, 0x56);
  EXPECT_EQ(reloaded_room.GetPotItems()[1].position, 0x5678);
  EXPECT_EQ(reloaded_room.GetPotItems()[1].item, 0x9A);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_UnloadedRoomPreservesExistingRomData) {
  SetupPotItemTable();
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0x34);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0x12);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 2], 0x56);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 3], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 4], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_LoadedRoomWithNoItemsWritesTerminator) {
  SetupPotItemTable();
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);
  // No pot items added in memory, but we mark dirty to declare
  // "this room's empty state is intentional and should overwrite
  // the seeded vanilla bytes with a fresh terminator".
  rooms[0].MarkPotItemsDirty();

  auto status = SaveAllPotItems(rom_.get(), rooms);
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0xFF);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0xFF);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_DirtyRoomTooLargeFailsAndStaysDirty) {
  SetupPotItemTable();
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetLoaded(true);

  for (int i = 0; i < 11; ++i) {
    PotItem item;
    item.position = static_cast<uint16_t>(0x1200 + i);
    item.item = static_cast<uint8_t>(0x40 + i);
    rooms[0].GetPotItems().push_back(item);
  }
  rooms[0].MarkPotItemsDirty();

  auto status = SaveAllPotItems(rom_.get(), rooms);
  EXPECT_FALSE(status.ok());
  EXPECT_NE(std::string(status.message()).find("pot item data too large"),
            std::string::npos);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 0], 0x34);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 1], 0x12);
  EXPECT_EQ(rom_->data()[kPotRoom0Pc + 2], 0x56);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_FinalStreamExactFitStopsAtHardEnd) {
  SetupPotItemTable();
  constexpr int kSerializedSize = (5 * 3) + 2;
  constexpr int kStreamStart = kRoomItemsDataEnd - kSerializedSize;
  SetPotRoomPointer(0, kStreamStart);
  rom_->mutable_data()[kRoomItemsDataEnd] = 0xA5;

  std::vector<Room> rooms(kNumberOfRooms);
  for (int i = 0; i < 5; ++i) {
    rooms[0].GetPotItems().push_back(PotItem{static_cast<uint16_t>(0x1200 + i),
                                             static_cast<uint8_t>(0x40 + i)});
  }
  rooms[0].MarkPotItemsDirty();

  const auto status = SaveAllPotItems(rom_.get(), rooms);

  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(rom_->data()[kRoomItemsDataEnd - 2], 0xFF);
  EXPECT_EQ(rom_->data()[kRoomItemsDataEnd - 1], 0xFF);
  EXPECT_EQ(rom_->data()[kRoomItemsDataEnd], 0xA5);
  EXPECT_FALSE(rooms[0].pot_items_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllPotItems_FinalStreamOneByteOverHardEndFailsWithoutMutation) {
  SetupPotItemTable();
  constexpr int kSerializedSize = (6 * 3) + 2;
  constexpr int kAvailableSize = kSerializedSize - 1;
  constexpr int kStreamStart = kRoomItemsDataEnd - kAvailableSize;
  SetPotRoomPointer(0, kStreamStart);

  std::vector<Room> rooms(kNumberOfRooms);
  for (int i = 0; i < 6; ++i) {
    rooms[0].GetPotItems().push_back(PotItem{static_cast<uint16_t>(0x1200 + i),
                                             static_cast<uint8_t>(0x40 + i)});
  }
  rooms[0].MarkPotItemsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllPotItems(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
}

TEST_F(DungeonSaveTest, SaveAllPotItems_SharedStreamFailsWithoutMutation) {
  SetupPotItemTable();
  SetPotRoomPointer(1, kPotRoom0Pc);
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetPotItems().push_back(PotItem{0x5678, 0x9A});
  rooms[0].MarkPotItemsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllPotItems(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllPotItems_RepackDetachesDirtyRoomFromSharedEmptyStream) {
  SetupSharedPotItemTable();
  auto layout = MakePotRepackLayout({0x00E100, 0x00E140});
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetPotItems().push_back(PotItem{0x1234, 0x56});
  rooms[0].MarkPotItemsDirty();

  const auto status =
      SaveAllPotItems(rom_.get(), absl::MakeConstSpan(rooms), &layout);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(rooms[0].pot_items_dirty());
  EXPECT_EQ(GetPotRoomPointer(0), 0x00E100);
  EXPECT_EQ(GetPotRoomPointer(1), 0x00E105);
  EXPECT_NE(GetPotRoomPointer(0), GetPotRoomPointer(1));
  Room reloaded(0, rom_.get());
  reloaded.LoadPotItems();
  ASSERT_EQ(reloaded.GetPotItems().size(), 1u);
  EXPECT_EQ(reloaded.GetPotItems()[0].position, 0x1234);
  EXPECT_EQ(reloaded.GetPotItems()[0].item, 0x56);
}

TEST_F(DungeonSaveTest, SaveAllPotItems_RepackAcceptsExactFit) {
  SetupSharedPotItemTable();
  auto layout = MakePotRepackLayout({0x00E100, 0x00E107});
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetPotItems().push_back(PotItem{0x1234, 0x56});
  rooms[0].MarkPotItemsDirty();

  const auto status =
      SaveAllPotItems(rom_.get(), absl::MakeConstSpan(rooms), &layout);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(rooms[0].pot_items_dirty());
  EXPECT_EQ(GetPotRoomPointer(0), 0x00E100);
  EXPECT_EQ(GetPotRoomPointer(1), 0x00E105);
}

TEST_F(DungeonSaveTest,
       SaveAllPotItems_RepackFailurePreservesRomAndDirtyState) {
  SetupSharedPotItemTable();
  auto layout = MakePotRepackLayout({0x00E100, 0x00E106});
  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetPotItems().push_back(PotItem{0x1234, 0x56});
  rooms[0].MarkPotItemsDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllPotItems(rom_.get(), absl::MakeConstSpan(rooms), &layout);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllPotItems_UsesNearestPhysicalPointerInsteadOfNextRoomId) {
  SetupPotItemTable();
  SetPotRoomPointer(1, kPotRoom2Pc);
  SetPotRoomPointer(2, kPotRoom1Pc);

  std::vector<Room> rooms(kNumberOfRooms);
  for (int i = 0; i < 11; ++i) {
    rooms[0].GetPotItems().push_back(PotItem{static_cast<uint16_t>(0x1200 + i),
                                             static_cast<uint8_t>(0x40 + i)});
  }
  rooms[0].MarkPotItemsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllPotItems(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllPotItems_PreflightsAllDirtyRoomsBeforeFirstWrite) {
  SetupPotItemTable();
  SetPotRoomPointer(2, kPotRoom1Pc);
  SeedPotItemBytes(kPotRoom0Pc, {0x34, 0x12, 0x56, 0xFF, 0xFF});

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].GetPotItems().push_back(PotItem{0x5678, 0x9A});
  rooms[0].MarkPotItemsDirty();
  rooms[1].GetPotItems().push_back(PotItem{0x1111, 0x22});
  rooms[1].MarkPotItemsDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllPotItems(rom_.get(), rooms);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(rooms[0].pot_items_dirty());
  EXPECT_TRUE(rooms[1].pot_items_dirty());
}

TEST_F(DungeonSaveTest, DungeonSpawnPointWriteRangesAreExactAndNonAliased) {
  constexpr int kSpawnId = 2;
  auto loaded = DungeonSpawnPoint::Load(*rom_, kSpawnId);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints> spawn_points{};
  spawn_points[kSpawnId] = *loaded;
  spawn_points[kSpawnId].MarkDirty();

  const std::vector<DungeonSpawnPointWriteRange> expected = {
      {kDungeonSpawnRoom + kSpawnId * 2, kDungeonSpawnRoom + kSpawnId * 2 + 2},
      {kDungeonSpawnCameraScrollBoundaries + kSpawnId * 8,
       kDungeonSpawnCameraScrollBoundaries + kSpawnId * 8 + 8},
      {kDungeonSpawnHorizontalScroll + kSpawnId * 2,
       kDungeonSpawnHorizontalScroll + kSpawnId * 2 + 2},
      {kDungeonSpawnVerticalScroll + kSpawnId * 2,
       kDungeonSpawnVerticalScroll + kSpawnId * 2 + 2},
      {kDungeonSpawnYCoordinate + kSpawnId * 2,
       kDungeonSpawnYCoordinate + kSpawnId * 2 + 2},
      {kDungeonSpawnXCoordinate + kSpawnId * 2,
       kDungeonSpawnXCoordinate + kSpawnId * 2 + 2},
      {kDungeonSpawnCameraTriggerY + kSpawnId * 2,
       kDungeonSpawnCameraTriggerY + kSpawnId * 2 + 2},
      {kDungeonSpawnCameraTriggerX + kSpawnId * 2,
       kDungeonSpawnCameraTriggerX + kSpawnId * 2 + 2},
      {kDungeonSpawnMainGfx + kSpawnId, kDungeonSpawnMainGfx + kSpawnId + 1},
      {kDungeonSpawnFloor + kSpawnId, kDungeonSpawnFloor + kSpawnId + 1},
      {kDungeonSpawnDungeonId + kSpawnId,
       kDungeonSpawnDungeonId + kSpawnId + 1},
      {kDungeonSpawnLayer + kSpawnId, kDungeonSpawnLayer + kSpawnId + 1},
      {kDungeonSpawnCameraScrollController + kSpawnId,
       kDungeonSpawnCameraScrollController + kSpawnId + 1},
      {kDungeonSpawnQuadrant + kSpawnId, kDungeonSpawnQuadrant + kSpawnId + 1},
      {kDungeonSpawnOverworldDoorTilemap + kSpawnId * 2,
       kDungeonSpawnOverworldDoorTilemap + kSpawnId * 2 + 2},
      {kDungeonSpawnEntranceId + kSpawnId * 2,
       kDungeonSpawnEntranceId + kSpawnId * 2 + 2},
      {kDungeonSpawnSong + kSpawnId, kDungeonSpawnSong + kSpawnId + 1},
  };

  const auto ranges = CollectDirtyDungeonSpawnPointWriteRanges(spawn_points);
  EXPECT_EQ(ranges, expected);
  size_t unique_bytes = 0;
  for (const auto& [begin, end] : ranges) {
    unique_bytes += end - begin;
  }
  EXPECT_EQ(ranges.size(), 17u);
  EXPECT_EQ(unique_bytes, 33u);
  EXPECT_EQ(std::count(ranges.begin(), ranges.end(),
                       DungeonSpawnPointWriteRange{
                           kDungeonSpawnQuadrant + kSpawnId,
                           kDungeonSpawnQuadrant + kSpawnId + 1}),
            1);
  EXPECT_EQ(kStartingEntranceDoor, kStartingEntranceScrollQuadrant);
}

TEST_F(DungeonSaveTest,
       DungeonSpawnPointLoadKeepsRoomDoorTilemapAndEntranceIdDistinct) {
  constexpr int kSpawnId = 1;
  ASSERT_TRUE(rom_->WriteShort(kDungeonSpawnRoom + kSpawnId * 2, 0x01AB).ok());
  ASSERT_TRUE(
      rom_->WriteShort(kDungeonSpawnHorizontalScroll + kSpawnId * 2, 0x1234)
          .ok());
  ASSERT_TRUE(
      rom_->WriteShort(kDungeonSpawnVerticalScroll + kSpawnId * 2, 0x5678)
          .ok());
  ASSERT_TRUE(rom_->WriteByte(kDungeonSpawnQuadrant + kSpawnId, 0x21).ok());
  ASSERT_TRUE(
      rom_->WriteShort(kDungeonSpawnOverworldDoorTilemap + kSpawnId * 2, 0xA5C3)
          .ok());
  ASSERT_TRUE(
      rom_->WriteShort(kDungeonSpawnEntranceId + kSpawnId * 2, 0x0084).ok());

  auto loaded = DungeonSpawnPoint::Load(*rom_, kSpawnId);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->room_id, 0x01AB);
  EXPECT_EQ(loaded->horizontal_scroll, 0x1234);
  EXPECT_EQ(loaded->vertical_scroll, 0x5678);
  EXPECT_EQ(loaded->quadrant, 0x21);
  EXPECT_EQ(loaded->overworld_door_tilemap, 0xA5C3);
  EXPECT_EQ(loaded->entrance_id, 0x0084);

  const RoomEntrance navigation_view(rom_.get(), kSpawnId, true);
  EXPECT_EQ(navigation_view.room_, 0x01AB);
  EXPECT_EQ(navigation_view.camera_x_, 0x1234);
  EXPECT_EQ(navigation_view.camera_y_, 0x5678);
  EXPECT_EQ(navigation_view.scroll_quadrant_, 0x21);
  EXPECT_EQ(navigation_view.door_, 0);
  EXPECT_EQ(static_cast<uint16_t>(navigation_view.exit_), 0xA5C3);
}

TEST_F(DungeonSaveTest,
       SaveAllDungeonSpawnPointsWritesOnlyExactDedicatedRecord) {
  constexpr int kSpawnId = 2;
  auto loaded = DungeonSpawnPoint::Load(*rom_, kSpawnId);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints> spawn_points{};
  spawn_points[kSpawnId] = *loaded;
  auto& spawn = spawn_points[kSpawnId];
  spawn.room_id = 0x01AB;
  spawn.camera_scroll_boundaries = {0x10, 0x11, 0x12, 0x13,
                                    0x14, 0x15, 0x16, 0x17};
  spawn.horizontal_scroll = 0x1234;
  spawn.vertical_scroll = 0x2345;
  spawn.y_coordinate = 0x3456;
  spawn.x_coordinate = 0x4567;
  spawn.camera_trigger_y = 0x5678;
  spawn.camera_trigger_x = 0x6789;
  spawn.main_gfx = 0x11;
  spawn.floor = 0xFE;
  spawn.dungeon_id = 0x22;
  spawn.layer = 0x31;
  spawn.camera_scroll_controller = 0x42;
  spawn.quadrant = 0x21;
  spawn.overworld_door_tilemap = 0xA5C3;
  spawn.entrance_id = 0x0084;
  spawn.song = 0x33;
  spawn.MarkDirty();
  const auto before = rom_->vector();
  const auto expected_ranges = DungeonSpawnPointWriteRanges(kSpawnId);

  const absl::Status status =
      SaveAllDungeonSpawnPoints(rom_.get(), spawn_points);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_FALSE(spawn.dirty());
  for (size_t pc = 0; pc < before.size(); ++pc) {
    if (before[pc] == rom_->data()[pc]) {
      continue;
    }
    EXPECT_TRUE(std::any_of(expected_ranges.begin(), expected_ranges.end(),
                            [pc](const DungeonSpawnPointWriteRange& range) {
                              return range.first <= pc && pc < range.second;
                            }))
        << "Spawn write escaped exact ranges at PC 0x" << std::hex << pc;
  }

  auto reopened = DungeonSpawnPoint::Load(*rom_, kSpawnId);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->room_id, 0x01AB);
  EXPECT_EQ(
      reopened->camera_scroll_boundaries,
      (std::array<uint8_t, 8>{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}));
  EXPECT_EQ(reopened->horizontal_scroll, 0x1234);
  EXPECT_EQ(reopened->vertical_scroll, 0x2345);
  EXPECT_EQ(reopened->y_coordinate, 0x3456);
  EXPECT_EQ(reopened->x_coordinate, 0x4567);
  EXPECT_EQ(reopened->camera_trigger_y, 0x5678);
  EXPECT_EQ(reopened->camera_trigger_x, 0x6789);
  EXPECT_EQ(reopened->main_gfx, 0x11);
  EXPECT_EQ(reopened->floor, 0xFE);
  EXPECT_EQ(reopened->dungeon_id, 0x22);
  EXPECT_EQ(reopened->layer, 0x31);
  EXPECT_EQ(reopened->camera_scroll_controller, 0x42);
  EXPECT_EQ(reopened->overworld_door_tilemap, 0xA5C3);
  EXPECT_EQ(reopened->entrance_id, 0x0084);
  EXPECT_EQ(reopened->quadrant, 0x21);
  EXPECT_EQ(reopened->song, 0x33);
}

TEST_F(DungeonSaveTest,
       SaveAllDungeonSpawnPointsPreflightsEveryRecordBeforeFirstWrite) {
  std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints> spawn_points{};
  for (int spawn_id : {0, 1}) {
    auto loaded = DungeonSpawnPoint::Load(*rom_, spawn_id);
    ASSERT_TRUE(loaded.ok()) << loaded.status();
    spawn_points[spawn_id] = *loaded;
  }
  spawn_points[0].room_id = 1;
  spawn_points[0].MarkDirty();
  spawn_points[1].room_id = 0x0200;
  spawn_points[1].MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status =
      SaveAllDungeonSpawnPoints(rom_.get(), spawn_points);

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(spawn_points[0].dirty());
  EXPECT_TRUE(spawn_points[1].dirty());
}

TEST_F(DungeonSaveTest,
       DungeonSpawnPointRejectsInvalidIdsAndTruncatedRomBeforeMutation) {
  auto loaded = DungeonSpawnPoint::Load(*rom_, 2);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  loaded->room_id = 1;
  loaded->MarkDirty();
  const auto before = rom_->vector();

  const absl::Status mismatch = loaded->Save(rom_.get(), 3);
  EXPECT_EQ(mismatch.code(), absl::StatusCode::kInvalidArgument) << mismatch;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(loaded->dirty());

  loaded->entrance_id = kNumRegularDungeonEntrances;
  const absl::Status invalid_entrance = loaded->Save(rom_.get(), 2);
  EXPECT_EQ(invalid_entrance.code(), absl::StatusCode::kOutOfRange)
      << invalid_entrance;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(loaded->dirty());

  Rom truncated;
  ASSERT_TRUE(
      truncated.LoadFromData(std::vector<uint8_t>(kDungeonSpawnSong, 0)).ok());
  const auto truncated_before = truncated.vector();
  auto truncated_load = DungeonSpawnPoint::Load(truncated, 0);
  EXPECT_EQ(truncated_load.status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(truncated.vector(), truncated_before);
}

TEST_F(DungeonSaveTest, SaveAllDungeonEntrances_WritesDirtyRegularEntrance) {
  std::array<RoomEntrance, kNumDungeonEntranceSlots> entrances{};
  entrances[kNumDungeonSpawnPoints + 3] = RoomEntrance(rom_.get(), 3, false);
  auto& entrance = entrances[kNumDungeonSpawnPoints + 3];
  entrance.room_ = 0x0123;
  entrance.x_position_ = 0x0456;
  entrance.y_position_ = 0x0789;
  entrance.camera_x_ = 0x0102;
  entrance.camera_y_ = 0x0304;
  entrance.camera_trigger_x_ = 0x0506;
  entrance.camera_trigger_y_ = 0x0708;
  entrance.exit_ = 0x0009;
  entrance.blockset_ = 0x0A;
  entrance.music_ = 0x0B;
  entrance.dungeon_id_ = 0x0C;
  entrance.door_ = 0x0D;
  entrance.floor_ = 0x0E;
  entrance.ladder_bg_ = 0x0F;
  entrance.scrolling_ = 0x10;
  entrance.scroll_quadrant_ = 0x11;
  entrance.camera_boundary_qn_ = 0x12;
  entrance.camera_boundary_fn_ = 0x13;
  entrance.camera_boundary_qs_ = 0x14;
  entrance.camera_boundary_fs_ = 0x15;
  entrance.camera_boundary_qw_ = 0x16;
  entrance.camera_boundary_fw_ = 0x17;
  entrance.camera_boundary_qe_ = 0x18;
  entrance.camera_boundary_fe_ = 0x19;
  entrance.MarkDirty();

  auto status = SaveAllDungeonEntrances(rom_.get(), entrances);
  ASSERT_TRUE(status.ok()) << status.message();

  const int entrance_id = 3;
  EXPECT_EQ(rom_->data()[kEntranceRoom + entrance_id * 2], 0x23);
  EXPECT_EQ(rom_->data()[kEntranceRoom + entrance_id * 2 + 1], 0x01);
  EXPECT_EQ(rom_->data()[kEntranceXPosition + entrance_id * 2], 0x56);
  EXPECT_EQ(rom_->data()[kEntranceYPosition + entrance_id * 2], 0x89);
  EXPECT_EQ(rom_->data()[kEntranceBlockset + entrance_id], 0x0A);
  EXPECT_EQ(rom_->data()[kEntranceMusic + entrance_id], 0x0B);
  EXPECT_EQ(rom_->data()[kEntranceScrollEdge + entrance_id * 8], 0x12);
  EXPECT_EQ(rom_->data()[kEntranceScrollEdge + entrance_id * 8 + 7], 0x19);
  EXPECT_FALSE(entrance.dirty());
}

TEST_F(DungeonSaveTest, CollectDirtyRegularDungeonEntranceWriteRangesIsExact) {
  constexpr int kEntranceId = 3;
  std::array<RoomEntrance, kNumDungeonEntranceSlots> entrances{};
  entrances[kNumDungeonSpawnPoints + kEntranceId].MarkDirty();

  const std::vector<DungeonEntranceWriteRange> expected = {
      {kEntranceRoom + kEntranceId * 2, kEntranceRoom + kEntranceId * 2 + 2},
      {kEntranceScrollEdge + kEntranceId * 8,
       kEntranceScrollEdge + kEntranceId * 8 + 8},
      {kEntranceYScroll + kEntranceId * 2,
       kEntranceYScroll + kEntranceId * 2 + 2},
      {kEntranceXScroll + kEntranceId * 2,
       kEntranceXScroll + kEntranceId * 2 + 2},
      {kEntranceYPosition + kEntranceId * 2,
       kEntranceYPosition + kEntranceId * 2 + 2},
      {kEntranceXPosition + kEntranceId * 2,
       kEntranceXPosition + kEntranceId * 2 + 2},
      {kEntranceCameraYTrigger + kEntranceId * 2,
       kEntranceCameraYTrigger + kEntranceId * 2 + 2},
      {kEntranceCameraXTrigger + kEntranceId * 2,
       kEntranceCameraXTrigger + kEntranceId * 2 + 2},
      {kEntranceBlockset + kEntranceId, kEntranceBlockset + kEntranceId + 1},
      {kEntranceFloor + kEntranceId, kEntranceFloor + kEntranceId + 1},
      {kEntranceDungeon + kEntranceId, kEntranceDungeon + kEntranceId + 1},
      {kEntranceDoor + kEntranceId, kEntranceDoor + kEntranceId + 1},
      {kEntranceLadderBG + kEntranceId, kEntranceLadderBG + kEntranceId + 1},
      {kEntrancescrolling + kEntranceId, kEntrancescrolling + kEntranceId + 1},
      {kEntranceScrollQuadrant + kEntranceId,
       kEntranceScrollQuadrant + kEntranceId + 1},
      {kEntranceExit + kEntranceId * 2, kEntranceExit + kEntranceId * 2 + 2},
      {kEntranceMusic + kEntranceId, kEntranceMusic + kEntranceId + 1},
  };

  const auto ranges = CollectDirtyRegularDungeonEntranceWriteRanges(entrances);
  EXPECT_EQ(ranges, expected);
  size_t unique_bytes = 0;
  for (const auto& [begin, end] : ranges) {
    unique_bytes += end - begin;
  }
  EXPECT_EQ(ranges.size(), 17u);
  EXPECT_EQ(unique_bytes, 32u);
}

TEST_F(DungeonSaveTest,
       SaveAllDungeonEntrances_PreflightsEveryRegularBeforeFirstWrite) {
  std::array<RoomEntrance, kNumDungeonEntranceSlots> entrances{};
  entrances[kNumDungeonSpawnPoints] = RoomEntrance(rom_.get(), 0, false);
  entrances[kNumDungeonSpawnPoints + 1] = RoomEntrance(rom_.get(), 1, false);
  auto& first = entrances[kNumDungeonSpawnPoints];
  first.room_ = 1;
  first.MarkDirty();
  auto& invalid = entrances[kNumDungeonSpawnPoints + 1];
  invalid.room_ = kNumberOfRooms;
  invalid.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = SaveAllDungeonEntrances(rom_.get(), entrances);

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(first.dirty());
  EXPECT_TRUE(invalid.dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllDungeonEntrances_RejectsOutOfBoundsRangeBeforeMutation) {
  ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(kEntranceMusic, 0)).ok());
  std::array<RoomEntrance, kNumDungeonEntranceSlots> entrances{};
  auto& entrance = entrances[kNumDungeonSpawnPoints];
  entrance.room_ = 1;
  entrance.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = SaveAllDungeonEntrances(rom_.get(), entrances);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(entrance.dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllDungeonEntrances_RejectsDirtySpawnBeforeMutation) {
  std::array<RoomEntrance, kNumDungeonEntranceSlots> entrances{};
  entrances[kNumDungeonSpawnPoints] = RoomEntrance(rom_.get(), 0, false);
  entrances[2] = RoomEntrance(rom_.get(), 2, true);
  auto& regular = entrances[kNumDungeonSpawnPoints];
  regular.room_ = 1;
  regular.MarkDirty();
  auto& spawn = entrances[2];
  spawn.room_ = 0x0034;
  spawn.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = SaveAllDungeonEntrances(rom_.get(), entrances);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(spawn.dirty());
  EXPECT_TRUE(regular.dirty());
}

TEST_F(DungeonSaveTest,
       RoomEntranceSave_DirectSpawnModelFailsClosedWithoutMutation) {
  RoomEntrance spawn(rom_.get(), 2, true);
  spawn.room_ = 0x0034;
  spawn.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = spawn.Save(rom_.get(), 2, true);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_NE(std::string(status.message()).find("dedicated spawn ROM schema"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(spawn.dirty());
}

TEST_F(DungeonSaveTest,
       RoomEntranceSave_RegularModelPassedAsSpawnFailsClosedWithoutMutation) {
  RoomEntrance entrance(rom_.get(), 3, false);
  entrance.room_ = 0x0123;
  entrance.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = entrance.Save(rom_.get(), 3, true);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_NE(std::string(status.message()).find("dedicated spawn ROM schema"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(entrance.dirty());
}

TEST_F(DungeonSaveTest, RoomEntranceSave_InvalidRegularIdsFailBeforeMutation) {
  for (const int invalid_id : {-1, kNumRegularDungeonEntrances}) {
    SCOPED_TRACE(invalid_id);
    RoomEntrance entrance(rom_.get(), 3, false);
    entrance.room_ = 0x0123;
    entrance.MarkDirty();
    const auto before = rom_->vector();

    const absl::Status status = entrance.Save(rom_.get(), invalid_id, false);

    EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange) << status;
    EXPECT_EQ(rom_->vector(), before);
    EXPECT_TRUE(entrance.dirty());
  }
}

TEST_F(DungeonSaveTest,
       RoomEntranceSave_SpawnModelAsRegularFailsBeforeMutation) {
  RoomEntrance spawn(rom_.get(), 2, true);
  spawn.room_ = 0x0034;
  spawn.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = spawn.Save(rom_.get(), 2, false);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(spawn.dirty());
}

TEST_F(DungeonSaveTest, RoomEntranceSave_ModelIdMismatchFailsBeforeMutation) {
  RoomEntrance entrance(rom_.get(), 3, false);
  entrance.room_ = 0x0123;
  entrance.MarkDirty();
  const auto before = rom_->vector();

  const absl::Status status = entrance.Save(rom_.get(), 4, false);

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument) << status;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(entrance.dirty());
}

TEST_F(DungeonSaveTest, SaveAllCollision_DirtyRoomWithoutCustomRegionFails) {
  std::vector<uint8_t> small_data(0x100000, 0);
  rom_->LoadFromData(small_data);

  std::vector<Room> rooms(kNumberOfRooms);
  rooms[0].SetCollisionTile(1, 1, 0x2A);

  auto status = SaveAllCollision(rom_.get(), absl::MakeSpan(rooms));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonSaveTest, SaveAllPits_ValidRegionPreservesExistingBytes) {
  SetupPitRegion();

  const auto before0 = rom_->data()[kPitDataPc + 0];
  const auto before1 = rom_->data()[kPitDataPc + 1];

  auto status = SaveAllPits(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kPitDataPc + 0], before0);
  EXPECT_EQ(rom_->data()[kPitDataPc + 1], before1);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_ValidRegionsPreserveExistingBytes) {
  SetupBlockRegions();

  auto status = SaveAllBlocks(rom_.get());
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0xAA);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0xBB);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0xCC);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0xDD);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RejectsCorruptLoaderOperand) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksPointer1 - 1] = 0xEA;  // Not LDA.l.

  const auto status = SaveAllBlocks(rom_.get());

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("LDA.l ...,X / STA.w"),
            std::string::npos);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RoomAware_RejectsCorruptLoaderOperand) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksPointer1 - 1] = 0xEA;  // Not LDA.l.

  const auto status =
      SaveAllBlocks(rom_.get(), 296, [this](int rid) -> const Room* {
        return rid == 0 ? room_.get() : nullptr;
      });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("LDA.l ...,X / STA.w"),
            std::string::npos);
}

TEST_F(DungeonSaveTest, LoadBlocks_RejectsCorruptLoaderOperand) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksPointer1 - 1] = 0xEA;  // Not LDA.l.
  RoomObject existing_block(0x0E00, 3, 3, 0, 0);
  existing_block.set_options(ObjectOption::Block);
  room_->AddTileObject(existing_block);

  room_->LoadBlocks();

  EXPECT_FALSE(room_->AreBlocksLoaded());
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);
  EXPECT_NE(room_->GetTileObjects()[0].options() & ObjectOption::Block,
            ObjectOption::Nothing);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_PreservesUnmaterializedRoomBytes) {
  // Migration safety invariant: when only some rooms are
  // materialized, blocks for the other rooms (lookup returns null,
  // OR returns a Room whose blocks weren't loaded) must keep their
  // existing ROM bytes verbatim. The encoder cannot drop them just
  // because no in-memory representation was supplied.
  //
  // Setup: two encoded entries in slot 0 (room 0, materialized) and
  // slot 1 (room 5, NOT materialized). Save with a lookup that only
  // returns room_ for id=0. Both slots must come out unchanged.
  SetupBlockRegions();
  // Slot 0: room 0, px=10, py=20, upper draw, lower behavior (0x4A14).
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;
  // Slot 1: room 5, px=15, py=25, both selectors clear.
  //   word = (15 << 1) | (25 << 7) = 30 | 3200 = 0x0C9E
  //   → b3 = 0x9E, b4 = 0x0C.
  rom_->mutable_data()[kBlocksRegion1Pc + 4] = 0x05;
  rom_->mutable_data()[kBlocksRegion1Pc + 5] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 6] = 0x9E;
  rom_->mutable_data()[kBlocksRegion1Pc + 7] = 0x0C;
  rom_->mutable_data()[kBlocksLength] = 0x08;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  const std::array<uint8_t, 8> before = {0x00, 0x00, 0x14, 0x4A,
                                         0x05, 0x00, 0x9E, 0x0C};

  // Materialize room 0 only. LoadBlocks scans the global buffer for
  // matching room_id, so only slot 0 lands in tile_objects_. Room 5
  // stays unloaded — its block bytes must survive the save.
  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);
  ASSERT_TRUE(room_->AreBlocksLoaded());

  // Lookup hands out room_ for id 0; everything else is null
  // (simulates an editor that only materialized room 0).
  auto status = SaveAllBlocks(rom_.get(), 296, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + i], before[i])
        << "byte " << i
        << " — unmaterialized room 5's block bytes must survive a save "
           "where only room 0 was materialized.";
  }
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_EditedBlockPreservesUnmaterializedNeighbor) {
  // Migration safety invariant #2: an edit to a materialized room's
  // block writes through, AND an unmaterialized room's adjacent slot
  // remains byte-identical. This is the test that pins the encoder
  // boundary between "owned by editor, re-encode" and "owned by ROM,
  // preserve verbatim".
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // px=10
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;
  rom_->mutable_data()[kBlocksRegion1Pc + 4] = 0x05;
  rom_->mutable_data()[kBlocksRegion1Pc + 5] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 6] = 0x9E;
  rom_->mutable_data()[kBlocksRegion1Pc + 7] = 0x0C;
  rom_->mutable_data()[kBlocksLength] = 0x08;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);

  // Edit room 0's block: px 10 -> 30. The independent lower behavior bit is
  // preserved: (30<<1)|(20<<7)|(1<<14) = 0x4A3C.
  for (auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) == ObjectOption::Block) {
      obj.set_x(30);
      break;
    }
  }

  auto status = SaveAllBlocks(rom_.get(), 296, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0x3C) << "edited px=30";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x4A);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 4], 0x05) << "room 5 preserved";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 5], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 6], 0x9E);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 7], 0x0C);
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_NoOpRoundTripPreservesPointedBytes) {
  // Real encoder invariant #1 (test-first for the SaveAllBlocks
  // follow-up). After Load → Save with no in-memory mutation, the
  // bytes at the dereferenced data region must be byte-identical to
  // the original — vanilla saves with no edits cannot reshuffle
  // anything. Single block in single room: source-order preservation
  // is trivial here, so this pins the simpler invariant first; a
  // multi-room ordering test belongs to the real ROM round-trip
  // follow-up.
  SetupBlockRegions();
  // Replace the fixture's sample 0xAA..0xDD with a real entry for
  // room 0 at (px=10, py=20, draw=upper, behavior=lower) -- the same shape used by
  // LoadBlocks_ReadsFromDereferencedPointerRegion.
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;

  const std::array<uint8_t, 4> before = {0x00, 0x00, 0x14, 0x4A};

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U)
      << "Pre-save: LoadBlocks should have produced exactly one Block "
         "tile object.";

  auto status = SaveAllBlocks(rom_.get(), 1, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + i], before[i])
        << "byte " << i
        << " — no-op round trip must not mutate the pointed region.";
  }
  // Length immediate also stays at 4 (one entry × 4 bytes).
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x04);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsDeletingFinalEntryWithoutMutation) {
  SetupBlockRegions();
  const auto only_entry =
      EncodePushableBlockEntry({/*room_id=*/0, /*px=*/10, /*py=*/20,
                                /*draw_layer=*/1, /*behavior_layer=*/0});
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = only_entry.b1;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = only_entry.b2;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = only_entry.b3;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = only_entry.b4;

  Room room0(0, rom_.get());
  Room room1(1, rom_.get());
  room0.LoadBlocks();
  room1.LoadBlocks();
  ASSERT_TRUE(room0.AreBlocksLoaded());
  ASSERT_TRUE(room1.AreBlocksLoaded());
  ASSERT_EQ(room0.GetTileObjects().size(), 1u);
  ASSERT_TRUE(room1.GetTileObjects().empty());

  room0.RemoveTileObject(0);
  room1.MarkBlocksDirty();
  ASSERT_TRUE(room0.blocks_dirty());
  ASSERT_TRUE(room1.blocks_dirty());
  const auto before = rom_->vector();

  const auto status = SaveAllBlocks(
      rom_.get(), 2, [&room0, &room1](int room_id) -> const Room* {
        if (room_id == 0)
          return &room0;
        if (room_id == 1)
          return &room1;
        return nullptr;
      });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("cannot be empty"),
            std::string::npos);
  EXPECT_NE(std::string(status.message()).find("at least one"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x04);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
  EXPECT_TRUE(room0.blocks_dirty());
  EXPECT_TRUE(room1.blocks_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsDirtyUnloadedRoomWithoutMutation) {
  SetupBlockRegions();
  Room room(0, rom_.get());
  room.AddTileObject(MakePushableBlock(/*px=*/10, /*py=*/20,
                                       /*draw_layer=*/1));
  ASSERT_FALSE(room.AreBlocksLoaded());
  ASSERT_TRUE(room.blocks_dirty());
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  ASSERT_EQ(room.GetTileObjects()[0].block_load_order(),
            RoomObject::kBlockLoadOrderNew);
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [&room](int room_id) -> const Room* {
        return room_id == 0 ? &room : nullptr;
      });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Room 0x000"),
            std::string::npos);
  EXPECT_NE(std::string(status.message()).find("block table is not loaded"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(),
            RoomObject::kBlockLoadOrderNew);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_OneEntryWritesFourByteRuntimeLimit) {
  SetupBlockRegions();
  const auto surviving_entry =
      EncodePushableBlockEntry({/*room_id=*/0, /*px=*/10, /*py=*/20,
                                /*draw_layer=*/1, /*behavior_layer=*/0});
  const auto deleted_entry =
      EncodePushableBlockEntry({/*room_id=*/1, /*px=*/15, /*py=*/25,
                                /*draw_layer=*/0, /*behavior_layer=*/1});
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = surviving_entry.b1;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = surviving_entry.b2;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = surviving_entry.b3;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = surviving_entry.b4;
  rom_->mutable_data()[kBlocksRegion1Pc + 4] = deleted_entry.b1;
  rom_->mutable_data()[kBlocksRegion1Pc + 5] = deleted_entry.b2;
  rom_->mutable_data()[kBlocksRegion1Pc + 6] = deleted_entry.b3;
  rom_->mutable_data()[kBlocksRegion1Pc + 7] = deleted_entry.b4;
  rom_->mutable_data()[kBlocksLength] = 0x08;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room room0(0, rom_.get());
  Room room1(1, rom_.get());
  room0.LoadBlocks();
  room1.LoadBlocks();
  ASSERT_EQ(room0.GetTileObjects().size(), 1u);
  ASSERT_EQ(room1.GetTileObjects().size(), 1u);
  EXPECT_EQ(room0.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_EQ(room1.GetTileObjects()[0].block_load_order(), 1);

  room0.MarkBlocksDirty();
  room1.RemoveTileObject(0);
  const auto status = SaveAllBlocks(
      rom_.get(), 2, [&room0, &room1](int room_id) -> const Room* {
        if (room_id == 0)
          return &room0;
        if (room_id == 1)
          return &room1;
        return nullptr;
      });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], surviving_entry.b1);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], surviving_entry.b2);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], surviving_entry.b3);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], surviving_entry.b4);
  // The runtime adds four after processing its first entry, then compares
  // against this immediate. Four is therefore the exact safe minimum.
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x04);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
  EXPECT_EQ(room0.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_FALSE(room0.blocks_dirty());
  EXPECT_FALSE(room1.blocks_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RebasesCompactedSlotsBeforeSecondSave) {
  SetupBlockRegions();
  constexpr int kLoadedRoomId = 0x10B;
  constexpr int kUnmaterializedRoomId = 0xA8;
  const auto deleted_entry =
      EncodePushableBlockEntry({/*room_id=*/kLoadedRoomId, /*px=*/10, /*py=*/20,
                                /*draw_layer=*/0, /*behavior_layer=*/0});
  const auto surviving_entry =
      EncodePushableBlockEntry({/*room_id=*/kLoadedRoomId, /*px=*/30, /*py=*/21,
                                /*draw_layer=*/1, /*behavior_layer=*/1});
  const auto unmaterialized_entry = EncodePushableBlockEntry(
      {/*room_id=*/kUnmaterializedRoomId, /*px=*/25, /*py=*/45,
       /*draw_layer=*/0, /*behavior_layer=*/1});
  const std::array<PushableBlockBytes, 3> entries = {
      deleted_entry, surviving_entry, unmaterialized_entry};
  for (size_t slot = 0; slot < entries.size(); ++slot) {
    const int pc = kBlocksRegion1Pc + static_cast<int>(slot * 4);
    rom_->mutable_data()[pc + 0] = entries[slot].b1;
    rom_->mutable_data()[pc + 1] = entries[slot].b2;
    rom_->mutable_data()[pc + 2] = entries[slot].b3;
    rom_->mutable_data()[pc + 3] = entries[slot].b4;
  }
  rom_->mutable_data()[kBlocksLength] = 0x0C;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room loaded_room(kLoadedRoomId, rom_.get());
  loaded_room.LoadBlocks();
  ASSERT_EQ(loaded_room.GetTileObjects().size(), 2u);
  EXPECT_EQ(loaded_room.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_EQ(loaded_room.GetTileObjects()[1].block_load_order(), 1);

  loaded_room.RemoveTileObject(0);
  ASSERT_EQ(loaded_room.GetTileObjects().size(), 1u);
  ASSERT_EQ(loaded_room.GetTileObjects()[0].block_load_order(), 1);
  const auto lookup = [&loaded_room](int room_id) -> const Room* {
    return room_id == kLoadedRoomId ? &loaded_room : nullptr;
  };

  const auto first_status = SaveAllBlocks(rom_.get(), kNumberOfRooms, lookup);
  ASSERT_TRUE(first_status.ok()) << first_status.message();
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
  EXPECT_EQ(loaded_room.GetTileObjects()[0].block_load_order(), 0)
      << "The surviving loaded block compacted from slot 1 to slot 0.";
  EXPECT_FALSE(loaded_room.blocks_dirty());
  const std::array<PushableBlockBytes, 2> expected_after_delete = {
      surviving_entry, unmaterialized_entry};
  for (size_t slot = 0; slot < expected_after_delete.size(); ++slot) {
    const int pc = kBlocksRegion1Pc + static_cast<int>(slot * 4);
    EXPECT_EQ(rom_->data()[pc + 0], expected_after_delete[slot].b1);
    EXPECT_EQ(rom_->data()[pc + 1], expected_after_delete[slot].b2);
    EXPECT_EQ(rom_->data()[pc + 2], expected_after_delete[slot].b3);
    EXPECT_EQ(rom_->data()[pc + 3], expected_after_delete[slot].b4);
  }

  // A second ordinary no-op save must use the rebased slot 0 identity. With
  // the stale pre-compaction identity (1), slot 0 appears deleted and the
  // unmaterialized room's shifted neighbor is the only entry left.
  const auto before_second_save = rom_->vector();
  const auto second_status = SaveAllBlocks(rom_.get(), kNumberOfRooms, lookup);
  ASSERT_TRUE(second_status.ok()) << second_status.message();
  EXPECT_EQ(rom_->vector(), before_second_save);
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
  EXPECT_EQ(loaded_room.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_FALSE(loaded_room.blocks_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_SaveUndoRedoSaveReconcilesStaleSlotIdentities) {
  SetupBlockRegions();
  constexpr int kLoadedRoomId = 0x10B;
  constexpr int kUnmaterializedRoomId = 0xA8;
  const auto first_loaded_entry =
      EncodePushableBlockEntry({/*room_id=*/kLoadedRoomId, /*px=*/10, /*py=*/20,
                                /*draw_layer=*/0, /*behavior_layer=*/0});
  const auto second_loaded_entry =
      EncodePushableBlockEntry({/*room_id=*/kLoadedRoomId, /*px=*/30, /*py=*/21,
                                /*draw_layer=*/1, /*behavior_layer=*/1});
  const auto unmaterialized_entry = EncodePushableBlockEntry(
      {/*room_id=*/kUnmaterializedRoomId, /*px=*/25, /*py=*/45,
       /*draw_layer=*/0, /*behavior_layer=*/1});
  const std::array<PushableBlockBytes, 3> entries = {
      first_loaded_entry, unmaterialized_entry, second_loaded_entry};
  for (size_t slot = 0; slot < entries.size(); ++slot) {
    const int pc = kBlocksRegion1Pc + static_cast<int>(slot * 4);
    rom_->mutable_data()[pc + 0] = entries[slot].b1;
    rom_->mutable_data()[pc + 1] = entries[slot].b2;
    rom_->mutable_data()[pc + 2] = entries[slot].b3;
    rom_->mutable_data()[pc + 3] = entries[slot].b4;
  }
  rom_->mutable_data()[kBlocksLength] = 0x0C;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room loaded_room(kLoadedRoomId, rom_.get());
  loaded_room.LoadBlocks();
  ASSERT_EQ(loaded_room.GetTileObjects().size(), 2u);
  const std::vector<RoomObject> before_objects = loaded_room.GetTileObjects();

  loaded_room.RemoveTileObject(0);
  ASSERT_EQ(loaded_room.GetTileObjects().size(), 1u);
  const std::vector<RoomObject> after_objects = loaded_room.GetTileObjects();
  ASSERT_EQ(after_objects[0].block_load_order(), 2);

  const auto lookup = [&loaded_room](int room_id) -> const Room* {
    return room_id == kLoadedRoomId ? &loaded_room : nullptr;
  };
  const auto delete_status = SaveAllBlocks(rom_.get(), kNumberOfRooms, lookup);
  ASSERT_TRUE(delete_status.ok()) << delete_status.message();
  ASSERT_EQ(rom_->data()[kBlocksLength], 0x08);
  ASSERT_EQ(loaded_room.GetTileObjects()[0].block_load_order(), 1);

  // DungeonObjectsAction restores these complete vectors. The pre-save
  // snapshot still claims slots 0 and 2, while the committed table is now
  // [unmaterialized_entry, second_loaded_entry]. Slot 0 therefore belongs to
  // another room and slot 2 is no longer materialized; neither stale identity
  // may replace or discard the unmaterialized neighbor.
  loaded_room.SetTileObjects(before_objects);
  ASSERT_TRUE(loaded_room.blocks_dirty());
  const auto undo_save_status =
      SaveAllBlocks(rom_.get(), kNumberOfRooms, lookup);
  ASSERT_TRUE(undo_save_status.ok()) << undo_save_status.message();
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x0C);
  EXPECT_FALSE(loaded_room.blocks_dirty());

  Room reopened_loaded_after_undo(kLoadedRoomId, rom_.get());
  Room reopened_neighbor_after_undo(kUnmaterializedRoomId, rom_.get());
  reopened_loaded_after_undo.LoadBlocks();
  reopened_neighbor_after_undo.LoadBlocks();
  ASSERT_EQ(reopened_loaded_after_undo.GetTileObjects().size(), 2u);
  std::vector<int> undo_loaded_x;
  for (const auto& object : reopened_loaded_after_undo.GetTileObjects()) {
    undo_loaded_x.push_back(object.x());
  }
  std::sort(undo_loaded_x.begin(), undo_loaded_x.end());
  EXPECT_EQ(undo_loaded_x, (std::vector<int>{10, 30}));
  ASSERT_EQ(reopened_neighbor_after_undo.GetTileObjects().size(), 1u);
  EXPECT_EQ(reopened_neighbor_after_undo.GetTileObjects()[0].x(), 25);

  // Redo restores the post-delete snapshot, whose sole block still carries
  // the old slot-2 identity. A second reconciliation must preserve both that
  // block and the unmaterialized neighbor.
  loaded_room.SetTileObjects(after_objects);
  ASSERT_TRUE(loaded_room.blocks_dirty());
  const auto redo_save_status =
      SaveAllBlocks(rom_.get(), kNumberOfRooms, lookup);
  ASSERT_TRUE(redo_save_status.ok()) << redo_save_status.message();
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x08);
  EXPECT_FALSE(loaded_room.blocks_dirty());

  Room reopened_loaded_after_redo(kLoadedRoomId, rom_.get());
  Room reopened_neighbor_after_redo(kUnmaterializedRoomId, rom_.get());
  reopened_loaded_after_redo.LoadBlocks();
  reopened_neighbor_after_redo.LoadBlocks();
  ASSERT_EQ(reopened_loaded_after_redo.GetTileObjects().size(), 1u);
  EXPECT_EQ(reopened_loaded_after_redo.GetTileObjects()[0].x(), 30);
  ASSERT_EQ(reopened_neighbor_after_redo.GetTileObjects().size(), 1u);
  EXPECT_EQ(reopened_neighbor_after_redo.GetTileObjects()[0].x(), 25);
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsDuplicateStaleSlotClaim) {
  SetupBlockRegions();
  const auto entry =
      EncodePushableBlockEntry({/*room_id=*/1, /*px=*/10, /*py=*/20,
                                /*draw_layer=*/0, /*behavior_layer=*/0});
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = entry.b1;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = entry.b2;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = entry.b3;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = entry.b4;

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());
  ASSERT_TRUE(room.GetTileObjects().empty());
  RoomObject first = MakePushableBlock(/*px=*/11, /*py=*/20,
                                       /*draw_layer=*/0);
  first.set_block_load_order(0);
  room.AddTileObject(first);
  RoomObject second = MakePushableBlock(/*px=*/12, /*py=*/20,
                                        /*draw_layer=*/0);
  second.set_block_load_order(0);
  room.AddTileObject(second);
  ASSERT_TRUE(room.blocks_dirty());
  ASSERT_EQ(room.GetTileObjects().size(), 2u);
  ASSERT_EQ(room.GetTileObjects()[0].block_load_order(), 0);
  ASSERT_EQ(room.GetTileObjects()[1].block_load_order(), 0);
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [&room](int room_id) -> const Room* {
        return room_id == 0 ? &room : nullptr;
      });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("multiple pushable blocks"),
            std::string::npos);
  EXPECT_NE(std::string(status.message()).find("load-order slot 0"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_EQ(room.GetTileObjects()[1].block_load_order(), 0);
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RoomAware_EditedBlockUpdatesRegion) {
  // Real encoder invariant #2: an in-memory edit (here, moving the
  // block from px=10 to px=30) writes back through the pointed
  // region. The block stays at load_order=0 since it was loaded —
  // the encoder must keep it in slot 0 and just rewrite its bytes.
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // px=10
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;  // py=20, behavior=lower

  room_->LoadBlocks();
  ASSERT_EQ(room_->GetTileObjects().size(), 1U);

  // Mutate px to 30 in place. New encoded word:
  //   word = (30 << 1) | (20 << 7) | (behavior_layer << 14)
  //        = 60 | 2560 | 16384 = 19004 = 0x4A3C
  // → b3 = 0x3C, b4 = 0x4A.
  for (auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) == ObjectOption::Block) {
      obj.set_x(30);
      break;
    }
  }

  auto status = SaveAllBlocks(rom_.get(), 1, [this](int rid) -> const Room* {
    return rid == 0 ? room_.get() : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0x00) << "room_id lo";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0x00) << "room_id hi";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0x3C) << "word lo (px=30)";
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x4A) << "word hi";
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RoomAware_ClearsBlockDirtyAfterWrite) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());

  RoomObject block(0x0E00, 10, 20, 0, 1);
  block.set_options(ObjectOption::Block);
  room.AddTileObject(block);
  ASSERT_TRUE(room.blocks_dirty());
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(),
            RoomObject::kBlockLoadOrderNew);

  auto status = SaveAllBlocks(rom_.get(), 1, [&room](int rid) -> const Room* {
    return rid == 0 ? &room : nullptr;
  });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksLength], 0x04);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0x14);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x2A);
  EXPECT_EQ(room.GetTileObjects()[0].block_load_order(), 0);
  EXPECT_FALSE(room.blocks_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsLayerTwoWithoutMutatingRom) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());

  RoomObject block(0x0E00, 10, 20, 0, 2);
  block.set_options(ObjectOption::Block);
  room.AddTileObject(block);
  ASSERT_TRUE(room.blocks_dirty());
  const auto before = rom_->vector();

  const auto status = SaveAllBlocks(
      rom_.get(), 1,
      [&room](int rid) -> const Room* { return rid == 0 ? &room : nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(status.message()).find("draw-layer selector 2"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(room.blocks_dirty());
}

TEST_F(DungeonSaveTest, SaveAllBlocks_RoomAware_AllowsExactVanillaCapacity) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());

  constexpr int kVanillaBlockCapacity = 128;
  for (int i = 0; i < kVanillaBlockCapacity; ++i) {
    room.AddTileObject(MakePushableBlock(i % 64, (i * 3) % 64, i % 2));
  }

  const auto status = SaveAllBlocks(
      rom_.get(), 1, [&room](int rid) { return rid == 0 ? &room : nullptr; });
  ASSERT_TRUE(status.ok()) << status.message();

  EXPECT_EQ(rom_->data()[kBlocksLength], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x02)
      << "128 entries * 4 bytes = 0x0200 bytes";

  const auto first = EncodePushableBlockEntry({0, 0, 0, 0});
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 0], first.b1);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 1], first.b2);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], first.b3);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], first.b4);

  const auto last = EncodePushableBlockEntry(
      {0, static_cast<uint8_t>(127 % 64), static_cast<uint8_t>((127 * 3) % 64),
       static_cast<uint8_t>(127 % 2), 0});
  const int last_entry_pc = kBlocksRegion4Pc + 0x7C;
  EXPECT_EQ(rom_->data()[last_entry_pc + 0], last.b1);
  EXPECT_EQ(rom_->data()[last_entry_pc + 1], last.b2);
  EXPECT_EQ(rom_->data()[last_entry_pc + 2], last.b3);
  EXPECT_EQ(rom_->data()[last_entry_pc + 3], last.b4);
  EXPECT_FALSE(room.blocks_dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsOverflowWithoutPartialWrite) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;
  std::fill_n(rom_->mutable_data() + kBlocksRegion1Pc, 0x80, 0x11);
  std::fill_n(rom_->mutable_data() + kBlocksRegion2Pc, 0x80, 0x22);
  std::fill_n(rom_->mutable_data() + kBlocksRegion3Pc, 0x80, 0x33);
  std::fill_n(rom_->mutable_data() + kBlocksRegion4Pc, 0x80, 0x44);

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());

  constexpr int kOverflowEntryCount = 129;
  for (int i = 0; i < kOverflowEntryCount; ++i) {
    room.AddTileObject(MakePushableBlock(i % 64, (i * 3) % 64, i % 2));
  }

  const auto status = SaveAllBlocks(
      rom_.get(), 1, [&room](int rid) { return rid == 0 ? &room : nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Pushable-block table overflow"),
            std::string::npos);
  EXPECT_EQ(rom_->data()[kBlocksLength], 0x00);
  EXPECT_EQ(rom_->data()[kBlocksLength + 1], 0x00);
  EXPECT_TRUE(std::all_of(rom_->data() + kBlocksRegion1Pc,
                          rom_->data() + kBlocksRegion1Pc + 0x80,
                          [](uint8_t b) { return b == 0x11; }));
  EXPECT_TRUE(std::all_of(rom_->data() + kBlocksRegion2Pc,
                          rom_->data() + kBlocksRegion2Pc + 0x80,
                          [](uint8_t b) { return b == 0x22; }));
  EXPECT_TRUE(std::all_of(rom_->data() + kBlocksRegion3Pc,
                          rom_->data() + kBlocksRegion3Pc + 0x80,
                          [](uint8_t b) { return b == 0x33; }));
  EXPECT_TRUE(std::all_of(rom_->data() + kBlocksRegion4Pc,
                          rom_->data() + kBlocksRegion4Pc + 0x80,
                          [](uint8_t b) { return b == 0x44; }));
  EXPECT_TRUE(room.blocks_dirty())
      << "Failed saves must not mark overflowing block edits clean.";
  EXPECT_TRUE(std::all_of(
      room.GetTileObjects().begin(), room.GetTileObjects().end(),
      [](const RoomObject& block) {
        return block.block_load_order() == RoomObject::kBlockLoadOrderNew;
      }))
      << "Failed saves must not commit pending slot identities.";
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsAliasedPagesWithoutMutation) {
  SetupBlockRegions();
  WriteLongPointer(kBlocksPointer2, PcToSnes(kBlocksRegion1Pc));
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [](int) -> const Room* { return nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("data pages 1 and 2 overlap"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsMalformedUnusedPageWithoutMutation) {
  SetupBlockRegions();
  // A one-entry table only reads page 1, but every loader page is a possible
  // future write destination and must be validated before any save mutation.
  rom_->mutable_data()[kBlocksPointer4 - 1] = 0xEA;  // Not LDA.l.
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [](int) -> const Room* { return nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("LDA.l ...,X / STA.w"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsLengthMetadataOverlapWithoutMutation) {
  SetupBlockRegions();
  WriteLongPointer(kBlocksPointer1, PcToSnes(kBlocksLength));
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [](int) -> const Room* { return nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("length metadata"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_RejectsLoaderMetadataOverlapWithoutMutation) {
  SetupBlockRegions();
  WriteLongPointer(kBlocksPointer1, PcToSnes(kBlocksPointer1 - 1));
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status =
      SaveAllBlocks(rom_.get(), 1, [](int) -> const Room* { return nullptr; });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("opcode/operand metadata"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_PreflightsNewRegionOperandBeforeWriting) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;
  std::fill_n(rom_->mutable_data() + kBlocksRegion1Pc, 0x80, 0x11);

  // Growth to 33 entries needs region 2, whose operand points at unrelated but
  // otherwise in-range data. A malformed loader opcode must be caught before
  // region 1 is written.
  constexpr int kUnrelatedPc = 0x120000;
  WriteLongPointer(kBlocksPointer2, PcToSnes(kUnrelatedPc));
  rom_->mutable_data()[kBlocksPointer2 - 1] = 0xEA;  // Not LDA.l.
  std::fill_n(rom_->mutable_data() + kUnrelatedPc, 4, 0x77);

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());
  constexpr int kEntriesRequiringSecondRegion = 33;
  for (int i = 0; i < kEntriesRequiringSecondRegion; ++i) {
    room.AddTileObject(MakePushableBlock(i % 64, (i * 3) % 64, i % 2));
  }
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllBlocks(rom_.get(), 1, [&room](int room_id) {
    return room_id == 0 ? &room : nullptr;
  });

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("LDA.l ...,X / STA.w"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_TRUE(std::all_of(
      room.GetTileObjects().begin(), room.GetTileObjects().end(),
      [](const RoomObject& block) {
        return block.block_load_order() == RoomObject::kBlockLoadOrderNew;
      }));
}

TEST_F(DungeonSaveTest,
       SaveAllBlocks_RoomAware_PreflightsNewRegionRangeBeforeWriting) {
  SetupBlockRegions();
  rom_->mutable_data()[kBlocksLength] = 0x00;
  rom_->mutable_data()[kBlocksLength + 1] = 0x00;
  std::fill_n(rom_->mutable_data() + kBlocksRegion1Pc, 0x80, 0x22);

  // Keep the region-2 loader instruction shape valid but point it beyond this
  // fixture ROM. The saver must reject the destination before writing region 1.
  WriteLongPointer(kBlocksPointer2, 0xFFFFFF);

  Room room(0, rom_.get());
  room.LoadBlocks();
  ASSERT_TRUE(room.AreBlocksLoaded());
  constexpr int kEntriesRequiringSecondRegion = 33;
  for (int i = 0; i < kEntriesRequiringSecondRegion; ++i) {
    room.AddTileObject(MakePushableBlock(i % 64, (i * 3) % 64, i % 2));
  }
  rom_->ClearDirty();
  const auto before = rom_->vector();

  const auto status = SaveAllBlocks(rom_.get(), 1, [&room](int room_id) {
    return room_id == 0 ? &room : nullptr;
  });

  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
  EXPECT_NE(std::string(status.message()).find("data region out of range"),
            std::string::npos);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_FALSE(rom_->dirty());
  EXPECT_TRUE(room.blocks_dirty());
  EXPECT_TRUE(std::all_of(
      room.GetTileObjects().begin(), room.GetTileObjects().end(),
      [](const RoomObject& block) {
        return block.block_load_order() == RoomObject::kBlockLoadOrderNew;
      }));
}

TEST_F(DungeonSaveTest, LoadBlocks_ReadsFromDereferencedPointerRegion) {
  // Loader/saver invariant: `kBlocksPointer1..4` are 3-byte SNES
  // long-address operand slots embedded in the bank_02 LDA.l
  // instructions that fan the pushable-block table into WRAM. The actual
  // entry bytes live at the pointed-to region (`kBlocksRegion1Pc..` in
  // this fixture's mock layout).
  //
  // `SaveAllBlocks` already dereferences each pointer slot and writes
  // data at the SNES address it encodes. `LoadBlocks` previously read
  // bytes directly out of the operand slots — i.e. it was decoding the
  // LDA.l opcode operand bytes as block data and silently producing
  // garbage. This test pins the dereference invariant.
  SetupBlockRegions();

  // Overwrite the fixture's sample 0xAA..0xDD seed with an encoded
  // pushable-block entry for room_id=0 at (px=10, py=20), upper draw and
  // lower behavior:
  //   word = (px << 1) | (py << 7) | (behavior_layer << 14)
  //        = 20 | 2560 | 16384 = 18964 = 0x4A14
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0x00;  // room_id lo
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;  // room_id hi
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0x14;  // word lo
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x4A;  // word hi

  room_->LoadBlocks();

  int block_count = 0;
  for (const auto& obj : room_->GetTileObjects()) {
    if ((obj.options() & ObjectOption::Block) != ObjectOption::Block)
      continue;
    ++block_count;
    EXPECT_EQ(obj.x(), 10);
    EXPECT_EQ(obj.y(), 20);
    EXPECT_EQ(obj.GetLayerValue(), 0);
    EXPECT_EQ(obj.block_behavior_layer(), 1);
  }
  EXPECT_EQ(block_count, 1)
      << "LoadBlocks must dereference kBlocksPointer1 as a 3-byte SNES "
         "long-address operand and load block data from the pointed "
         "region — not from the operand slot itself.";
}

TEST_F(DungeonSaveTest,
       PushableBlockIndependentSelectorsSurviveEditSaveAndReopen) {
  SetupBlockRegions();
  // Exact vanilla room 0xCA sample: $56B2 decodes to (25,45), draws on
  // upper/BG1 (bit 13 clear), and uses lower-layer pit behavior (bit 14 set).
  rom_->mutable_data()[kBlocksRegion1Pc + 0] = 0xCA;
  rom_->mutable_data()[kBlocksRegion1Pc + 1] = 0x00;
  rom_->mutable_data()[kBlocksRegion1Pc + 2] = 0xB2;
  rom_->mutable_data()[kBlocksRegion1Pc + 3] = 0x56;

  Room room(/*room_id=*/0xCA, rom_.get());
  room.LoadBlocks();
  ASSERT_EQ(room.GetTileObjects().size(), 1u);
  auto& block = room.GetTileObjects()[0];
  EXPECT_EQ(block.x(), 25);
  EXPECT_EQ(block.y(), 45);
  EXPECT_EQ(block.GetLayerValue(), 0);
  EXPECT_EQ(block.block_behavior_layer(), 1);

  // Exercise a normal editor mutation: move right and change only the draw
  // target. The behavior bit must stay set independently.
  block.set_x(26);
  block.layer_ = RoomObject::LayerType::BG2;
  room.MarkBlocksDirty();
  ASSERT_TRUE(SaveAllBlocks(rom_.get(), kNumberOfRooms,
                            [&room](int room_id) -> const Room* {
                              return room_id == room.id() ? &room : nullptr;
                            })
                  .ok());
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 2], 0xB4);
  EXPECT_EQ(rom_->data()[kBlocksRegion1Pc + 3], 0x76);

  Room reopened(/*room_id=*/0xCA, rom_.get());
  reopened.LoadBlocks();
  ASSERT_EQ(reopened.GetTileObjects().size(), 1u);
  const auto& reopened_block = reopened.GetTileObjects()[0];
  EXPECT_EQ(reopened_block.x(), 26);
  EXPECT_EQ(reopened_block.y(), 45);
  EXPECT_EQ(reopened_block.GetLayerValue(), 1);
  EXPECT_EQ(reopened_block.block_behavior_layer(), 1);
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
