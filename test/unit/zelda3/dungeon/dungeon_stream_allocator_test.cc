#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::zelda3::test {
namespace {

class DungeonStreamAllocatorTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kRomSize = 0x100000;
  static constexpr uint32_t kPotData = 0x00E000;
  static constexpr uint32_t kObjectTable = 0x020000;
  static constexpr uint32_t kObjectData = 0x022000;
  static constexpr uint32_t kSpriteTable = 0x048000;
  static constexpr uint32_t kSpriteData = 0x049000;

  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(kRomSize, 0)).ok());
  }

  DungeonStreamLayout PotLayout(
      uint32_t count, std::vector<DungeonStreamPcRange> data_ranges,
      std::vector<DungeonStreamPcRange> allocation_ranges) {
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kPotItem;
    layout.pointer_table_pc = kRoomItemsPointers;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x01;
    layout.data_ranges = std::move(data_ranges);
    layout.allocation_ranges = std::move(allocation_ranges);
    return layout;
  }

  DungeonStreamLayout ObjectLayout(uint32_t count,
                                   DungeonStreamPcRange data_range) {
    WriteLong(kRoomObjectPointer, PcToSnes(kObjectTable));
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kObject;
    layout.pointer_table_pc = kObjectTable;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kLong24;
    layout.data_ranges = {data_range};
    return layout;
  }

  DungeonStreamLayout SpriteLayout(uint32_t count,
                                   DungeonStreamPcRange data_range) {
    const uint32_t table_snes = PcToSnes(kSpriteTable);
    WriteWord(kRoomsSpritePointer, table_snes & 0xFFFF);
    DungeonStreamLayout layout;
    layout.kind = DungeonStreamKind::kSprite;
    layout.pointer_table_pc = kSpriteTable;
    layout.pointer_count = count;
    layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
    layout.pointer_bank = 0x09;
    layout.data_ranges = {data_range};
    return layout;
  }

  void WriteBytes(uint32_t address, const std::vector<uint8_t>& bytes) {
    ASSERT_LE(address + bytes.size(), rom_->size());
    std::copy(bytes.begin(), bytes.end(), rom_->mutable_data() + address);
  }

  void WriteWord(uint32_t address, uint16_t value) {
    rom_->mutable_data()[address] = value & 0xFF;
    rom_->mutable_data()[address + 1] = (value >> 8) & 0xFF;
  }

  void WriteLong(uint32_t address, uint32_t value) {
    rom_->mutable_data()[address] = value & 0xFF;
    rom_->mutable_data()[address + 1] = (value >> 8) & 0xFF;
    rom_->mutable_data()[address + 2] = (value >> 16) & 0xFF;
  }

  void SetPointer(const DungeonStreamLayout& layout, uint32_t room_id,
                  uint32_t data_pc) {
    const uint32_t snes = PcToSnes(data_pc);
    const uint32_t width =
        layout.pointer_encoding == DungeonPointerEncoding::kLong24 ? 3 : 2;
    const uint32_t slot = layout.pointer_table_pc + room_id * width;
    if (width == 3) {
      WriteLong(slot, snes);
    } else {
      ASSERT_EQ(SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) |
                         (snes & 0xFFFF)),
                data_pc);
      WriteWord(slot, snes & 0xFFFF);
    }
  }

  void SetPointersFrom(const DungeonStreamLayout& layout, uint32_t first_room,
                       uint32_t data_pc) {
    for (uint32_t room_id = first_room; room_id < layout.pointer_count;
         ++room_id) {
      SetPointer(layout, room_id, data_pc);
    }
  }

  uint32_t ReadFixedPointer(const DungeonStreamLayout& layout,
                            uint32_t room_id) const {
    const uint32_t slot = layout.pointer_table_pc + room_id * 2;
    const uint32_t word = rom_->data()[slot] | (rom_->data()[slot + 1] << 8);
    return SnesToPc((static_cast<uint32_t>(layout.pointer_bank) << 16) | word);
  }

  uint32_t ReadLongPointer(uint32_t slot) const {
    const uint32_t snes = rom_->data()[slot] | (rom_->data()[slot + 1] << 8) |
                          (rom_->data()[slot + 2] << 16);
    return SnesToPc(snes);
  }

  std::unique_ptr<Rom> rom_;
};

TEST_F(DungeonStreamAllocatorTest, ReadsSingleObjectStreamThroughLiveTable) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  const std::vector<uint8_t> encoded{0,    0,    0xFF, 0xFF, 0xFF, 0xFF,
                                     0xF0, 0xFF, 0x63, 0,    0xFF, 0xFF};
  SetPointersFrom(layout, 0, kObjectData);
  SetPointer(layout, 3, kObjectData + 0x40);
  WriteBytes(kObjectData + 0x40, encoded);
  const auto before = rom_->vector();
  const auto dirty = rom_->dirty();
  const auto record = ReadDungeonObjectStream(*rom_, 3);
  ASSERT_TRUE(record.ok()) << record.status();
  EXPECT_TRUE(record->valid);
  EXPECT_EQ(record->room_id, 3);
  EXPECT_EQ(record->pointer_slot_pc, kObjectTable + 9);
  EXPECT_EQ(record->data_pc, kObjectData + 0x40);
  EXPECT_EQ(record->logical_end_pc, kObjectData + 0x40 + encoded.size());
  EXPECT_EQ(record->encoded_stream, encoded);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), dirty);
}

TEST_F(DungeonStreamAllocatorTest, SingleObjectReadAllowsExactSharedPointers) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  SetPointersFrom(layout, 0, kObjectData);
  WriteBytes(kObjectData, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  const auto first = ReadDungeonObjectStream(*rom_, 0);
  const auto last = ReadDungeonObjectStream(*rom_, kNumberOfRooms - 1);
  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(last.ok()) << last.status();
  EXPECT_EQ(first->encoded_stream, last->encoded_stream);
}

TEST_F(DungeonStreamAllocatorTest, SingleObjectReadStopsAtNextRoomPointer) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  SetPointersFrom(layout, 0, kObjectData);
  SetPointer(layout, 1, kObjectData + 8);
  // First room omits its third list terminator. Without a strict bound the
  // parser could consume bytes from the next complete room as that terminator.
  WriteBytes(kObjectData, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0xFF, 0xFF,
                           0xFF, 0xFF, 0xFF, 0xFF});
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
  EXPECT_TRUE(ReadDungeonObjectStream(*rom_, 1).ok());
}

TEST_F(DungeonStreamAllocatorTest, SingleObjectReadStopsAtBankEnd) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  SetPointersFrom(layout, 0, 0x27FFA);
  WriteBytes(0x27FFA, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
}

TEST_F(DungeonStreamAllocatorTest, SingleObjectReadStopsAtKnownRegionEnd) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  constexpr uint32_t start = 0x053730 - 6;
  SetPointersFrom(layout, 0, start);
  WriteBytes(start, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
}

TEST_F(DungeonStreamAllocatorTest, SingleObjectReadRejectsInvalidSourceTables) {
  WriteLong(kRoomObjectPointer, 0x008000 | 0x7E0000);
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
  WriteLong(kRoomObjectPointer, 0x007FFF);
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
  WriteLong(kRoomObjectPointer, PcToSnes(kRomSize - 3));
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, -1).ok());
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, kNumberOfRooms).ok());
}

TEST_F(DungeonStreamAllocatorTest,
       SingleObjectReadRejectsInvalidStreamPointers) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  SetPointersFrom(layout, 0, kObjectData);
  for (uint32_t pointer :
       {0x007FFFu, 0x7E8000u, 0x7F8000u, 0xFE8000u, 0xFF8000u,
        PcToSnes(kRomSize), PcToSnes(kObjectTable), PcToSnes(kDoorPointers)}) {
    WriteLong(kObjectTable + 3, pointer);
    EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 1).ok()) << pointer;
  }
}

TEST_F(DungeonStreamAllocatorTest,
       SingleObjectReadRejectsMissingDoorTerminator) {
  const auto layout =
      ObjectLayout(kNumberOfRooms, {kObjectData, kObjectData + 0x100});
  SetPointersFrom(layout, 0, kObjectData);
  WriteBytes(kObjectData, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0x63, 0});
  const auto before = rom_->vector();
  EXPECT_FALSE(ReadDungeonObjectStream(*rom_, 0).ok());
  EXPECT_EQ(rom_->vector(), before);
}

TEST_F(DungeonStreamAllocatorTest, PotReadAcceptsSharedEmptyListTerminator) {
  const auto layout = PotLayout(kNumberOfRooms, {}, {});
  SetPointersFrom(layout, 0, kPotData + 6);
  SetPointer(layout, 4, kPotData);
  const std::vector<uint8_t> encoded{0xCC, 0x13, 0x0A, 0x60,
                                     0x26, 0x0B, 0xFF, 0xFF};
  WriteBytes(kPotData, encoded);
  const auto before = rom_->vector();
  const auto record = ReadDungeonPotItemStream(*rom_, 4);
  ASSERT_TRUE(record.ok()) << record.status();
  EXPECT_EQ(record->encoded_stream, encoded);
  EXPECT_EQ(record->logical_end_pc, kPotData + 8);
  const auto empty = ReadDungeonPotItemStream(*rom_, 5);
  ASSERT_TRUE(empty.ok()) << empty.status();
  EXPECT_EQ(empty->encoded_stream, (std::vector<uint8_t>{0xFF, 0xFF}));
  EXPECT_EQ(rom_->vector(), before);
}

TEST_F(DungeonStreamAllocatorTest, PotReadDoesNotConsumeNextRoomsRecords) {
  const auto layout = PotLayout(kNumberOfRooms, {}, {});
  SetPointersFrom(layout, 0, kPotData + 3);
  SetPointer(layout, 0, kPotData);
  WriteBytes(kPotData, {0xCC, 0x13, 0x0A, 0x60, 0x26, 0x0B, 0xFF, 0xFF});
  EXPECT_FALSE(ReadDungeonPotItemStream(*rom_, 0).ok());
  EXPECT_TRUE(ReadDungeonPotItemStream(*rom_, 1).ok());
}

TEST_F(DungeonStreamAllocatorTest,
       PotSharedTerminatorCannotFinishPartialRecord) {
  const auto layout = PotLayout(kNumberOfRooms, {}, {});
  SetPointersFrom(layout, 0, kPotData + 2);
  SetPointer(layout, 0, kPotData);
  WriteBytes(kPotData, {0xCC, 0x13, 0xFF, 0xFF});
  EXPECT_FALSE(ReadDungeonPotItemStream(*rom_, 0).ok());
  EXPECT_TRUE(ReadDungeonPotItemStream(*rom_, 1).ok());
}

TEST_F(DungeonStreamAllocatorTest, PotSharedTerminatorCannotCrossStorageEnd) {
  const auto layout = PotLayout(kNumberOfRooms, {}, {});
  for (const uint32_t end : {uint32_t(kRoomItemsDataEnd), 0x10000u}) {
    // The bank-end pointer cannot be encoded in the same bank, so room 1
    // points to the final byte instead, leaving a truncated shared terminator.
    const uint32_t next = end == 0x10000u ? end - 1 : end;
    SetPointersFrom(layout, 0, next);
    SetPointer(layout, 0, next - 3);
    WriteBytes(next - 3, {0xCC, 0x13, 0x0A, 0xFF, 0xFF});
    EXPECT_FALSE(ReadDungeonPotItemStream(*rom_, 0).ok());
  }
}

class DungeonFixedStreamReadTest
    : public DungeonStreamAllocatorTest,
      public ::testing::WithParamInterface<DungeonStreamKind> {
 protected:
  void SetUp() override {
    DungeonStreamAllocatorTest::SetUp();
    if (GetParam() == DungeonStreamKind::kSprite) {
      layout_ =
          SpriteLayout(kNumberOfRooms, {kSpriteData, kSpriteData + 0x100});
      start_ = kSpriteData;
      encoded_ = {7, 0x11, 0x22, 0x33, 0xFF};
    } else {
      layout_ = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x100}}, {});
      start_ = kPotData;
      encoded_ = {0x11, 0x22, 0x33, 0xFF, 0xFF};
    }
    SetPointersFrom(layout_, 0, start_);
    WriteBytes(start_, encoded_);
  }

  absl::StatusOr<DungeonStreamRecord> Read(int room = 0) const {
    return GetParam() == DungeonStreamKind::kSprite
               ? ReadDungeonSpriteStream(*rom_, room)
               : ReadDungeonPotItemStream(*rom_, room);
  }

  DungeonStreamLayout layout_;
  uint32_t start_ = 0;
  std::vector<uint8_t> encoded_;
};

TEST_P(DungeonFixedStreamReadTest, ReadsExactBytesWithoutMutation) {
  SetPointer(layout_, 3, start_ + 0x40);
  WriteBytes(start_ + 0x40, encoded_);
  const auto before = rom_->vector();
  const auto dirty = rom_->dirty();
  const auto record = Read(3);
  ASSERT_TRUE(record.ok()) << record.status();
  EXPECT_TRUE(record->valid);
  EXPECT_EQ(record->room_id, 3);
  EXPECT_EQ(record->pointer_slot_pc, layout_.pointer_table_pc + 6);
  EXPECT_EQ(record->data_pc, start_ + 0x40);
  EXPECT_EQ(record->logical_end_pc, start_ + 0x40 + encoded_.size());
  EXPECT_EQ(record->encoded_stream, encoded_);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), dirty);
}

TEST_P(DungeonFixedStreamReadTest, AllowsExactSharedPointers) {
  const auto first = Read();
  const auto last = Read(kNumberOfRooms - 1);
  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(last.ok()) << last.status();
  EXPECT_EQ(first->encoded_stream, last->encoded_stream);
}

TEST_P(DungeonFixedStreamReadTest, RejectsInvalidRoomAndPointer) {
  EXPECT_FALSE(Read(-1).ok());
  EXPECT_FALSE(Read(kNumberOfRooms).ok());
  WriteWord(layout_.pointer_table_pc, 0x7FFF);
  EXPECT_FALSE(Read().ok());
}

TEST_P(DungeonFixedStreamReadTest, RejectsMissingTerminatorWithoutMutation) {
  WriteBytes(start_, std::vector<uint8_t>(encoded_.size(), 0));
  const auto before = rom_->vector();
  const auto dirty = rom_->dirty();
  EXPECT_FALSE(Read().ok());
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), dirty);
}

TEST_P(DungeonFixedStreamReadTest, StopsAtNextRoomPointer) {
  // The first stream lacks its terminator. The next room is well formed,
  // but its terminator cannot supply the missing end of the first stream.
  WriteBytes(start_, {0, 0, 0, 0});
  SetPointer(layout_, 1, start_ + 4);
  WriteBytes(start_ + 4, encoded_);
  EXPECT_FALSE(Read().ok());
  EXPECT_TRUE(Read(1).ok());
}

TEST_P(DungeonFixedStreamReadTest, StopsAtKnownRegionEnd) {
  const uint32_t region_end = GetParam() == DungeonStreamKind::kSprite
                                  ? kSpritesDataEndExclusive
                                  : kRoomItemsDataEnd;
  const uint32_t stream = region_end - encoded_.size() + 1;
  SetPointersFrom(layout_, 0, stream);
  WriteBytes(stream, encoded_);
  EXPECT_FALSE(Read().ok());
}

TEST_P(DungeonFixedStreamReadTest, StopsAtBankEnd) {
  const uint32_t bank_end =
      GetParam() == DungeonStreamKind::kSprite ? 0x050000 : 0x010000;
  SetPointersFrom(layout_, 0, bank_end - encoded_.size() + 1);
  WriteBytes(bank_end - encoded_.size() + 1, encoded_);
  EXPECT_FALSE(Read().ok());
}

TEST_P(DungeonFixedStreamReadTest, RejectsPointerInsideTable) {
  SetPointer(layout_, 0, layout_.pointer_table_pc);
  EXPECT_FALSE(Read().ok());
}

TEST_P(DungeonFixedStreamReadTest, StopsBeforePointerTable) {
  // Read room 1 so writing the deliberately crossing payload cannot change
  // this room's own pointer slot.
  if (GetParam() == DungeonStreamKind::kSprite) {
    // The normal fixture table is at its bank start; move this live table to
    // leave room for a stream immediately before it.
    layout_.pointer_table_pc += 0x100;
    WriteWord(kRoomsSpritePointer, PcToSnes(layout_.pointer_table_pc) & 0xFFFF);
  }
  const uint32_t before_table = layout_.pointer_table_pc - encoded_.size() + 1;
  SetPointersFrom(layout_, 0, before_table);
  WriteBytes(before_table, encoded_);
  EXPECT_FALSE(Read(1).ok());
}

TEST_P(DungeonFixedStreamReadTest, RejectsTruncatedPointerTable) {
  // Sprite pointer indirection must still be present to isolate table bounds.
  if (GetParam() == DungeonStreamKind::kSprite) {
    layout_.pointer_table_pc = kRoomsSpritePointer + 0x10;
    WriteWord(kRoomsSpritePointer, PcToSnes(layout_.pointer_table_pc) & 0xFFFF);
  }
  auto truncated = rom_->vector();
  truncated.resize(layout_.pointer_table_pc + kNumberOfRooms * 2 - 1);
  ASSERT_TRUE(rom_->LoadFromData(truncated).ok());
  EXPECT_FALSE(Read().ok());
}

TEST_P(DungeonFixedStreamReadTest, RejectsStreamTruncatedAtRomEnd) {
  // Sprite data must follow the live indirection source in this short ROM.
  const uint32_t stream =
      GetParam() == DungeonStreamKind::kSprite ? kSpritesData : start_;
  SetPointersFrom(layout_, 0, stream);
  WriteBytes(stream, encoded_);
  auto truncated = rom_->vector();
  truncated.resize(stream + encoded_.size() - 1);
  ASSERT_TRUE(rom_->LoadFromData(truncated).ok());
  EXPECT_FALSE(Read().ok());
}

INSTANTIATE_TEST_SUITE_P(SpriteAndPotItems, DungeonFixedStreamReadTest,
                         ::testing::Values(DungeonStreamKind::kSprite,
                                           DungeonStreamKind::kPotItem));

TEST_F(DungeonStreamAllocatorTest, SingleSpriteReadRejectsInvalidLiveTable) {
  WriteWord(kRoomsSpritePointer, 0x7FFF);
  EXPECT_FALSE(ReadDungeonSpriteStream(*rom_, 0).ok());
  WriteWord(kRoomsSpritePointer, 0xFFFF);
  EXPECT_FALSE(ReadDungeonSpriteStream(*rom_, 0).ok());
  WriteWord(kRoomsSpritePointer, PcToSnes(kRoomsSpritePointer) & 0xFFFF);
  EXPECT_FALSE(ReadDungeonSpriteStream(*rom_, 0).ok());
}

TEST_F(DungeonStreamAllocatorTest, SingleSpriteReadRejectsLiveSourceOverlap) {
  const auto layout =
      SpriteLayout(kNumberOfRooms, {kSpriteData, kSpriteData + 8});
  SetPointersFrom(layout, 0, kRoomsSpritePointer);
  EXPECT_FALSE(ReadDungeonSpriteStream(*rom_, 0).ok());
  SetPointersFrom(layout, 0, kRoomsSpritePointer - 1);
  EXPECT_FALSE(ReadDungeonSpriteStream(*rom_, 0).ok());
}

TEST_F(DungeonStreamAllocatorTest, InventoriesExactAliasesAndSuffixOverlap) {
  auto layout = PotLayout(3, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  // Two 3-byte entries followed by the 0xFFFF terminator. A pointer to the
  // second entry is a valid suffix stream.
  WriteBytes(kPotData, {0x01, 0x00, 0x10, 0x02, 0x00, 0x20, 0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 3);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->aliases.size(), 1);
  EXPECT_EQ(inventory->aliases[0].data_pc, kPotData);
  EXPECT_EQ(inventory->aliases[0].room_ids, (std::vector<uint32_t>{0, 1}));
  ASSERT_EQ(inventory->overlaps.size(), 1);
  EXPECT_EQ(inventory->overlaps[0].kind, DungeonStreamOverlapKind::kSuffix);
  EXPECT_EQ(inventory->overlaps[0].first_room_ids,
            (std::vector<uint32_t>{0, 1}));
  EXPECT_EQ(inventory->overlaps[0].second_room_ids, (std::vector<uint32_t>{2}));
  EXPECT_EQ(inventory->occupied_intervals,
            (std::vector<DungeonStreamPcRange>{{kPotData, kPotData + 8}}));
  EXPECT_EQ(
      inventory->allocatable_free_intervals,
      (std::vector<DungeonStreamPcRange>{{kPotData + 0x20, kPotData + 0x40}}));
}

TEST_F(DungeonStreamAllocatorTest, InventoriesInteriorOverlap) {
  auto layout = SpriteLayout(2, {kSpriteData, kSpriteData + 0x20});
  // Room 0: sort, one record, terminator => [data,data+5).
  // Room 1 starts at byte 2; byte 3 is a terminator at its record boundary,
  // yielding [data+2,data+4), an interior (not suffix) overlap.
  WriteBytes(kSpriteData, {0x00, 0x11, 0x22, 0xFF, 0xFF});
  SetPointer(layout, 0, kSpriteData);
  SetPointer(layout, 1, kSpriteData + 2);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->overlaps.size(), 1);
  EXPECT_EQ(inventory->overlaps[0].kind, DungeonStreamOverlapKind::kInterior);
  EXPECT_EQ(inventory->overlaps[0].intersection,
            (DungeonStreamPcRange{kSpriteData + 2, kSpriteData + 4}));
}

TEST_F(DungeonStreamAllocatorTest, ReportsMalformedObjectTerminator) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 9});
  // Valid header and first two list terminators, then a door marker with only
  // one byte left instead of the final 0xFFFF terminator.
  WriteBytes(kObjectData,
             {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF});
  SetPointer(layout, 0, kObjectData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_FALSE(inventory->ok());
  ASSERT_EQ(inventory->issues.size(), 1);
  EXPECT_EQ(inventory->issues[0].code,
            DungeonStreamIssueCode::kMalformedStream);
  EXPECT_EQ(inventory->issues[0].room_id, 0);
}

TEST_F(DungeonStreamAllocatorTest, RejectsPcRangesThatMapThroughWramBanks) {
  ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(0x400000, 0)).ok());
  auto layout = ObjectLayout(1, {0x3F0000, 0x3F0020});
  SetPointer(layout, 0, 0x3F0000);

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(inventory.status().message()).find("WRAM"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest, RejectsDirectAndMirroredWramPointerBanks) {
  const auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x20});

  for (const uint8_t bank : {0x7E, 0x7F, 0xFE, 0xFF}) {
    SCOPED_TRACE(::testing::Message()
                 << "bank=0x" << std::hex << static_cast<int>(bank));
    WriteLong(kObjectTable, (static_cast<uint32_t>(bank) << 16) | 0x8000);

    const auto inventory = InventoryDungeonStreams(*rom_, layout);

    ASSERT_TRUE(inventory.ok()) << inventory.status();
    ASSERT_FALSE(inventory->ok());
    ASSERT_EQ(inventory->issues.size(), 1u);
    EXPECT_EQ(inventory->issues[0].code,
              DungeonStreamIssueCode::kInvalidPointer);
    EXPECT_NE(inventory->issues[0].message.find("WRAM"), std::string::npos)
        << inventory->issues[0].message;
  }
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsPointerTableOverlapWithLivePointerSource) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x20});
  layout.pointer_table_pc = kRoomObjectPointer;

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("live pointer-table source"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsDataRangeOverlapWithLivePointerSource) {
  const auto layout =
      ObjectLayout(1, {kRoomObjectPointer, kRoomObjectPointer + 0x20});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("live pointer-table source"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsPointerTableOverlapWithObjectDoorMetadata) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x20});
  layout.pointer_table_pc = kDoorPointers;

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("door-pointer table"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsDataRangeOverlapWithObjectDoorMetadata) {
  const auto layout = ObjectLayout(1, {kDoorPointers, kDoorPointers + 0x20});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("door-pointer table"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest, RejectsPointerTableAndDataRangeOverlap) {
  const auto layout = ObjectLayout(1, {kObjectTable, kObjectTable + 0x20});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("overlap the pointer table"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsRomWithoutCompleteObjectDoorPointerTable) {
  constexpr uint32_t kCompleteDoorTableEnd = kDoorPointers + kNumberOfRooms * 3;
  ASSERT_TRUE(
      rom_->LoadFromData(std::vector<uint8_t>(kCompleteDoorTableEnd - 1, 0))
          .ok());
  const auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x20});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kOutOfRange);
  EXPECT_NE(
      inventory.status().message().find("Complete object door-pointer table"),
      std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       AllowsExactAdjacencyAroundLivePointerSource) {
  constexpr uint32_t kTable = kRoomObjectPointer - 3;
  constexpr uint32_t kData = kRoomObjectPointer + 3;
  DungeonStreamLayout layout;
  layout.kind = DungeonStreamKind::kObject;
  layout.pointer_table_pc = kTable;
  layout.pointer_count = 1;
  layout.pointer_encoding = DungeonPointerEncoding::kLong24;
  layout.data_ranges = {{kData, kData + 8}};
  WriteLong(kRoomObjectPointer, PcToSnes(kTable));
  WriteLong(kTable, PcToSnes(kData));
  WriteBytes(kData, {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_TRUE(inventory->ok());
}

TEST_F(DungeonStreamAllocatorTest,
       AllowsExactAdjacencyAroundObjectDoorMetadata) {
  constexpr uint32_t kCompleteDoorTableEnd = kDoorPointers + kNumberOfRooms * 3;
  constexpr uint32_t kTable = kDoorPointers - 3;
  constexpr uint32_t kData = kCompleteDoorTableEnd;
  DungeonStreamLayout layout;
  layout.kind = DungeonStreamKind::kObject;
  layout.pointer_table_pc = kTable;
  layout.pointer_count = 1;
  layout.pointer_encoding = DungeonPointerEncoding::kLong24;
  layout.data_ranges = {{kData, kData + 8}};
  WriteLong(kRoomObjectPointer, PcToSnes(kTable));
  WriteLong(kTable, PcToSnes(kData));
  WriteBytes(kData, {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_TRUE(inventory->ok());
}

TEST_F(DungeonStreamAllocatorTest,
       RejectsFixedBankPointerTableCrossingRuntimeCpuBank) {
  auto layout = SpriteLayout(296, {kSpriteData, kSpriteData + 0x20});
  layout.pointer_table_pc = SnesToPc(0x09FF00);
  WriteWord(kRoomsSpritePointer, 0xFF00);

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  EXPECT_EQ(inventory.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(inventory.status().message().find("runtime CPU bank"),
            std::string::npos);
}

TEST_F(DungeonStreamAllocatorTest,
       AllowsFixedBankPointerTableEndingExactlyAtRuntimeBankEnd) {
  auto layout = SpriteLayout(296, {kSpriteData, kSpriteData + 0x20});
  layout.pointer_table_pc = SnesToPc(0x09FDB0);
  WriteWord(kRoomsSpritePointer, 0xFDB0);
  WriteBytes(kSpriteData, {0x00, 0xFF});
  for (uint32_t room_id = 0; room_id < layout.pointer_count; ++room_id) {
    SetPointer(layout, room_id, kSpriteData);
  }

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_TRUE(inventory->ok());
  EXPECT_EQ(inventory->streams.size(), 296u);
}

TEST_F(DungeonStreamAllocatorTest,
       AcceptsOracleSpritePointerTableAndAdjacentDataLayout) {
  constexpr uint32_t kOraclePointerTableSnes = 0x09D2B2;
  constexpr uint32_t kOracleDataStartSnes = 0x09D502;
  constexpr uint32_t kOracleDataEndSnes = 0x09EC9F;
  DungeonStreamLayout layout;
  layout.kind = DungeonStreamKind::kSprite;
  layout.pointer_table_pc = SnesToPc(kOraclePointerTableSnes);
  layout.pointer_count = 296;
  layout.pointer_encoding = DungeonPointerEncoding::kFixedBank16;
  layout.pointer_bank = 0x09;
  layout.data_ranges = {
      {SnesToPc(kOracleDataStartSnes), SnesToPc(kOracleDataEndSnes)}};
  layout.allocation_ranges = layout.data_ranges;
  WriteWord(kRoomsSpritePointer, kOraclePointerTableSnes & 0xFFFFu);
  WriteBytes(SnesToPc(kOracleDataStartSnes), {0x00, 0xFF});
  for (uint32_t room_id = 0; room_id < layout.pointer_count; ++room_id) {
    SetPointer(layout, room_id, SnesToPc(kOracleDataStartSnes));
  }

  const auto inventory = InventoryDungeonStreams(*rom_, layout);

  ASSERT_TRUE(inventory.ok()) << inventory.status();
  EXPECT_TRUE(inventory->ok());
  EXPECT_EQ(inventory->streams.size(), 296u);
  EXPECT_EQ(layout.pointer_table_pc + layout.pointer_count * 2,
            layout.data_ranges.front().begin);
}

TEST_F(DungeonStreamAllocatorTest,
       AcceptsCanonicalDoorlessObjectAndRepointsDoorTerminator) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x200});
  layout.allocation_ranges = {{kObjectData + 0x100, kObjectData + 0x120}};
  const std::vector<uint8_t> canonical_empty = {
      0x00, 0x00,  // floor/layout header
      0xFF, 0xFF,  // list 0
      0xFF, 0xFF,  // list 1
      0xFF, 0xFF,  // doorless list 2 / final terminator
  };
  WriteBytes(kObjectData, canonical_empty);
  SetPointer(layout, 0, kObjectData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());
  ASSERT_EQ(inventory->streams.size(), 1);
  EXPECT_EQ(inventory->streams[0].size(), canonical_empty.size());

  auto plan = PlanDungeonStreamWrites(*inventory, {{0, canonical_empty}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 1);
  ASSERT_EQ(plan->auxiliary_pointer_writes.size(), 1);
  const uint32_t relocated_pc = kObjectData + 0x100;
  EXPECT_EQ(plan->payload_writes[0].address, relocated_pc);
  EXPECT_EQ(plan->auxiliary_pointer_writes[0].address, kDoorPointers);

  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadLongPointer(kObjectTable), relocated_pc);
  // With no F0 FF marker, the door pointer targets the third/final 0xFFFF.
  EXPECT_EQ(ReadLongPointer(kDoorPointers), relocated_pc + 6);
}

TEST_F(DungeonStreamAllocatorTest, ObjectDoorPointerTargetsFirstDoorByte) {
  auto layout = ObjectLayout(1, {kObjectData, kObjectData + 0x200});
  layout.allocation_ranges = {{kObjectData + 0x100, kObjectData + 0x140}};
  WriteBytes(kObjectData, {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
  SetPointer(layout, 0, kObjectData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  // Header, two empty lists, F0 FF marker, one door, final terminator.
  const std::vector<uint8_t> with_door = {
      0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0x12, 0x34, 0xFF, 0xFF,
  };
  auto plan = PlanDungeonStreamWrites(*inventory, {{0, with_door}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->auxiliary_pointer_writes.size(), 1);
  const auto& door_write = plan->auxiliary_pointer_writes[0];
  const uint32_t door_snes = door_write.bytes[0] | (door_write.bytes[1] << 8) |
                             (door_write.bytes[2] << 16);
  EXPECT_EQ(SnesToPc(door_snes), plan->payload_writes[0].address + 8);
}

TEST_F(DungeonStreamAllocatorTest,
       PlansDeterministicFirstFitAndAppliesPayloadsAndPointers) {
  auto layout = PotLayout(
      3, {{kPotData, kPotData + 0x80}},
      {{kPotData + 0x20, kPotData + 0x28}, {kPotData + 0x40, kPotData + 0x50}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  for (uint32_t room = 0; room < 3; ++room) {
    SetPointer(layout, room, kPotData);
  }
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  // Input order is deliberately reversed. Planning order is room ID.
  auto plan = PlanDungeonStreamWrites(
      *inventory, {{2, {0x01, 0x00, 0x33, 0xFF, 0xFF}}, {0, {0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 2);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  EXPECT_EQ(plan->payload_writes[0].address, kPotData + 0x20);
  EXPECT_EQ(plan->payload_writes[1].room_id, 2);
  EXPECT_EQ(plan->payload_writes[1].address, kPotData + 0x22);

  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadFixedPointer(layout, 0), kPotData + 0x20);
  EXPECT_EQ(ReadFixedPointer(layout, 1), kPotData);
  EXPECT_EQ(ReadFixedPointer(layout, 2), kPotData + 0x22);
  EXPECT_EQ(rom_->data()[kPotData + 0x22], 0x01);
  EXPECT_EQ(rom_->data()[kPotData + 0x26], 0xFF);
}

TEST_F(DungeonStreamAllocatorTest, NoFitLeavesRomUnchanged) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x30}},
                          {{kPotData + 0x20, kPotData + 0x24}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  const auto before = rom_->vector();

  auto plan = PlanDungeonStreamWrites(*inventory,
                                      {{0, {0x01, 0x00, 0x44, 0xFF, 0xFF}}});
  EXPECT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_->vector(), before);
}

TEST_F(DungeonStreamAllocatorTest, RejectsStalePlanWithoutMutation) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamWrites(*inventory, {{0, {0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();

  ASSERT_TRUE(rom_->WriteByte(0x10, 0x7A).ok());
  const auto before_apply = rom_->vector();
  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kAborted);
  EXPECT_EQ(rom_->vector(), before_apply);
}

TEST_F(DungeonStreamAllocatorTest,
       FixedBankPlannerRejectsFreeSpaceInAnotherBank) {
  auto layout =
      PotLayout(1, {{kPotData, kPotData + 0x10}, {0x010000, 0x010020}},
                {{0x010000, 0x010020}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  auto plan = PlanDungeonStreamWrites(*inventory, {{0, {0xFF, 0xFF}}});
  EXPECT_FALSE(plan.ok());
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonStreamAllocatorTest, ApplyRollsBackPayloadWhenPointerWriteFails) {
  auto layout = PotLayout(1, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointer(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamWrites(*inventory,
                                      {{0, {0x01, 0x00, 0x55, 0xFF, 0xFF}}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto before = rom_->vector();
  const bool was_dirty = rom_->dirty();

  rom::WriteFence payload_only;
  ASSERT_TRUE(payload_only
                  .Allow(plan->payload_writes[0].address,
                         plan->payload_writes[0].address +
                             plan->payload_writes[0].bytes.size(),
                         "test payload")
                  .ok());
  rom::ScopedWriteFence fence_scope(rom_.get(), &payload_only);
  auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), was_dirty);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackPlanIsDeterministicAndCanonicalizesByLowestRoom) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x80}},
                          {{kPotData + 0x20, kPotData + 0x50}});
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item_a = {0x01, 0x00, 0x11, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x02, 0x00, 0x22, 0xFF, 0xFF};
  const std::vector<uint8_t> item_c = {0x03, 0x00, 0x33, 0xFF, 0xFF};
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item_a);
  WriteBytes(kPotData + 0x10, item_b);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 0x08);
  SetPointer(layout, 3, kPotData + 0x10);
  SetPointersFrom(layout, 4, kPotData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_TRUE(inventory->ok());

  auto reversed =
      PlanDungeonStreamRepack(*inventory, {{3, item_c}, {1, item_a}});
  auto ordered =
      PlanDungeonStreamRepack(*inventory, {{1, item_a}, {3, item_c}});
  ASSERT_TRUE(reversed.ok()) << reversed.status();
  ASSERT_TRUE(ordered.ok()) << ordered.status();
  EXPECT_EQ(reversed->mode, DungeonStreamWriteMode::kRepackAll);
  EXPECT_EQ(reversed->payload_writes, ordered->payload_writes);
  EXPECT_EQ(reversed->pointer_writes, ordered->pointer_writes);
  ASSERT_EQ(reversed->payload_writes.size(), 3);
  EXPECT_EQ(reversed->payload_writes[0].room_id, 0);
  EXPECT_EQ(reversed->payload_writes[1].room_id, 1);
  EXPECT_EQ(reversed->payload_writes[2].room_id, 3);
  EXPECT_EQ(reversed->payload_writes[0].address, kPotData + 0x20);
  EXPECT_EQ(reversed->payload_writes[1].address, kPotData + 0x22);
  EXPECT_EQ(reversed->payload_writes[2].address, kPotData + 0x27);
  ASSERT_EQ(reversed->pointer_writes.size(), kNumberOfRooms);
  EXPECT_EQ(reversed->pointer_writes[1].bytes,
            reversed->pointer_writes[2].bytes);
}

TEST_F(DungeonStreamAllocatorTest, RepackDeduplicatesSharedEmptyStream) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x60}},
                          {{kPotData + 0x20, kPotData + 0x40}});
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item = {0x01, 0x00, 0x44, 0xFF, 0xFF};
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData);
  SetPointer(layout, 2, kPotData + 0x08);
  SetPointersFrom(layout, 3, kPotData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 2);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  EXPECT_EQ(plan->payload_writes[1].room_id, 2);
  ASSERT_TRUE(ApplyDungeonStreamWritePlan(rom_.get(), *plan).ok());
  EXPECT_EQ(ReadFixedPointer(layout, 0), ReadFixedPointer(layout, 1));
  EXPECT_NE(ReadFixedPointer(layout, 0), ReadFixedPointer(layout, 2));
}

TEST_F(DungeonStreamAllocatorTest, RepackPlansAll296RoomPointers) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  for (uint32_t room_id = 0; room_id < layout.pointer_count; ++room_id) {
    SetPointer(layout, room_id, kPotData);
  }

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->payload_writes.size(), 1);
  EXPECT_EQ(plan->payload_writes[0].room_id, 0);
  ASSERT_EQ(plan->pointer_writes.size(), kNumberOfRooms);
  EXPECT_EQ(plan->pointer_writes.back().room_id, kNumberOfRooms - 1);
  EXPECT_EQ(plan->pointer_writes.front().bytes,
            plan->pointer_writes.back().bytes);
  ASSERT_TRUE(ApplyDungeonStreamWritePlan(rom_.get(), *plan).ok());

  auto after = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(after.ok()) << after.status();
  ASSERT_TRUE(after->ok());
  ASSERT_EQ(after->streams.size(), kNumberOfRooms);
  for (const DungeonStreamRecord& stream : after->streams) {
    EXPECT_EQ(stream.encoded_stream, (std::vector<uint8_t>{0xFF, 0xFF}));
  }
}

TEST_F(DungeonStreamAllocatorTest, RepackRejects295PointerLayoutAndPlan) {
  auto short_layout =
      PotLayout(kNumberOfRooms - 1, {{kPotData, kPotData + 0x40}},
                {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointersFrom(short_layout, 0, kPotData);
  auto short_inventory = InventoryDungeonStreams(*rom_, short_layout);
  ASSERT_TRUE(short_inventory.ok()) << short_inventory.status();
  auto rejected = PlanDungeonStreamRepack(*short_inventory, {});
  EXPECT_EQ(rejected.status().code(), absl::StatusCode::kInvalidArgument);

  auto full_layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                               {{kPotData + 0x20, kPotData + 0x30}});
  SetPointersFrom(full_layout, 0, kPotData);
  auto full_inventory = InventoryDungeonStreams(*rom_, full_layout);
  ASSERT_TRUE(full_inventory.ok()) << full_inventory.status();
  auto plan = PlanDungeonStreamRepack(*full_inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  plan->layout.pointer_count = kNumberOfRooms - 1;
  plan->pointer_writes.resize(kNumberOfRooms - 1);
  const auto before = rom_->vector();
  const auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_->vector(), before);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackAcceptsExactFitAndRejectsOneByteShortRange) {
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> item = {0x01, 0x00, 0x55, 0xFF, 0xFF};
  auto exact_layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                                {{kPotData + 0x20, kPotData + 0x27}});
  WriteBytes(kPotData, empty);
  WriteBytes(kPotData + 0x08, item);
  SetPointer(exact_layout, 0, kPotData);
  SetPointer(exact_layout, 1, kPotData + 0x08);
  SetPointersFrom(exact_layout, 2, kPotData);

  auto exact_inventory = InventoryDungeonStreams(*rom_, exact_layout);
  ASSERT_TRUE(exact_inventory.ok()) << exact_inventory.status();
  auto exact_plan = PlanDungeonStreamRepack(*exact_inventory, {});
  ASSERT_TRUE(exact_plan.ok()) << exact_plan.status();
  ASSERT_EQ(exact_plan->payload_writes.size(), 2);
  EXPECT_EQ(exact_plan->payload_writes[1].address +
                exact_plan->payload_writes[1].bytes.size(),
            kPotData + 0x27);

  auto short_layout = exact_layout;
  short_layout.allocation_ranges = {{kPotData + 0x20, kPotData + 0x26}};
  auto short_inventory = InventoryDungeonStreams(*rom_, short_layout);
  ASSERT_TRUE(short_inventory.ok()) << short_inventory.status();
  auto short_plan = PlanDungeonStreamRepack(*short_inventory, {});
  EXPECT_EQ(short_plan.status().code(), absl::StatusCode::kResourceExhausted);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackPreservesUntouchedStreamsAndUpdatesEveryPointer) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x80}},
                          {{kPotData + 0x20, kPotData + 0x60}});
  const std::vector<uint8_t> item_a = {0x01, 0x00, 0x10, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x02, 0x00, 0x20, 0xFF, 0xFF};
  const std::vector<uint8_t> empty = {0xFF, 0xFF};
  const std::vector<uint8_t> replacement = {0x03, 0x00, 0x30, 0xFF, 0xFF};
  WriteBytes(kPotData, item_a);
  WriteBytes(kPotData + 0x08, item_b);
  WriteBytes(kPotData + 0x10, empty);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData + 0x08);
  SetPointer(layout, 2, kPotData + 0x10);
  SetPointersFrom(layout, 3, kPotData + 0x10);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {{1, replacement}});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->pointer_writes.size(), layout.pointer_count);
  ASSERT_TRUE(ApplyDungeonStreamWritePlan(rom_.get(), *plan).ok());

  auto after = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(after.ok()) << after.status();
  ASSERT_TRUE(after->ok());
  EXPECT_EQ(after->streams[0].encoded_stream, item_a);
  EXPECT_EQ(after->streams[1].encoded_stream, replacement);
  EXPECT_EQ(after->streams[2].encoded_stream, empty);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackRejectsMultipleRangesEvenWhenPackingIsFeasible) {
  const std::vector<uint8_t> item_8 = {0x01, 0x00, 0x10, 0x02,
                                       0x00, 0x11, 0xFF, 0xFF};
  const std::vector<uint8_t> item_a = {0x03, 0x00, 0x20, 0xFF, 0xFF};
  const std::vector<uint8_t> item_b = {0x04, 0x00, 0x30, 0xFF, 0xFF};
  const std::vector<uint8_t> item_c = {0x05, 0x00, 0x40, 0xFF, 0xFF};
  auto layout = PotLayout(
      kNumberOfRooms, {{kPotData, kPotData + 0x100}},
      {{kPotData + 0x40, kPotData + 0x4A}, {kPotData + 0x50, kPotData + 0x5D}});
  WriteBytes(kPotData, item_8);
  WriteBytes(kPotData + 0x10, item_a);
  WriteBytes(kPotData + 0x18, item_b);
  WriteBytes(kPotData + 0x20, item_c);
  SetPointer(layout, 0, kPotData);
  SetPointer(layout, 1, kPotData + 0x10);
  SetPointer(layout, 2, kPotData + 0x18);
  SetPointer(layout, 3, kPotData + 0x20);
  SetPointersFrom(layout, 4, kPotData);

  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_NE(std::string(plan.status().message()).find("one normalized"),
            std::string::npos);

  auto single_range_layout = layout;
  single_range_layout.allocation_ranges = {{kPotData + 0x40, kPotData + 0x57}};
  auto single_range_inventory =
      InventoryDungeonStreams(*rom_, single_range_layout);
  ASSERT_TRUE(single_range_inventory.ok()) << single_range_inventory.status();
  auto tampered_plan = PlanDungeonStreamRepack(*single_range_inventory, {});
  ASSERT_TRUE(tampered_plan.ok()) << tampered_plan.status();
  tampered_plan->layout.allocation_ranges = layout.allocation_ranges;
  const auto before_apply = rom_->vector();
  const auto apply_status =
      ApplyDungeonStreamWritePlan(rom_.get(), *tampered_plan);
  EXPECT_EQ(apply_status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_->vector(), before_apply);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackRejectsAllocationRangeOutsidePointerBank) {
  const std::vector<uint8_t> item = {0x02, 0x00, 0x20, 0xFF, 0xFF};
  WriteBytes(kPotData, item);
  auto wrong_bank_layout = PotLayout(
      kNumberOfRooms, {{kPotData, kPotData + 0x20}, {0x010000, 0x010020}},
      {{0x010000, 0x010020}});
  SetPointersFrom(wrong_bank_layout, 0, kPotData);
  auto wrong_bank_inventory = InventoryDungeonStreams(*rom_, wrong_bank_layout);
  ASSERT_TRUE(wrong_bank_inventory.ok()) << wrong_bank_inventory.status();
  auto wrong_bank_plan = PlanDungeonStreamRepack(*wrong_bank_inventory, {});
  EXPECT_EQ(wrong_bank_plan.status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(DungeonStreamAllocatorTest, RepackApplyRejectsStaleCrcWithoutMutation) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointersFrom(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();

  ASSERT_TRUE(rom_->WriteByte(0x10, 0x7A).ok());
  const auto before_apply = rom_->vector();
  const auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kAborted);
  EXPECT_EQ(rom_->vector(), before_apply);
}

TEST_F(DungeonStreamAllocatorTest,
       RepackApplyRollsBackPayloadWhenPointerWriteFails) {
  auto layout = PotLayout(kNumberOfRooms, {{kPotData, kPotData + 0x40}},
                          {{kPotData + 0x20, kPotData + 0x30}});
  WriteBytes(kPotData, {0xFF, 0xFF});
  SetPointersFrom(layout, 0, kPotData);
  auto inventory = InventoryDungeonStreams(*rom_, layout);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  auto plan = PlanDungeonStreamRepack(*inventory, {});
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto before = rom_->vector();
  const bool was_dirty = rom_->dirty();

  rom::WriteFence payload_only;
  ASSERT_TRUE(payload_only
                  .Allow(plan->payload_writes[0].address,
                         plan->payload_writes[0].address +
                             plan->payload_writes[0].bytes.size(),
                         "test repack payload")
                  .ok());
  rom::ScopedWriteFence fence_scope(rom_.get(), &payload_only);
  const auto status = ApplyDungeonStreamWritePlan(rom_.get(), *plan);
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_EQ(rom_->dirty(), was_dirty);
}

}  // namespace
}  // namespace yaze::zelda3::test
